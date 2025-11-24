// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdprimitem.h"
#include "commanddispatcher.h"
#include "usdqtutils.h"
#include <QPointer>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class PrimItemPrivate {
public:
    void init();
    struct Data {
        UsdStageRefPtr stage;
        SdfPath path;
        PrimItem* item;
    };
    Data d;
};

void
PrimItemPrivate::init()
{}

PrimItem::PrimItem(QTreeWidget* parent, const UsdStageRefPtr& stage, const SdfPath& path)
    : QTreeWidgetItem(parent)
    , p(new PrimItemPrivate())
{
    p->d.item = this;
    p->d.stage = stage;
    p->d.path = path;
    setFlags(flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    setCheckState(0, Qt::Unchecked);
    p->init();
}

PrimItem::PrimItem(QTreeWidgetItem* parent, const UsdStageRefPtr& stage, const SdfPath& path)
    : QTreeWidgetItem(parent)
    , p(new PrimItemPrivate())
{
    p->d.item = this;
    p->d.stage = stage;
    p->d.path = path;
    setFlags(flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    setCheckState(0, Qt::Unchecked);
    p->init();
}
PrimItem::~PrimItem() {}

QVariant
PrimItem::data(int column, int role) const
{
    if (!p->d.stage)
        return QVariant();

    UsdPrim prim = p->d.stage->GetPrimAtPath(p->d.path);
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
        switch (column) {
        case Name:
            if (prim)
                return StringToQString(prim.GetName().GetString());
            else
                return StringToQString(p->d.path.GetName());
        case Type:
            if (prim)
                return StringToQString(prim.GetTypeName().GetString());
            else
                return QString();
        case Visible: {
            if (prim && prim.IsActive()) {
                {
                    QReadLocker locker(CommandDispatcher::stageLock());
                    UsdGeomImageable imageable(prim);
                    if (imageable) {
                        TfToken vis;
                        imageable.GetVisibilityAttr().Get(&vis);
                        return (vis == UsdGeomTokens->invisible) ? "H" : "V";
                        return "V";
                    }
                }
            }
        }
        default: break;
        }
    }
    else if (role == Qt::UserRole) {
        return StringToQString(p->d.path.GetString());
    }
    return QTreeWidgetItem::data(column, role);
}

bool
PrimItem::isVisible() const
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
