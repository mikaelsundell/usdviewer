// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "session.h"
#include "commandstack.h"
#include "qtutils.h"
#include "selectionlist.h"
#include "stageutils.h"
#include "tracelocks.h"
#include <QMap>
#include <QThreadPool>
#include <QVariant>
#include <QtConcurrent>
#include <pxr/base/tf/weakBase.h>
#include <pxr/usd/usd/notice.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xform.h>
#include <stack>

namespace usdviewer {

namespace {

    QString pathToQString(const SdfPath& path) { return qt::StringToQString(path.GetString()); }

    QString pathListToQString(const QList<SdfPath>& paths)
    {
        QStringList values;
        values.reserve(paths.size());
        for (const SdfPath& path : paths)
            values.append(pathToQString(path));
        return QString("[%1]").arg(values.join(", "));
    }

}  // namespace



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
    bool newStage(Session::LoadPolicy policy);
    bool loadFromFile(const QString& filename, Session::LoadPolicy loadPolicy);
    bool saveToFile(const QString& filename);
    bool copyToFile(const QString& filename);
    bool flattenPathsToFile(const QList<SdfPath>& paths, const QString& filename);
    bool close();
    bool reload();
    bool isLoaded() const;
    void setMask(const QList<SdfPath>& paths);
    Session::StageUp stageUp();
    void setStageUp(Session::StageUp stageUp);
    GfBBox3d boundingBox();

public:
    void updatePrims(const QList<SdfPath>& paths, const QList<SdfPath>& invalidated);
    void updateStage();
    void flushPrimsUpdates();

public:
    class StageWatcher : public TfWeakBase {
    public:
        StageWatcher(SessionPrivate* parent)
            : d { false, TfNotice::Key(), parent, UsdStageRefPtr() }
        {}
        void init()
        {
            if (d.key.IsValid())
                TfNotice::Revoke(d.key);
            d.stage = nullptr;
        }
        void watch(const UsdStageRefPtr& stage)
        {
            init();
            d.stage = stage;
            if (stage) {
                d.key = TfNotice::Register(TfWeakPtr<StageWatcher>(this), &StageWatcher::objectsChanged, stage);
            }
        }
        void objectsChanged(const UsdNotice::ObjectsChanged& notice, const UsdStageWeakPtr& sender)
        {
            if (d.suppress.load()) {
                qDebug() << "StageWatcher::objectsChanged: suppressed";
                return;
            }

            UsdStageRefPtr senderStage = sender;
            if (!d.stage || !senderStage || d.stage != senderStage) {
                qDebug() << "StageWatcher::objectsChanged: ignoring stale notice from non-current stage";
                return;
            }

            QList<SdfPath> paths;
            QList<SdfPath> invalidated;
            for (const auto& p : notice.GetResyncedPaths())
                invalidated.append(p);

            for (const auto& p : notice.GetChangedInfoOnlyPaths())
                paths.append(p);

            qDebug() << "StageWatcher::objectsChanged:"
                     << "changed" << paths.size() << pathListToQString(paths) << "resynced" << invalidated.size()
                     << pathListToQString(invalidated);

            if (invalidated.isEmpty() && paths.isEmpty()) {
                qDebug() << "StageWatcher::objectsChanged: nothing to forward";
                return;
            }

            QMetaObject::invokeMethod(d.parent->d.session, [this, paths, invalidated]() {
                qDebug() << "StageWatcher::objectsChanged: invoke updatePrims";
                d.parent->updatePrims(paths, invalidated);
            });
        }
        void blockSignals(bool block) { d.suppress.store(block); }
        bool signalsBlocked() const { return d.suppress.load(); }
        struct Data {
            std::atomic<bool> suppress { false };
            TfNotice::Key key;
            SessionPrivate* parent;
            UsdStageRefPtr stage;
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
    QList<SdfPath> uniquePaths(const QList<SdfPath>& paths);
    QList<SdfPath> collapseDescendants(const QList<SdfPath>& paths);
    QList<SdfPath> removeDescendantsOf(const QList<SdfPath>& paths, const QList<SdfPath>& ancestors);

public:
    struct Data {
        UsdStageRefPtr stage;
        Session::LoadPolicy loadPolicy;
        Session::PrimsUpdate primsUpdate;
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
        QList<SdfPath> pendingInvalidated;
        QList<SdfPath> mask;
        QThreadPool pool;
        mutable QReadWriteLock stageLock;
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
    d.primsUpdate = Session::PrimsUpdate::Immediate;
    d.changeDepth = 0;
    d.expectedChanges = 0;
    d.completedChanges = 0;
    d.pendingPaths.clear();
    d.pendingInvalidated.clear();
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
}

void
SessionPrivate::initStage()
{
    UsdStageRefPtr stage;
    const GfBBox3d bbox = boundingBox();
    {
        READ_LOCKER(locker, &d.stageLock, "stageLock");
        stage = d.stage;
    }
    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        d.stageStatus = Session::StageStatus::Loaded;
        d.bboxCache.reset();
        d.bbox = bbox;
        d.pendingPaths.clear();
        d.pendingInvalidated.clear();
    }
    d.stageWatcher->watch(stage);
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
        d.pendingInvalidated.clear();
        return;
    }

