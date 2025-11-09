// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdstagemodel.h"
#include <QMap>
#include <QThreadPool>
#include <QVariant>
#include <QtConcurrent>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/metrics.h>

namespace usd {

class StageModelPrivate : public QSharedData {
public:
    StageModelPrivate();
    ~StageModelPrivate();
    bool loadFromFile(const QString& filename, StageModel::load_type loadType);
    bool loadPayloads(const QList<SdfPath>& paths);
    bool unloadPayloads(const QList<SdfPath>& paths);
    bool exportPathsToFile(const QList<SdfPath>& paths, const QString& filename);
    bool close();
    bool reload();
    bool isLoaded() const;
    void setMask(const QList<SdfPath>& paths);
    void setVisible(const QList<SdfPath>& paths, bool visible, bool recursive);
    GfBBox3d boundingBox();
    GfBBox3d boundingBox(const QList<SdfPath> paths);
    
public:
    void primsChanged(const QList<SdfPath> paths);
    void stageChanged();
    struct Data {
        UsdStageRefPtr stage;
        StageModel::load_type loadType;
        GfBBox3d bbox;
        QList<SdfPath> mask;
        QThreadPool pool;
        QReadWriteLock stageLock;
        QScopedPointer<UsdGeomBBoxCache> bboxCache;
        QPointer<StageModel> stageModel;
    };
    Data d;
};

StageModelPrivate::StageModelPrivate()
{
    d.loadType = StageModel::load_type::load_none;
    d.pool.setMaxThreadCount(QThread::idealThreadCount());
    d.pool.setThreadPriority(QThread::HighPriority);
}

StageModelPrivate::~StageModelPrivate() {}

bool
StageModelPrivate::loadFromFile(const QString& filename, StageModel::load_type loadType)
{
    {
        QWriteLocker locker(&d.stageLock);

        if (loadType == StageModel::load_type::load_all)
            d.stage = UsdStage::Open(filename.toStdString(), UsdStage::LoadAll);
        else
            d.stage = UsdStage::Open(filename.toStdString(), UsdStage::LoadNone);

        d.loadType = loadType;

        if (d.stage) {
            d.bboxCache.reset(new UsdGeomBBoxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens(),
                                                   true));  // use extents hint
            d.bbox = d.bboxCache->ComputeWorldBound(d.stage->GetPseudoRoot());
        }
        else {
            d.bboxCache.reset();
            return false;
        }
    }
    QMetaObject::invokeMethod(
        d.stageModel,
        [this]() {
            stageChanged();
        },
        Qt::QueuedConnection);
    return true;
}

bool
StageModelPrivate::loadPayloads(const QList<SdfPath>& paths)
{
    if (!isLoaded())
        return false;

    Q_EMIT d.stageModel->payloadsRequested(paths);

    auto stage = d.stage;
    auto pool = &d.pool;

    QFuture<void> future = QtConcurrent::run(pool, [this, stage, paths]() {
        QList<SdfPath> failed;
        QList<SdfPath> loaded;
        {
            QWriteLocker locker(&d.stageLock);
            for (const SdfPath& path : paths) {
                UsdPrim prim = stage->GetPrimAtPath(path);
                if (!prim) {
                    failed.append(path);
                    continue;
                }

                if (prim.IsLoaded()) {
                    loaded.append(path);
                    Q_EMIT d.stageModel->payloadsLoaded(path);
                    continue;
                }

                try {
                    prim.Load();
                    loaded.append(path);
                    Q_EMIT d.stageModel->payloadsLoaded(path);
                } catch (const std::exception& e) {
                    qWarning() << "failed to load prim:" << QString::fromStdString(path.GetString())
                               << "error:" << e.what();
                    Q_EMIT d.stageModel->payloadsFailed(path);
                    failed.append(path);
                }
            }
        }

        QMetaObject::invokeMethod(
            d.stageModel,
            [this, loaded]() {
                if (!loaded.isEmpty()) {
                    {
                        QWriteLocker locker(&d.stageLock);
                        d.bboxCache.reset(new UsdGeomBBoxCache(UsdTimeCode::Default(),
                                                               UsdGeomImageable::GetOrderedPurposeTokens(),
                                                               true));  // use extents hint
                        GfBBox3d bbox = d.bboxCache->ComputeWorldBound(d.stage->GetPseudoRoot());
                        d.bbox = bbox;
                    }
                    primsChanged(loaded);
                }
            },
            Qt::QueuedConnection);
    });

    return true;
}

