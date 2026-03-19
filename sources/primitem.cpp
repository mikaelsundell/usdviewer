// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "primitem.h"
#include "application.h"
#include "commanddispatcher.h"
#include "qtutils.h"
#include "stageutils.h"
#include "style.h"
#include <QIcon>
#include <QPixmap>
#include <QPointer>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdviewer {

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
{
    d.item->setFlags(d.item->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    d.item->setCheckState(0, Qt::Unchecked);
}

PrimItem::PrimItem(QTreeWidget* parent, const UsdStageRefPtr& stage, const SdfPath& path)
    : TreeItem(parent)
    , p(new PrimItemPrivate())
{
    p->d.item = this;
    p->d.stage = stage;
    p->d.path = path;
    p->init();
}

PrimItem::PrimItem(QTreeWidgetItem* parent, const UsdStageRefPtr& stage, const SdfPath& path)
    : TreeItem(parent)
    , p(new PrimItemPrivate())
{
    p->d.item = this;
    p->d.stage = stage;
    p->d.path = path;
    p->init();
}

PrimItem::~PrimItem() = default;

QVariant
PrimItem::data(int column, int role) const
{
    if (!p->d.stage)
        return QVariant();

    const SdfPath path = p->d.path;
    const UsdPrim prim = p->d.stage->GetPrimAtPath(path);
    if (role == Qt::DisplayRole) {
        switch (column) {
        case Name: return prim ? StringToQString(prim.GetName().GetString()) : StringToQString(path.GetName());
        case Vis: return QString();
        default: break;
        }
    }

    if (role == Qt::ToolTipRole) {
        if (!prim)
            return QVariant();

        return QString("%1 (%2)")
            .arg(StringToQString(path.GetString()))
            .arg(StringToQString(prim.GetTypeName().GetString()));
    }

    if (role == Qt::DecorationRole && column == Name) {
        if (!prim)
            return QVariant();

        const QString typeName = StringToQString(prim.GetTypeName().GetString());

        if (typeName == "Material" || typeName == "Shader")
            return QIcon(style()->icon(Style::IconRole::Material, Style::UIScale::Medium));
        else if (typeName == "Mesh")
            return QIcon(style()->icon(Style::IconRole::Mesh, Style::UIScale::Medium));
        else
            return QIcon(style()->icon(Style::IconRole::Prim, Style::UIScale::Medium));
    }

    if (role == Qt::DecorationRole && column == Vis) {
        if (!prim || !prim.IsActive())
            return QVariant();

        if (prim == p->d.stage->GetPseudoRoot())
            return QVariant();

        const UsdGeomImageable imageable(prim);
        if (!imageable)
            return QVariant();

        const bool visible = stage::isVisible(p->d.stage, path);
        return style()->icon(visible ? Style::IconRole::Visible : Style::IconRole::Hidden, Style::UIScale::Medium);
    }

    if (role == PrimItem::PrimPath) {
        return StringToQString(path.GetString());
    }

    if (role == TreeItem::ItemActive) {
        return stage::isVisible(p->d.stage, path);
    }

    return TreeItem::data(column, role);
}

}  // namespace usdviewer