    if (d.pendingPaths.isEmpty() && d.pendingInvalidated.isEmpty())
        return;

    flushPrimsUpdates();
}

bool
SessionPrivate::isProgressBlockCancelled() const
{
    return d.changeCancelled.load();
}

bool
SessionPrivate::newStage(Session::LoadPolicy policy)
{
    close();
    beginProgressBlock("New stage", 1);

    QList<SdfPath> mask;
    bool created = false;
    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        StageBlocker blocker(d.stageWatcher.data());
        d.stageWatcher->init();

        UsdStageRefPtr stage = UsdStage::CreateInMemory();
        if (!stage) {
            endProgressBlock();
            return false;
        }

        d.stage = stage;
        UsdGeomXform root = UsdGeomXform::Define(d.stage, SdfPath("/World"));
        d.stage->SetDefaultPrim(root.GetPrim());
        d.filename.clear();
        d.loadPolicy = policy;
        d.mask.clear();
        mask = d.mask;
        created = true;
    }
    d.commandStack->clear();
    d.selectionList->clear();

    if (created)
        initStage();

    endProgressBlock();
    QMetaObject::invokeMethod(
        d.session,
        [this, mask]() {
            setMask(mask);
            updateStage();
        },
        Qt::QueuedConnection);
    return true;
}

bool
SessionPrivate::loadFromFile(const QString& filename, Session::LoadPolicy policy)
{
    QList<SdfPath> mask;
    bool loaded = false;
    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        StageBlocker blocker(d.stageWatcher.data());
        d.stageWatcher->init();

        if (policy == Session::LoadPolicy::All)
            d.stage = UsdStage::Open(QStringToString(filename), UsdStage::LoadAll);
        else
            d.stage = UsdStage::Open(QStringToString(filename), UsdStage::LoadNone);

        d.loadPolicy = policy;
        d.mask = QList<SdfPath>();

        if (d.stage) {
            d.filename = QFileInfo(filename).absoluteFilePath();
            loaded = true;
        }
        else {
            d.stageStatus = Session::StageStatus::Failed;
            d.bboxCache.reset();
            return false;
        }

        mask = d.mask;
    }
    d.commandStack->clear();
    d.selectionList->clear();

    if (loaded)
        initStage();

    QMetaObject::invokeMethod(
        d.session,
        [this, mask]() {
            setMask(mask);
            updateStage();
        },
        Qt::QueuedConnection);
    return true;
}

