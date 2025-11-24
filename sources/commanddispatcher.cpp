// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "commanddispatcher.h"

namespace usd {
CommandDispatcher::Data CommandDispatcher::d = { nullptr };

CommandStack*
CommandDispatcher::commandStack()
{
    return d.stack;
}

void
CommandDispatcher::setCommandStack(CommandStack* stack)
{
    d.stack = stack;
}

void
CommandDispatcher::run(Command* cmd)
{
    d.stack->execute(cmd);
}

void
CommandDispatcher::requestAccess(std::function<void()> fn, bool write)
{
    auto* model = d.stack->stageModel();
    if (write) {
        QWriteLocker locker(&model->p->stageLock);
        fn();
    }
    else {
        QReadLocker locker(&model->p->stageLock);
        fn();
    }
}
}  // namespace usd
