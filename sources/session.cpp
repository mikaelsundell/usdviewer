// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "session.h"
#include "commandstack.h"
#include "qtutils.h"
#include "selectionlist.h"
#include "tracelocks.h"
#include "usdutils.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QThreadPool>
#include <QVariant>
#include <QtConcurrent>
#include <algorithm>
#include <pxr/base/tf/weakBase.h>
#include <pxr/usd/usd/notice.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xform.h>
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
    bool newStage(Session::LoadPolicy policy);
    bool loadFromFile(const QString& filename, Session::LoadPolicy loadPolicy);
    bool saveToFile(const QString& filename);
    bool copyToFile(const QString& filename);
    bool flattenPathsToFile(const QList<SdfPath>& paths, const QString& filename);
    bool loadState(const QString& filename);
    bool saveState(const QString& filename);
    bool close();
    bool reload();
    bool isLoaded() const;
    void setMask(const QList<SdfPath>& paths);
    void setPayloads(const QList<SdfPath>& paths, bool loaded);
    Session::StageUp stageUp();
    void setStageUp(Session::StageUp stageUp);
    GfBBox3d boundingBox();

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
            if (stage)
                d.key = TfNotice::Register(TfWeakPtr<StageWatcher>(this), &StageWatcher::objectsChanged, stage);
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
                     << "changed" << paths.size() << qt::SdfPathListToQString(paths) << "resynced" << invalidated.size()
                     << qt::SdfPathListToQString(invalidated);

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

    QList<SdfPath> uniquePaths(const QList<SdfPath>& paths) const;
    QList<SdfPath> collapseDescendants(const QList<SdfPath>& paths) const;
    QList<SdfPath> removeDescendantsOf(const QList<SdfPath>& paths, const QList<SdfPath>& ancestors) const;
    QList<SdfPath> reduceInvalidated(const QList<SdfPath>& invalidated) const;

public:
    bool needsBoundingBoxUpdate(const QList<SdfPath>& changed, const QList<SdfPath>& invalidated) const;

    void updatePrims(const QList<SdfPath>& paths, const QList<SdfPath>& invalidated);
    void flushPrims();
    void updateStage();

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
    d.stageStatus = Session::StageStatus::Closed;
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

    const bool cancelled = d.changeCancelled.load();
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

    flushPrims();
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

    if (loaded && policy == Session::LoadPolicy::None) {
        const QString stateFilename = QFileInfo(d.filename + ".session").absoluteFilePath();
        if (!loadState(stateFilename)) {
            WRITE_LOCKER(locker, &d.stageLock, "stageLock");
            d.stage = nullptr;
            d.stageStatus = Session::StageStatus::Failed;
            d.bboxCache.reset();
            d.filename.clear();
            return false;
        }
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
    QString stageFilename;
    Session::LoadPolicy loadPolicy = Session::LoadPolicy::All;
    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        try {
            if (!d.stage)
                return false;

            const SdfLayerHandle rootLayer = d.stage->GetRootLayer();
            if (!rootLayer)
                return false;

            stageFilename = QFileInfo(filename).absoluteFilePath();

            QString currentFile;
            if (!rootLayer->IsAnonymous())
                currentFile = QFileInfo(qt::StringToQString(rootLayer->GetRealPath())).absoluteFilePath();

            if (!rootLayer->IsAnonymous() && currentFile == stageFilename) {
                d.stage->Save();
            }
            else {
                if (!rootLayer->Export(QStringToString(stageFilename)))
                    return false;
            }

            d.filename = stageFilename;
            loadPolicy = d.loadPolicy;
        } catch (const std::exception&) {
            return false;
        }
    }

    if (loadPolicy == Session::LoadPolicy::None) {
        const QString stateFilename = QFileInfo(stageFilename + ".session").absoluteFilePath();
        if (!saveState(stateFilename))
            return false;
    }

    return true;
}

bool
SessionPrivate::copyToFile(const QString& filename)
{
    QString stageFilename;
    Session::LoadPolicy loadPolicy = Session::LoadPolicy::All;
    {
        READ_LOCKER(locker, &d.stageLock, "stageLock");
        try {
            if (!d.stage)
                return false;

            const SdfLayerHandle rootLayer = d.stage->GetRootLayer();
            if (!rootLayer)
                return false;

            stageFilename = QFileInfo(filename).absoluteFilePath();

            if (!rootLayer->Export(QStringToString(stageFilename)))
                return false;

            loadPolicy = d.loadPolicy;
        } catch (const std::exception&) {
            return false;
        }
    }

    if (loadPolicy == Session::LoadPolicy::None) {
        const QString stateFilename = QFileInfo(stageFilename + ".session").absoluteFilePath();
        if (!saveState(stateFilename))
            return false;
    }

    return true;
}

