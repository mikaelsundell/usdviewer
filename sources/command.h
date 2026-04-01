// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "session.h"
#include <pxr/usd/sdf/path.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdviewer {

/**
 * @class Command
 * @brief Encapsulates an executable action with optional undo support.
 *
 * Commands are used by the CommandStack to perform operations that modify
 * the scene or application state. Each command defines a redo operation
 * and optionally an undo operation.
 *
 * Commands operate on a Session and SelectionModel, allowing them to
 * interact with the USD stage and the current prim selection.
 */
class Command {
public:
    /**
     * @brief Function signature used by commands.
     *
     * The function receives the active Session and SelectionModel.
     */
    using Func = std::function<void(Session*)>;

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
     * @param s   Session used by the command.
     */
    void execute(Session* s)
    {
        if (m_redo)
            m_redo(s);
    }

    /**
     * @brief Undoes the command.
     *
     * @param s   Session used by the command.
     */
    void undo(Session* s)
    {
        if (m_undo)
            m_undo(s);
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
isolatePaths(const QList<SdfPath>& paths);

/**
 * @brief Creates a command that updates the current selection.
 *
 * @param paths Prim paths to select.
 */
Command
selectPaths(const QList<SdfPath>& paths);

/**
 * @brief Creates a command that makes prims visible.
 *
 * @param paths     Prim paths to show.
 * @param recursive If true, apply visibility to descendant prims.
 */
Command
showPaths(const QList<SdfPath>& paths, bool recursive);

/**
 * @brief Creates a command that sets the stage up axis.
 *
 * @param stageUp Up axis to apply to the current stage.
 */
Command
stageUp(Session::StageUp stageUp);

/**
 * @brief Creates a command that hides prims.
 *
 * @param paths     Prim paths to hide.
 * @param recursive If true, apply visibility to descendant prims.
 */
Command
hidePaths(const QList<SdfPath>& paths, bool recursive);

/**
 * @brief Creates a command that delete the current selection.
 *
 * @param paths Prim paths to delete.
 */
Command
deletePaths(const QList<SdfPath>& paths);

Command
renamePath(const SdfPath& path, const QString& newNameInput);

Command
newXformPath(const SdfPath& parentPath, const QString& nameInput);

Command
movePath(const SdfPath& fromPath, const SdfPath& newParentPath);

///@}

}  // namespace usdviewer
