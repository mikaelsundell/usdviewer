// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "treeitem.h"

namespace usd {

class TreeItemPrivate {
public:
    struct Data {};
    Data d;
};

TreeItem::TreeItem(QTreeWidget* parent)
    : QTreeWidgetItem(parent)
    , p(new TreeItemPrivate())
{}

TreeItem::TreeItem(QTreeWidgetItem* parent)
    : QTreeWidgetItem(parent)
    , p(new TreeItemPrivate())
{}

TreeItem::~TreeItem() {}

QVariant
TreeItem::data(int column, int role) const
{
    if (role == ItemActive) {
        return true;
    }
    return QTreeWidgetItem::data(column, role);
}

}  // namespace usd
