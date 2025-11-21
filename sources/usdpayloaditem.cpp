// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdpayloaditem.h"
#include <QPointer>

namespace usd {
class PayloadItemPrivate {
public:
    void init();
    struct Data {
        PayloadItem* item;
    };
    Data d;
};

void
PayloadItemPrivate::init()
{}

PayloadItem::PayloadItem(QTreeWidget* parent)
    : QTreeWidgetItem(parent)
    , p(new PayloadItemPrivate())
{}

PayloadItem::PayloadItem(QTreeWidgetItem* parent)
    : QTreeWidgetItem(parent)
    , p(new PayloadItemPrivate())
{}

PayloadItem::~PayloadItem() {}

}  // namespace usd
