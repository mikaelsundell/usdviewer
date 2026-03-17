// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QTreeWidgetItem>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {

class PrimItemPrivate;

/**
 * @class PrimItem
 * @brief Tree item representing a USD prim in the stage hierarchy.
 *
 * Used by StageTree to represent a prim within the USD stage.
 * Each item corresponds to a prim path and displays information
 * such as the prim name, type, and visibility state.
 */
class PrimItem : public QTreeWidgetItem {
public:
    enum DataRole { DataPath = Qt::UserRole, DataVisible };

    /**
     * @brief Column indices used by the stage tree.
     */
    enum Column {
        Name = 0,  ///< Prim name column.
        Vis = 1    ///< Visibility state column.
    };

    /**
     * @brief Constructs a root-level prim item.
     *
     * @param parent Parent tree widget.
     * @param stage  USD stage containing the prim.
     * @param path   Path of the prim represented by the item.
     */
    PrimItem(QTreeWidget* parent, const UsdStageRefPtr& stage, const SdfPath& path);

    /**
     * @brief Constructs a child prim item.
     *
     * @param parent Parent tree item.
     * @param stage  USD stage containing the prim.
     * @param path   Path of the prim represented by the item.
     */
    PrimItem(QTreeWidgetItem* parent, const UsdStageRefPtr& stage, const SdfPath& path);

    /**
     * @brief Destroys the PrimItem instance.
     */
    virtual ~PrimItem();

    /**
     * @brief Returns item data for the specified column and role.
     *
     * Provides display and decoration data used by the tree view.
     */
    QVariant data(int column, int role) const override;

    /**
     * @brief Returns whether the represented prim is visible.
     */
    bool isVisible() const;

private:
    QScopedPointer<PrimItemPrivate> p;
};

}  // namespace usd
