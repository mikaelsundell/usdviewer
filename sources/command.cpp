// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "command.h"
#include "commandstack.h"
#include "qtutils.h"
#include "stageutils.h"
#include <QPointer>
#include <QtConcurrent>
#include <pxr/usd/sdf/copyUtils.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/variantSets.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>

namespace usd {
Command
loadPayloads(const QList<SdfPath>& paths, const QString& variantSet, const QString& variantValue)
{
    return Command(
        // redo
        [paths, variantSet, variantValue](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("redo load payloads", paths.size());

            QFuture<void> future = QtConcurrent::run([dm, paths, variantSet, variantValue]() {
                const bool useVariant = (!variantSet.isEmpty() && !variantValue.isEmpty());
                std::string setNameStd = QStringToString(variantSet);
                std::string setValueStd = QStringToString(variantValue);
                UsdStageRefPtr stage = dm->stage();

                size_t completed = 0;
                QList<SdfPath> loaded;
                QList<SdfPath> failed;
                {
                    QWriteLocker locker(dm->stageLock());
                    for (const SdfPath& path : paths) {
                        if (dm->isProgressBlockCancelled())
                            break;

                        QVariantMap info;
                        info["variantSet"] = variantSet;
                        info["variantValue"] = variantValue;

                        // todo: we need to add notify ordering here
                        /*QMetaObject::invokeMethod(
                            dm,
                            [dm, path, info, completed]() {
                                dm->updateProgressNotify(DataModel::Notify("loading payload", { path }, info),
                                                         completed);
                            },
                            Qt::QueuedConnection);*/

                        UsdPrim prim = stage->GetPrimAtPath(path);
                        if (!prim || !prim.HasPayload()) {
                            failed.append(path);

                            QMetaObject::invokeMethod(
                                dm,
                                [dm, path, completed]() {
                                    dm->updateProgressNotify(DataModel::Notify("payload failed", { path }), completed);
                                },
                                Qt::QueuedConnection);

                            continue;
                        }

                        try {
                            if (useVariant) {
                                if (prim.IsLoaded())
                                    prim.Unload();

                                UsdVariantSet vs = prim.GetVariantSet(setNameStd);
                                if (!vs.IsValid())
                                    throw std::runtime_error("variant set missing");

                                auto variants = vs.GetVariantNames();
                                if (std::find(variants.begin(), variants.end(), setValueStd) == variants.end())
                                    throw std::runtime_error("variant value missing");

                                vs.SetVariantSelection(setValueStd);
                            }

                            prim.Load();
                            loaded.append(path);
                            QMetaObject::invokeMethod(
                                dm,
                                [dm, path, completed]() {
                                    qDebug() << "sending payload loaded";

                                    dm->updateProgressNotify(DataModel::Notify("payload loaded", { path }),
                                                             completed + 1);
                                },
                                Qt::QueuedConnection);

                        } catch (...) {
                            failed.append(path);

                            QMetaObject::invokeMethod(
                                dm,
                                [dm, path, completed]() {
                                    dm->updateProgressNotify(DataModel::Notify("payload failed", { path }),
                                                             completed + 1);
                                },
                                Qt::QueuedConnection);
                        }

                        completed++;
                    }
                }
                QMetaObject::invokeMethod(
                    dm, [dm]() { dm->endProgressBlock(); }, Qt::QueuedConnection);
            });
        },
        // undo
        [paths](DataModel* dm, SelectionModel*) {
            // todo: undo should be fixed
        });
}

Command
unloadPayloads(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("unload payloads", paths.size());
            QPointer<DataModel> safe_dm = dm;
            QFuture<void> future = QtConcurrent::run([safe_dm, paths]() {
                if (!safe_dm)
                    return;

                UsdStageRefPtr stage = safe_dm->stage();
                size_t completed = 0;
                {
                    QWriteLocker locker(safe_dm->stageLock());
                    for (const SdfPath& path : paths) {
                        if (!safe_dm || safe_dm->isProgressBlockCancelled())
                            break;

                        UsdPrim prim = stage->GetPrimAtPath(path);
                        if (prim && prim.HasPayload())
                            prim.Unload();

                        QMetaObject::invokeMethod(
                            safe_dm,
                            [safe_dm, path, completed]() {
                                safe_dm->updateProgressNotify(DataModel::Notify("payload unloaded", { path }),
                                                              completed + 1);
                            },
                            Qt::QueuedConnection);

                        completed++;
                    }
                }
                QMetaObject::invokeMethod(
                    safe_dm,
                    [safe_dm]() {
                        if (!safe_dm)
                            return;
                        safe_dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [paths](DataModel* dm, SelectionModel*) {
            // todo: undo should be fixed
        });
}

Command
isolatePaths(const QList<SdfPath>& paths)
{
    auto previous = std::make_shared<QList<SdfPath>>();
    return Command(
        // redo
        [paths, previous](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("redo isolate", 1);

            QFuture<void> future = QtConcurrent::run([dm, paths, previous]() {
                {
                    *previous = dm->mask();
                    dm->setMask(paths);
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm, paths]() {
                        dm->updateProgressNotify(DataModel::Notify("Isolate set", paths), 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [previous](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("undo isolate", 1);

            QFuture<void> future = QtConcurrent::run([dm, previous]() {
                {
                    dm->setMask(*previous);
                }

                QMetaObject::invokeMethod(
                    dm,
                    [dm, previous]() {
                        dm->updateProgressNotify(DataModel::Notify("Isolate restored", *previous), 1);
                        dm->endProgressBlock();
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
        [paths, previous](DataModel*, SelectionModel* sel) {
            *previous = sel->paths();
            sel->updatePaths(paths);
        },
        // undo
        [previous](DataModel*, SelectionModel* sel) { sel->updatePaths(*previous); });
}

Command
showPaths(const QList<SdfPath>& paths, bool recursive)
{
    auto previous = std::make_shared<QHash<SdfPath, bool>>();
    return Command(
        // redo
        [paths, recursive, previous](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("redo show", 1);
            QFuture<void> future = QtConcurrent::run([dm, paths, recursive, previous]() {
                {
                    QWriteLocker lock(dm->stageLock());
                    previous->clear();
                    for (const SdfPath& path : paths) {
                        bool visible = isVisible(dm->stage(), path);
                        previous->insert(path, visible);
                    }
                    setVisible(dm->stage(), paths, true, recursive);
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm, paths]() {
                        dm->updateProgressNotify(DataModel::Notify("Shown", paths), 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [previous, recursive](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("undo show", 1);
            QFuture<void> future = QtConcurrent::run([dm, previous, recursive]() {
                {
                    QWriteLocker lock(dm->stageLock());
                    for (auto it = previous->cbegin(); it != previous->cend(); ++it) {
                        setVisible(dm->stage(), { it.key() }, it.value(), recursive);
                    }
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm, previous]() {
                        dm->updateProgressNotify(DataModel::Notify("Visibility restored", previous->keys()), 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

Command
hidePaths(const QList<SdfPath>& paths, bool recursive)
{
    return Command(
        // redo
        [paths, recursive](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("redo hide", 1);
            QFuture<void> future = QtConcurrent::run([dm, paths, recursive]() {
                {
                    QWriteLocker lock(dm->stageLock());
                    setVisible(dm->stage(), paths, false, recursive);
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm, paths]() {
                        dm->updateProgressNotify(DataModel::Notify("Hidden", paths), 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [paths, recursive](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("undo hide", 1);
            QFuture<void> future = QtConcurrent::run([dm, paths, recursive]() {
                {
                    QWriteLocker lock(dm->stageLock());
                    setVisible(dm->stage(), paths, true, recursive);
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm, paths]() {
                        dm->updateProgressNotify(DataModel::Notify("Shown", paths), 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

namespace utils {
    struct PrimState {
        SdfPath stagePath;  // composed path
        SdfPath specPath;   // authored path
        SdfLayerRefPtr layer;
    };
    using Snapshot = QVector<PrimState>;
    inline bool isStrongestEditable(UsdStageRefPtr stage, const UsdPrim& prim)
    {
        if (!prim)
            return false;

        const auto& stack = prim.GetPrimStack();
        if (stack.empty())
            return false;

        SdfLayerHandle editLayer = stage->GetEditTarget().GetLayer();
        const SdfPrimSpecHandle& strongest = stack.front();
        return strongest && strongest->GetLayer() == editLayer;
    }

    inline QList<SdfPath> filterEditablePaths(UsdStageRefPtr stage, const QList<SdfPath>& paths)
    {
        QList<SdfPath> result;
        for (const SdfPath& path : paths) {
            UsdPrim prim = stage->GetPrimAtPath(path);
            if (isStrongestEditable(stage, prim))
                result.append(path);
        }
        return result;
    }


    inline QList<SdfPath> minimalRootPaths(QList<SdfPath> paths)
    {
        std::sort(paths.begin(), paths.end(),
                  [](const SdfPath& a, const SdfPath& b) { return a.GetPathElementCount() < b.GetPathElementCount(); });
        QList<SdfPath> result;
        for (const SdfPath& p : paths) {
            bool isChild = false;
            for (const SdfPath& r : result) {
                if (p.HasPrefix(r)) {
                    isChild = true;
                    break;
                }
            }
            if (!isChild)
                result.append(p);
        }
        return result;
    }

    inline void ensureParentSpecs(const SdfLayerHandle& layer, const SdfPath& path)
    {
        SdfPath parent = path.GetParentPath();
        if (parent.IsEmpty() || parent == SdfPath::AbsoluteRootPath())
            return;

        if (!layer->GetPrimAtPath(parent)) {
            ensureParentSpecs(layer, parent);
            SdfCreatePrimInLayer(layer, parent);
        }
    }

    inline bool capturePrimToLayer(UsdStageRefPtr stage, const SdfPath& stagePath, PrimState& out)
    {
        UsdPrim prim = stage->GetPrimAtPath(stagePath);
        if (!prim)
            return false;

        const auto& stack = prim.GetPrimStack();
        if (stack.empty())
            return false;

        for (const SdfPrimSpecHandle& spec : stack) {
            if (!spec)
                continue;

            SdfLayerHandle srcLayer = spec->GetLayer();
            SdfPath specPath = spec->GetPath();
            if (!srcLayer || specPath.IsEmpty())
                continue;

            if (!srcLayer->GetPrimAtPath(specPath))
                continue;

            SdfLayerRefPtr tmp = SdfLayer::CreateAnonymous();
            if (SdfCopySpec(srcLayer, specPath, tmp, specPath)) {
                out.stagePath = stagePath;
                out.specPath = specPath;
                out.layer = tmp;
                return true;
            }
        }
        return false;
    }


    inline void restorePrimFromLayer(const SdfLayerHandle& dstLayer, const PrimState& state)
    {
        if (!state.layer)
            return;
        ensureParentSpecs(dstLayer, state.stagePath);
        SdfCopySpec(state.layer, state.specPath, dstLayer, state.stagePath);
    }

    inline void sortByHierarchy(Snapshot& snapShot)
    {
        std::sort(snapShot.begin(), snapShot.end(), [](const PrimState& a, const PrimState& b) {
            return a.stagePath.GetPathElementCount() < b.stagePath.GetPathElementCount();
        });
    }
}  // namespace utils

Command
deletePaths(const QList<SdfPath>& inPaths)
{
    auto snapshots = std::make_shared<utils::Snapshot>();
    return Command(
        // redo
        [inPaths, snapshots](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("delete prims", 1);
            QFuture<void> future = QtConcurrent::run([dm, inPaths, snapshots]() {
                QWriteLocker lock(dm->stageLock());
                UsdStageRefPtr stage = dm->stage();
                QList<SdfPath> filtered = utils::filterEditablePaths(stage, inPaths);
                QList<SdfPath> paths = utils::minimalRootPaths(filtered);
                snapshots->clear();

                for (const SdfPath& path : paths) {
                    utils::PrimState state;
                    if (utils::capturePrimToLayer(stage, path, state)) {
                        snapshots->append(state);
                        stage->RemovePrim(path);
                    }
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm, paths]() {
                        dm->updateProgressNotify(DataModel::Notify("Deleted", paths), 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [snapshots](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("undo delete", 1);
            QFuture<void> future = QtConcurrent::run([dm, snapshots]() {
                QWriteLocker lock(dm->stageLock());
                UsdStageRefPtr stage = dm->stage();
                SdfLayerHandle layer = stage->GetEditTarget().GetLayer();
                sortByHierarchy(*snapshots);

                QList<SdfPath> restored;
                for (const auto& s : *snapshots) {
                    restorePrimFromLayer(layer, s);
                    restored.append(s.stagePath);
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm, restored]() {
                        dm->updateProgressNotify(DataModel::Notify("Delete restored", restored), 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}
}  // namespace usd
