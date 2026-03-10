// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "datamodel.h"
#include "selectionmodel.h"
#include <QTreeWidget>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {

class StageTreePrivate;

/**
 * @class StageTree
 * @brief Tree view for browsing USD stage hierarchies.
 *
 * Displays the prim hierarchy of a USD stage and allows the user
 * to navigate, filter, and select prims. The widget integrates
 * with the viewer selection model and emits signals when the
 * prim selection changes.
 */
class StageTree : public QTreeWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs the stage tree widget.
     *
     * @param parent Optional parent widget.
     */
    StageTree(QWidget* parent = nullptr);

    /**
     * @brief Destroys the StageTree instance.
     */
    virtual ~StageTree();

    /** @name Tree Control */
    ///@{

    /**
     * @brief Clears and closes the current stage view.
     */
    void close();

    /**
     * @brief Collapses all tree nodes.
     */
    void collapse();

    /**
     * @brief Expands all tree nodes.
     */
    void expand();

    ///@}

    /** @name Filtering */
    ///@{

    /**
     * @brief Returns the active prim name filter.
     */
    QString filter() const;

    /**
     * @brief Sets a filter used to restrict visible prims.
     *
     * @param filter Filter string applied to prim names.
     */
    void setFilter(const QString& filter);

    ///@}

    /** @name Payload Control */
    ///@{

    /**
     * @brief Returns whether payload loading is enabled.
     */
    bool payloadEnabled() const;

    /**
     * @brief Enables or disables payload loading.
     *
     * @param enabled Payload loading state.
     */
    void setPayloadEnabled(bool enabled);

    ///@}

    /** @name Stage Updates */
    ///@{

    /**
     * @brief Updates the tree from a USD stage.
     *
     * Rebuilds the tree hierarchy to reflect the contents
     * of the provided stage.
     *
     * @param stage USD stage to display.
     */
    void updateStage(UsdStageRefPtr stage);

    /**
     * @brief Updates tree items for the specified prim paths.
     *
     * Typically used when prim properties change and the
     * corresponding items must be refreshed.
     *
     * @param paths Prim paths to update.
     */
    void updatePrims(const QList<SdfPath>& paths);

    /**
     * @brief Updates the tree selection.
     *
     * Synchronizes the widget selection with the provided
     * list of prim paths.
     *
     * @param paths Selected prim paths.
     */
    void updateSelection(const QList<SdfPath>& paths);

    ///@}

Q_SIGNALS:

    /**
     * @brief Emitted when the prim selection changes.
     *
     * @param paths Selected prim paths.
     */
    void primSelectionChanged(const QList<SdfPath>& paths);

protected:
    /** @name Event Handling */
    ///@{

    /**
     * @brief Handles context menu requests.
     */
    void contextMenuEvent(QContextMenuEvent* event) override;

    /**
     * @brief Handles key press events.
     */
    void keyPressEvent(QKeyEvent* event) override;

    /**
     * @brief Handles mouse press events.
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * @brief Handles mouse move events.
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    ///@}

private:
    QScopedPointer<StageTreePrivate> p;
};

}  // namespace usd