bool
SessionPrivate::saveToFile(const QString& filename)
{
    WRITE_LOCKER(locker, &d.stageLock, "stageLock");
    if (!d.stage)
        return false;

    try {
        const SdfLayerHandle rootLayer = d.stage->GetRootLayer();
        if (!rootLayer)
            return false;

        const QString targetFile = QFileInfo(filename).absoluteFilePath();

        QString currentFile;
        if (!rootLayer->IsAnonymous())
            currentFile = QFileInfo(qt::StringToQString(rootLayer->GetRealPath())).absoluteFilePath();

        if (!rootLayer->IsAnonymous() && currentFile == targetFile) {
            d.stage->Save();
        }
        else {
            if (!rootLayer->Export(QStringToString(targetFile)))
                return false;
        }

        d.filename = targetFile;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool
SessionPrivate::copyToFile(const QString& filename)
{
    READ_LOCKER(locker, &d.stageLock, "stageLock");
    if (!d.stage)
        return false;

    try {
        const SdfLayerHandle rootLayer = d.stage->GetRootLayer();
        if (!rootLayer)
            return false;

        const QString targetFile = QFileInfo(filename).absoluteFilePath();
        return rootLayer->Export(QStringToString(targetFile));
    } catch (const std::exception&) {
        return false;
    }
}

bool
SessionPrivate::flattenPathsToFile(const QList<SdfPath>& paths, const QString& filename)
{
    READ_LOCKER(locker, &d.stageLock, "stageLock");
    if (!d.stage)
        return false;

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
    return maskedStage->Export(QStringToString(QFileInfo(filename).absoluteFilePath()));
}

bool
SessionPrivate::close()
{
    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        StageBlocker blocker(d.stageWatcher.data());
        d.stageWatcher->init();
        d.stage = nullptr;
        d.stageStatus = Session::StageStatus::Closed;
        d.bboxCache.reset();
        d.pendingPaths.clear();
        d.pendingInvalidated.clear();
        d.changeDepth = 0;
        d.filename.clear();
    }
    d.commandStack->clear();
    d.selectionList->clear();

    QMetaObject::invokeMethod(
        d.session, [this]() { updateStage(); }, Qt::QueuedConnection);
    return true;
}

bool
SessionPrivate::reload()
{
    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        if (!d.stage)
            return false;

        StageBlocker blocker(d.stageWatcher.data());
        d.stage->Reload();
        d.bboxCache.reset();
    }
    const GfBBox3d bbox = boundingBox();
    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        d.bbox = bbox;
    }
    QMetaObject::invokeMethod(
        d.session, [this]() { updateStage(); }, Qt::QueuedConnection);
    return true;
}

bool
SessionPrivate::isLoaded() const
{
    READ_LOCKER(locker, &d.stageLock, "stageLock");
    return d.stage != nullptr;
}

void
SessionPrivate::setMask(const QList<SdfPath>& paths)
{
    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        d.mask = paths;
    }
    Q_EMIT d.session->maskChanged(paths);
}

Session::StageUp
SessionPrivate::stageUp()
{
    READ_LOCKER(locker, &d.stageLock, "stageLock");
    if (!d.stage)
        return Session::StageUp::Z;
    const TfToken upAxis = UsdGeomGetStageUpAxis(d.stage);
    return (upAxis == UsdGeomTokens->y) ? Session::StageUp::Y : Session::StageUp::Z;
}

void
SessionPrivate::setStageUp(Session::StageUp stageUp)
{
    bool changed = false;
    GfBBox3d bbox;
    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        if (!d.stage)
            return;
        const TfToken upAxis = (stageUp == Session::StageUp::Y) ? UsdGeomTokens->y : UsdGeomTokens->z;

        if (UsdGeomGetStageUpAxis(d.stage) == upAxis)
            return;
        UsdGeomSetStageUpAxis(d.stage, upAxis);
        d.bboxCache.reset();
        changed = true;
    }
    if (!changed)
        return;
    bbox = boundingBox();
    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        d.bbox = bbox;
    }
    Q_EMIT d.session->stageUpChanged(stageUp);
    Q_EMIT d.session->boundingBoxChanged(bbox);
}

GfBBox3d
SessionPrivate::boundingBox()
{
    UsdStageRefPtr stage;
    QList<SdfPath> mask;
    {
        READ_LOCKER(locker, &d.stageLock, "stageLock");
        stage = d.stage;
        mask = d.mask;
    }
    Q_ASSERT("stage is not loaded" && stage);
    if (!stage)
        return GfBBox3d();
    if (mask.isEmpty()) {
        UsdGeomBBoxCache bboxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens(), true);
        return bboxCache.ComputeWorldBound(stage->GetPseudoRoot());
    }
    return stage::boundingBox(stage, mask);
}

