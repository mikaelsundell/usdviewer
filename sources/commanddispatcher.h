// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "commandstack.h"
#include <QReadWriteLock>

namespace usd {
class CommandDispatcher {
public:
    static CommandStack* commandStack();
    static void setCommandStack(CommandStack* commandStack);
    static void run(Command* command);
    template<typename T, typename... Args> static void run(Args&&... args);
    static QReadWriteLock* stageLock();

private:
    struct Data {
        CommandStack* stack;
    };
    static Data d;
};
};  // namespace usd
