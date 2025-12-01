// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdpropertytree.h"
#include "selectionmodel.h"
#include "signalguard.h"
#include "usdpropertyitem.h"
#include "usdqtutils.h"
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
class PropertyTreePrivate : public QObject, public SignalGuard {
public:
    void init();
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
{}

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
    beginGuard();
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
        addChild("upAxis", StringToQString(UsdGeomGetStageUpAxis(stage).GetString()));
        addChild("timeCodesPerSecond", QString::number(stage->GetTimeCodesPerSecond()));
        addChild("startTimeCode", QString::number(stage->GetStartTimeCode()));
        addChild("endTimeCode", QString::number(stage->GetEndTimeCode()));
        std::string comment = stage->GetRootLayer()->GetComment();
        if (!comment.empty())
            addChild("comment", StringToQString(comment));
        std::string filePath = stage->GetRootLayer()->GetRealPath();
        addChild("filePath", QFileInfo(StringToQString(filePath)).fileName());
    }
    endGuard();
}

void
PropertyTreePrivate::updatePrims(const QList<SdfPath>& paths)
{
    beginGuard();
    {
        if (paths.contains(d.path)) {
            updateSelection({ d.path });
        }
    }
    endGuard();
}

void
PropertyTreePrivate::updateSelection(const QList<SdfPath>& paths)
{
    beginGuard();
    {
        d.tree->clear();
        if (paths.size()) {
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
            primItem->setText(PropertyItem::Name, StringToQString(path.GetString()));
            primItem->setExpanded(true);
            d.tree->addTopLevelItem(primItem);
            
            auto addChild = [&](const QString& name, const QString& value) {
                PropertyItem* item = new PropertyItem(primItem);
                item->setText(PropertyItem::Name, name);
                item->setText(PropertyItem::Value, value);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            };
            
            
            /*
             addChild("Type", StringToQString(prim.GetTypeName().GetString()));
             addChild("Active", prim.IsActive() ? "true" : "false");
             addChild("Visibility",
             StringToQString(UsdGeomImageable(prim).ComputeVisibility(UsdTimeCode::Default()).GetString()));
             
             addChild("Kind", StringToQString(UsdModelAPI(prim).GetKind()));
             
             if (prim.IsA<UsdGeomXformable>()) {
             GfMatrix4d worldXf;
             UsdGeomXformable(prim).GetLocalTransformation(&worldXf);
             addChild("LocalToWorldXform", StringToQString(GfMatrixToString(worldXf)));
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
                    addChild(StringToQString(name), StringToQString(value.GetTypeName()));
                }
            }
            d.tree->expandAll();
            d.path = path;
        }
        else {
            updateStage(d.stage);
        }
    }
    endGuard();
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

PropertyTree::~PropertyTree() = default;

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
