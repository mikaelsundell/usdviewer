// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdprogressitem.h"
#include <QPointer>

namespace usd {
class ProgressItemPrivate {
public:
    void init();
    struct Data {
        ProgressItem* item;
    };
    Data d;
};

void
ProgressItemPrivate::init()
{}

ProgressItem::ProgressItem(QTreeWidget* parent)
    : QTreeWidgetItem(parent)
    , p(new ProgressItemPrivate())
{}

ProgressItem::ProgressItem(QTreeWidgetItem* parent)
    : QTreeWidgetItem(parent)
    , p(new ProgressItemPrivate())
{}

ProgressItem::~ProgressItem() = default;

}  // namespace usd
