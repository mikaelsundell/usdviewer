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
        [paths, variantSet, variantValue](StageModel* sm, SelectionModel*) {
            sm->loadPayloads(paths, variantSet, variantValue);
        },
        // undo
        [paths](StageModel* sm, SelectionModel*) { sm->unloadPayloads(paths); });
}

Command
unloadPayloads(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](StageModel* sm, SelectionModel*) { sm->unloadPayloads(paths); },
        // undo
        [paths](StageModel* sm, SelectionModel*) { sm->loadPayloads(paths); });
}

Command
isolate(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](StageModel* sm, SelectionModel*) { sm->setMask(paths); },
        // undo
        [paths](StageModel* sm, SelectionModel*) { sm->setMask(QList<SdfPath>()); });
}

Command
select(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](StageModel*, SelectionModel* sel) { sel->updatePaths(paths); },
        // undo
        [](StageModel*, SelectionModel* sel) { sel->clear(); });
}

Command
show(const QList<SdfPath>& paths, bool recursive)
{
    return Command(
        // redo
        [paths, recursive](StageModel* sm, SelectionModel*) {
            QWriteLocker locker(sm->stageLock());
            setVisibility(sm->stage(), paths, true, recursive);
        },
        // undo
        [paths, recursive](StageModel* sm, SelectionModel*) {
            QWriteLocker locker(sm->stageLock());
            setVisibility(sm->stage(), paths, false, recursive);
        });
}

Command
hide(const QList<SdfPath>& paths, bool recursive)
{
    return Command(
        // redo
        [paths, recursive](StageModel* sm, SelectionModel*) {
            QWriteLocker locker(sm->stageLock());
            setVisibility(sm->stage(), paths, false, recursive);
        },
        // undo
        [paths, recursive](StageModel* sm, SelectionModel*) {
            QWriteLocker locker(sm->stageLock());
            setVisibility(sm->stage(), paths, true, recursive);
        });
}
}  // namespace usd
