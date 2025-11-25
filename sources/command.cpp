// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "command.h"
#include "commandstack.h"
#include "usdqtutils.h"
#include "usdstageutils.h"
#include <QPointer>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/variantSets.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <QtConcurrent>

namespace usd {
Command
loadPayloads(const QList<SdfPath>& paths, const QString& variantSet, const QString& variantValue)
{
    return Command(
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

                        QMetaObject::invokeMethod(
                            dm,
                            [dm, path, info, completed]() {
                                dm->updateProgressNotify(DataModel::Notify("loading payload", { path }, info),
                                                         completed);
                            },
                            Qt::QueuedConnection);

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
                    dm,
                    [dm]() {
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        },
        [paths](DataModel* dm, SelectionModel*) {
            // todo: undo should be fixed
        });
}

Command
unloadPayloads(const QList<SdfPath>& paths)
{
    return Command(
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
        [paths](DataModel* dm, SelectionModel*) {
            // todo: undo should be fixed
        });
}

Command
isolate(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("redo isolate", 1);
            dm->setMask(paths);
            DataModel::Notify notify("set mask", paths);
            dm->updateProgressNotify(notify, 1);
            dm->endProgressBlock();
        },
        // undo
        [paths](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("undo isolate", 1);
            dm->setMask({});
            DataModel::Notify notify("clear mask", {});
            dm->updateProgressNotify(notify, 1);
            dm->endProgressBlock();
        });
}

Command
select(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](DataModel*, SelectionModel* sel) { sel->updatePaths(paths); },
        // undo
        [](DataModel*, SelectionModel* sel) { sel->clear(); });
}

Command
show(const QList<SdfPath>& paths, bool recursive)
{
    return Command(
        // redo
        [paths, recursive](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("redo show", paths.size());
            size_t completed = 0;

            QWriteLocker locker(dm->stageLock());
            for (const SdfPath& p : paths) {
                if (dm->isProgressBlockCancelled())
                    break;

                setVisibility(dm->stage(), { p }, true, recursive);

                DataModel::Notify notify("show prim", { p });
                dm->updateProgressNotify(notify, ++completed);
            }

            dm->endProgressBlock();
        },
        // undo
        [paths, recursive](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("undo show", paths.size());
            size_t completed = 0;

            QWriteLocker locker(dm->stageLock());
            for (const SdfPath& p : paths) {
                if (dm->isProgressBlockCancelled())
                    break;

                setVisibility(dm->stage(), { p }, false, recursive);

                DataModel::Notify notify("hide prim", { p });
                dm->updateProgressNotify(notify, ++completed);
            }

            dm->endProgressBlock();
        });
}

Command
hide(const QList<SdfPath>& paths, bool recursive)
{
    return Command(
        // redo
        [paths, recursive](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("redo hide", paths.size());
            size_t completed = 0;
            QWriteLocker locker(dm->stageLock());
            for (const SdfPath& p : paths) {
                if (dm->isProgressBlockCancelled())
                    break;

                setVisibility(dm->stage(), { p }, false, recursive);

                DataModel::Notify notify("hide prim", { p });
                dm->updateProgressNotify(notify, ++completed);
            }
            dm->endProgressBlock();
        },

        // undo
        [paths, recursive](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("undo hide", paths.size());
            size_t completed = 0;
            QWriteLocker locker(dm->stageLock());
            for (const SdfPath& p : paths) {
                if (dm->isProgressBlockCancelled())
                    break;
                setVisibility(dm->stage(), { p }, true, recursive);
                DataModel::Notify notify("show prim", { p });
                dm->updateProgressNotify(notify, ++completed);
            }
            dm->endProgressBlock();
        });
}
}  // namespace usd
