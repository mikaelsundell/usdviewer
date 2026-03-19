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

namespace usdviewer {
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
            dm->beginProgressBlock("Isolate", 1);

            QFuture<void> future = QtConcurrent::run([dm, paths, previous]() {
                {
                    *previous = dm->mask();
                    dm->setMask(paths);
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm, paths]() {
                        dm->updateProgressNotify(DataModel::Notify("Isolate", paths), 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [previous](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("Isolate restored", 1);
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
        [paths, previous](DataModel* dm, SelectionModel* sel) {
            dm->beginProgressBlock("Select", 1);
            QFuture<void> future = QtConcurrent::run([dm, sel, paths, previous]() {
                {
                    *previous = sel->paths();
                    sel->updatePaths(paths);
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm, paths]() {
                        using Status = DataModel::Notify::Status;
                        DataModel::Notify notify("Select", paths, Status::Info);

                        dm->updateProgressNotify(notify, 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [previous](DataModel* dm, SelectionModel* sel) {
            dm->beginProgressBlock("Select restored", 1);
            QFuture<void> future = QtConcurrent::run([dm, sel, previous]() {
                {
                    sel->updatePaths(*previous);
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm, previous]() {
                        using Status = DataModel::Notify::Status;
                        DataModel::Notify notify("Select restored", *previous, Status::Info);
                        dm->updateProgressNotify(notify, 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

Command
showPaths(const QList<SdfPath>& paths, bool recursive)
{
    auto previous = std::make_shared<QHash<SdfPath, bool>>();
    return Command(
        // redo
        [paths, recursive, previous](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("Show", 1);
            QFuture<void> future = QtConcurrent::run([dm, paths, recursive, previous]() {
                {
                    QWriteLocker lock(dm->stageLock());
                    previous->clear();
                    for (const SdfPath& path : paths) {
                        previous->insert(path, stage::isVisible(dm->stage(), path));
                    }
                    stage::setVisible(dm->stage(), paths, true, recursive);
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm, paths]() {
                        using Status = DataModel::Notify::Status;
                        DataModel::Notify notify("Show", paths, Status::Info);
                        dm->updateProgressNotify(notify, 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [previous, recursive](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("Show restored", 1);
            QFuture<void> future = QtConcurrent::run([dm, previous, recursive]() {
                {
                    QWriteLocker lock(dm->stageLock());
                    for (auto it = previous->cbegin(); it != previous->cend(); ++it) {
                        stage::setVisible(dm->stage(), { it.key() }, it.value(), recursive);
                    }
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm, previous]() {
                        using Status = DataModel::Notify::Status;
                        DataModel::Notify notify("Show restored", previous->keys(), Status::Info);
                        dm->updateProgressNotify(notify, 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

Command
hidePaths(const QList<SdfPath>& paths, bool recursive)
{
    auto previous = std::make_shared<QHash<SdfPath, bool>>();
    return Command(
        // redo
        [paths, recursive, previous](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("Hide", 1);
            QFuture<void> future = QtConcurrent::run([dm, paths, recursive, previous]() {
                {
                    QWriteLocker lock(dm->stageLock());
                    previous->clear();
                    for (const SdfPath& path : paths) {
                        previous->insert(path, stage::isVisible(dm->stage(), path));
                    }
                    stage::setVisible(dm->stage(), paths, false, recursive);
                }

                QMetaObject::invokeMethod(
                    dm,
                    [dm, paths]() {
                        using Status = DataModel::Notify::Status;
                        DataModel::Notify notify("Hide", paths, Status::Info);

                        dm->updateProgressNotify(notify, 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [previous, recursive](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("Hide restored", 1);
            QFuture<void> future = QtConcurrent::run([dm, previous, recursive]() {
                {
                    QWriteLocker lock(dm->stageLock());

                    for (auto it = previous->cbegin(); it != previous->cend(); ++it) {
                        stage::setVisible(dm->stage(), { it.key() }, it.value(), recursive);
                    }
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm, previous]() {
                        using Status = DataModel::Notify::Status;
                        DataModel::Notify notify("Hide restored", previous->keys(), Status::Info);

                        dm->updateProgressNotify(notify, 1);
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
            dm->beginProgressBlock("Delete", 1);
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
                        dm->updateProgressNotify(DataModel::Notify("Delete", paths), 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        // undo
        [snapshots](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("Delete restored", 1);
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

inline std::string
makeUsdSafeName(const std::string& input)
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

static UsdStageLoadRules
remapLoadRules(const UsdStageLoadRules& rules, const SdfPath& oldPath, const SdfPath& newPath)
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

Command
renamePath(const SdfPath& path, const SdfPath& newPathInput)
{
    auto buildNewPath = [](const SdfPath& path, const SdfPath& input) {
        std::string name = makeUsdSafeName(input.GetName());
        if (!SdfPath::IsValidIdentifier(name))
            return SdfPath();

        return path.GetParentPath().AppendChild(TfToken(name));
    };

    auto applyRename = [](UsdStageRefPtr stage, const SdfPath& from, const SdfPath& to, QString& error) -> bool {
        UsdPrim prim = stage->GetPrimAtPath(from);
        if (!prim) {
            error = "Invalid prim";
            return false;
        }

        auto stack = prim.GetPrimStack();
        if (stack.empty() || !stack.front()) {
            error = "No spec";
            return false;
        }

        SdfLayerHandle layer = stack.front()->GetLayer();
        if (!layer) {
            error = "No layer";
            return false;
        }

        if (stage->GetPrimAtPath(to)) {
            error = "Target exists";
            return false;
        }

        SdfBatchNamespaceEdit edits;
        edits.Add(SdfNamespaceEdit(from, to));

        if (!layer->CanApply(edits)) {
            error = "CanApply failed";
            return false;
        }

        layer->Apply(edits);
        return true;
    };

    return Command(
        [path, newPathInput, buildNewPath, applyRename](DataModel* dm, SelectionModel* sel) {
            dm->beginProgressBlock("rename prim", 1);

            QtConcurrent::run([=]() {
                QWriteLocker lock(dm->stageLock());
                UsdStageRefPtr stage = dm->stage();

                if (!stage) {
                    QMetaObject::invokeMethod(
                        dm,
                        [=]() {
                            dm->updateProgressNotify({ "Rename failed (no stage)", {} }, 1);
                            dm->endProgressBlock();
                        },
                        Qt::QueuedConnection);
                    return;
                }

                SdfPath newPath = buildNewPath(path, newPathInput);
                if (newPath.IsEmpty() || newPath == path) {
                    QMetaObject::invokeMethod(
                        dm,
                        [=]() {
                            dm->updateProgressNotify({ "Rename noop", {} }, 1);
                            dm->endProgressBlock();
                        },
                        Qt::QueuedConnection);
                    return;
                }
                UsdStageLoadRules rules = stage->GetLoadRules();

                QString error;
                if (!applyRename(stage, path, newPath, error)) {
                    QMetaObject::invokeMethod(
                        dm,
                        [=]() {
                            dm->updateProgressNotify({ "Rename failed (" + error + ")", {} }, 1);
                            dm->endProgressBlock();
                        },
                        Qt::QueuedConnection);
                    return;
                }
                stage->SetLoadRules(remapLoadRules(rules, path, newPath));
                QMetaObject::invokeMethod(
                    dm,
                    [=]() {
                        if (sel) {
                            QList<SdfPath> updated;
                            for (const auto& p : sel->paths()) {
                                if (p.HasPrefix(path)) {
                                    updated.append(newPath.AppendPath(p.MakeRelativePath(path)));
                                }
                                else {
                                    updated.append(p);
                                }
                            }
                            sel->updatePaths(updated);
                        }

                        dm->updateProgressNotify({ "Renamed", { path, newPath } }, 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },

        // =========================
        // UNDO
        // =========================
        [path, newPathInput, buildNewPath, applyRename](DataModel* dm, SelectionModel* sel) {
            dm->beginProgressBlock("undo rename", 1);

            QtConcurrent::run([=]() {
                QWriteLocker lock(dm->stageLock());
                UsdStageRefPtr stage = dm->stage();

                if (!stage) {
                    QMetaObject::invokeMethod(
                        dm,
                        [=]() {
                            dm->updateProgressNotify({ "Undo failed (no stage)", {} }, 1);
                            dm->endProgressBlock();
                        },
                        Qt::QueuedConnection);
                    return;
                }

                SdfPath newPath = buildNewPath(path, newPathInput);
                if (newPath.IsEmpty()) {
                    QMetaObject::invokeMethod(
                        dm,
                        [=]() {
                            dm->updateProgressNotify({ "Undo failed (invalid path)", {} }, 1);
                            dm->endProgressBlock();
                        },
                        Qt::QueuedConnection);
                    return;
                }

                UsdStageLoadRules rules = stage->GetLoadRules();
                QString error;
                if (!applyRename(stage, newPath, path, error)) {
                    QMetaObject::invokeMethod(
                        dm,
                        [=]() {
                            dm->updateProgressNotify({ "Undo failed (" + error + ")", {} }, 1);
                            dm->endProgressBlock();
                        },
                        Qt::QueuedConnection);
                    return;
                }
                stage->SetLoadRules(remapLoadRules(rules, newPath, path));
                
                QMetaObject::invokeMethod(
                    dm,
                    [=]() {
                        if (sel) {
                            QList<SdfPath> updated;
                            for (const auto& p : sel->paths()) {
                                if (p.HasPrefix(newPath)) {
                                    updated.append(path.AppendPath(p.MakeRelativePath(newPath)));
                                }
                                else {
                                    updated.append(p);
                                }
                            }
                            sel->updatePaths(updated);
                        }

                        dm->updateProgressNotify({ "Rename restored", { path } }, 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

}  // namespace usdviewer
