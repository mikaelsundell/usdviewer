// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdstagemodel.h"
#include "usdqtutils.h"
#include <QMap>
#include <QThreadPool>
#include <QVariant>
#include <QtConcurrent>
#include <pxr/base/tf/weakBase.h>
#include <pxr/usd/usd/notice.h>
#include <pxr/usd/usd/variantSets.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <stack>

namespace usd {
class StageModelPrivate : public QSharedData {
public:
    StageModelPrivate();
    ~StageModelPrivate();
    void initStage();
    bool loadFromFile(const QString& filename, StageModel::load_policy loadPolicy);
    bool loadPayloads(const QList<SdfPath>& paths, const QString& variantSet, const QString& variantValue);
    bool unloadPayloads(const QList<SdfPath>& paths);
    void cancelPayloads();
    bool saveToFile(const QString& filename);
    bool exportPathsToFile(const QList<SdfPath>& paths, const QString& filename);
    bool close();
    bool reload();
    bool isLoaded() const;
    void setMask(const QList<SdfPath>& paths);
    void setVisible(const QList<SdfPath>& paths, bool visible, bool recursive);
    GfBBox3d boundingBox();
    GfBBox3d boundingBox(const QList<SdfPath>& paths);
    std::map<std::string, std::vector<std::string>> variantSets(const QList<SdfPath>& paths, bool recursive);

public:
    void updatePrims(const QList<SdfPath> paths);
    void updateStage();

public:
    class StageWatcher : public TfWeakBase {
    public:
        StageWatcher(StageModelPrivate* parent)
            : d { TfNotice::Key(), parent }
        {}
        void init()
        {
            if (d.key.IsValid())
                TfNotice::Revoke(d.key);
        }
        void objectsChanged(const UsdNotice::ObjectsChanged& notice, const UsdStageWeakPtr& sender)
        {
            QList<SdfPath> updated;
            for (const auto& p : notice.GetResyncedPaths())
                updated.append(p);
            for (const auto& p : notice.GetChangedInfoOnlyPaths())
                updated.append(p);
            if (updated.isEmpty())
                return;
            QMetaObject::invokeMethod(
                d.parent->d.stageModel, [this, updated]() { d.parent->updatePrims(updated); }, Qt::QueuedConnection);
        }
        struct Data {
            TfNotice::Key key;
            StageModelPrivate* parent;
        };
        Data d;
    };

public:
    struct Data {
        UsdStageRefPtr stage;
        StageModel::load_policy loadPolicy;
        StageModel::stage_status stageStatus;
        QString filename;
        GfBBox3d bbox;
        QFuture<void> payloadJob;
        std::atomic<bool> cancelRequested { false };
        QList<SdfPath> mask;
        QThreadPool pool;
        QReadWriteLock stageLock;
        QScopedPointer<UsdGeomBBoxCache> bboxCache;
        QScopedPointer<StageWatcher> stageWatcher;
        QPointer<StageModel> stageModel;
    };
    Data d;
};

StageModelPrivate::StageModelPrivate()
{
    d.loadPolicy = StageModel::load_policy::load_all;
    d.pool.setMaxThreadCount(QThread::idealThreadCount());
    d.pool.setThreadPriority(QThread::HighPriority);
    d.stageWatcher.reset(new StageWatcher(this));
}

StageModelPrivate::~StageModelPrivate() {}

void
StageModelPrivate::initStage()
{
    d.stageStatus = StageModel::stage_status::stage_loaded;
    d.bboxCache.reset();
    d.bbox = boundingBox();
    d.stageWatcher->init();
    d.stageWatcher->d.key = TfNotice::Register(TfWeakPtr<StageWatcher>(d.stageWatcher.data()),
                                               &StageWatcher::objectsChanged, d.stage);
}

bool
StageModelPrivate::loadFromFile(const QString& filename, StageModel::load_policy policy)
{
    {
        QWriteLocker locker(&d.stageLock);
        if (policy == StageModel::load_all) {
            d.stage = UsdStage::Open(filename.toStdString(), UsdStage::LoadAll);
        }
        else {
            d.stage = UsdStage::Open(filename.toStdString(),
                                     UsdStage::LoadNone);  // load stage without loading payloads
        }
        d.loadPolicy = policy;
        d.mask = QList<SdfPath>();
    }
    if (d.stage) {
        initStage();
        d.filename = filename;
    }
    else {
        d.stageStatus = StageModel::stage_status::stage_failed;
        d.bboxCache.reset();
        return false;
    }
    QMetaObject::invokeMethod(
        d.stageModel,
        [this]() {
            setMask(d.mask);
            updateStage();
        },
        Qt::QueuedConnection);
    return true;
}

bool
StageModelPrivate::loadPayloads(const QList<SdfPath>& paths, const QString& variantSet, const QString& variantValue)
{
    // this function expects *prim paths that directly contain payloads.
    // it does NOT recursively load payloads on child prims and it does NOT
    // accept higher-level ancestor prims that merely contain payloads deeper
    // in the hierarchy.

    const bool useVariant = (!variantSet.isEmpty() && !variantValue.isEmpty());
    Q_EMIT d.stageModel->payloadsRequested(paths, StageModel::payload_loaded);

    auto stage = d.stage;
    auto pool = &d.pool;

    d.cancelRequested = false;

    d.payloadJob = QtConcurrent::run(pool, [this, stage, paths, variantSet, variantValue, useVariant]() {
        QList<SdfPath> loaded;
        QList<SdfPath> failed;

        std::string setNameStd;
        std::string setValueStd;

        if (useVariant) {
            setNameStd = QStringToString(variantSet);
            setValueStd = QStringToString(variantValue);
        }
        {
            QWriteLocker locker(&d.stageLock);
            for (const SdfPath& path : paths) {
                if (d.cancelRequested)
                    break;

                UsdPrim prim = stage->GetPrimAtPath(path);
                if (!prim) {
                    failed.append(path);
                    Q_EMIT d.stageModel->payloadChanged(path, StageModel::payload_failed);
                    continue;
                }

                if (!prim.HasPayload()) {
                    failed.append(path);
                    Q_EMIT d.stageModel->payloadChanged(path, StageModel::payload_failed);
                    continue;
                }

                try {
                    if (useVariant) {
                        if (prim.IsLoaded())
                            prim.Unload();

                        UsdVariantSet vs = prim.GetVariantSet(setNameStd);
                        if (!vs.IsValid()) {
                            failed.append(path);
                            Q_EMIT d.stageModel->payloadChanged(path, StageModel::payload_failed);
                            continue;
                        }
                        auto variants = vs.GetVariantNames();
                        if (std::find(variants.begin(), variants.end(), setValueStd) == variants.end()) {
                            failed.append(path);
                            Q_EMIT d.stageModel->payloadChanged(path, StageModel::payload_failed);
                            continue;
                        }

                        vs.SetVariantSelection(setValueStd);
                    }

                    if (prim.IsLoaded()) {
                        loaded.append(path);
                        Q_EMIT d.stageModel->payloadChanged(path, StageModel::payload_loaded);
                        continue;
                    }

                    prim.Load();
                    loaded.append(path);
                    Q_EMIT d.stageModel->payloadChanged(path, StageModel::payload_loaded);
                } catch (const std::exception& e) {
                    failed.append(path);
                    Q_EMIT d.stageModel->payloadChanged(path, StageModel::payload_failed);
                }
            }
        }

        if (!loaded.isEmpty()) {
            QMetaObject::invokeMethod(
                d.stageModel,
                [this, loaded]() {
                    d.bboxCache.reset();
                    d.bbox = boundingBox();
                    updatePrims(loaded);
                },
                Qt::QueuedConnection);
        }
    });
    return true;
}

bool
StageModelPrivate::unloadPayloads(const QList<SdfPath>& paths)
{
    if (!isLoaded())
        return false;

    Q_EMIT d.stageModel->payloadsRequested(paths, StageModel::payload_unloaded);

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

                Q_EMIT d.stageModel->payloadChanged(path, StageModel::payload_unloaded);
            }
        }
        QMetaObject::invokeMethod(
            d.stageModel,
            [this, unloaded]() {
                if (!unloaded.isEmpty()) {
                    {
                        d.bboxCache.reset();
                        d.bbox = boundingBox();
                    }
                    updatePrims(unloaded);
                }
            },
            Qt::QueuedConnection);
    });
    return true;
}

