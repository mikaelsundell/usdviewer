// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "datamodel.h"
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
class DataModelPrivate : public QSharedData {
public:
    DataModelPrivate();
    ~DataModelPrivate();
    void initStage();
    bool loadFromFile(const QString& filename, DataModel::load_policy loadPolicy);
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
        StageWatcher(DataModelPrivate* parent)
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
                d.parent->d.dataModel, [this, updated]() { d.parent->updatePrims(updated); }, Qt::QueuedConnection);
        }
        struct Data {
            TfNotice::Key key;
            DataModelPrivate* parent;
        };
        Data d;
    };

public:
    struct Data {
        UsdStageRefPtr stage;
        DataModel::load_policy loadPolicy;
        DataModel::stage_status stageStatus;
        QString filename;
        GfBBox3d bbox;
        QFuture<void> payloadJob;
        std::atomic<bool> cancelRequested { false };
        QList<SdfPath> mask;
        QThreadPool pool;
        QReadWriteLock stageLock;
        QScopedPointer<UsdGeomBBoxCache> bboxCache;
        QScopedPointer<StageWatcher> stageWatcher;
        QPointer<DataModel> dataModel;
    };
    Data d;
};

DataModelPrivate::DataModelPrivate()
{
    d.loadPolicy = DataModel::load_policy::load_all;
    d.pool.setMaxThreadCount(QThread::idealThreadCount());
    d.pool.setThreadPriority(QThread::HighPriority);
    d.stageWatcher.reset(new StageWatcher(this));
}

DataModelPrivate::~DataModelPrivate() {}

void
DataModelPrivate::initStage()
{
    d.stageStatus = DataModel::stage_status::stage_loaded;
    d.bboxCache.reset();
    d.bbox = boundingBox();
    d.stageWatcher->init();
    d.stageWatcher->d.key = TfNotice::Register(TfWeakPtr<StageWatcher>(d.stageWatcher.data()),
                                               &StageWatcher::objectsChanged, d.stage);
}

bool
DataModelPrivate::loadFromFile(const QString& filename, DataModel::load_policy policy)
{
    {
        QWriteLocker locker(&d.stageLock);
        if (policy == DataModel::load_all) {
            d.stage = UsdStage::Open(QStringToString(filename), UsdStage::LoadAll);
        }
        else {
            d.stage = UsdStage::Open(QStringToString(filename),
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
        d.stageStatus = DataModel::stage_status::stage_failed;
        d.bboxCache.reset();
        return false;
    }
    QMetaObject::invokeMethod(
        d.dataModel,
        [this]() {
            setMask(d.mask);
            updateStage();
        },
        Qt::QueuedConnection);
    return true;
}

bool
DataModelPrivate::loadPayloads(const QList<SdfPath>& paths, const QString& variantSet, const QString& variantValue)
{
    // this function expects *prim paths that directly contain payloads.
    // it does NOT recursively load payloads on child prims and it does NOT
    // accept higher-level ancestor prims that merely contain payloads deeper
    // in the hierarchy.

    const bool useVariant = (!variantSet.isEmpty() && !variantValue.isEmpty());
    Q_EMIT d.dataModel->payloadsRequested(paths, DataModel::payload_loaded);

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
                qDebug() << "loadPayloads: loading path: " << path;


                if (d.cancelRequested)
                    break;

                UsdPrim prim = stage->GetPrimAtPath(path);
                if (!prim) {
                    failed.append(path);
                    Q_EMIT d.dataModel->payloadChanged(path, DataModel::payload_failed);
                    continue;
                }

                if (!prim.HasPayload()) {
                    failed.append(path);
                    Q_EMIT d.dataModel->payloadChanged(path, DataModel::payload_failed);
                    continue;
                }

                try {
                    if (useVariant) {
                        if (prim.IsLoaded())
                            prim.Unload();

                        UsdVariantSet vs = prim.GetVariantSet(setNameStd);
                        if (!vs.IsValid()) {
                            failed.append(path);
                            Q_EMIT d.dataModel->payloadChanged(path, DataModel::payload_failed);
                            continue;
                        }
                        auto variants = vs.GetVariantNames();
                        if (std::find(variants.begin(), variants.end(), setValueStd) == variants.end()) {
                            failed.append(path);
                            Q_EMIT d.dataModel->payloadChanged(path, DataModel::payload_failed);
                            continue;
                        }

                        vs.SetVariantSelection(setValueStd);
                    }

                    if (prim.IsLoaded()) {
                        loaded.append(path);
                        Q_EMIT d.dataModel->payloadChanged(path, DataModel::payload_loaded);
                        continue;
                    }

                    prim.Load();
                    loaded.append(path);
                    Q_EMIT d.dataModel->payloadChanged(path, DataModel::payload_loaded);
                } catch (const std::exception& e) {
                    failed.append(path);
                    Q_EMIT d.dataModel->payloadChanged(path, DataModel::payload_failed);
                }
            }
        }

        QMetaObject::invokeMethod(
            d.dataModel,
            [this, loaded]() {
                {
                    QWriteLocker locker(&d.stageLock);
                    d.bboxCache.reset();
                }
                d.bbox = boundingBox();
                updatePrims(loaded);
            },
            Qt::QueuedConnection);
    });
    return true;
}

