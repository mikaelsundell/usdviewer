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
    if (!d.stack) {
        delete cmd;
        return;
    }
    d.stack->execute(cmd);
}
}  // namespace usd
