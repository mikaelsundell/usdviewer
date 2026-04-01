// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "command.h"
#include "commandstack.h"
#include "qtutils.h"
#include "stageutils.h"
#include "tracelocks.h"
#include <QPointer>
#include <QThreadPool>
#include <pxr/usd/sdf/copyUtils.h>
#include <pxr/usd/usd/namespaceEditor.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/variantSets.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xform.h>

namespace usdviewer {

namespace payload {
    struct UndoItem {
        SdfPath path;
        bool wasLoaded = false;
        bool hadVariantSet = false;
        std::string variantSetName;
        std::string previousVariantSelection;
    };

    struct State {
        QList<UndoItem> undoItems;
    };

    struct Result {
        SdfPath path;
        bool success = false;
        QString message;
        Session::Notify::Status status = Session::Notify::Status::Info;
    };

    inline void flushResults(Session* session, const QList<payload::Result>& results, int completed)
    {
        if (!session || results.isEmpty())
            return;

        const int start = completed - static_cast<int>(results.size()) + 1;
        for (int i = 0; i < results.size(); ++i) {
            const payload::Result& r = results[i];
            session->updateProgressNotify(Session::Notify(r.message, { r.path }, r.status), start + i);
        }
    }

    inline bool applyLoad(UsdStageRefPtr stage, const SdfPath& path, bool useVariant, const std::string& variantSetName,
                          const std::string& variantSelection, UndoItem& undoItem, QString& error)
    {
        if (!stage) {
            error = "stage missing";
            return false;
        }

        UsdPrim prim = stage->GetPrimAtPath(path);
        if (!prim || !prim.HasPayload()) {
            error = "payload missing";
            return false;
        }

        undoItem.path = path;
        undoItem.wasLoaded = prim.IsLoaded();

        if (useVariant) {
            UsdVariantSet vs = prim.GetVariantSet(variantSetName);
            if (!vs.IsValid()) {
                error = "variant set missing";
                return false;
            }

            const auto variants = vs.GetVariantNames();
            if (std::find(variants.begin(), variants.end(), variantSelection) == variants.end()) {
                error = "variant value missing";
                return false;
            }

            undoItem.hadVariantSet = true;
            undoItem.variantSetName = variantSetName;
            undoItem.previousVariantSelection = vs.GetVariantSelection();

            if (prim.IsLoaded())
                prim.Unload();

            if (vs.GetVariantSelection() != variantSelection)
                vs.SetVariantSelection(variantSelection);
        }

        if (!prim.IsLoaded())
            prim.Load();

        return true;
    }

    inline bool applyUnload(UsdStageRefPtr stage, const SdfPath& path, UndoItem& undoItem, QString& error)
    {
        if (!stage) {
            error = "stage missing";
            return false;
        }

        UsdPrim prim = stage->GetPrimAtPath(path);
        if (!prim || !prim.HasPayload()) {
            error = "payload missing";
            return false;
        }

        undoItem.path = path;
        undoItem.wasLoaded = prim.IsLoaded();

        if (prim.IsLoaded())
            prim.Unload();

        return true;
    }

