// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QTreeWidgetItem>

namespace usd {
class PropertyItemPrivate;
class PropertyItem : public QTreeWidgetItem {
public:
    enum Column { Name = 0, Value };
    PropertyItem(QTreeWidget* parent);
    PropertyItem(QTreeWidgetItem* parent);
    virtual ~PropertyItem();

private:
    QScopedPointer<PropertyItemPrivate> p;
};
}  // namespace usd
