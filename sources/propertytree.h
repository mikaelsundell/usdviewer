// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "treewidget.h"
#include <QTreeWidget>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdviewer {

class PropertyTreePrivate;

/**
 * @class PropertyTree
 * @brief Tree view displaying properties of selected USD prims.
 *
 * Provides a hierarchical view of prim properties such as attributes,
 * relationships, and metadata. The tree updates when the stage changes
 * or when the selection of prims is modified.
 *
 * Typically used alongside the StageTree and RenderView to inspect
 * detailed data for the currently selected prims.
 */
class PropertyTree : public TreeWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs the property tree widget.
     *
     * @param parent Optional parent widget.
     */
    PropertyTree(QWidget* parent = nullptr);

    /**
     * @brief Destroys the PropertyTree instance.
     */
    virtual ~PropertyTree();

    /** @name Tree Control */
    ///@{

    /**
     * @brief Clears the current property view.
     */
    void close();

    ///@}

    /** @name Stage Updates */
    ///@{

    /**
     * @brief Updates the property tree for the given USD stage.
     *
     * @param stage USD stage to inspect.
     */
    void updateStage(UsdStageRefPtr stage);

    /**
     * @brief Updates properties for the specified prim paths.
     *
     * @param paths Prim paths to refresh.
     */
    void updatePrims(const QList<SdfPath>& paths);

    /**
     * @brief Updates the tree to reflect the current selection.
     *
     * @param paths Selected prim paths.
     */
    void updateSelection(const QList<SdfPath>& paths);

    ///@}

private:
    QScopedPointer<PropertyTreePrivate> p;
};

}  // namespace usdviewer
