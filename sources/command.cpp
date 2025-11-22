// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "command.h"
#include "commandstack.h"
#include <QPointer>

namespace usd {

Command
loadPayload(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](StageModel* stageModel, SelectionModel*) { stageModel->loadPayloads(paths); },
        // undo
        [paths](StageModel* stageModel, SelectionModel*) { stageModel->unloadPayloads(paths); });
}

Command
unloadPayload(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](StageModel* stageModel, SelectionModel*) { stageModel->unloadPayloads(paths); },
        // undo
        [paths](StageModel* stageModel, SelectionModel*) { stageModel->loadPayloads(paths); });
}
Command
setSelection(const QList<SdfPath>& paths)
{
    return Command(
        // redo
        [paths](StageModel*, SelectionModel* sel) { sel->updatePaths(paths); },
        // undo
        [](StageModel*, SelectionModel* sel) { sel->clear(); });
}

Command
setVisible(const QList<SdfPath>& paths, bool visible)
{
    return Command(
        // redo
        [paths, visible](StageModel* stageModel, SelectionModel*) {
            stageModel->setVisible(paths, visible);
        },
        // undo
        [paths, visible](StageModel* stageModel, SelectionModel*) {
            stageModel->setVisible(paths, !visible);
        }
    );
}
}  // namespace usd
