// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "command.h"
#include "commandstack.h"
#include "usdqtutils.h"
#include "usdstageutils.h"
#include <QPointer>
#include <QtConcurrent>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/variantSets.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>

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
        [paths](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("redo isolate", 1);

            QFuture<void> future = QtConcurrent::run([dm, paths]() {
                {
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
        [paths](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("undo isolate", 1);
            QFuture<void> future = QtConcurrent::run([dm]() {
                {
                    dm->setMask({});
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm]() {
                        dm->updateProgressNotify(DataModel::Notify("Isolate cleared", {}), 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
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
        [paths, recursive](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("redo show", 1);
            QFuture<void> future = QtConcurrent::run([dm, paths, recursive]() {
                {
                    QWriteLocker lock(dm->stageLock());
                    setVisibility(dm->stage(), paths, true, recursive);
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
        [paths, recursive](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("undo show", 1);
            QFuture<void> future = QtConcurrent::run([dm, paths, recursive]() {
                {
                    QWriteLocker lock(dm->stageLock());
                    setVisibility(dm->stage(), paths, false, recursive);
                }
                QMetaObject::invokeMethod(
                    dm,
                    [dm, paths]() {
                        dm->updateProgressNotify(DataModel::Notify("Hidden", paths), 1);
                        dm->endProgressBlock();
                    },
                    Qt::QueuedConnection);
            });
        });
}

Command
hide(const QList<SdfPath>& paths, bool recursive)
{
    return Command(
        [paths, recursive](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("redo hide", 1);
            QFuture<void> future = QtConcurrent::run([dm, paths, recursive]() {
                {
                    QWriteLocker lock(dm->stageLock());
                    setVisibility(dm->stage(), paths, false, recursive);
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
        [paths, recursive](DataModel* dm, SelectionModel*) {
            dm->beginProgressBlock("undo hide", 1);
            QFuture<void> future = QtConcurrent::run([dm, paths, recursive]() {
                {
                    QWriteLocker lock(dm->stageLock());
                    setVisibility(dm->stage(), paths, true, recursive);
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
}  // namespace usd
