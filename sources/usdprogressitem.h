// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QTreeWidgetItem>

namespace usd {
class ProgressItemPrivate;
class ProgressItem : public QTreeWidgetItem {
public:
    enum Column { Name = 0, Value };
    ProgressItem(QTreeWidget* parent);
    ProgressItem(QTreeWidgetItem* parent);
    virtual ~ProgressItem();

private:
    QScopedPointer<ProgressItemPrivate> p;
};
}  // namespace usd
