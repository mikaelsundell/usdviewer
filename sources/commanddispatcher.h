// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "commandstack.h"

namespace usd {
class CommandDispatcher {
public:
    static CommandStack* commandStack();
    static void setCommandStack(CommandStack* commandStack);
    static void run(Command* command);
    template<typename T, typename... Args> static void run(Args&&... args);
    static void requestAccess(std::function<void()> fn, bool write = false);

private:
    struct Data {
        CommandStack* stack;
    };
    static Data d;
};
};  // namespace usd