void
SessionPrivate::updatePrims(const QList<SdfPath>& paths, const QList<SdfPath>& invalidated)
{
    qDebug() << "SessionPrivate::updatePrims: incoming"
             << "changed" << paths.size() << pathListToQString(paths) << "invalidated" << invalidated.size()
             << pathListToQString(invalidated) << "changeDepth" << static_cast<qulonglong>(d.changeDepth)
             << "primsUpdate" << (d.primsUpdate == Session::Deferred ? "Deferred" : "Immediate");

    if (d.changeDepth > 0 || d.primsUpdate == Session::Deferred) {
        d.pendingPaths.append(paths);
        d.pendingInvalidated.append(invalidated);

        qDebug() << "SessionPrivate::updatePrims: deferred"
                 << "pending changed" << d.pendingPaths.size() << pathListToQString(d.pendingPaths)
                 << "pending invalidated" << d.pendingInvalidated.size() << pathListToQString(d.pendingInvalidated);
        return;
    }

    QList<SdfPath> uniqueInvalidated = collapseDescendants(invalidated);
    QList<SdfPath> uniqueChanged = collapseDescendants(removeDescendantsOf(paths, uniqueInvalidated));

    QList<SdfPath> parentPaths;
    for (const SdfPath& path : uniqueInvalidated) {
        const SdfPath parent = path.GetParentPath();
        if (!parent.IsEmpty() && parent != SdfPath::AbsoluteRootPath())
            parentPaths.append(parent);
    }
    uniqueChanged.append(parentPaths);
    uniqueChanged = collapseDescendants(uniquePaths(uniqueChanged));

    qDebug() << "SessionPrivate::updatePrims: reduced"
             << "changed" << uniqueChanged.size() << pathListToQString(uniqueChanged) << "invalidated"
             << uniqueInvalidated.size() << pathListToQString(uniqueInvalidated);

    if (uniqueChanged.isEmpty() && uniqueInvalidated.isEmpty()) {
        qDebug() << "SessionPrivate::updatePrims: nothing to emit after reduction";
        return;
    }
    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        d.bboxCache.reset();
    }
    const GfBBox3d bbox = boundingBox();
    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        d.bbox = bbox;
    }

    qDebug() << "SessionPrivate::updatePrims: emit primsChanged"
             << "changed" << uniqueChanged.size() << pathListToQString(uniqueChanged) << "invalidated"
             << uniqueInvalidated.size() << pathListToQString(uniqueInvalidated);

    Q_EMIT d.session->primsChanged(uniqueChanged, uniqueInvalidated);
    Q_EMIT d.session->boundingBoxChanged(bbox);

    QList<SdfPath> mask;
    {
        READ_LOCKER(locker, &d.stageLock, "stageLock");
        mask = d.mask;
    }
    if (!mask.isEmpty()) {
        QList<SdfPath> newMask;
        bool updated = false;

        {
            READ_LOCKER(locker, &d.stageLock, "stageLock");
            if (d.stage) {
                for (const SdfPath& path : d.mask) {
                    UsdPrim prim = d.stage->GetPrimAtPath(path);
                    if (prim && prim.IsValid() && prim.IsActive())
                        newMask.append(path);
                    else
                        updated = true;
                }
            }
            else {
                updated = true;
            }
        }
        if (updated) {
            qDebug() << "SessionPrivate::updatePrims: mask updated" << pathListToQString(newMask);
            {
                WRITE_LOCKER(locker, &d.stageLock, "stageLock");
                d.mask = newMask;
            }
            Q_EMIT d.session->maskChanged(newMask);
        }
    }
}

void
SessionPrivate::updateStage()
{
    UsdStageRefPtr stage;
    Session::LoadPolicy loadPolicy;
    Session::StageStatus stageStatus;
    GfBBox3d bbox;
    {
        READ_LOCKER(locker, &d.stageLock, "stageLock");
        stage = d.stage;
        loadPolicy = d.loadPolicy;
        stageStatus = d.stageStatus;
        bbox = d.bbox;
    }
    Q_EMIT d.session->stageChanged(stage, loadPolicy, stageStatus);
    Q_EMIT d.session->stageUpChanged(stageUp());
    Q_EMIT d.session->boundingBoxChanged(bbox);
}