bool
SessionPrivate::flattenPathsToFile(const QList<SdfPath>& paths, const QString& filename)
{
    READ_LOCKER(locker, &d.stageLock, "stageLock");
    if (!d.stage)
        return false;

    UsdStagePopulationMask mask;
    const QList<SdfPath> roots = path::topLevelPaths(paths);
    for (const SdfPath& path : roots)
        mask.Add(path);

    if (mask.GetPaths().empty())
        return false;

    UsdStageRefPtr maskedStage = UsdStage::OpenMasked(d.stage->GetRootLayer(), mask);
    if (!maskedStage)
        return false;

    maskedStage->ExpandPopulationMask();
    return maskedStage->Export(QStringToString(QFileInfo(filename).absoluteFilePath()));
}

bool
SessionPrivate::loadState(const QString& filename)
{
    QFile file(QFileInfo(filename).absoluteFilePath());
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;

    const QJsonObject root = doc.object();
    const QJsonArray payloads = root.value("loadedPayloads").toArray();

    WRITE_LOCKER(locker, &d.stageLock, "stageLock");
    if (!d.stage)
        return false;

    for (const QJsonValue& value : payloads) {
        const QString pathString = value.toString().trimmed();
        if (pathString.isEmpty())
            continue;

        const SdfPath path(qt::QStringToString(pathString));
        if (!path.IsAbsolutePath())
            continue;

        const UsdPrim prim = d.stage->GetPrimAtPath(path);
        if (!prim || !prim.IsValid())
            continue;
        if (!stage::isPayload(d.stage, path))
            continue;

        prim.Load();
    }

    return true;
}

