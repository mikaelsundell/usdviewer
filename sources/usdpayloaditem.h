// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QTreeWidgetItem>

namespace usd {
class PayloadItemPrivate;
class PayloadItem : public QTreeWidgetItem {
public:
    enum Column { Name = 0, Value };
    PayloadItem(QTreeWidget* parent);
    PayloadItem(QTreeWidgetItem* parent);
    virtual ~PayloadItem();

private:
    QScopedPointer<PayloadItemPrivate> p;
};
}  // namespace usd