void
SessionPrivate::flushPrimsUpdates()
{
    qDebug() << "SessionPrivate::flushPrimsUpdates: incoming pending"
             << "changed" << d.pendingPaths.size() << pathListToQString(d.pendingPaths) << "invalidated"
             << d.pendingInvalidated.size() << pathListToQString(d.pendingInvalidated);

    if (d.pendingPaths.isEmpty() && d.pendingInvalidated.isEmpty()) {
        qDebug() << "SessionPrivate::flushPrimsUpdates: nothing pending";
        return;
    }

    QList<SdfPath> invalidatedInput = uniquePaths(d.pendingInvalidated);

    const bool hasSpecificInvalidation
        = std::any_of(invalidatedInput.begin(), invalidatedInput.end(),
                      [](const SdfPath& path) { return path != SdfPath::AbsoluteRootPath(); });

    if (hasSpecificInvalidation) {
        QList<SdfPath> filtered;
        filtered.reserve(invalidatedInput.size());
        for (const SdfPath& path : invalidatedInput) {
            if (path != SdfPath::AbsoluteRootPath())
                filtered.append(path);
        }
        invalidatedInput = filtered;
    }

    QList<SdfPath> invalidated = collapseDescendants(invalidatedInput);
    QList<SdfPath> changed = collapseDescendants(removeDescendantsOf(d.pendingPaths, invalidated));

    qDebug() << "SessionPrivate::flushPrimsUpdates: reduced"
             << "changed" << changed.size() << pathListToQString(changed) << "invalidated" << invalidated.size()
             << pathListToQString(invalidated);

    d.pendingPaths.clear();
    d.pendingInvalidated.clear();

    if (changed.isEmpty() && invalidated.isEmpty()) {
        qDebug() << "SessionPrivate::flushPrimsUpdates: nothing to emit after reduction";
        return;
    }

    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        d.bboxCache.reset();
    }

    const GfBBox3d bbox = boundingBox();
    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        d.bbox = bbox;
    }

    qDebug() << "SessionPrivate::flushPrimsUpdates: emit primsChanged"
             << "changed" << changed.size() << pathListToQString(changed) << "invalidated" << invalidated.size()
             << pathListToQString(invalidated);

    Q_EMIT d.session->primsChanged(changed, invalidated);
    Q_EMIT d.session->boundingBoxChanged(bbox);

    QList<SdfPath> mask;
    {
        READ_LOCKER(locker, &d.stageLock, "stageLock");
        mask = d.mask;
    }
    if (!mask.isEmpty()) {
        QList<SdfPath> newMask;
        bool updated = false;
        {
            READ_LOCKER(locker, &d.stageLock, "stageLock");
            if (d.stage) {
                for (const SdfPath& path : d.mask) {
                    UsdPrim prim = d.stage->GetPrimAtPath(path);
                    if (prim && prim.IsValid() && prim.IsActive())
                        newMask.append(path);
                    else
                        updated = true;
                }
            }
            else {
                updated = true;
            }
        }
        if (updated) {
            qDebug() << "SessionPrivate::flushPrimsUpdates: mask updated" << pathListToQString(newMask);

            {
                WRITE_LOCKER(locker, &d.stageLock, "stageLock");
                d.mask = newMask;
            }
            Q_EMIT d.session->maskChanged(newMask);
        }
    }
}



QList<SdfPath>
SessionPrivate::uniquePaths(const QList<SdfPath>& paths)
{
    qDebug() << "SessionPrivate::uniquePaths: input" << paths.size() << pathListToQString(paths);

    QList<SdfPath> result;
    QSet<SdfPath> seen;

    for (const SdfPath& path : paths) {
        if (!seen.contains(path)) {
            seen.insert(path);
            result.append(path);
        }
    }

    qDebug() << "SessionPrivate::uniquePaths: output" << result.size() << pathListToQString(result);

    return result;
}