    inline bool restoreState(UsdStageRefPtr stage, const UndoItem& item, QString& error)
    {
        if (!stage) {
            error = "stage missing";
            return false;
        }

        UsdPrim prim = stage->GetPrimAtPath(item.path);
        if (!prim || !prim.HasPayload()) {
            error = "payload missing";
            return false;
        }

        if (item.hadVariantSet) {
            if (prim.IsLoaded())
                prim.Unload();

            UsdVariantSet vs = prim.GetVariantSet(item.variantSetName);
            if (!vs.IsValid()) {
                error = "variant set missing";
                return false;
            }

            if (vs.GetVariantSelection() != item.previousVariantSelection)
                vs.SetVariantSelection(item.previousVariantSelection);
        }

        if (item.wasLoaded) {
            if (!prim.IsLoaded())
                prim.Load();
        }
        else {
            if (prim.IsLoaded())
                prim.Unload();
        }

        return true;
    }

}  // namespace payload

Command
loadPayloads(const QList<SdfPath>& paths, const QString& variantSet, const QString& variantValue)
{
    auto state = std::make_shared<payload::State>();

    return Command(
        // redo
        [paths, variantSet, variantValue, state](Session* session) {
            if (!session || paths.isEmpty())
                return;

            session->beginProgressBlock("load payloads", paths.size());
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([session, paths, variantSet, variantValue, state]() {
                const bool useVariant = !variantSet.isEmpty() && !variantValue.isEmpty();
                const std::string variantSetName = QStringToString(variantSet);
                const std::string variantSelection = QStringToString(variantValue);

                QList<payload::Result> pending;
                pending.reserve(16);

                QList<payload::UndoItem> undoItems;
                undoItems.reserve(paths.size());

                int completed = 0;
                for (const SdfPath& path : paths) {
                    if (!session || session->isProgressBlockCancelled())
                        break;

                    payload::Result result;
                    result.path = path;
                    result.message = "payload failed";
                    result.status = Session::Notify::Status::Error;

                    payload::UndoItem undoItem;
                    QString error;

                    try {
                        WRITE_LOCKER(locker, session->stageLock(), "stageLock");
                        const UsdStageRefPtr stage = session->stageUnsafe();
                        result.success = payload::applyLoad(stage, path, useVariant, variantSetName, variantSelection,
                                                            undoItem, error);
                    } catch (...) {
                        result.success = false;
                        error = "exception";
                    }

                    result.message = result.success ? "payload loaded" : "payload failed";
                    result.status = result.success ? Session::Notify::Status::Info : Session::Notify::Status::Error;

                    if (result.success)
                        undoItems.append(undoItem);

                    pending.append(result);
                    ++completed;

                    if (pending.size() >= 16) {
                        const QList<payload::Result> batch = pending;
                        QMetaObject::invokeMethod(
                            session,
                            [session, batch, completed]() { payload::flushResults(session, batch, completed); },
                            Qt::QueuedConnection);
                        pending.clear();
                    }
                }

                if (!pending.isEmpty()) {
                    const QList<payload::Result> batch = pending;
                    QMetaObject::invokeMethod(
                        session, [session, batch, completed]() { payload::flushResults(session, batch, completed); },
                        Qt::QueuedConnection);
                }

                state->undoItems = undoItems;

                QMetaObject::invokeMethod(
                    session,
                    [session]() {
                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [state](Session* session) {
            if (!session || state->undoItems.isEmpty())
                return;

            session->beginProgressBlock("undo load payloads", state->undoItems.size());
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([session, state]() {
                QList<payload::Result> pending;
                pending.reserve(16);

                int completed = 0;
                for (const payload::UndoItem& item : state->undoItems) {
                    if (!session || session->isProgressBlockCancelled())
                        break;

                    payload::Result result;
                    result.path = item.path;
                    result.message = "payload undo failed";
                    result.status = Session::Notify::Status::Error;

                    QString error;
                    try {
                        WRITE_LOCKER(locker, session->stageLock(), "stageLock");
                        const UsdStageRefPtr stage = session->stageUnsafe();
                        result.success = payload::restoreState(stage, item, error);
                    } catch (...) {
                        result.success = false;
                        error = "exception";
                    }

                    result.message = result.success ? "payload undone" : "payload undo failed";
                    result.status = result.success ? Session::Notify::Status::Info : Session::Notify::Status::Error;

                    pending.append(result);
                    ++completed;

                    if (pending.size() >= 16) {
                        const QList<payload::Result> batch = pending;
                        QMetaObject::invokeMethod(
                            session,
                            [session, batch, completed]() { payload::flushResults(session, batch, completed); },
                            Qt::QueuedConnection);
                        pending.clear();
                    }
                }

                if (!pending.isEmpty()) {
                    const QList<payload::Result> batch = pending;
                    QMetaObject::invokeMethod(
                        session, [session, batch, completed]() { payload::flushResults(session, batch, completed); },
                        Qt::QueuedConnection);
                }

                QMetaObject::invokeMethod(
                    session,
                    [session]() {
                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

Command
unloadPayloads(const QList<SdfPath>& paths)
{
    auto state = std::make_shared<payload::State>();

    return Command(
        // redo
        [paths, state](Session* session) {
            if (!session || paths.isEmpty())
                return;

            session->beginProgressBlock("unload payloads", paths.size());
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([session, paths, state]() {
                QList<payload::Result> pending;
                pending.reserve(16);

                QList<payload::UndoItem> undoItems;
                undoItems.reserve(paths.size());

                int completed = 0;
                for (const SdfPath& path : paths) {
                    if (!session || session->isProgressBlockCancelled())
                        break;

                    payload::Result result;
                    result.path = path;
                    result.message = "payload unload failed";
                    result.status = Session::Notify::Status::Error;

                    payload::UndoItem undoItem;
                    QString error;

                    try {
                        WRITE_LOCKER(locker, session->stageLock(), "stageLock");
                        const UsdStageRefPtr stage = session->stageUnsafe();
                        result.success = payload::applyUnload(stage, path, undoItem, error);
                    } catch (...) {
                        result.success = false;
                        error = "exception";
                    }

                    result.message = result.success ? "payload unloaded" : "payload unload failed";
                    result.status = result.success ? Session::Notify::Status::Info : Session::Notify::Status::Error;

                    if (result.success)
                        undoItems.append(undoItem);

                    pending.append(result);
                    ++completed;

                    if (pending.size() >= 16) {
                        const QList<payload::Result> batch = pending;
                        QMetaObject::invokeMethod(
                            session,
                            [session, batch, completed]() { payload::flushResults(session, batch, completed); },
                            Qt::QueuedConnection);
                        pending.clear();
                    }
                }

                if (!pending.isEmpty()) {
                    const QList<payload::Result> batch = pending;
                    QMetaObject::invokeMethod(
                        session, [session, batch, completed]() { payload::flushResults(session, batch, completed); },
                        Qt::QueuedConnection);
                }

                state->undoItems = undoItems;

                QMetaObject::invokeMethod(
                    session,
                    [session]() {
                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },

        // undo
        [state](Session* session) {
            if (!session || state->undoItems.isEmpty())
                return;

            session->beginProgressBlock("undo unload payloads", state->undoItems.size());
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([session, state]() {
                QList<payload::Result> pending;
                pending.reserve(16);

                int completed = 0;
                for (const payload::UndoItem& item : state->undoItems) {
                    if (!session || session->isProgressBlockCancelled())
                        break;

                    payload::Result result;
                    result.path = item.path;
                    result.message = "payload undo failed";
                    result.status = Session::Notify::Status::Error;

                    QString error;
                    try {
                        WRITE_LOCKER(locker, session->stageLock(), "stageLock");
                        const UsdStageRefPtr stage = session->stageUnsafe();
                        result.success = payload::restoreState(stage, item, error);
                    } catch (...) {
                        result.success = false;
                        error = "exception";
                    }

                    result.message = result.success ? "payload restored" : "payload undo failed";
                    result.status = result.success ? Session::Notify::Status::Info : Session::Notify::Status::Error;

                    pending.append(result);
                    ++completed;

                    if (pending.size() >= 16) {
                        const QList<payload::Result> batch = pending;
                        QMetaObject::invokeMethod(
                            session,
                            [session, batch, completed]() { payload::flushResults(session, batch, completed); },
                            Qt::QueuedConnection);
                        pending.clear();
                    }
                }

                if (!pending.isEmpty()) {
                    const QList<payload::Result> batch = pending;
                    QMetaObject::invokeMethod(
                        session, [session, batch, completed]() { payload::flushResults(session, batch, completed); },
                        Qt::QueuedConnection);
                }

                QMetaObject::invokeMethod(
                    session,
                    [session]() {
                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

Command
isolatePaths(const QList<SdfPath>& paths)
{
    auto state = std::make_shared<QList<SdfPath>>();

    return Command(
        // redo
        [paths, state](Session* session) {
            session->beginProgressBlock("isolate paths", 1);

            QThreadPool::globalInstance()->start([session, paths, state]() {
                *state = session->mask();
                session->setMask(paths);

                QMetaObject::invokeMethod(
                    session,
                    [session, paths]() {
                        using Status = Session::Notify::Status;
                        session->updateProgressNotify(Session::Notify("paths isolated", paths, Status::Info), 1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [state](Session* session) {
            session->beginProgressBlock("undo isolate paths", 1);

            QThreadPool::globalInstance()->start([session, state]() {
                session->setMask(*state);

                QMetaObject::invokeMethod(
                    session,
                    [session, state]() {
                        using Status = Session::Notify::Status;
                        session->updateProgressNotify(Session::Notify("isolate undone", *state, Status::Info), 1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

Command
selectPaths(const QList<SdfPath>& paths)
{
    auto previous = std::make_shared<QList<SdfPath>>();
    return Command(
        // redo
        [paths, previous](Session* session) {
            session->beginProgressBlock("select paths", 1);

            QThreadPool::globalInstance()->start([session, paths, previous]() {
                *previous = session->selectionList()->paths();
                session->selectionList()->updatePaths(paths);

                QMetaObject::invokeMethod(
                    session,
                    [session, paths]() {
                        using Status = Session::Notify::Status;
                        Session::Notify notify("paths selected", paths, Status::Info);
                        session->updateProgressNotify(notify, 1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [previous](Session* session) {
            session->beginProgressBlock("undo select paths", 1);

            QThreadPool::globalInstance()->start([session, previous]() {
                session->selectionList()->updatePaths(*previous);

                QMetaObject::invokeMethod(
                    session,
                    [session, previous]() {
                        using Status = Session::Notify::Status;
                        Session::Notify notify("select undone", *previous, Status::Info);
                        session->updateProgressNotify(notify, 1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

Command
showPaths(const QList<SdfPath>& paths, bool recursive)
{
    auto state = std::make_shared<QHash<SdfPath, bool>>();
    return Command(
        // redo
        [paths, recursive, state](Session* session) {
            session->beginProgressBlock("show paths", 1);
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([session, paths, recursive, state]() {
                bool success = false;
                {
                    WRITE_LOCKER(locker, session->stageLock(), "stageLock");

                    const UsdStageRefPtr stage = session->stageUnsafe();
                    if (stage) {
                        state->clear();
                        for (const SdfPath& path : paths)
                            state->insert(path, stage::isVisible(stage, path));

                        stage::setVisible(stage, paths, true, recursive);
                        success = true;
                    }
                }

                QMetaObject::invokeMethod(
                    session,
                    [session, paths, success]() {
                        using Status = Session::Notify::Status;
                        Session::Notify notify(success ? "paths shown" : "show paths failed", paths,
                                               success ? Status::Info : Status::Error);
                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);
                        session->updateProgressNotify(notify, 1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [state, recursive](Session* session) {
            session->beginProgressBlock("undo show paths", 1);
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([session, state, recursive]() {
                bool success = false;
                QList<SdfPath> restoredPaths;
                {
                    WRITE_LOCKER(locker, session->stageLock(), "stageLock");

                    const UsdStageRefPtr stage = session->stageUnsafe();
                    if (stage) {
                        restoredPaths = state->keys();
                        for (auto it = state->cbegin(); it != state->cend(); ++it)
                            stage::setVisible(stage, { it.key() }, it.value(), recursive);

                        success = true;
                    }
                }

                QMetaObject::invokeMethod(
                    session,
                    [session, restoredPaths, success]() {
                        using Status = Session::Notify::Status;
                        Session::Notify notify(success ? "show undone" : "undo show paths failed", restoredPaths,
                                               success ? Status::Info : Status::Error);
                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);
                        session->updateProgressNotify(notify, 1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

Command
hidePaths(const QList<SdfPath>& paths, bool recursive)
{
    auto state = std::make_shared<QHash<SdfPath, bool>>();
    return Command(
        // redo
        [paths, recursive, state](Session* session) {
            session->beginProgressBlock("hide paths", 1);
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([session, paths, recursive, state]() {
                bool success = false;
                {
                    WRITE_LOCKER(locker, session->stageLock(), "stageLock");

                    const UsdStageRefPtr stage = session->stageUnsafe();
                    if (stage) {
                        state->clear();
                        for (const SdfPath& path : paths)
                            state->insert(path, stage::isVisible(stage, path));

                        stage::setVisible(stage, paths, false, recursive);
                        success = true;
                    }
                }

                QMetaObject::invokeMethod(
                    session,
                    [session, paths, success]() {
                        using Status = Session::Notify::Status;
                        Session::Notify notify(success ? "paths hidden" : "hide paths failed", paths,
                                               success ? Status::Info : Status::Error);
                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);
                        session->updateProgressNotify(notify, 1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [state, recursive](Session* session) {
            session->beginProgressBlock("undo hide paths", 1);
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([session, state, recursive]() {
                bool success = false;
                QList<SdfPath> restoredPaths;
                {
                    WRITE_LOCKER(locker, session->stageLock(), "stageLock");

                    const UsdStageRefPtr stage = session->stageUnsafe();
                    if (stage) {
                        restoredPaths = state->keys();
                        for (auto it = state->cbegin(); it != state->cend(); ++it)
                            stage::setVisible(stage, { it.key() }, it.value(), recursive);

                        success = true;
                    }
                }

                QMetaObject::invokeMethod(
                    session,
                    [session, restoredPaths, success]() {
                        using Status = Session::Notify::Status;
                        Session::Notify notify(success ? "hide undone" : "undo hide paths failed", restoredPaths,
                                               success ? Status::Info : Status::Error);
                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);
                        session->updateProgressNotify(notify, 1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

Command
stageUp(Session::StageUp stageUp)
{
    auto state = std::make_shared<Session::StageUp>(Session::StageUp::Y);
    return Command(
        // redo
        [stageUp, state](Session* session) {
            session->beginProgressBlock("set stage up", 1);

            QThreadPool::globalInstance()->start([session, stageUp, state]() {
                *state = session->stageUp();
                session->setStageUp(stageUp);

                QMetaObject::invokeMethod(
                    session,
                    [session, stageUp]() {
                        using Status = Session::Notify::Status;
                        const QString axis = (stageUp == Session::StageUp::Z) ? "Z" : "Y";
                        session->updateProgressNotify(Session::Notify(QString("stage up set to %1").arg(axis), {},
                                                                      Status::Info),
                                                      1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [state](Session* session) {
            session->beginProgressBlock("undo set stage up", 1);

            QThreadPool::globalInstance()->start([session, state]() {
                session->setStageUp(*state);
                QMetaObject::invokeMethod(
                    session,
                    [session, state]() {
                        using Status = Session::Notify::Status;
                        const QString axis = (*state == Session::StageUp::Z) ? "Z" : "Y";
                        session->updateProgressNotify(Session::Notify(QString("set stage up undone to %1").arg(axis),
                                                                      {}, Status::Info),
                                                      1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

namespace snapshot {
    struct PrimState {
        SdfPath stagePath;
        SdfPath specPath;
        SdfLayerRefPtr snapshotLayer;
    };

    struct DeleteState {
        QVector<PrimState> prims;
        QHash<SdfPath, TfTokenVector> parentOrders;
    };

    using PrimSnapshot = QVector<PrimState>;

    inline void ensureParentSpecs(const SdfLayerHandle& layer, const SdfPath& path)
    {
        if (!layer)
            return;

        const SdfPath parent = path.GetParentPath();
        if (parent.IsEmpty() || parent == SdfPath::AbsoluteRootPath())
            return;

        if (!layer->GetPrimAtPath(parent)) {
            ensureParentSpecs(layer, parent);
            SdfCreatePrimInLayer(layer, parent);
        }
    }

    inline bool capturePrimToLayer(UsdStageRefPtr stage, const SdfPath& stagePath, PrimState& out)
    {
        if (!stage)
            return false;

        const UsdPrim prim = stage->GetPrimAtPath(stagePath);
        if (!prim)
            return false;

        const auto& stack = prim.GetPrimStack();
        if (stack.empty())
            return false;

        for (const SdfPrimSpecHandle& spec : stack) {
            if (!spec)
                continue;

            const SdfLayerHandle srcLayer = spec->GetLayer();
            const SdfPath specPath = spec->GetPath();
            if (!srcLayer || specPath.IsEmpty())
                continue;

            if (!srcLayer->GetPrimAtPath(specPath))
                continue;

            SdfLayerRefPtr snapshotLayer = SdfLayer::CreateAnonymous(".usda");
            if (!snapshotLayer)
                continue;

            ensureParentSpecs(snapshotLayer, specPath);

            if (SdfCopySpec(srcLayer, specPath, snapshotLayer, specPath)) {
                out.stagePath = stagePath;
                out.specPath = specPath;
                out.snapshotLayer = snapshotLayer;
                return true;
            }
        }

        return false;
    }

    inline void restorePrimFromSnapshotLayer(const SdfLayerHandle& dstLayer, const PrimState& state)
    {
        if (!dstLayer || !state.snapshotLayer)
            return;

        ensureParentSpecs(dstLayer, state.stagePath);
        SdfCopySpec(state.snapshotLayer, state.specPath, dstLayer, state.stagePath);
    }

    inline bool isStrongestEditable(UsdStageRefPtr stage, const UsdPrim& prim)
    {
        if (!stage || !prim)
            return false;

        const SdfLayerHandle editLayer = stage->GetEditTarget().GetLayer();
        if (!editLayer)
            return false;

        const auto& stack = prim.GetPrimStack();
        if (stack.empty())
            return false;

        const SdfPrimSpecHandle& strongest = stack.front();
        if (!strongest)
            return false;

        return strongest->GetLayer() == editLayer;
    }

    inline QList<SdfPath> filterEditablePaths(UsdStageRefPtr stage, const QList<SdfPath>& paths)
    {
        QList<SdfPath> result;
        if (!stage)
            return result;

        for (const SdfPath& path : paths) {
            const SdfPath primPath = path.IsPropertyPath() ? path.GetPrimPath() : path;
            const UsdPrim prim = stage->GetPrimAtPath(primPath);
            if (isStrongestEditable(stage, prim))
                result.append(primPath);
        }
        return result;
    }

    inline QList<SdfPath> minimalRootPaths(QList<SdfPath> paths)
    {
        std::sort(paths.begin(), paths.end(),
                  [](const SdfPath& a, const SdfPath& b) { return a.GetPathElementCount() < b.GetPathElementCount(); });

        QList<SdfPath> result;
        for (const SdfPath& path : paths) {
            bool covered = false;
            for (const SdfPath& existing : result) {
                if (path == existing || path.HasPrefix(existing)) {
                    covered = true;
                    break;
                }
            }
            if (!covered)
                result.append(path);
        }
        return result;
    }

    inline bool captureParentOrder(UsdStageRefPtr stage, const SdfPath& parentPath, TfTokenVector& out)
    {
        if (!stage)
            return false;

        const UsdPrim parent = stage->GetPrimAtPath(parentPath);
        if (!parent)
            return false;

        out.clear();
        for (const UsdPrim& child : parent.GetAllChildren())
            out.push_back(child.GetName());

        return !out.empty();
    }

    inline void restoreParentOrder(UsdStageRefPtr stage, const SdfPath& parentPath, const TfTokenVector& childOrder)
    {
        if (!stage || childOrder.empty())
            return;

        const SdfLayerHandle editLayer = stage->GetEditTarget().GetLayer();
        if (!editLayer)
            return;

        ensureParentSpecs(editLayer, parentPath);
        if (!editLayer->GetPrimAtPath(parentPath))
            SdfCreatePrimInLayer(editLayer, parentPath);

        const UsdPrim parent = stage->GetPrimAtPath(parentPath);
        if (!parent)
            return;

        parent.SetChildrenReorder(childOrder);
    }

    inline bool removePrimSpec(const SdfLayerHandle& layer, const SdfPath& specPath)
    {
        if (!layer || specPath.IsEmpty() || specPath == SdfPath::AbsoluteRootPath())
            return false;

        if (!layer->GetPrimAtPath(specPath))
            return false;

        SdfBatchNamespaceEdit edits;
        edits.Add(specPath, SdfPath::EmptyPath());

        if (!layer->CanApply(edits))
            return false;

        return layer->Apply(edits);
    }

    inline void sortByHierarchy(PrimSnapshot& snapshot)
    {
        std::sort(snapshot.begin(), snapshot.end(), [](const PrimState& a, const PrimState& b) {
            const size_t ac = a.stagePath.GetPathElementCount();
            const size_t bc = b.stagePath.GetPathElementCount();
            if (ac != bc)
                return ac < bc;
            return a.stagePath.GetString() < b.stagePath.GetString();
        });
    }

}  // namespace snapshot

Command
deletePaths(const QList<SdfPath>& inPaths)
{
    auto state = std::make_shared<snapshot::DeleteState>();

    return Command(
        // redo
        [inPaths, state](Session* session) {
            session->beginProgressBlock("delete paths", 1);
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([session, inPaths, state]() {
                bool success = false;
                QList<SdfPath> changed;

                {
                    WRITE_LOCKER(locker, session->stageLock(), "stageLock");

                    const UsdStageRefPtr stage = session->stageUnsafe();
                    if (stage) {
                        QList<SdfPath> editable = snapshot::filterEditablePaths(stage, inPaths);
                        QList<SdfPath> paths = snapshot::minimalRootPaths(editable);

                        qDebug() << "deletePaths redo: requested" << inPaths.size() << "editable" << editable.size()
                                 << "root paths" << paths.size();

                        state->prims.clear();
                        state->parentOrders.clear();

                        QSet<SdfPath> changedSet;

                        for (const SdfPath& path : paths) {
                            qDebug() << "deletePaths redo: candidate path" << qt::StringToQString(path.GetString());

                            changedSet.insert(path);

                            const SdfPath parentPath = path.GetParentPath();
                            if (!parentPath.IsEmpty() && parentPath != SdfPath::AbsoluteRootPath()) {
                                changedSet.insert(parentPath);

                                if (!state->parentOrders.contains(parentPath)) {
                                    TfTokenVector order;
                                    if (snapshot::captureParentOrder(stage, parentPath, order)) {
                                        state->parentOrders.insert(parentPath, order);
                                        qDebug() << "deletePaths redo: captured parent order for"
                                                 << qt::StringToQString(parentPath.GetString()) << "children"
                                                 << static_cast<int>(order.size());
                                    }
                                    else {
                                        qDebug() << "deletePaths redo: failed to capture parent order for"
                                                 << qt::StringToQString(parentPath.GetString());
                                    }
                                }
                            }
                        }

                        bool removedAny = false;
                        const SdfLayerHandle editLayer = stage->GetEditTarget().GetLayer();
                        if (editLayer) {
                            for (const SdfPath& path : paths) {
                                snapshot::PrimState primState;
                                if (!snapshot::capturePrimToLayer(stage, path, primState)) {
                                    qDebug() << "deletePaths redo: failed to capture prim"
                                             << qt::StringToQString(path.GetString());
                                    continue;
                                }

                                qDebug() << "deletePaths redo: removing prim" << qt::StringToQString(path.GetString())
                                         << "spec path" << qt::StringToQString(primState.specPath.GetString());

                                if (!snapshot::removePrimSpec(editLayer, primState.specPath)) {
                                    qDebug() << "deletePaths redo: failed to remove prim spec"
                                             << qt::StringToQString(primState.specPath.GetString());
                                    continue;
                                }

                                state->prims.append(primState);
                                removedAny = true;
                            }
                        }
                        else {
                            qDebug() << "deletePaths redo: missing edit layer";
                        }

                        qDebug() << "deletePaths redo: captured prims" << state->prims.size()
                                 << "captured parent orders" << state->parentOrders.size();

                        if (removedAny) {
                            changed = changedSet.values();
                            success = true;
                            qDebug() << "deletePaths redo: prims removed";
                        }
                    }
                }

                QMetaObject::invokeMethod(
                    session,
                    [session, changed, success]() {
                        using Status = Session::Notify::Status;
                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);
                        session->updateProgressNotify(Session::Notify(success ? "paths deleted" : "delete paths failed",
                                                                      changed, success ? Status::Info : Status::Error),
                                                      1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [state](Session* session) {
            session->beginProgressBlock("undo delete paths", 1);
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([session, state]() {
                bool success = false;
                QList<SdfPath> changed;
                {
                    WRITE_LOCKER(locker, session->stageLock(), "stageLock");

                    const UsdStageRefPtr stage = session->stageUnsafe();
                    if (stage) {
                        const SdfLayerHandle editLayer = stage->GetEditTarget().GetLayer();
                        if (editLayer) {
                            qDebug() << "deletePaths undo: restoring prims" << state->prims.size() << "parent orders"
                                     << state->parentOrders.size();

                            snapshot::sortByHierarchy(state->prims);

                            QSet<SdfPath> changedSet;

                            for (const auto& primState : state->prims) {
                                qDebug() << "deletePaths undo: restoring prim"
                                         << qt::StringToQString(primState.stagePath.GetString()) << "from spec"
                                         << qt::StringToQString(primState.specPath.GetString());

                                snapshot::restorePrimFromSnapshotLayer(editLayer, primState);
                                changedSet.insert(primState.stagePath);

                                const SdfPath parentPath = primState.stagePath.GetParentPath();
                                if (!parentPath.IsEmpty() && parentPath != SdfPath::AbsoluteRootPath())
                                    changedSet.insert(parentPath);
                            }

                            for (auto it = state->parentOrders.cbegin(); it != state->parentOrders.cend(); ++it) {
                                qDebug() << "deletePaths undo: restoring parent order for"
                                         << qt::StringToQString(it.key().GetString()) << "children"
                                         << static_cast<int>(it.value().size());

                                snapshot::restoreParentOrder(stage, it.key(), it.value());
                                changedSet.insert(it.key());
                            }

                            qDebug() << "deletePaths undo: restored paths" << state->prims.size();

                            changed = changedSet.values();
                            success = true;
                        }
                        else {
                            qDebug() << "deletePaths undo: missing edit layer";
                        }
                    }
                }

                QMetaObject::invokeMethod(
                    session,
                    [session, changed, success]() {
                        using Status = Session::Notify::Status;
                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);
                        session->updateProgressNotify(Session::Notify(success ? "delete undone"
                                                                              : "undo delete paths failed",
                                                                      changed, success ? Status::Info : Status::Error),
                                                      1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

namespace names {
    inline std::string makeUsdSafeName(const std::string& input)
    {
        if (input.empty())
            return "Prim";

        std::string result;
        result.reserve(input.size());

        for (char c : input) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
                result.push_back(c);
            }
            else {
                result.push_back('_');
            }
        }
        if (!result.empty() && (result[0] >= '0' && result[0] <= '9')) {
            result[0] = '_';
        }
        bool allUnderscore = true;
        for (char c : result) {
            if (c != '_') {
                allUnderscore = false;
                break;
            }
        }

        if (allUnderscore)
            return "Prim";

        if (!SdfPath::IsValidIdentifier(result))
            return "Prim";

        return result;
    }

    inline UsdStageLoadRules remapLoadRules(const UsdStageLoadRules& rules, const SdfPath& oldPath,
                                            const SdfPath& newPath)
    {
        UsdStageLoadRules out;

        for (const auto& r : rules.GetRules()) {
            const SdfPath& p = r.first;
            const auto& policy = r.second;

            if (p.HasPrefix(oldPath)) {
                SdfPath rel = p.MakeRelativePath(oldPath);
                SdfPath mapped = newPath.AppendPath(rel);
                out.AddRule(mapped, policy);
            }
            else {
                out.AddRule(p, policy);
            }
        }
        return out;
    }
}  // namespace names

Command
renamePath(const SdfPath& path, const QString& newNameInput)
{
    struct RenameState {
        SdfPath oldPath;
        SdfPath newPath;
        SdfPath parentPath;
        TfTokenVector oldOrder;
        TfTokenVector newOrder;
    };

    auto state = std::make_shared<RenameState>();

    auto buildNewPath = [](const SdfPath& path, const QString& input) {
        const QString trimmed = input.trimmed();
        if (trimmed.isEmpty())
            return SdfPath();

        const std::string name = names::makeUsdSafeName(QStringToString(trimmed));
        if (!SdfPath::IsValidIdentifier(name))
            return SdfPath();

        return path.GetParentPath().AppendChild(TfToken(name));
    };

    auto remapChildOrder = [](const TfTokenVector& order, const TfToken& oldName, const TfToken& newName) {
        TfTokenVector out = order;
        for (TfToken& token : out) {
            if (token == oldName) {
                token = newName;
                break;
            }
        }
        return out;
    };

    auto applyRename = [](UsdStageRefPtr stage, const SdfPath& from, const SdfPath& to, QString& error) -> bool {
        if (!stage) {
            error = "Invalid stage";
            return false;
        }

        UsdPrim prim = stage->GetPrimAtPath(from);
        if (!prim) {
            error = "Invalid prim";
            return false;
        }

        const SdfLayerHandle editLayer = stage->GetEditTarget().GetLayer();
        if (!editLayer) {
            error = "No edit layer";
            return false;
        }

        if (!editLayer->GetPrimAtPath(from)) {
            error = "Path not in edit layer";
            return false;
        }

        if (stage->GetPrimAtPath(to)) {
            error = "Target exists";
            return false;
        }

        SdfBatchNamespaceEdit edits;
        edits.Add(SdfNamespaceEdit(from, to));

        if (!editLayer->CanApply(edits)) {
            error = "CanApply failed";
            return false;
        }

        return editLayer->Apply(edits);
    };

    return Command(
        // redo
        [path, newNameInput, buildNewPath, remapChildOrder, applyRename, state](Session* session) {
            session->beginProgressBlock("rename path", 1);
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([=]() {
                bool hadStage = true;
                bool renamed = false;
                bool noop = false;
                QString error;
                SdfPath newPath;

                {
                    WRITE_LOCKER(locker, session->stageLock(), "stageLock");
                    const UsdStageRefPtr stage = session->stageUnsafe();

                    if (!stage) {
                        hadStage = false;
                    }
                    else {
                        newPath = buildNewPath(path, newNameInput);
                        if (newPath.IsEmpty() || newPath == path) {
                            noop = true;
                        }
                        else {
                            state->oldPath = path;
                            state->newPath = newPath;
                            state->parentPath = path.GetParentPath();
                            state->oldOrder.clear();
                            state->newOrder.clear();

                            if (!state->parentPath.IsEmpty() && state->parentPath != SdfPath::AbsoluteRootPath())
                                snapshot::captureParentOrder(stage, state->parentPath, state->oldOrder);

                            const UsdStageLoadRules rules = stage->GetLoadRules();

                            if (applyRename(stage, path, newPath, error)) {
                                stage->SetLoadRules(names::remapLoadRules(rules, path, newPath));

                                if (!state->oldOrder.empty()) {
                                    state->newOrder = remapChildOrder(state->oldOrder, path.GetNameToken(),
                                                                      newPath.GetNameToken());
                                    snapshot::restoreParentOrder(stage, state->parentPath, state->newOrder);
                                }

                                renamed = true;
                            }
                        }
                    }
                }

                QMetaObject::invokeMethod(
                    session,
                    [=]() {
                        using Status = Session::Notify::Status;

                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);

                        if (!hadStage) {
                            session->updateProgressNotify(Session::Notify("rename path failed", {}, Status::Error), 1);
                            session->endProgressBlock();
                            return;
                        }

                        if (noop) {
                            session->updateProgressNotify(Session::Notify("rename skipped", {}, Status::Info), 1);
                            session->endProgressBlock();
                            return;
                        }

                        if (!renamed) {
                            session->updateProgressNotify(Session::Notify("rename path failed", {}, Status::Error), 1);
                            session->endProgressBlock();
                            return;
                        }

                        QList<SdfPath> updated;
                        for (const auto& p : session->selectionList()->paths()) {
                            if (p.HasPrefix(path))
                                updated.append(newPath.AppendPath(p.MakeRelativePath(path)));
                            else
                                updated.append(p);
                        }

                        session->selectionList()->updatePaths(updated);
                        session->updateProgressNotify(Session::Notify("path renamed", { path, newPath }, Status::Info),
                                                      1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },

        // undo
        [applyRename, state](Session* session) {
            session->beginProgressBlock("undo rename path", 1);
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([=]() {
                bool hadStage = true;
                bool restored = false;
                QString error;

                {
                    WRITE_LOCKER(locker, session->stageLock(), "stageLock");
                    const UsdStageRefPtr stage = session->stageUnsafe();

                    if (!stage) {
                        hadStage = false;
                    }
                    else if (state->oldPath.IsEmpty() || state->newPath.IsEmpty()) {
                        error = "invalid state";
                    }
                    else {
                        const UsdStageLoadRules rules = stage->GetLoadRules();

                        if (applyRename(stage, state->newPath, state->oldPath, error)) {
                            stage->SetLoadRules(names::remapLoadRules(rules, state->newPath, state->oldPath));

                            if (!state->oldOrder.empty() && !state->parentPath.IsEmpty()
                                && state->parentPath != SdfPath::AbsoluteRootPath()) {
                                snapshot::restoreParentOrder(stage, state->parentPath, state->oldOrder);
                            }

                            restored = true;
                        }
                    }
                }

                QMetaObject::invokeMethod(
                    session,
                    [=]() {
                        using Status = Session::Notify::Status;

                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);

                        if (!hadStage) {
                            session->updateProgressNotify(Session::Notify("undo rename path failed", {}, Status::Error),
                                                          1);
                            session->endProgressBlock();
                            return;
                        }

                        if (!restored) {
                            session->updateProgressNotify(Session::Notify("undo rename path failed", {}, Status::Error),
                                                          1);
                            session->endProgressBlock();
                            return;
                        }

                        QList<SdfPath> updated;
                        for (const auto& p : session->selectionList()->paths()) {
                            if (p.HasPrefix(state->newPath))
                                updated.append(state->oldPath.AppendPath(p.MakeRelativePath(state->newPath)));
                            else
                                updated.append(p);
                        }

                        session->selectionList()->updatePaths(updated);
                        session->updateProgressNotify(Session::Notify("rename undone", { state->oldPath }, Status::Info),
                                                      1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

Command
newXformPath(const SdfPath& parentPath, const QString& nameInput)
{
    struct NewXformState {
        SdfPath parentPath;
        SdfPath createdPath;
        TfTokenVector oldOrder;
        TfTokenVector newOrder;
    };

    auto state = std::make_shared<NewXformState>();

    auto buildNewPath = [](const SdfPath& parentPath, const QString& input) {
        if (parentPath.IsEmpty() || parentPath == SdfPath::AbsoluteRootPath())
            return SdfPath();

        const QString trimmed = input.trimmed();
        if (trimmed.isEmpty())
            return SdfPath();

        const std::string name = names::makeUsdSafeName(QStringToString(trimmed));
        if (!SdfPath::IsValidIdentifier(name))
            return SdfPath();

        return parentPath.AppendChild(TfToken(name));
    };

    return Command(
        // redo
        [parentPath, nameInput, buildNewPath, state](Session* session) {
            session->beginProgressBlock("new xform", 1);
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([=]() {
                bool hadStage = true;
                bool created = false;
                bool noop = false;
                QString error;
                SdfPath newPath;

                {
                    WRITE_LOCKER(locker, session->stageLock(), "stageLock");
                    const UsdStageRefPtr stage = session->stageUnsafe();

                    if (!stage) {
                        hadStage = false;
                    }
                    else {
                        newPath = buildNewPath(parentPath, nameInput);
                        if (newPath.IsEmpty()) {
                            noop = true;
                        }
                        else {
                            state->parentPath = parentPath;
                            state->createdPath = newPath;
                            state->oldOrder.clear();
                            state->newOrder.clear();

                            const SdfLayerHandle editLayer = stage->GetEditTarget().GetLayer();
                            if (!editLayer) {
                                error = "no edit layer";
                            }
                            else {
                                UsdPrim parentPrim = stage->GetPrimAtPath(parentPath);
                                if (!parentPrim) {
                                    error = "invalid parent";
                                }
                                else if (stage->GetPrimAtPath(newPath)) {
                                    error = "target exists";
                                }
                                else {
                                    snapshot::captureParentOrder(stage, parentPath, state->oldOrder);

                                    UsdGeomXform xform = UsdGeomXform::Define(stage, newPath);
                                    if (!xform || !xform.GetPrim()) {
                                        error = "define failed";
                                    }
                                    else {
                                        state->newOrder = state->oldOrder;
                                        state->newOrder.push_back(newPath.GetNameToken());
                                        snapshot::restoreParentOrder(stage, parentPath, state->newOrder);
                                        created = true;
                                    }
                                }
                            }
                        }
                    }
                }

                QMetaObject::invokeMethod(
                    session,
                    [=]() {
                        using Status = Session::Notify::Status;

                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);

                        if (!hadStage) {
                            session->updateProgressNotify(Session::Notify("new xform failed", {}, Status::Error), 1);
                            session->endProgressBlock();
                            return;
                        }

                        if (noop) {
                            session->updateProgressNotify(Session::Notify("new xform skipped", {}, Status::Info), 1);
                            session->endProgressBlock();
                            return;
                        }

                        if (!created) {
                            session->updateProgressNotify(Session::Notify("new xform failed", {}, Status::Error), 1);
                            session->endProgressBlock();
                            return;
                        }

                        session->selectionList()->updatePaths({ newPath });
                        session->updateProgressNotify(Session::Notify("xform created", { parentPath, newPath },
                                                                      Status::Info),
                                                      1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },

        // undo
        [state](Session* session) {
            session->beginProgressBlock("undo new xform", 1);
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([=]() {
                bool hadStage = true;
                bool removed = false;

                {
                    WRITE_LOCKER(locker, session->stageLock(), "stageLock");
                    const UsdStageRefPtr stage = session->stageUnsafe();

                    if (!stage) {
                        hadStage = false;
                    }
                    else if (state->createdPath.IsEmpty()) {}
                    else {
                        const SdfLayerHandle editLayer = stage->GetEditTarget().GetLayer();
                        if (editLayer) {
                            removed = snapshot::removePrimSpec(editLayer, state->createdPath);

                            if (removed && !state->oldOrder.empty() && !state->parentPath.IsEmpty()
                                && state->parentPath != SdfPath::AbsoluteRootPath()) {
                                snapshot::restoreParentOrder(stage, state->parentPath, state->oldOrder);
                            }
                        }
                    }
                }

                QMetaObject::invokeMethod(
                    session,
                    [=]() {
                        using Status = Session::Notify::Status;

                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);

                        if (!hadStage) {
                            session->updateProgressNotify(Session::Notify("undo new xform failed", {}, Status::Error),
                                                          1);
                            session->endProgressBlock();
                            return;
                        }

                        if (!removed) {
                            session->updateProgressNotify(Session::Notify("undo new xform failed", {}, Status::Error),
                                                          1);
                            session->endProgressBlock();
                            return;
                        }

                        QList<SdfPath> updated;
                        for (const auto& p : session->selectionList()->paths()) {
                            if (p != state->createdPath)
                                updated.append(p);
                        }

                        session->selectionList()->updatePaths(updated);
                        session->updateProgressNotify(Session::Notify("new xform undone", { state->createdPath },
                                                                      Status::Info),
                                                      1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

Command
movePath(const SdfPath& fromPath, const SdfPath& newParentPath)
{
    struct MoveState {
        SdfPath oldPath;
        SdfPath newPath;
        SdfPath oldParentPath;
        SdfPath newParentPath;
        TfTokenVector oldParentOrder;
        TfTokenVector newParentOldOrder;
        TfTokenVector newParentNewOrder;
    };

    auto state = std::make_shared<MoveState>();

    auto removeToken = [](const TfTokenVector& order, const TfToken& name) {
        TfTokenVector out;
        out.reserve(order.size());
        for (const TfToken& token : order) {
            if (token != name)
                out.push_back(token);
        }
        return out;
    };

    auto appendTokenIfMissing = [](const TfTokenVector& order, const TfToken& name) {
        TfTokenVector out = order;
        if (std::find(out.begin(), out.end(), name) == out.end())
            out.push_back(name);
        return out;
    };

    auto applyMove = [](UsdStageRefPtr stage, const SdfPath& from, const SdfPath& toParent, QString& error) -> bool {
        if (!stage) {
            error = "Invalid stage";
            return false;
        }

        UsdPrim prim = stage->GetPrimAtPath(from);
        if (!prim) {
            error = "Invalid prim";
            return false;
        }

        UsdPrim newParent = stage->GetPrimAtPath(toParent);
        if (!newParent) {
            error = "Invalid parent";
            return false;
        }

        if (from.IsEmpty() || toParent.IsEmpty()) {
            error = "Invalid path";
            return false;
        }

        if (from == toParent || toParent.HasPrefix(from)) {
            error = "Invalid hierarchy";
            return false;
        }

        const SdfPath newPath = toParent.AppendChild(from.GetNameToken());
        if (stage->GetPrimAtPath(newPath)) {
            error = "Target exists";
            return false;
        }

        UsdNamespaceEditor editor(stage);
        if (!editor.ReparentPrim(prim, newParent)) {
            error = "ReparentPrim failed";
            return false;
        }

        std::string whyNot;
        if (!editor.CanApplyEdits(&whyNot)) {
            error = qt::StringToQString(whyNot);
            return false;
        }

        return editor.ApplyEdits();
    };

    return Command(
        // redo
        [fromPath, newParentPath, removeToken, appendTokenIfMissing, applyMove, state](Session* session) {
            session->beginProgressBlock("move path", 1);
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([=]() {
                bool hadStage = true;
                bool moved = false;
                bool noop = false;
                QString error;

                {
                    WRITE_LOCKER(locker, session->stageLock(), "stageLock");
                    const UsdStageRefPtr stage = session->stageUnsafe();

                    if (!stage) {
                        hadStage = false;
                    }
                    else {
                        const SdfPath oldParentPath = fromPath.GetParentPath();
                        const SdfPath targetPath = newParentPath.AppendChild(fromPath.GetNameToken());

                        if (fromPath.IsEmpty() || newParentPath.IsEmpty() || oldParentPath.IsEmpty()
                            || oldParentPath == SdfPath::AbsoluteRootPath()) {
                            noop = true;
                        }
                        else if (oldParentPath == newParentPath) {
                            noop = true;
                        }
                        else {
                            state->oldPath = fromPath;
                            state->newPath = targetPath;
                            state->oldParentPath = oldParentPath;
                            state->newParentPath = newParentPath;
                            state->oldParentOrder.clear();
                            state->newParentOldOrder.clear();
                            state->newParentNewOrder.clear();

                            snapshot::captureParentOrder(stage, oldParentPath, state->oldParentOrder);
                            snapshot::captureParentOrder(stage, newParentPath, state->newParentOldOrder);

                            if (applyMove(stage, fromPath, newParentPath, error)) {
                                const TfToken movedName = fromPath.GetNameToken();

                                if (!state->oldParentOrder.empty())
                                    snapshot::restoreParentOrder(stage, oldParentPath,
                                                                 removeToken(state->oldParentOrder, movedName));

                                state->newParentNewOrder = appendTokenIfMissing(state->newParentOldOrder, movedName);

                                if (!state->newParentNewOrder.empty())
                                    snapshot::restoreParentOrder(stage, newParentPath, state->newParentNewOrder);

                                moved = true;
                            }
                        }
                    }
                }

                QMetaObject::invokeMethod(
                    session,
                    [=]() {
                        using Status = Session::Notify::Status;

                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);

                        if (!hadStage) {
                            session->updateProgressNotify(Session::Notify("move path failed", {}, Status::Error), 1);
                            session->endProgressBlock();
                            return;
                        }

                        if (noop) {
                            session->updateProgressNotify(Session::Notify("move skipped", {}, Status::Info), 1);
                            session->endProgressBlock();
                            return;
                        }

                        if (!moved) {
                            session->updateProgressNotify(Session::Notify("move path failed", {}, Status::Error), 1);
                            session->endProgressBlock();
                            return;
                        }

                        QList<SdfPath> updated;
                        for (const auto& p : session->selectionList()->paths()) {
                            if (p.HasPrefix(fromPath))
                                updated.append(state->newPath.AppendPath(p.MakeRelativePath(fromPath)));
                            else
                                updated.append(p);
                        }

                        session->selectionList()->updatePaths(updated);
                        session->updateProgressNotify(Session::Notify("path moved", { state->oldPath, state->newPath },
                                                                      Status::Info),
                                                      1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },

        // undo
        [applyMove, state](Session* session) {
            session->beginProgressBlock("undo move path", 1);
            session->setPrimsUpdate(Session::PrimsUpdate::Deferred);

            QThreadPool::globalInstance()->start([=]() {
                bool hadStage = true;
                bool restored = false;
                QString error;

                {
                    WRITE_LOCKER(locker, session->stageLock(), "stageLock");
                    const UsdStageRefPtr stage = session->stageUnsafe();

                    if (!stage) {
                        hadStage = false;
                    }
                    else if (state->oldPath.IsEmpty() || state->newPath.IsEmpty() || state->oldParentPath.IsEmpty()
                             || state->newParentPath.IsEmpty()) {
                        error = "invalid state";
                    }
                    else {
                        if (applyMove(stage, state->newPath, state->oldParentPath, error)) {
                            if (!state->oldParentOrder.empty())
                                snapshot::restoreParentOrder(stage, state->oldParentPath, state->oldParentOrder);

                            if (!state->newParentOldOrder.empty())
                                snapshot::restoreParentOrder(stage, state->newParentPath, state->newParentOldOrder);

                            restored = true;
                        }
                    }
                }

                QMetaObject::invokeMethod(
                    session,
                    [=]() {
                        using Status = Session::Notify::Status;

                        session->setPrimsUpdate(Session::PrimsUpdate::Immediate);

                        if (!hadStage) {
                            session->updateProgressNotify(Session::Notify("undo move path failed", {}, Status::Error),
                                                          1);
                            session->endProgressBlock();
                            return;
                        }

                        if (!restored) {
                            session->updateProgressNotify(Session::Notify("undo move path failed", {}, Status::Error),
                                                          1);
                            session->endProgressBlock();
                            return;
                        }

                        QList<SdfPath> updated;
                        for (const auto& p : session->selectionList()->paths()) {
                            if (p.HasPrefix(state->newPath))
                                updated.append(state->oldPath.AppendPath(p.MakeRelativePath(state->newPath)));
                            else
                                updated.append(p);
                        }

                        session->selectionList()->updatePaths(updated);
                        session->updateProgressNotify(Session::Notify("move undone", { state->oldPath }, Status::Info),
                                                      1);
                        session->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

}  // namespace usdviewer
