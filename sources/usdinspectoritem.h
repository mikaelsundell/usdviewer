// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QTreeWidgetItem>

namespace usd {
class InspectorItemPrivate;
class InspectorItem : public QTreeWidgetItem {
public:
    enum Column { Key = 0, Value };
    InspectorItem(QTreeWidget* parent);
    InspectorItem(QTreeWidgetItem* parent);
    virtual ~InspectorItem();

private:
    QScopedPointer<InspectorItemPrivate> p;
};
}  // namespace usd
