// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdinspectoritem.h"
#include <QPointer>

namespace usd {
class InspectorItemPrivate {
public:
    void init();
    struct Data {
        InspectorItem* item;
    };
    Data d;
};

void
InspectorItemPrivate::init()
{}

InspectorItem::InspectorItem(QTreeWidget* parent)
: QTreeWidgetItem(parent)
, p(new InspectorItemPrivate())
{
}

InspectorItem::InspectorItem(QTreeWidgetItem* parent)
: QTreeWidgetItem(parent)
, p(new InspectorItemPrivate())
{
}

InspectorItem::~InspectorItem() {}

}  // namespace usd
