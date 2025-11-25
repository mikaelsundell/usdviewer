// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "command.h"
#include "commandstack.h"
#include "usdstageutils.h"
#include <QPointer>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>

namespace usd {
Command
loadPayloads(const QList<SdfPath>& paths, const QString& variantSet, const QString& variantValue)
{
    return Command(
        // redo
        [paths, variantSet, variantValue](DataModel* dm, SelectionModel*) {
            dm->beginChangeBlock(paths.size());
            dm->loadPayloads(paths, variantSet, variantValue);
            dm->endChangeBlock();
        },
        // undo
        [paths](DataModel* dm, SelectionModel*) {
            dm->beginChangeBlock(paths.size());
            dm->unloadPayloads(paths);
            dm->endChangeBlock();
        });
}

Command
unloadPayloads(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](DataModel* dm, SelectionModel*) {
            dm->beginChangeBlock(paths.size());
            dm->unloadPayloads(paths);
            dm->endChangeBlock();
        },
        // undo
        [paths](DataModel* dm, SelectionModel*) {
            dm->beginChangeBlock(paths.size());
            dm->loadPayloads(paths);
            dm->endChangeBlock();
        });
}

Command
isolate(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](DataModel* dm, SelectionModel*) {
            dm->beginChangeBlock(1);
            dm->setMask(paths);
            dm->endChangeBlock();
        },
        // undo
        [paths](DataModel* dm, SelectionModel*) {
            dm->beginChangeBlock(1);
            dm->setMask({});
            dm->endChangeBlock();
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
            dm->beginChangeBlock(paths.size());
            size_t completed = 0;
            {
                QWriteLocker locker(dm->stageLock());
                for (const SdfPath& p : paths) {
                    setVisibility(dm->stage(), { p }, true, recursive);
                    dm->progressChangeBlock(++completed);
                }
            }
            dm->endChangeBlock();
        },

        // undo
        [paths, recursive](DataModel* dm, SelectionModel*) {
            dm->beginChangeBlock(paths.size());
            size_t completed = 0;
            {
                QWriteLocker locker(dm->stageLock());
                for (const SdfPath& p : paths) {
                    setVisibility(dm->stage(), { p }, false, recursive);
                    dm->progressChangeBlock(++completed);
                }
            }
            dm->endChangeBlock();
        });
}

Command
hide(const QList<SdfPath>& paths, bool recursive)
{
    return Command(
        // redo
        [paths, recursive](DataModel* dm, SelectionModel*) {
            dm->beginChangeBlock(paths.size());
            size_t completed = 0;
            {
                QWriteLocker locker(dm->stageLock());
                for (const SdfPath& p : paths) {
                    setVisibility(dm->stage(), { p }, false, recursive);
                    dm->progressChangeBlock(++completed);
                }
            }
            dm->endChangeBlock();
        },
        // undo
        [paths, recursive](DataModel* dm, SelectionModel*) {
            dm->beginChangeBlock(paths.size());
            size_t completed = 0;
            {
                QWriteLocker locker(dm->stageLock());
                for (const SdfPath& p : paths) {
                    setVisibility(dm->stage(), { p }, true, recursive);
                    dm->progressChangeBlock(++completed);
                }
            }
            dm->endChangeBlock();
        });
}

}  // namespace usd
