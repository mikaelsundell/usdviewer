// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "command.h"
#include "datamodel.h"
#include "selectionmodel.h"

namespace usd {

class CommandStackPrivate;

/**
 * @class CommandStack
 * @brief Manages command execution with undo and redo support.
 *
 * Implements a command stack used to execute operations that modify
 * the scene or application state. Commands are stored in a history
 * allowing them to be undone or redone.
 *
 * The stack integrates with DataModel and SelectionModel so commands
 * can interact with the currently loaded USD stage and selection.
 */
class CommandStack : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs a command stack.
     *
     * @param parent Optional parent object.
     */
    CommandStack(QObject* parent = nullptr);

    /**
     * @brief Destroys the CommandStack instance.
     */
    virtual ~CommandStack();

    /** @name Command Execution */
    ///@{

    /**
     * @brief Executes a command and adds it to the stack.
     *
     * @param command Command to execute.
     */
    void execute(Command* command);

    /**
     * @brief Returns whether undo is available.
     */
    bool canUndo() const;

    /**
     * @brief Returns whether redo is available.
     */
    bool canRedo() const;

    ///@}

    /** @name Data Models */
    ///@{

    /**
     * @brief Returns the associated data model.
     */
    DataModel* dataModel() const;

    /**
     * @brief Sets the data model used by commands.
     *
     * @param dataModel Scene data model.
     */
    void setDataModel(DataModel* dataModel);

    /**
     * @brief Returns the associated selection model.
     */
    SelectionModel* selectionModel();

    /**
     * @brief Sets the selection model used by commands.
     *
     * @param selectionModel Selection model.
     */
    void setSelectionModel(SelectionModel* selectionModel);

    ///@}

public Q_SLOTS:

    /** @name History Navigation */
    ///@{

    /**
     * @brief Undoes the most recently executed command.
     */
    void undo();

    /**
     * @brief Redoes the most recently undone command.
     */
    void redo();

    ///@}

Q_SIGNALS:

    /**
     * @brief Emitted when a command has been executed.
     *
     * @param command Executed command.
     */
    void commandExecuted(Command* command);

    /**
     * @brief Emitted when the command stack state changes.
     */
    void changed();

private:
    QScopedPointer<CommandStackPrivate> p;
};

}  // namespace usd