void
StageModelPrivate::cancelPayloads()
{
    if (!d.payloadJob.isRunning())
        return;
    d.cancelRequested = true;
}

bool
StageModelPrivate::saveToFile(const QString& filename)
{
    if (!isLoaded())
        return false;

    QWriteLocker locker(&d.stageLock);
    try {
        SdfLayerHandle rootLayer = d.stage->GetRootLayer();
        if (!rootLayer) {
            return false;
        }
        if (rootLayer->IsAnonymous()) {
            return d.stage->Export(filename.toStdString());
        }
        if (QFileInfo(d.filename).canonicalFilePath() == QFileInfo(filename).canonicalFilePath()) {
            d.stage->Save();
            return true;
        }
        return d.stage->Export(filename.toStdString());
    } catch (const std::exception& e) {
        return false;
    }
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
        d.stageModel, [this]() { updateStage(); }, Qt::QueuedConnection);
    return true;
}

bool
StageModelPrivate::reload()
{
    {
        QWriteLocker locker(&d.stageLock);
        if (d.stage) {
            d.stage->Reload();
            d.bboxCache.reset();
            d.bbox = boundingBox();
        }
    }
    QMetaObject::invokeMethod(
        d.stageModel, [this]() { updateStage(); }, Qt::QueuedConnection);

    return true;
}

