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
    void setVisible(const QList<SdfPath>& paths, bool visible);
    bool close();
    bool reload();
    bool isLoaded() const;
    GfBBox3d boundingBox() const;
    GfBBox3d boundingBox(const QList<SdfPath> paths) const;

    struct Data {
        UsdStageRefPtr stage;
        StageModel::load_type loadType;
        GfBBox3d bbox;
        QThreadPool pool;
        QPointer<StageModel> stageModel;
    };
    Data d;
};

StageModelPrivate::StageModelPrivate()
{
    d.loadType = StageModel::load_type::load_none;
    d.pool.setMaxThreadCount(4 /*QThread::idealThreadCount()*/);
    d.pool.setThreadPriority(QThread::HighPriority);
}

StageModelPrivate::~StageModelPrivate() {}

bool
StageModelPrivate::loadFromFile(const QString& filename, StageModel::load_type loadType)
{
    if (d.loadType == StageModel::load_type::load_all) {
        d.stage = UsdStage::Open(filename.toStdString(), UsdStage::LoadAll);
    }
    else {
        d.stage = UsdStage::Open(filename.toStdString(), UsdStage::LoadNone);
    }
    d.loadType = loadType;
    if (d.stage) {
        d.bbox = boundingBox();
        Q_EMIT d.stageModel->boundingBoxChanged(d.bbox);
        Q_EMIT d.stageModel->stageChanged();
        return true;
    }
    else {
        return false;
    }
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

        for (const SdfPath& path : paths) {
            UsdPrim prim = d.stage->GetPrimAtPath(path);
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
        QMetaObject::invokeMethod(
            d.stageModel,
            [this, loaded]() {
                if (!loaded.isEmpty()) {
                    GfBBox3d bbox = boundingBox();
                    if (bbox != d.bbox) {
                        d.bbox = bbox;
                        Q_EMIT d.stageModel->boundingBoxChanged(bbox);
                    }
                    Q_EMIT d.stageModel->primsChanged(loaded);
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
        for (const SdfPath& path : paths) {
            UsdPrim prim = stage->GetPrimAtPath(path);
            if (!prim)
                continue;

            prim.Unload();
            for (const UsdPrim& child : prim.GetAllDescendants())
                child.Unload();

            Q_EMIT d.stageModel->payloadsUnloaded(path);
        }
        QMetaObject::invokeMethod(
            d.stageModel,
            [this, paths]() {
                GfBBox3d bbox = boundingBox();
                if (bbox != d.bbox) {
                    d.bbox = bbox;
                    Q_EMIT d.stageModel->boundingBoxChanged(bbox);
                }
                Q_EMIT d.stageModel->primsChanged(paths);
            },
            Qt::QueuedConnection);
    });
    return true;
}

bool
StageModelPrivate::exportPathsToFile(const QList<SdfPath>& paths, const QString& filename)
{
    UsdStagePopulationMask mask;
    for (const SdfPath& path : paths) {
        bool isChildOfAnother = false;
        for (const SdfPath& other : paths) {
            if (path.HasPrefix(other) && path != other) {
                isChildOfAnother = true;
                break;
            }
        }
        if (!isChildOfAnother) {
            mask.Add(path);
        }
    }
    if (mask.GetPaths().empty()) {
        return false;
    }
    UsdStageRefPtr maskedStage = UsdStage::OpenMasked(d.stage->GetRootLayer(), mask);
    if (!maskedStage) {
        return false;
    }
    maskedStage->ExpandPopulationMask();
    return maskedStage->Export(filename.toStdString());
}

void
StageModelPrivate::setVisible(const QList<SdfPath>& paths, bool visible)
{
    Q_ASSERT("stage is not loaded" && isLoaded());
    for (const SdfPath& path : paths) {
        UsdPrim prim = d.stage->GetPrimAtPath(path);
        UsdGeomImageable imageable(prim);
        if (visible) {
            imageable.MakeVisible();
        }
        else {
            imageable.MakeInvisible();
        }
    }
    Q_EMIT d.stageModel->primsChanged(paths);
}

bool
StageModelPrivate::close()
{
    try {
        d.stage = nullptr;
        Q_EMIT d.stageModel->stageChanged();
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool
StageModelPrivate::reload()
{
    try {
        if (d.stage) {
            d.stage->Reload();
            Q_EMIT d.stageModel->stageChanged();
        }
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool
StageModelPrivate::isLoaded() const
{
    return d.stage != nullptr;
}

GfBBox3d
StageModelPrivate::boundingBox() const
{
    Q_ASSERT("stage is not loaded" && isLoaded());
    UsdGeomBBoxCache bboxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens());
    return bboxCache.ComputeWorldBound(d.stage->GetPseudoRoot());
}

GfBBox3d
StageModelPrivate::boundingBox(const QList<SdfPath> paths) const
{
    Q_ASSERT("stage is not loaded" && isLoaded());
    UsdGeomBBoxCache bboxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens());
    GfBBox3d bbox;
    for (const SdfPath& path : paths) {
        UsdPrim prim = d.stage->GetPrimAtPath(path);
        if (!prim || !prim.IsA<UsdGeomImageable>()) {
            continue;
        }
        GfBBox3d worldbbox = bboxCache.ComputeWorldBound(prim);
        bbox = GfBBox3d::Combine(bbox, worldbbox);
    }
    return bbox;
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

void
StageModel::setVisible(const QList<SdfPath>& paths, bool visible)
{
    p->setVisible(paths, visible);
}

bool
StageModel::exportToFile(const QString& filename)
{
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

GfBBox3d
StageModel::boundingBox() const
{
    return p->boundingBox();
}

StageModel::load_type
StageModel::loadType() const
{
    return p->d.loadType;
}

GfBBox3d
StageModel::boundingBox(const QList<SdfPath> paths) const
{
    return p->boundingBox(paths);
}

UsdStageRefPtr
StageModel::stage() const
{
    Q_ASSERT("stage is not loaded" && isLoaded());
    return p->d.stage;
}
}  // namespace usd
