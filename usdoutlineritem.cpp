// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdoutlineritem.h"
#include "usdselection.h"
#include <pxr/usd/usd/prim.h>
#include <QPointer>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class OutlinerItemPrivate {
public:
    void init();
    struct Data {
        UsdPrim prim;
        OutlinerItem* item;
    };
    Data d;
};

void
OutlinerItemPrivate::init()
{
}

OutlinerItem::OutlinerItem(QTreeWidget* parent, const UsdPrim& prim)
: QTreeWidgetItem(parent)
, p(new OutlinerItemPrivate())
{
    p->d.item = this;
    p->d.prim = prim;
    p->init();
}

OutlinerItem::OutlinerItem(QTreeWidgetItem* parent, const UsdPrim& prim)
: QTreeWidgetItem(parent)
, p(new OutlinerItemPrivate())
{
    p->d.item = this;
    p->d.prim = prim;
    p->init();
}

OutlinerItem::~OutlinerItem()
{
}

QVariant
OutlinerItem::data(int column, int role) const
{
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
        switch (column) {
            case OutlinerItem::Name:
                return QString::fromStdString(p->d.prim.GetName().GetString());
            case OutlinerItem::Type:
                return QString::fromStdString(p->d.prim.GetTypeName().GetString());
            case OutlinerItem::Visible:
                return p->d.prim.IsActive() ? "Visible" : "Hidden";
            default:
                break;
        }
    }
    else if (role == Qt::UserRole) {
        return QString::fromStdString(p->d.prim.GetPath().GetText());
    }
    return QTreeWidgetItem::data(column, role);
}
}
