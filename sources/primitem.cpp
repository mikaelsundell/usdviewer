// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "primitem.h"
#include "application.h"
#include "commanddispatcher.h"
#include "qtutils.h"
#include "style.h"
#include <QIcon>
#include <QPixmap>
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
        PrimItem* item = nullptr;
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

PrimItem::~PrimItem() = default;

QVariant
PrimItem::data(int column, int role) const
{
    if (!p->d.stage)
        return QVariant();

    const UsdPrim prim = p->d.stage->GetPrimAtPath(p->d.path);
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
        switch (column) {
        case Name: return prim ? StringToQString(prim.GetName().GetString()) : StringToQString(p->d.path.GetName());
        case Type: return prim ? StringToQString(prim.GetTypeName().GetString()) : QString();
        case Vis: return QString();
        default: break;
        }
    }
    if (role == Qt::DecorationRole && column == Name) {
        if (prim) {
            const QString typeName = StringToQString(prim.GetTypeName().GetString());

            if (typeName == "Material" || typeName == "Shader")
                return QIcon(style()->icon(Style::IconMaterial, Style::UIMedium));
            else if (typeName == "Mesh")
                return QIcon(style()->icon(Style::IconMesh, Style::UIMedium));
            else
                return QIcon(style()->icon(Style::IconPrim, Style::UIMedium));
        }
    }
    if (role == Qt::DecorationRole && column == Vis) {
        if (!prim || !prim.IsActive())
            return QVariant();
        if (prim == p->d.stage->GetPseudoRoot())
            return QVariant();
        UsdGeomImageable imageable(prim);
        if (!imageable)
            return QVariant();
        return style()->icon(isVisible() ? Style::IconVisible : Style::IconHidden,
                             Style::UISmall);
    }
    if (role == PrimItem::DataPath) {
        return StringToQString(p->d.path.GetString());
    }
    if (role == PrimItem::DataVisible) {
        return isVisible();
    }
    return QTreeWidgetItem::data(column, role);
}

bool
PrimItem::isVisible() const
{
    if (!p->d.stage)
        return false;

    const UsdPrim prim = p->d.stage->GetPrimAtPath(p->d.path);
    if (!prim || !prim.IsActive())
        return false;

    const UsdGeomImageable imageable(prim);
    if (!imageable)
        return true;

    QReadLocker locker(CommandDispatcher::stageLock());

    const TfToken visibility = imageable.ComputeVisibility();
    return visibility != UsdGeomTokens->invisible;
}

}  // namespace usd
