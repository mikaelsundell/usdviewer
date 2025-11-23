// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "command.h"
#include "commandstack.h"
#include <QPointer>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>

namespace usd {
namespace {
    void visibility(UsdStageRefPtr stage, const QList<SdfPath>& paths, bool visible, bool recursive = false)
    {
        QList<SdfPath> affected;
        for (const SdfPath& path : paths) {
            UsdPrim prim = stage->GetPrimAtPath(path);
            if (!prim)
                continue;
            UsdGeomImageable imageable(prim);
            if (imageable) {
                if (visible)
                    imageable.MakeVisible();
                else
                    imageable.MakeInvisible();
                affected.append(path);
            }
            if (recursive) {
                for (const UsdPrim& child : prim.GetAllDescendants()) {
                    UsdGeomImageable childImageable(child);
                    if (!childImageable)
                        continue;
                    TfToken currentVis;
                    childImageable.GetVisibilityAttr().Get(&currentVis);
                    TfToken desiredVis = visible ? UsdGeomTokens->inherited : UsdGeomTokens->invisible;
                    if (currentVis != desiredVis) {
                        if (visible)
                            childImageable.MakeVisible();
                        else
                            childImageable.MakeInvisible();
                        affected.append(child.GetPath());
                    }
                }
            }
        }
    }
}  // namespace

Command
loadPayload(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](StageModel* sm, SelectionModel*) { sm->loadPayloads(paths); },
        // undo
        [paths](StageModel* sm, SelectionModel*) { sm->unloadPayloads(paths); });
}

Command
unloadPayload(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](StageModel* sm, SelectionModel*) { sm->unloadPayloads(paths); },
        // undo
        [paths](StageModel* sm, SelectionModel*) { sm->loadPayloads(paths); });
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
        [paths, recursive](StageModel* sm, SelectionModel*) { visibility(sm->stage(), paths, true, recursive); },
        // undo
        [paths, recursive](StageModel* sm, SelectionModel*) { visibility(sm->stage(), paths, false, recursive); });
}

Command
hide(const QList<SdfPath>& paths, bool recursive)
{
    return Command(
        // redo
        [paths, recursive](StageModel* sm, SelectionModel*) { visibility(sm->stage(), paths, false, recursive); },
        // undo
        [paths, recursive](StageModel* sm, SelectionModel*) { visibility(sm->stage(), paths, true, recursive); });
}
}  // namespace usd
