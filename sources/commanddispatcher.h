// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "commandstack.h"
#include <QReadWriteLock>

namespace usd {

/**
 * @class CommandDispatcher
 * @brief Global dispatcher for executing commands.
 *
 * Provides a centralized interface for submitting commands to the
 * active CommandStack. This allows application components to execute
 * commands without needing direct access to the command stack instance.
 *
 * The dispatcher also exposes the stage lock used for thread-safe
 * access to the underlying USD stage.
 */
class CommandDispatcher {
public:
    /** @name Command Stack Access */
    ///@{

    /**
     * @brief Returns the active command stack.
     */
    static CommandStack* commandStack();

    /**
     * @brief Sets the command stack used by the dispatcher.
     *
     * @param commandStack Command stack instance.
     */
    static void setCommandStack(CommandStack* commandStack);

    ///@}

    /** @name Command Execution */
    ///@{

    /**
     * @brief Executes a command through the command stack.
     *
     * @param command Command instance to execute.
     */
    static void run(Command* command);

    /**
     * @brief Constructs and executes a command.
     *
     * This helper creates a command of type @p T and forwards the
     * provided arguments to its constructor.
     */
    template<typename T, typename... Args> static void run(Args&&... args);

    ///@}

    /** @name Stage Synchronization */
    ///@{

    /**
     * @brief Returns the stage read/write lock.
     *
     * Used to synchronize access to the USD stage across threads.
     */
    static QReadWriteLock* stageLock();

    ///@}

private:
    /**
     * @brief Internal dispatcher data.
     */
    struct Data {
        CommandStack* stack;  ///< Active command stack.
    };

    static Data d;
};

}  // namespace usd
