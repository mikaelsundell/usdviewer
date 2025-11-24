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
            dm->loadPayloads(paths, variantSet, variantValue);
        },
        // undo
        [paths](DataModel* dm, SelectionModel*) { dm->unloadPayloads(paths); });
}

Command
unloadPayloads(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](DataModel* dm, SelectionModel*) { dm->unloadPayloads(paths); },
        // undo
        [paths](DataModel* dm, SelectionModel*) { dm->loadPayloads(paths); });
}

Command
isolate(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](DataModel* dm, SelectionModel*) { dm->setMask(paths); },
        // undo
        [paths](DataModel* dm, SelectionModel*) { dm->setMask(QList<SdfPath>()); });
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
            QWriteLocker locker(dm->stageLock());
            setVisibility(dm->stage(), paths, true, recursive);
        },
        // undo
        [paths, recursive](DataModel* dm, SelectionModel*) {
            QWriteLocker locker(dm->stageLock());
            setVisibility(dm->stage(), paths, false, recursive);
        });
}

Command
hide(const QList<SdfPath>& paths, bool recursive)
{
    return Command(
        // redo
        [paths, recursive](DataModel* dm, SelectionModel*) {
            QWriteLocker locker(dm->stageLock());
            setVisibility(dm->stage(), paths, false, recursive);
        },
        // undo
        [paths, recursive](DataModel* dm, SelectionModel*) {
            QWriteLocker locker(dm->stageLock());
            setVisibility(dm->stage(), paths, true, recursive);
        });
}
}  // namespace usd
