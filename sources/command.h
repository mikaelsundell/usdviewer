// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "datamodel.h"
#include "selectionmodel.h"
#include <pxr/usd/sdf/path.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {

/**
 * @class Command
 * @brief Encapsulates an executable action with optional undo support.
 *
 * Commands are used by the CommandStack to perform operations that modify
 * the scene or application state. Each command defines a redo operation
 * and optionally an undo operation.
 *
 * Commands operate on a DataModel and SelectionModel, allowing them to
 * interact with the USD stage and the current prim selection.
 */
class Command {
public:
    /**
     * @brief Function signature used by commands.
     *
     * The function receives the active DataModel and SelectionModel.
     */
    using Func = std::function<void(DataModel*, SelectionModel*)>;

    /**
     * @brief Constructs a command with redo and optional undo functions.
     *
     * @param redo Function executed when the command runs.
     * @param undo Function executed when the command is undone.
     */
    Command(Func redo, Func undo = Func())
        : m_redo(std::move(redo))
        , m_undo(std::move(undo))
    {}

    /**
     * @brief Executes the command.
     *
     * @param s   Data model used by the command.
     * @param sel Selection model used by the command.
     */
    void execute(DataModel* s, SelectionModel* sel)
    {
        if (m_redo)
            m_redo(s, sel);
    }

    /**
     * @brief Undoes the command.
     *
     * @param s   Data model used by the command.
     * @param sel Selection model used by the command.
     */
    void undo(DataModel* s, SelectionModel* sel)
    {
        if (m_undo)
            m_undo(s, sel);
    }

    /**
     * @brief Returns whether the command supports undo.
     */
    bool isUndoable() const { return static_cast<bool>(m_undo); }

private:
    Func m_redo;  ///< Function executed when the command runs.
    Func m_undo;  ///< Function executed when the command is undone.
};

/** @name Command Factory Helpers */
///@{

/**
 * @brief Creates a command that loads payloads for the specified prim paths.
 *
 * Optionally applies a variant selection after loading the payload.
 *
 * @param paths        Prim paths containing payloads.
 * @param variantSet   Optional variant set name.
 * @param variantValue Optional variant selection value.
 */
Command
loadPayloads(const QList<SdfPath>& paths, const QString& variantSet = QString(),
             const QString& variantValue = QString());

/**
 * @brief Creates a command that unloads payloads for the specified prim paths.
 *
 * @param paths Prim paths containing payloads.
 */
Command
unloadPayloads(const QList<SdfPath>& paths);

/**
 * @brief Creates a command that isolates the specified prim paths.
 *
 * Typically hides all other prims in the scene.
 *
 * @param paths Prim paths to isolate.
 */
Command
isolate(const QList<SdfPath>& paths);

/**
 * @brief Creates a command that updates the current selection.
 *
 * @param paths Prim paths to select.
 */
Command
select(const QList<SdfPath>& paths);

/**
 * @brief Creates a command that makes prims visible.
 *
 * @param paths     Prim paths to show.
 * @param recursive If true, apply visibility to descendant prims.
 */
Command
show(const QList<SdfPath>& paths, bool recursive);

/**
 * @brief Creates a command that hides prims.
 *
 * @param paths     Prim paths to hide.
 * @param recursive If true, apply visibility to descendant prims.
 */
Command
hide(const QList<SdfPath>& paths, bool recursive);

///@}

}  // namespace usd
