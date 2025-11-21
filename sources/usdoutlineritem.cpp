// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdoutlineritem.h"
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <QPointer>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class OutlinerItemPrivate {
public:
    void init();
    struct Data {
        UsdStageRefPtr stage;
        SdfPath path;
        OutlinerItem* item;
    };
    Data d;
};

void
OutlinerItemPrivate::init()
{}

OutlinerItem::OutlinerItem(QTreeWidget* parent, const UsdStageRefPtr& stage, const SdfPath& path)
    : QTreeWidgetItem(parent)
    , p(new OutlinerItemPrivate())
{
    p->d.item = this;
    p->d.stage = stage;
    p->d.path = path;
    setFlags(flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    setCheckState(0, Qt::Unchecked);
    p->init();
}

OutlinerItem::OutlinerItem(QTreeWidgetItem* parent, const UsdStageRefPtr& stage, const SdfPath& path)
    : QTreeWidgetItem(parent)
    , p(new OutlinerItemPrivate())
{
    p->d.item = this;
    p->d.stage = stage;
    p->d.path = path;
    setFlags(flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    setCheckState(0, Qt::Unchecked);
    p->init();
}
OutlinerItem::~OutlinerItem() {}

QVariant
OutlinerItem::data(int column, int role) const
{
    if (!p->d.stage)
        return QVariant();

    UsdPrim prim = p->d.stage->GetPrimAtPath(p->d.path);
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
        switch (column) {
        case Name:
            if (prim)
                return QString::fromStdString(prim.GetName().GetString());
            else
                return QString::fromStdString(p->d.path.GetName());
        case Type:
            if (prim)
                return QString::fromStdString(prim.GetTypeName().GetString());
            else
                return QString();
        case Visible: {
            if (prim && prim.IsActive()) {
                UsdGeomImageable imageable(prim);
                if (imageable) {
                    TfToken vis;
                    imageable.GetVisibilityAttr().Get(&vis);
                    return (vis == UsdGeomTokens->invisible) ? "H" : "V";
                    return "V";
                }
            }
        }
        default: break;
        }
    }
    else if (role == Qt::UserRole) {
        return QString::fromStdString(p->d.path.GetString());
    }
    return QTreeWidgetItem::data(column, role);
}

bool
OutlinerItem::isVisible() const
{
    if (!p->d.stage)
        return false;

    UsdPrim prim = p->d.stage->GetPrimAtPath(p->d.path);
    if (!prim || !prim.IsActive())
        return false;

    UsdGeomImageable imageable(prim);
    if (!imageable)
        return false;

    TfToken vis;
    imageable.GetVisibilityAttr().Get(&vis);
    return vis != UsdGeomTokens->invisible;
}
}  // namespace usd
