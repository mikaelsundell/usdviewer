// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "datamodel.h"
#include "usdqtutils.h"
#include "usdstageutils.h"
#include <QMap>
#include <QThreadPool>
#include <QVariant>
#include <QtConcurrent>
#include <pxr/base/tf/weakBase.h>
#include <pxr/usd/usd/notice.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <stack>

namespace usd {
class DataModelPrivate : public QSharedData {
public:
    DataModelPrivate();
    ~DataModelPrivate();
    void initStage();
    void beginProgressBlock(const QString& name, size_t count);
    void updateProgressNotify(const DataModel::Notify& notify, size_t completed);
    void cancelProgressBlock();
    void endProgressBlock();
    bool isProgressBlockCancelled() const;
    bool loadFromFile(const QString& filename, DataModel::load_policy loadPolicy);
    bool saveToFile(const QString& filename);
    bool exportPathsToFile(const QList<SdfPath>& paths, const QString& filename);
    bool close();
    bool reload();
    bool isLoaded() const;
    void setMask(const QList<SdfPath>& paths);
    GfBBox3d boundingBox();

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
                d.parent->d.dataModel, [this, updated]() { d.parent->updatePrims(updated); }, Qt::DirectConnection);
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
        QString changeName;
        size_t changeDepth;
        size_t expectedChanges;
        size_t completedChanges;
        std::atomic<bool> changeCancelled { false };
        QString filename;
        GfBBox3d bbox;
        QFuture<void> payloadJob;
        QList<SdfPath> pendingPaths;
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
    d.changeDepth = 0;
    d.expectedChanges = 0;
    d.completedChanges = 0;
    d.pendingPaths.clear();
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
    d.pendingPaths.clear();
}

void
DataModelPrivate::beginProgressBlock(const QString& name, size_t count)
{
    d.changeCancelled.store(false);
    d.changeName = name;
    d.changeDepth++;
    if (d.changeDepth == 1) {
        d.expectedChanges = count;
        d.completedChanges = 0;
        Q_EMIT d.dataModel->progressBlockChanged(name, DataModel::progress_mode::progress_running);
    }
}

void
DataModelPrivate::updateProgressNotify(const DataModel::Notify& notify, size_t completed)
{
    d.completedChanges = completed;
    Q_EMIT d.dataModel->progressNotifyChanged(notify, completed, d.expectedChanges);
}

void
DataModelPrivate::cancelProgressBlock()
{
    d.changeCancelled.store(true);
}

void
DataModelPrivate::endProgressBlock()
{
    if (d.changeDepth == 0)
        return;

    d.changeDepth--;
    if (d.changeDepth > 0)
        return;

    bool cancelled = d.changeCancelled.load();
    d.changeCancelled.store(false);

    Q_EMIT d.dataModel->progressBlockChanged(d.changeName, DataModel::progress_mode::progress_idle);
    d.changeName.clear();

    if (cancelled) {
        d.pendingPaths.clear();
        return;
    }

    if (d.pendingPaths.isEmpty())
        return;

    QList<SdfPath> unique;
    QSet<SdfPath> set;
    for (const SdfPath& p : d.pendingPaths) {
        if (!set.contains(p)) {
            set.insert(p);
            unique.append(p);
        }
    }
    d.pendingPaths.clear();
    d.bboxCache.reset();
    d.bbox = boundingBox();

    Q_EMIT d.dataModel->primsChanged(unique);
    Q_EMIT d.dataModel->boundingBoxChanged(d.bbox);
}

bool
DataModelPrivate::isProgressBlockCancelled() const
{
    return d.changeCancelled.load();
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
        d.pendingPaths.clear();
        d.changeDepth = 0;
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
        return usd::boundingBox(d.stage, d.mask);
    }
}

void
DataModelPrivate::updatePrims(const QList<SdfPath> paths)
{
    if (d.changeDepth > 0) {
        d.pendingPaths.append(paths);
        return;
    }
    Q_EMIT d.dataModel->primsChanged(paths);
    Q_EMIT d.dataModel->boundingBoxChanged(d.bbox);
    if (!d.mask.isEmpty()) {
        QList<SdfPath> newMask;
        bool updated = false;

        QReadLocker locker(&d.stageLock);
        for (const SdfPath& path : d.mask) {
            UsdPrim prim = d.stage->GetPrimAtPath(path);
            if (prim && prim.IsValid() && prim.IsActive())
                newMask.append(path);
            else
                updated = true;
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

void
DataModel::beginProgressBlock(const QString& name, size_t count)
{
    p->beginProgressBlock(name, count);
}

void
DataModel::updateProgressNotify(const Notify& notify, size_t completed)
{
    p->updateProgressNotify(notify, completed);
}

void
DataModel::cancelProgressBlock()
{
    return p->cancelProgressBlock();
}

void
DataModel::endProgressBlock()
{
    p->endProgressBlock();
}

bool
DataModel::isProgressBlockCancelled() const
{
    return p->isProgressBlockCancelled();
}

bool
DataModel::loadFromFile(const QString& filename, DataModel::load_policy loadPolicy)
{
    return p->loadFromFile(filename, loadPolicy);
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