QList<SdfPath>
SessionPrivate::collapseDescendants(const QList<SdfPath>& paths)
{
    qDebug() << "SessionPrivate::collapseDescendants: input" << paths.size() << pathListToQString(paths);

    QList<SdfPath> result;
    QList<SdfPath> unique = uniquePaths(paths);

    std::sort(unique.begin(), unique.end(),
              [](const SdfPath& a, const SdfPath& b) { return a.GetPathElementCount() < b.GetPathElementCount(); });

    qDebug() << "SessionPrivate::collapseDescendants: sorted" << unique.size() << pathListToQString(unique);

    for (const SdfPath& path : unique) {
        bool covered = false;
        for (const SdfPath& existing : result) {
            if (path.HasPrefix(existing)) {
                qDebug() << "SessionPrivate::collapseDescendants: dropping descendant" << pathToQString(path)
                         << "because of" << pathToQString(existing);
                covered = true;
                break;
            }
        }
        if (!covered) {
            qDebug() << "SessionPrivate::collapseDescendants: keeping" << pathToQString(path);
            result.append(path);
        }
    }

    qDebug() << "SessionPrivate::collapseDescendants: output" << result.size() << pathListToQString(result);

    return result;
}

QList<SdfPath>
SessionPrivate::removeDescendantsOf(const QList<SdfPath>& changedPaths, const QList<SdfPath>& ancestors)
{
    qDebug() << "SessionPrivate::removeDescendantsOf: input changed" << changedPaths.size()
             << pathListToQString(changedPaths) << "ancestors" << ancestors.size() << pathListToQString(ancestors);

    QList<SdfPath> result;
    for (const SdfPath& path : changedPaths) {
        bool covered = false;
        for (const SdfPath& resynced : ancestors) {
            if (path.HasPrefix(resynced)) {
                qDebug() << "SessionPrivate::removeDescendantsOf: dropping" << pathToQString(path)
                         << "because it is under" << pathToQString(resynced);
                covered = true;
                break;
            }
        }
        if (!covered) {
            qDebug() << "SessionPrivate::removeDescendantsOf: keeping" << pathToQString(path);
            result.append(path);
        }
    }

    qDebug() << "SessionPrivate::removeDescendantsOf: output" << result.size() << pathListToQString(result);

    return result;
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
Session::newStage(Session::LoadPolicy policy)
{
    return p->newStage(policy);
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
Session::copyToFile(const QString& filename)
{
    return p->copyToFile(filename);
}

bool
Session::flattenToFile(const QString& filename)
{
    READ_LOCKER(locker, stageLock(), "stageLock");
    if (!p->d.stage)
        return false;

    try {
        return p->d.stage->Export(QStringToString(QFileInfo(filename).absoluteFilePath()));
    } catch (const std::exception&) {
        return false;
    }
}

bool
Session::flattenPathsToFile(const QList<SdfPath>& paths, const QString& filename)
{
    return p->flattenPathsToFile(paths, filename);
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

Session::StageUp
Session::stageUp()
{
    return p->stageUp();
}

void
Session::setStageUp(Session::StageUp stageUp)
{
    p->setStageUp(stageUp);
}

QList<SdfPath>
Session::mask() const
{
    READ_LOCKER(locker, stageLock(), "stageLock");
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
    READ_LOCKER(locker, stageLock(), "stageLock");
    return p->d.loadPolicy;
}

QString
Session::filename() const
{
    READ_LOCKER(locker, stageLock(), "stageLock");
    return p->d.filename;
}

UsdStageRefPtr
Session::stage() const
{
    READ_LOCKER(locker, stageLock(), "stageLock");
    Q_ASSERT("stage is not loaded" && p->d.stage);
    return p->d.stage;
}

UsdStageRefPtr
Session::stageUnsafe() const
{
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

Session::PrimsUpdate
Session::primsUpdate() const
{
    READ_LOCKER(locker, stageLock(), "stageLock");
    return p->d.primsUpdate;
}

void
Session::setPrimsUpdate(PrimsUpdate update)
{
    bool flush = false;
    {
        WRITE_LOCKER(locker, stageLock(), "stageLock");
        if (p->d.primsUpdate == update)
            return;

        p->d.primsUpdate = update;
        flush = (update == Immediate);
    }
    if (flush)
        p->flushPrimsUpdates();
}

void
Session::flushPrimsUpdates()
{
    p->flushPrimsUpdates();
}

}  // namespace usdviewer
