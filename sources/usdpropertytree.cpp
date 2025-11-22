// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdpropertytree.h"
#include "usdpropertyitem.h"
#include "usdselectionmodel.h"
#include <QFileInfo>
#include <QHeaderView>
#include <QKeyEvent>
#include <QPointer>
#include <pxr/base/gf/bbox3d.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/range3d.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/modelAPI.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class PropertyTreePrivate : public QObject {
public:
    void init();
    bool eventFilter(QObject* obj, QEvent* event);
    void close();
    void updateStage(UsdStageRefPtr stage);
    void updatePrims(const QList<SdfPath>& paths);
    void updateSelection(const QList<SdfPath>& paths);

public:
    std::string matrixString(const GfMatrix4d& matrix);
    struct Data {
        UsdStageRefPtr stage;
        SdfPath path;
        QPointer<PropertyTree> tree;
    };
    Data d;
};

void
PropertyTreePrivate::init()
{
    d.tree->installEventFilter(this);
}

bool
PropertyTreePrivate::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Show) {
        static bool init = false;
        if (!init) {
            init = true;
            d.tree->setColumnWidth(PropertyItem::Name, 200);
            d.tree->setColumnWidth(PropertyItem::Value, 80);
            d.tree->header()->setSectionResizeMode(PropertyItem::Value, QHeaderView::Stretch);
        }
    }
    return QObject::eventFilter(obj, event);
}

void
PropertyTreePrivate::close()
{
    d.stage = nullptr;
    d.path = SdfPath();
    d.tree->clear();
}

void
PropertyTreePrivate::updateStage(UsdStageRefPtr stage)
{
    close();
    d.stage = stage;
    PropertyItem* stageItem = new PropertyItem(d.tree.data());
    stageItem->setText(PropertyItem::Name, "Stage");
    d.tree->addTopLevelItem(stageItem);
    stageItem->setExpanded(true);
    auto addChild = [&](const QString& name, const QString& value) {
        PropertyItem* item = new PropertyItem(stageItem);
        item->setText(PropertyItem::Name, name);
        item->setText(PropertyItem::Value, value);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    };
    addChild("metersPerUnit", QString::number(UsdGeomGetStageMetersPerUnit(stage)));
    addChild("upAxis", QString::fromStdString(UsdGeomGetStageUpAxis(stage).GetString()));
    addChild("timeCodesPerSecond", QString::number(stage->GetTimeCodesPerSecond()));
    addChild("startTimeCode", QString::number(stage->GetStartTimeCode()));
    addChild("endTimeCode", QString::number(stage->GetEndTimeCode()));
    std::string comment = stage->GetRootLayer()->GetComment();
    if (!comment.empty())
        addChild("comment", QString::fromStdString(comment));
    std::string filePath = stage->GetRootLayer()->GetRealPath();
    addChild("filePath", QFileInfo(QString::fromStdString(filePath)).fileName());
}

void
PropertyTreePrivate::updatePrims(const QList<SdfPath>& paths)
{
    if (paths.contains(d.path)) {
        updateSelection({ d.path });
    }
}

void
PropertyTreePrivate::updateSelection(const QList<SdfPath>& paths)
{
    d.tree->clear();
    if (paths.size() > 1) {
        PropertyItem* multiItem = new PropertyItem(d.tree.data());
        multiItem->setText(PropertyItem::Name, "[Multiple selection]");
        d.tree->addTopLevelItem(multiItem);
        multiItem->setExpanded(true);
        return;
    }
    SdfPath path = paths.first();
    UsdPrim prim = d.stage->GetPrimAtPath(path);
    if (!prim)
        return;

    PropertyItem* primItem = new PropertyItem(d.tree.data());
    primItem->setText(PropertyItem::Name, QString::fromStdString(path.GetString()));
    primItem->setExpanded(true);
    d.tree->addTopLevelItem(primItem);

    auto addChild = [&](const QString& name, const QString& value) {
        PropertyItem* item = new PropertyItem(primItem);
        item->setText(PropertyItem::Name, name);
        item->setText(PropertyItem::Value, value);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    };


    /*
    addChild("Type", QString::fromStdString(prim.GetTypeName().GetString()));
    addChild("Active", prim.IsActive() ? "true" : "false");
    addChild("Visibility",
             QString::fromStdString(UsdGeomImageable(prim).ComputeVisibility(UsdTimeCode::Default()).GetString()));

    addChild("Kind", QString::fromStdString(UsdModelAPI(prim).GetKind()));

    if (prim.IsA<UsdGeomXformable>()) {
        GfMatrix4d worldXf;
        UsdGeomXformable(prim).GetLocalTransformation(&worldXf);
        addChild("LocalToWorldXform", QString::fromStdString(GfMatrixToString(worldXf)));
    }

    if (UsdGeomImageable imageable = UsdGeomImageable(prim)) {
        GfBBox3d bbox = imageable.ComputeWorldBound(UsdTimeCode::Default());
        GfRange3d range = bbox.ComputeAlignedRange();
        QString bboxString = QString("[%1, %2]").arg(range.GetMin().GetString().c_str(), range.GetMax().GetString().c_str());
        addChild("WorldBoundingBox", bboxString);
    }
    */

    for (const UsdAttribute& attr : prim.GetAttributes()) {
        std::string name = attr.GetName().GetString();
        VtValue value;
        if (attr.Get(&value)) {
            addChild(QString::fromStdString(name), QString::fromStdString(value.GetTypeName()));
        }
    }
    d.tree->expandAll();
    d.path = path;
}

std::string
PropertyTreePrivate::matrixString(const GfMatrix4d& m)
{
    std::ostringstream ss;
    ss << "(";
    for (int r = 0; r < 4; ++r) {
        ss << "(";
        for (int c = 0; c < 4; ++c) {
            ss << m[r][c];
            if (c < 3)
                ss << ", ";
        }
        ss << ")";
        if (r < 3)
            ss << ", ";
    }
    ss << ")";
    return ss.str();
}

PropertyTree::PropertyTree(QWidget* parent)
    : QTreeWidget(parent)
    , p(new PropertyTreePrivate())
{
    p->d.tree = this;
    p->init();
}

PropertyTree::~PropertyTree() {}

void
PropertyTree::close()
{
    p->close();
}

void
PropertyTree::updateStage(UsdStageRefPtr stage)
{
    p->updateStage(stage);
}

void
PropertyTree::updatePrims(const QList<SdfPath>& paths)
{
    p->updatePrims(paths);
}

void
PropertyTree::updateSelection(const QList<SdfPath>& paths)
{
    p->updateSelection(paths);
}
}  // namespace usd