bool
StageModelPrivate::unloadPayloads(const QList<SdfPath>& paths)
{
    if (!isLoaded())
        return false;

    Q_EMIT d.stageModel->payloadsRequested(paths);

    auto stage = d.stage;
    auto pool = &d.pool;
    QFuture<void> future = QtConcurrent::run(pool, [this, stage, paths]() {
        QList<SdfPath> unloaded;
        {
            QWriteLocker locker(&d.stageLock);
            for (const SdfPath& path : paths) {
                UsdPrim prim = stage->GetPrimAtPath(path);
                if (!prim)
                    continue;

                prim.Unload();
                unloaded.append(path);

                Q_EMIT d.stageModel->payloadsUnloaded(path);
            }
        }
        QMetaObject::invokeMethod(
            d.stageModel,
            [this, unloaded]() {
                if (!unloaded.isEmpty()) {
                    {
                        QWriteLocker locker(&d.stageLock);
                        d.bboxCache.reset(new UsdGeomBBoxCache(UsdTimeCode::Default(),
                                                               UsdGeomImageable::GetOrderedPurposeTokens(),
                                                               true));  // use extents hint
                        GfBBox3d bbox = d.bboxCache->ComputeWorldBound(d.stage->GetPseudoRoot());
                        d.bbox = bbox;
                    }
                    primsChanged(unloaded);
                }
            },
            Qt::QueuedConnection);
    });
    return true;
}

bool
StageModelPrivate::exportPathsToFile(const QList<SdfPath>& paths, const QString& filename)
{
    QReadLocker locker(&d.stageLock);
    UsdStagePopulationMask mask;
    for (const SdfPath& path : paths) {
        bool isChildOfAnother = false;
        for (const SdfPath& other : paths) {
            if (path.HasPrefix(other) && path != other) {
                isChildOfAnother = true;
                break;
            }
        }
        if (!isChildOfAnother)
            mask.Add(path);
    }
    if (mask.GetPaths().empty())
        return false;
    UsdStageRefPtr maskedStage = UsdStage::OpenMasked(d.stage->GetRootLayer(), mask);
    if (!maskedStage)
        return false;

    maskedStage->ExpandPopulationMask();
    return maskedStage->Export(filename.toStdString());
}

bool
StageModelPrivate::close()
{
    {
        QWriteLocker locker(&d.stageLock);
        d.stage = nullptr;
        d.bboxCache.reset();
    }
    QMetaObject::invokeMethod(
        d.stageModel,
        [this]() {
            stageChanged();
        },
        Qt::QueuedConnection);

    return true;
}

bool
StageModelPrivate::reload()
{
    {
        QWriteLocker locker(&d.stageLock);
        if (d.stage) {
            d.stage->Reload();
            d.bboxCache.reset(
                new UsdGeomBBoxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens(), true));
            d.bbox = d.bboxCache->ComputeWorldBound(d.stage->GetPseudoRoot());
        }
    }
    QMetaObject::invokeMethod(
        d.stageModel,
        [this]() {
            stageChanged();
        },
        Qt::QueuedConnection);

    return true;
}

bool
StageModelPrivate::isLoaded() const
{
    return d.stage != nullptr;
}

void
StageModelPrivate::setVisible(const QList<SdfPath>& paths, bool visible, bool recursive)
{
    QList<SdfPath> affected;
    {
        QWriteLocker locker(&d.stageLock);
        Q_ASSERT("stage is not loaded" && isLoaded());
        for (const SdfPath& path : paths) {
            UsdPrim prim = d.stage->GetPrimAtPath(path);
            if (!prim)
                continue;

            UsdGeomImageable imageable(prim);
            if (imageable) {
                if (visible)
                    imageable.MakeVisible();
                else
                    imageable.MakeInvisible();
                affected.append(path);
            }
            if (recursive) {
                for (const UsdPrim& child : prim.GetAllDescendants()) {
                    UsdGeomImageable childImageable(child);
                    if (!childImageable)
                        continue;
                    if (visible)
                        childImageable.MakeVisible();
                    else
                        childImageable.MakeInvisible();
                    affected.append(child.GetPath());
                }
            }
        }
    }
    if (!affected.isEmpty()) {
        QMetaObject::invokeMethod(
            d.stageModel, [this, affected]() { primsChanged(affected); }, Qt::QueuedConnection);
    }
}

void
StageModelPrivate::setMask(const QList<SdfPath>& paths)
{
    Q_EMIT d.stageModel->maskChanged(paths);
}

