// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "session.h"
#include "commanddispatcher.h"
#include "qtutils.h"
#include "selectionlist.h"
#include "stageutils.h"
#include <QMap>
#include <QThreadPool>
#include <QVariant>
#include <QtConcurrent>
#include <pxr/base/tf/weakBase.h>
#include <pxr/usd/usd/notice.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <stack>

namespace usdviewer {
class SessionPrivate : public QSharedData {
public:
    SessionPrivate();
    ~SessionPrivate();
    void init();
    void initStage();
    void beginProgressBlock(const QString& name, size_t count);
    void updateProgressNotify(const Session::Notify& notify, size_t completed);
    void cancelProgressBlock();
    void endProgressBlock();
    bool isProgressBlockCancelled() const;
    bool loadFromFile(const QString& filename, Session::LoadPolicy loadPolicy);
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
        StageWatcher(SessionPrivate* parent)
            : d { false, TfNotice::Key(), parent }
        {}
        void init()
        {
            if (d.key.IsValid())
                TfNotice::Revoke(d.key);
        }
        void objectsChanged(const UsdNotice::ObjectsChanged& notice, const UsdStageWeakPtr& sender)
        {
            if (d.suppress.load())
                return;
            QList<SdfPath> updated;
            for (const auto& p : notice.GetResyncedPaths())
                updated.append(p);
            for (const auto& p : notice.GetChangedInfoOnlyPaths())
                updated.append(p);
            if (updated.isEmpty())
                return;
            QMetaObject::invokeMethod(
                d.parent->d.session, [this, updated]() { d.parent->updatePrims(updated); }, Qt::DirectConnection);
        }
        void blockSignals(bool block) { d.suppress.store(block); }
        bool signalsBlocked() const { return d.suppress.load(); }
        struct Data {
            std::atomic<bool> suppress { false };
            TfNotice::Key key;
            SessionPrivate* parent;
        };
        Data d;
    };
    class StageBlocker {
    public:
        explicit StageBlocker(StageWatcher* w)
            : watcher(w)
        {
            if (watcher)
                watcher->blockSignals(true);
        }
        ~StageBlocker()
        {
            if (watcher)
                watcher->blockSignals(false);
        }

    private:
        StageWatcher* watcher = nullptr;
    };

public:
    struct Data {
        UsdStageRefPtr stage;
        Session::LoadPolicy loadPolicy;
        Session::StageStatus stageStatus;
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
        QScopedPointer<CommandStack> commandStack;
        QScopedPointer<SelectionList> selectionList;
        QScopedPointer<StageWatcher> stageWatcher;
        QPointer<Session> session;
    };
    Data d;
};

SessionPrivate::SessionPrivate()
{
    d.loadPolicy = Session::LoadPolicy::All;
    d.changeDepth = 0;
    d.expectedChanges = 0;
    d.completedChanges = 0;
    d.pendingPaths.clear();
    d.pool.setMaxThreadCount(QThread::idealThreadCount());
    d.pool.setThreadPriority(QThread::HighPriority);
    d.stageWatcher.reset(new StageWatcher(this));
}

SessionPrivate::~SessionPrivate() = default;

void
SessionPrivate::init()
{
    d.commandStack.reset(new CommandStack());
    d.selectionList.reset(new SelectionList());
    CommandDispatcher::setCommandStack(d.commandStack.data());
}

void
SessionPrivate::initStage()
{
    d.stageStatus = Session::StageStatus::Loaded;
    d.bboxCache.reset();
    d.bbox = boundingBox();
    d.stageWatcher->init();
    d.stageWatcher->d.key = TfNotice::Register(TfWeakPtr<StageWatcher>(d.stageWatcher.data()),
                                               &StageWatcher::objectsChanged, d.stage);
    d.pendingPaths.clear();
}

void
SessionPrivate::beginProgressBlock(const QString& name, size_t count)
{
    d.changeCancelled.store(false);
    d.changeName = name;
    d.changeDepth++;
    if (d.changeDepth == 1) {
        d.expectedChanges = count;
        d.completedChanges = 0;
        Q_EMIT d.session->progressBlockChanged(name, Session::ProgressMode::Running);
    }
}

void
SessionPrivate::updateProgressNotify(const Session::Notify& notify, size_t completed)
{
    d.completedChanges = completed;
    Q_EMIT d.session->progressNotifyChanged(notify, completed, d.expectedChanges);
}

void
SessionPrivate::cancelProgressBlock()
{
    d.changeCancelled.store(true);
}

void
SessionPrivate::endProgressBlock()
{
    if (d.changeDepth == 0)
        return;

    d.changeDepth--;
    if (d.changeDepth > 0)
        return;

    bool cancelled = d.changeCancelled.load();
    d.changeCancelled.store(false);

    Q_EMIT d.session->progressBlockChanged(d.changeName, Session::ProgressMode::Idle);
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

    Q_EMIT d.session->primsChanged(unique);
    Q_EMIT d.session->boundingBoxChanged(d.bbox);
}

bool
SessionPrivate::isProgressBlockCancelled() const
{
    return d.changeCancelled.load();
}