bool
DataModelPrivate::unloadPayloads(const QList<SdfPath>& paths)
{
    if (!isLoaded())
        return false;

    Q_EMIT d.dataModel->payloadsRequested(paths, DataModel::payload_unloaded);

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

                Q_EMIT d.dataModel->payloadChanged(path, DataModel::payload_unloaded);
            }
        }
        QMetaObject::invokeMethod(
            d.dataModel,
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
DataModelPrivate::cancelPayloads()
{
    if (!d.payloadJob.isRunning())
        return;
    d.cancelRequested = true;
}

bool
DataModelPrivate::saveToFile(const QString& filename)
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
            return d.stage->Export(QStringToString(filename));
        }
        if (QFileInfo(d.filename).canonicalFilePath() == QFileInfo(filename).canonicalFilePath()) {
            d.stage->Save();
            return true;
        }
        return d.stage->Export(QStringToString(filename));
    } catch (const std::exception& e) {
        return false;
    }
}

bool
DataModelPrivate::exportPathsToFile(const QList<SdfPath>& paths, const QString& filename)
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
    return maskedStage->Export(QStringToString(filename));
}

bool
DataModelPrivate::close()
{
    {
        QWriteLocker locker(&d.stageLock);
        d.stage = nullptr;
        d.bboxCache.reset();
    }
    QMetaObject::invokeMethod(
        d.dataModel, [this]() { updateStage(); }, Qt::QueuedConnection);
    return true;
}

bool
DataModelPrivate::reload()
{
    {
        QWriteLocker locker(&d.stageLock);
        if (d.stage) {
            d.stage->Reload();
            d.bboxCache.reset();
        }
    }
    d.bbox = boundingBox();
    QMetaObject::invokeMethod(
        d.dataModel, [this]() { updateStage(); }, Qt::QueuedConnection);

    return true;
}

bool
DataModelPrivate::isLoaded() const
{
    return d.stage != nullptr;
}

void
DataModelPrivate::setMask(const QList<SdfPath>& paths)
{
    {
        QWriteLocker locker(&d.stageLock);
        d.mask = paths;
    }
    Q_EMIT d.dataModel->maskChanged(paths);
}

GfBBox3d
DataModelPrivate::boundingBox()
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
DataModelPrivate::boundingBox(const QList<SdfPath>& paths)
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
DataModelPrivate::updatePrims(const QList<SdfPath> paths)
{
    Q_EMIT d.dataModel->primsChanged(paths);
    Q_EMIT d.dataModel->boundingBoxChanged(d.bbox);
    if (!d.mask.isEmpty()) {
        QList<SdfPath> newMask;
        bool updated = false;
        {
            QReadLocker locker(&d.stageLock);
            for (const SdfPath& path : d.mask) {
                UsdPrim prim = d.stage->GetPrimAtPath(path);
                if (prim && prim.IsValid() && prim.IsActive()) {
                    newMask.append(path);
                }
                else {
                    updated = true;
                }
            }
        }
        if (updated) {
            {
                QWriteLocker locker(&d.stageLock);
                d.mask = newMask;
            }
            Q_EMIT d.dataModel->maskChanged(newMask);
        }
    }
}

void
DataModelPrivate::updateStage()
{
    Q_EMIT d.dataModel->stageChanged(d.stage, d.loadPolicy, d.stageStatus);
    Q_EMIT d.dataModel->boundingBoxChanged(d.bbox);
}

DataModel::DataModel()
    : p(new DataModelPrivate())
{
    p->d.dataModel = this;
}

DataModel::DataModel(const QString& filename, DataModel::load_policy loadPolicy)
    : p(new DataModelPrivate())
{
    p->d.dataModel = this;
    loadFromFile(filename, loadPolicy);
}

DataModel::DataModel(const DataModel& other)
    : p(other.p)
{}

DataModel::~DataModel() {}

bool
DataModel::loadFromFile(const QString& filename, DataModel::load_policy loadPolicy)
{
    return p->loadFromFile(filename, loadPolicy);
}

bool
DataModel::loadPayloads(const QList<SdfPath>& paths, const QString& variantSet, const QString& variantValue)
{
    return p->loadPayloads(paths, variantSet, variantValue);
}

bool
DataModel::unloadPayloads(const QList<SdfPath>& paths)
{
    return p->unloadPayloads(paths);
}

void
DataModel::cancelPayloads()
{
    p->cancelPayloads();
}

bool
DataModel::saveToFile(const QString& filename)
{
    return p->saveToFile(filename);
}

bool
DataModel::exportToFile(const QString& filename)
{
    QReadLocker locker(&p->d.stageLock);
    if (!p->d.stage)
        return false;
    return p->d.stage->Export(QStringToString(filename));
}

bool
DataModel::exportPathsToFile(const QList<SdfPath>& paths, const QString& filename)
{
    return p->exportPathsToFile(paths, filename);
}

bool
DataModel::reload()
{
    return p->reload();
}

bool
DataModel::close()
{
    return p->close();
}

bool
DataModel::isLoaded() const
{
    return p->isLoaded();
}

void
DataModel::setMask(const QList<SdfPath>& paths)
{
    p->setMask(paths);
}

GfBBox3d
DataModel::boundingBox()
{
    return p->boundingBox();
}

DataModel::load_policy
DataModel::loadPolicy() const
{
    return p->d.loadPolicy;
}

GfBBox3d
DataModel::boundingBox(const QList<SdfPath>& paths)
{
    return p->boundingBox(paths);
}

QString
DataModel::filename() const
{
    return p->d.filename;
}

UsdStageRefPtr
DataModel::stage() const
{
    Q_ASSERT("stage is not loaded" && isLoaded());
    return p->d.stage;
}

QReadWriteLock*
DataModel::stageLock() const
{
    return &p->d.stageLock;
}

}  // namespace usd