bool
SessionPrivate::saveState(const QString& filename)
{
    QString stageFilename;
    QJsonArray payloads;
    {
        READ_LOCKER(locker, &d.stageLock, "stageLock");
        if (!d.stage)
            return false;

        stageFilename = QFileInfo(d.filename).absoluteFilePath();

        std::stack<UsdPrim> stack;
        stack.push(d.stage->GetPseudoRoot());

        while (!stack.empty()) {
            const UsdPrim prim = stack.top();
            stack.pop();

            if (!prim || !prim.IsValid())
                continue;

            if (stage::isPayload(d.stage, prim.GetPath()) && prim.IsLoaded())
                payloads.append(qt::SdfPathToQString(prim.GetPath()));

            for (const UsdPrim& child : prim.GetChildren())
                stack.push(child);
        }
    }

    QJsonObject root;
    root["version"] = 1;
    root["stageFile"] = stageFilename;
    root["loadedPayloads"] = payloads;

    QFile file(QFileInfo(filename).absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    const QJsonDocument doc(root);
    return file.write(doc.toJson(QJsonDocument::Indented)) != -1;
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
        d.expectedChanges = 0;
        d.completedChanges = 0;
        d.changeName.clear();
        d.changeCancelled.store(false);
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
    QString currentFilename;
    Session::LoadPolicy loadPolicy = Session::LoadPolicy::All;

    {
        WRITE_LOCKER(locker, &d.stageLock, "stageLock");
        if (!d.stage)
            return false;

        currentFilename = d.filename;
        loadPolicy = d.loadPolicy;

        StageBlocker blocker(d.stageWatcher.data());
        d.stage->Reload();
        d.bboxCache.reset();
    }

    if (loadPolicy == Session::LoadPolicy::None && !currentFilename.isEmpty()) {
        const QString stateFilename = QFileInfo(currentFilename + ".session").absoluteFilePath();
        if (!loadState(stateFilename))
            return false;
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

void
SessionPrivate::setPayloads(const QList<SdfPath>& paths, bool loaded)
{
    WRITE_LOCKER(locker, &d.stageLock, "stageLock");
    if (!d.stage)
        return;

    StageBlocker blocker(d.stageWatcher.data());

    for (const SdfPath& path : paths) {
        const SdfPath primPath = path.IsPropertyPath() ? path.GetPrimPath() : path;
        UsdPrim prim = d.stage->GetPrimAtPath(primPath);
        if (!prim || !prim.IsValid())
            continue;
        if (!stage::isPayload(d.stage, primPath))
            continue;

        if (loaded) {
            if (!prim.IsLoaded())
                prim.Load();
        }
        else {
            if (prim.IsLoaded())
                prim.Unload();
        }
    }

    d.bboxCache.reset();
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

    Q_ASSERT(stage && "stage is not loaded");
    if (!stage)
        return GfBBox3d();

    if (mask.isEmpty()) {
        UsdGeomBBoxCache bboxCache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens(), true);
        return bboxCache.ComputeWorldBound(stage->GetPseudoRoot());
    }

    return stage::boundingBox(stage, mask);
}

bool
SessionPrivate::needsBoundingBoxUpdate(const QList<SdfPath>& changed, const QList<SdfPath>& invalidated) const
{
    if (!invalidated.isEmpty()) {
        qDebug() << "SessionPrivate::needsBoundingBoxUpdate: true because of invalidated paths"
                 << qt::SdfPathListToQString(invalidated);
        return true;
    }

    for (const SdfPath& path : changed) {
        if (!path.IsPropertyPath()) {
            qDebug() << "SessionPrivate::needsBoundingBoxUpdate: true because prim path changed"
                     << qt::SdfPathToQString(path);
            return true;
        }
    }

    qDebug() << "SessionPrivate::needsBoundingBoxUpdate: false for property-only changes"
             << qt::SdfPathListToQString(changed);
    return false;
}

void
SessionPrivate::updatePrims(const QList<SdfPath>& paths, const QList<SdfPath>& invalidated)
{
    qDebug() << "SessionPrivate::updatePrims: incoming"
             << "changed" << paths.size() << qt::SdfPathListToQString(paths) << "invalidated" << invalidated.size()
             << qt::SdfPathListToQString(invalidated) << "changeDepth" << static_cast<qulonglong>(d.changeDepth)
             << "primsUpdate" << (d.primsUpdate == Session::PrimsUpdate::Deferred ? "Deferred" : "Immediate");

    if (d.changeDepth > 0 || d.primsUpdate == Session::PrimsUpdate::Deferred) {
        d.pendingPaths.append(paths);
        d.pendingInvalidated.append(invalidated);

        qDebug() << "SessionPrivate::updatePrims: deferred"
                 << "pending changed" << d.pendingPaths.size() << qt::SdfPathListToQString(d.pendingPaths)
                 << "pending invalidated" << d.pendingInvalidated.size()
                 << qt::SdfPathListToQString(d.pendingInvalidated);
        return;
    }

    const QList<SdfPath> reducedInvalidated = reduceInvalidated(invalidated);
    const QList<SdfPath> reducedChanged = collapseDescendants(removeDescendantsOf(paths, reducedInvalidated));

    qDebug() << "SessionPrivate::updatePrims: reduced"
             << "changed" << reducedChanged.size() << qt::SdfPathListToQString(reducedChanged) << "invalidated"
             << reducedInvalidated.size() << qt::SdfPathListToQString(reducedInvalidated);

    if (reducedChanged.isEmpty() && reducedInvalidated.isEmpty()) {
        qDebug() << "SessionPrivate::updatePrims: nothing to emit after reduction";
        return;
    }

    const bool updateBBox = needsBoundingBoxUpdate(reducedChanged, reducedInvalidated);

    if (updateBBox) {
        {
            WRITE_LOCKER(locker, &d.stageLock, "stageLock");
            d.bboxCache.reset();
        }

        const GfBBox3d bbox = boundingBox();
        {
            WRITE_LOCKER(locker, &d.stageLock, "stageLock");
            d.bbox = bbox;
        }

        qDebug() << "SessionPrivate::updatePrims: emit primsChanged + boundingBoxChanged"
                 << "changed" << reducedChanged.size() << qt::SdfPathListToQString(reducedChanged) << "invalidated"
                 << reducedInvalidated.size() << qt::SdfPathListToQString(reducedInvalidated);

        Q_EMIT d.session->primsChanged(reducedChanged, reducedInvalidated);
        Q_EMIT d.session->boundingBoxChanged(bbox);
    }
    else {
        qDebug() << "SessionPrivate::updatePrims: emit primsChanged only (property-only fast path)"
                 << "changed" << reducedChanged.size() << qt::SdfPathListToQString(reducedChanged) << "invalidated"
                 << reducedInvalidated.size() << qt::SdfPathListToQString(reducedInvalidated);

        Q_EMIT d.session->primsChanged(reducedChanged, reducedInvalidated);
    }
}

void
SessionPrivate::flushPrims()
{
    qDebug() << "SessionPrivate::flushPrims: incoming pending"
             << "changed" << d.pendingPaths.size() << qt::SdfPathListToQString(d.pendingPaths) << "invalidated"
             << d.pendingInvalidated.size() << qt::SdfPathListToQString(d.pendingInvalidated);

    if (d.pendingPaths.isEmpty() && d.pendingInvalidated.isEmpty()) {
        qDebug() << "SessionPrivate::flushPrims: nothing pending";
        return;
    }

    const QList<SdfPath> reducedInvalidated = reduceInvalidated(d.pendingInvalidated);
    const QList<SdfPath> reducedChanged = collapseDescendants(removeDescendantsOf(d.pendingPaths, reducedInvalidated));

    qDebug() << "SessionPrivate::flushPrims: reduced"
             << "changed" << reducedChanged.size() << qt::SdfPathListToQString(reducedChanged) << "invalidated"
             << reducedInvalidated.size() << qt::SdfPathListToQString(reducedInvalidated);

    d.pendingPaths.clear();
    d.pendingInvalidated.clear();

    if (reducedChanged.isEmpty() && reducedInvalidated.isEmpty()) {
        qDebug() << "SessionPrivate::flushPrims: nothing to emit after reduction";
        return;
    }

    const bool updateBBox = needsBoundingBoxUpdate(reducedChanged, reducedInvalidated);

    if (updateBBox) {
        {
            WRITE_LOCKER(locker, &d.stageLock, "stageLock");
            d.bboxCache.reset();
        }

        const GfBBox3d bbox = boundingBox();
        {
            WRITE_LOCKER(locker, &d.stageLock, "stageLock");
            d.bbox = bbox;
        }

        qDebug() << "SessionPrivate::flushPrims: emit primsChanged + boundingBoxChanged"
                 << "changed" << reducedChanged.size() << qt::SdfPathListToQString(reducedChanged) << "invalidated"
                 << reducedInvalidated.size() << qt::SdfPathListToQString(reducedInvalidated);

        Q_EMIT d.session->primsChanged(reducedChanged, reducedInvalidated);
        Q_EMIT d.session->boundingBoxChanged(bbox);
    }
    else {
        qDebug() << "SessionPrivate::flushPrims: emit primsChanged only (property-only fast path)"
                 << "changed" << reducedChanged.size() << qt::SdfPathListToQString(reducedChanged) << "invalidated"
                 << reducedInvalidated.size() << qt::SdfPathListToQString(reducedInvalidated);

        Q_EMIT d.session->primsChanged(reducedChanged, reducedInvalidated);
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

QList<SdfPath>
SessionPrivate::uniquePaths(const QList<SdfPath>& paths) const
{
    qDebug() << "SessionPrivate::uniquePaths: input" << paths.size() << qt::SdfPathListToQString(paths);

    QList<SdfPath> result;
    QSet<SdfPath> seen;
    result.reserve(paths.size());

    for (const SdfPath& path : paths) {
        if (!seen.contains(path)) {
            seen.insert(path);
            result.append(path);
        }
    }

    qDebug() << "SessionPrivate::uniquePaths: output" << result.size() << qt::SdfPathListToQString(result);
    return result;
}

QList<SdfPath>
SessionPrivate::collapseDescendants(const QList<SdfPath>& paths) const
{
    qDebug() << "SessionPrivate::collapseDescendants: input" << paths.size() << qt::SdfPathListToQString(paths);

    QList<SdfPath> result;
    QList<SdfPath> unique = uniquePaths(paths);

    std::sort(unique.begin(), unique.end(), [](const SdfPath& a, const SdfPath& b) {
        const size_t aCount = a.GetPathElementCount();
        const size_t bCount = b.GetPathElementCount();
        if (aCount != bCount)
            return aCount < bCount;
        return a.GetString() < b.GetString();
    });

    qDebug() << "SessionPrivate::collapseDescendants: sorted" << unique.size() << qt::SdfPathListToQString(unique);

    for (const SdfPath& path : unique) {
        bool covered = false;
        for (const SdfPath& existing : result) {
            if (path.HasPrefix(existing)) {
                qDebug() << "SessionPrivate::collapseDescendants: dropping descendant" << qt::SdfPathToQString(path)
                         << "because of" << qt::SdfPathToQString(existing);
                covered = true;
                break;
            }
        }
        if (!covered) {
            qDebug() << "SessionPrivate::collapseDescendants: keeping" << qt::SdfPathToQString(path);
            result.append(path);
        }
    }

    qDebug() << "SessionPrivate::collapseDescendants: output" << result.size() << qt::SdfPathListToQString(result);
    return result;
}

QList<SdfPath>
SessionPrivate::removeDescendantsOf(const QList<SdfPath>& changedPaths, const QList<SdfPath>& ancestors) const
{
    qDebug() << "SessionPrivate::removeDescendantsOf: input changed" << changedPaths.size()
             << qt::SdfPathListToQString(changedPaths) << "ancestors" << ancestors.size()
             << qt::SdfPathListToQString(ancestors);

    QList<SdfPath> result;
    result.reserve(changedPaths.size());

    for (const SdfPath& path : changedPaths) {
        bool covered = false;
        for (const SdfPath& ancestor : ancestors) {
            if (path.HasPrefix(ancestor)) {
                qDebug() << "SessionPrivate::removeDescendantsOf: dropping" << qt::SdfPathToQString(path)
                         << "because it is under" << qt::SdfPathToQString(ancestor);
                covered = true;
                break;
            }
        }
        if (!covered) {
            qDebug() << "SessionPrivate::removeDescendantsOf: keeping" << qt::SdfPathToQString(path);
            result.append(path);
        }
    }

    qDebug() << "SessionPrivate::removeDescendantsOf: output" << result.size() << qt::SdfPathListToQString(result);
    return result;
}

QList<SdfPath>
SessionPrivate::reduceInvalidated(const QList<SdfPath>& invalidated) const
{
    qDebug() << "SessionPrivate::reduceInvalidated: input" << invalidated.size()
             << qt::SdfPathListToQString(invalidated);

    QList<SdfPath> filtered = uniquePaths(invalidated);

    const bool hasSpecificInvalidation = std::any_of(filtered.begin(), filtered.end(), [](const SdfPath& path) {
        return path != SdfPath::AbsoluteRootPath();
    });

    if (hasSpecificInvalidation) {
        QList<SdfPath> withoutRoot;
        withoutRoot.reserve(filtered.size());
        for (const SdfPath& path : filtered) {
            if (path != SdfPath::AbsoluteRootPath())
                withoutRoot.append(path);
        }
        filtered = withoutRoot;
    }

    std::sort(filtered.begin(), filtered.end(), [](const SdfPath& a, const SdfPath& b) {
        const size_t aCount = a.GetPathElementCount();
        const size_t bCount = b.GetPathElementCount();
        if (aCount != bCount)
            return aCount < bCount;
        return a.GetString() < b.GetString();
    });

    QList<SdfPath> reduced;
    reduced.reserve(filtered.size());

    for (int i = 0; i < filtered.size(); ++i) {
        const SdfPath& path = filtered[i];

        bool hasAncestorInSet = false;
        bool hasDescendantInSet = false;

        for (int j = 0; j < filtered.size(); ++j) {
            if (i == j)
                continue;

            const SdfPath& other = filtered[j];

            if (path.HasPrefix(other)) {
                hasAncestorInSet = true;
            }
            else if (other.HasPrefix(path)) {
                hasDescendantInSet = true;
            }

            if (hasAncestorInSet && hasDescendantInSet)
                break;
        }

        // Keep:
        // 1) top-level invalidated roots
        // 2) intermediate invalidated containers that also have deeper invalidations
        //
        // Drop:
        // - leaf invalidations already covered by an ancestor invalidation
        if (!hasAncestorInSet || hasDescendantInSet)
            reduced.append(path);
    }

    qDebug() << "SessionPrivate::reduceInvalidated: output" << reduced.size()
             << qt::SdfPathListToQString(reduced);
    return reduced;
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
    p->cancelProgressBlock();
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
    Q_ASSERT(p->d.stage && "stage is not loaded");
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
        flush = (update == Session::PrimsUpdate::Immediate);
    }

    if (flush)
        p->flushPrims();
}

void
Session::flushPrimsUpdates()
{
    p->flushPrims();
}

}  // namespace usdviewer