bool
StageModelPrivate::isLoaded() const
{
    return d.stage != nullptr;
}

void
StageModelPrivate::setMask(const QList<SdfPath>& paths)
{
    d.mask = paths;
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
StageModelPrivate::boundingBox(const QList<SdfPath>& paths)
{
    QReadLocker locker(&d.stageLock);
    Q_ASSERT("stage is not loaded" && isLoaded());
    UsdGeomBBoxCache cache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens(), true);
    GfBBox3d bbox;
    for (const SdfPath& path : paths) {
        UsdPrim prim = d.stage->GetPrimAtPath(path);
        if (!prim || !prim.IsA<UsdGeomImageable>())
            continue;

        bbox = GfBBox3d::Combine(bbox, cache.ComputeWorldBound(prim));
    }
    return bbox;
}

void
StageModelPrivate::updatePrims(const QList<SdfPath> paths)
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
                }
                else {
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
StageModelPrivate::updateStage()
{
    Q_EMIT d.stageModel->stageChanged(d.stage, d.loadPolicy, d.stageStatus);
    Q_EMIT d.stageModel->boundingBoxChanged(d.bbox);
}

StageModel::StageModel()
    : p(new StageModelPrivate())
{
    p->d.stageModel = this;
}

StageModel::StageModel(const QString& filename, StageModel::load_policy loadPolicy)
    : p(new StageModelPrivate())
{
    p->d.stageModel = this;
    loadFromFile(filename, loadPolicy);
}

StageModel::StageModel(const StageModel& other)
    : p(other.p)
{}

StageModel::~StageModel() {}

bool
StageModel::loadFromFile(const QString& filename, StageModel::load_policy loadPolicy)
{
    return p->loadFromFile(filename, loadPolicy);
}

bool
StageModel::loadPayloads(const QList<SdfPath>& paths, const QString& variantSet, const QString& variantValue)
{
    return p->loadPayloads(paths, variantSet, variantValue);
}

bool
StageModel::unloadPayloads(const QList<SdfPath>& paths)
{
    return p->unloadPayloads(paths);
}

void
StageModel::cancelPayloads()
{
    p->cancelPayloads();
}

bool
StageModel::saveToFile(const QString& filename)
{
    return p->saveToFile(filename);
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
    p->setMask(paths);
}

GfBBox3d
StageModel::boundingBox()
{
    return p->boundingBox();
}

StageModel::load_policy
StageModel::loadPolicy() const
{
    return p->d.loadPolicy;
}

GfBBox3d
StageModel::boundingBox(const QList<SdfPath>& paths)
{
    return p->boundingBox(paths);
}

QString
StageModel::filename() const
{
    return p->d.filename;
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
