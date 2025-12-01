// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdpropertyitem.h"
#include <QPointer>

namespace usd {
class PropertyItemPrivate {
public:
    void init();
    struct Data {
        PropertyItem* item;
    };
    Data d;
};

void
PropertyItemPrivate::init()
{}

PropertyItem::PropertyItem(QTreeWidget* parent)
    : QTreeWidgetItem(parent)
    , p(new PropertyItemPrivate())
{}

PropertyItem::PropertyItem(QTreeWidgetItem* parent)
    : QTreeWidgetItem(parent)
    , p(new PropertyItemPrivate())
{}

PropertyItem::~PropertyItem() = default;
}  // namespace usd