bool
SessionPrivate::loadFromFile(const QString& filename, Session::LoadPolicy policy)
{
    {
        QWriteLocker locker(&d.stageLock);
        StageBlocker blocker(d.stageWatcher.data());
        {
            if (policy == Session::LoadPolicy::All) {
                d.stage = UsdStage::Open(QStringToString(filename), UsdStage::LoadAll);
            }
            else {
                d.stage = UsdStage::Open(QStringToString(filename),
                                         UsdStage::LoadNone);  // load stage without loading payloads
            }
            d.loadPolicy = policy;
            d.mask = QList<SdfPath>();
        }
    }
    if (d.stage) {
        initStage();
        d.filename = filename;
    }
    else {
        d.stageStatus = Session::StageStatus::Failed;
        d.bboxCache.reset();
        return false;
    }
    QMetaObject::invokeMethod(
        d.session,
        [this]() {
            setMask(d.mask);
            updateStage();
        },
        Qt::QueuedConnection);
    return true;
}

bool
SessionPrivate::saveToFile(const QString& filename)
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
SessionPrivate::exportPathsToFile(const QList<SdfPath>& paths, const QString& filename)
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
SessionPrivate::close()
{
    {
        QWriteLocker locker(&d.stageLock);
        StageBlocker blocker(d.stageWatcher.data());
        {
            d.stage = nullptr;
            d.stageStatus = Session::StageStatus::Closed;
            d.bboxCache.reset();
            d.pendingPaths.clear();
            d.changeDepth = 0;
        }
    }
    QMetaObject::invokeMethod(
        d.session, [this]() { updateStage(); }, Qt::QueuedConnection);
    return true;
}

bool
SessionPrivate::reload()
{
    {
        QWriteLocker locker(&d.stageLock);
        if (d.stage) {
            StageBlocker blocker(d.stageWatcher.data());
            {
                d.stage->Reload();
                d.bboxCache.reset();
            }
        }
    }
    d.bbox = boundingBox();
    QMetaObject::invokeMethod(
        d.session, [this]() { updateStage(); }, Qt::QueuedConnection);

    return true;
}

bool
SessionPrivate::isLoaded() const
{
    return d.stage != nullptr;
}

void
SessionPrivate::setMask(const QList<SdfPath>& paths)
{
    {
        QWriteLocker locker(&d.stageLock);
        d.mask = paths;
    }
    Q_EMIT d.session->maskChanged(paths);
}

GfBBox3d
SessionPrivate::boundingBox()
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
        return stage::boundingBox(d.stage, d.mask);
    }
}

void
SessionPrivate::updatePrims(const QList<SdfPath> paths)
{
    if (d.changeDepth > 0) {
        d.pendingPaths.append(paths);
        return;
    }
    Q_EMIT d.session->primsChanged(paths);
    Q_EMIT d.session->boundingBoxChanged(d.bbox);
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
            Q_EMIT d.session->maskChanged(newMask);
        }
    }
}

void
SessionPrivate::updateStage()
{
    Q_EMIT d.session->stageChanged(d.stage, d.loadPolicy, d.stageStatus);
    Q_EMIT d.session->boundingBoxChanged(d.bbox);
}

Session::Session()
    : p(new SessionPrivate())
{
    p->d.session = this;
    p->init();
}

Session::Session(const QString& filename, Session::LoadPolicy loadPolicy)
    : p(new SessionPrivate())
{
    p->d.session = this;
    p->init();
    loadFromFile(filename, loadPolicy);
}

Session::Session(const Session& other)
    : p(other.p)
{}

Session::~Session() = default;

void
Session::beginProgressBlock(const QString& name, size_t count)
{
    p->beginProgressBlock(name, count);
}

void
Session::updateProgressNotify(const Notify& notify, size_t completed)
{
    p->updateProgressNotify(notify, completed);
}

void
Session::cancelProgressBlock()
{
    return p->cancelProgressBlock();
}

void
Session::endProgressBlock()
{
    p->endProgressBlock();
}

bool
Session::isProgressBlockCancelled() const
{
    return p->isProgressBlockCancelled();
}

bool
Session::loadFromFile(const QString& filename, Session::LoadPolicy loadPolicy)
{
    return p->loadFromFile(filename, loadPolicy);
}

bool
Session::saveToFile(const QString& filename)
{
    return p->saveToFile(filename);
}

bool
Session::exportToFile(const QString& filename)
{
    QReadLocker locker(&p->d.stageLock);
    if (!p->d.stage)
        return false;
    return p->d.stage->Export(QStringToString(filename));
}

bool
Session::exportPathsToFile(const QList<SdfPath>& paths, const QString& filename)
{
    return p->exportPathsToFile(paths, filename);
}

bool
Session::reload()
{
    return p->reload();
}

bool
Session::close()
{
    return p->close();
}

bool
Session::isLoaded() const
{
    return p->isLoaded();
}

QList<SdfPath>
Session::mask() const
{
    return p->d.mask;
}

void
Session::setMask(const QList<SdfPath>& paths)
{
    p->setMask(paths);
}

void
Session::setStatus(const QString& status)
{
    Q_EMIT statusChanged(status);
}

GfBBox3d
Session::boundingBox()
{
    return p->boundingBox();
}

Session::LoadPolicy
Session::loadPolicy() const
{
    return p->d.loadPolicy;
}

QString
Session::filename() const
{
    return p->d.filename;
}

UsdStageRefPtr
Session::stage() const
{
    Q_ASSERT("stage is not loaded" && isLoaded());
    return p->d.stage;
}

QReadWriteLock*
Session::stageLock() const
{
    return &p->d.stageLock;
}

SelectionList*
Session::selectionList() const
{
    return p->d.selectionList.data();
}

CommandStack*
Session::commandStack() const
{
    return p->d.commandStack.data();
}

}  // namespace usdviewer