GfBBox3d
StageModelPrivate::boundingBox()
{
    QReadLocker locker(&d.stageLock);
    if (d.mask.isEmpty()) {
        Q_ASSERT("stage is not loaded" && isLoaded());
        if (!d.bboxCache) {
            d.bboxCache.reset(
                              new UsdGeomBBoxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens(), true));
        }
        return d.bboxCache->ComputeWorldBound(d.stage->GetPseudoRoot());
    }
    else {
        return boundingBox(d.mask);
    }
}

GfBBox3d
StageModelPrivate::boundingBox(const QList<SdfPath> paths)
{
    QReadLocker locker(&d.stageLock);
    Q_ASSERT("stage is not loaded" && isLoaded());
    if (!d.bboxCache) {
        d.bboxCache.reset(
            new UsdGeomBBoxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens(), true));
    }

    GfBBox3d bbox;
    for (const SdfPath& path : paths) {
        UsdPrim prim = d.stage->GetPrimAtPath(path);
        if (!prim || !prim.IsA<UsdGeomImageable>())
            continue;
        GfBBox3d worldbbox = d.bboxCache->ComputeWorldBound(prim);
        bbox = GfBBox3d::Combine(bbox, worldbbox);
    }
    return bbox;
}

void
StageModelPrivate::primsChanged(const QList<SdfPath> paths)
{
    Q_EMIT d.stageModel->primsChanged(paths);
    Q_EMIT d.stageModel->boundingBoxChanged(d.bbox);
    if (!d.mask.isEmpty()) {
        QList<SdfPath> newMask;
        bool changed = false;
        {
            QReadLocker locker(&d.stageLock);
            for (const SdfPath& path : d.mask) {
                UsdPrim prim = d.stage->GetPrimAtPath(path);
                if (prim && prim.IsValid() && prim.IsActive()) {
                    newMask.append(path);
                } else {
                    changed = true;
                }
            }
        }
        if (changed) {
            d.mask = newMask;
            Q_EMIT d.stageModel->maskChanged(newMask);
        }
    }
}

void
StageModelPrivate::stageChanged()
{
    Q_EMIT d.stageModel->stageChanged();
    Q_EMIT d.stageModel->boundingBoxChanged(d.bbox);
}

StageModel::StageModel()
    : p(new StageModelPrivate())
{
    p->d.stageModel = this;
}

StageModel::StageModel(const QString& filename, load_type loadType)
    : p(new StageModelPrivate())
{
    p->d.stageModel = this;
    loadFromFile(filename, loadType);
}

StageModel::StageModel(const StageModel& other)
    : p(other.p)
{}

StageModel::~StageModel() {}

bool
StageModel::loadFromFile(const QString& filename, load_type loadType)
{
    return p->loadFromFile(filename, loadType);
}

bool
StageModel::loadPayloads(const QList<SdfPath>& paths)
{
    return p->loadPayloads(paths);
}

bool
StageModel::unloadPayloads(const QList<SdfPath>& paths)
{
    return p->unloadPayloads(paths);
}

bool
StageModel::exportToFile(const QString& filename)
{
    QReadLocker locker(&p->d.stageLock);
    return p->d.stage->Export(filename.toStdString());
}

bool
StageModel::exportPathsToFile(const QList<SdfPath>& paths, const QString& filename)
{
    return p->exportPathsToFile(paths, filename);
}

bool
StageModel::reload()
{
    return p->reload();
}

bool
StageModel::close()
{
    return p->close();
}

bool
StageModel::isLoaded() const
{
    return p->isLoaded();
}

void
StageModel::setMask(const QList<SdfPath>& paths)
{
    p->d.mask = paths;
    p->setMask(paths);
}

void
StageModel::setVisible(const QList<SdfPath>& paths, bool visible, bool recursive)
{
    p->setVisible(paths, visible, recursive);
}

GfBBox3d
StageModel::boundingBox()
{
    return p->boundingBox();
}

StageModel::load_type
StageModel::loadType() const
{
    return p->d.loadType;
}

GfBBox3d
StageModel::boundingBox(const QList<SdfPath> paths)
{
    return p->boundingBox(paths);
}

UsdStageRefPtr
StageModel::stage() const
{
    QReadLocker locker(&p->d.stageLock);
    Q_ASSERT("stage is not loaded" && isLoaded());
    return p->d.stage;
}

QReadWriteLock*
StageModel::stageLock() const
{
    return &p->d.stageLock;
}

}  // namespace usd
