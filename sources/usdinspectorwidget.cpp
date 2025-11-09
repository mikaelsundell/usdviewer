// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdinspectorwidget.h"
#include "usdinspectoritem.h"
#include "usdselectionmodel.h"
#include <QFileInfo>
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
class InspectorWidgetPrivate : public QObject {
public:
    void init();
    void initStageModel();
    void initSelection();

public Q_SLOTS:
    void selectionChanged();
    void stageChanged();

public:
    struct Data {
        QPointer<StageModel> stageModel;
        QPointer<SelectionModel> selectionModel;
        QPointer<InspectorWidget> widget;
    };
    Data d;
};

void
InspectorWidgetPrivate::init()
{}

void
InspectorWidgetPrivate::initStageModel()
{
    connect(d.stageModel.data(), &StageModel::stageChanged, this, &InspectorWidgetPrivate::stageChanged);
}

void
InspectorWidgetPrivate::initSelection()
{
    connect(d.selectionModel.data(), &SelectionModel::selectionChanged, this, &InspectorWidgetPrivate::selectionChanged);
}

void
InspectorWidgetPrivate::stageChanged()
{
    d.widget->clear();
    if (d.stageModel->isLoaded()) {
        UsdStageRefPtr stage = d.stageModel->stage();
        InspectorItem* stageItem = new InspectorItem(d.widget.data());
        stageItem->setText(InspectorItem::Key, "Stage");
        d.widget->addTopLevelItem(stageItem);
        stageItem->setExpanded(true);

        auto addChild = [&](const QString& key, const QString& value) {
            InspectorItem* item = new InspectorItem(stageItem);
            item->setText(InspectorItem::Key, key);
            item->setText(InspectorItem::Value, value);
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
}

static std::string
GfMatrixToString(const GfMatrix4d& m)
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

void
InspectorWidgetPrivate::selectionChanged()
{
    QList<SdfPath> selectedPaths = d.selectionModel->paths();
    d.widget->clear();

    if (!d.stageModel->isLoaded()) {
        return;
    }

    if (selectedPaths.isEmpty()) {
        return;
    }

    if (selectedPaths.size() > 1) {
        InspectorItem* multiItem = new InspectorItem(d.widget.data());
        multiItem->setText(InspectorItem::Key, "[Multiple selection]");
        d.widget->addTopLevelItem(multiItem);
        multiItem->setExpanded(true);
        return;
    }

    SdfPath path = selectedPaths.first();
    UsdStageRefPtr stage = d.stageModel->stage();
    UsdPrim prim = stage->GetPrimAtPath(path);
    if (!prim)
        return;

    InspectorItem* primItem = new InspectorItem(d.widget.data());
    primItem->setText(InspectorItem::Key, QString::fromStdString(path.GetString()));
    primItem->setExpanded(true);
    d.widget->addTopLevelItem(primItem);

    auto addChild = [&](const QString& key, const QString& value) {
        InspectorItem* item = new InspectorItem(primItem);
        item->setText(InspectorItem::Key, key);
        item->setText(InspectorItem::Value, value);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    };

    addChild("Type", QString::fromStdString(prim.GetTypeName().GetString()));
    addChild("Active", prim.IsActive() ? "true" : "false");
    addChild("Visibility",
             QString::fromStdString(UsdGeomImageable(prim).ComputeVisibility(UsdTimeCode::Default()).GetString()));

    /*addChild("Kind", QString::fromStdString(UsdModelAPI(prim).GetKind()));

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

    d.widget->expandAll();
}

InspectorWidget::InspectorWidget(QWidget* parent)
    : QTreeWidget(parent)
    , p(new InspectorWidgetPrivate())
{
    p->d.widget = this;
    p->init();
}

InspectorWidget::~InspectorWidget() {}

StageModel*
InspectorWidget::stageModel() const
{
    return p->d.stageModel;
}

void
InspectorWidget::setStageModel(StageModel* stageModel)
{
    if (p->d.stageModel != stageModel) {
        p->d.stageModel = stageModel;
        p->initStageModel();
        update();
    }
}

SelectionModel*
InspectorWidget::selectionModel()
{
    return p->d.selectionModel;
}

void
InspectorWidget::setSelectionModel(SelectionModel* selectionModel)
{
    if (p->d.selectionModel != selectionModel) {
        p->d.selectionModel = selectionModel;
        p->initSelection();
        update();
    }
}
}  // namespace usd
