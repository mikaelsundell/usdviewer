// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdinspectorwidget.h"
#include "usdinspectoritem.h"
#include "usdselection.h"
#include <QFileInfo>
#include <QPointer>
#include <pxr/usd/usdGeom/metrics.h>


// Core USD
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/modelAPI.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

// Geometry (for transforms, bounds, visibility)
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>

// Math / Types
#include <pxr/base/gf/bbox3d.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/range3d.h>
#include <pxr/base/gf/vec3d.h>

// Utilities
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>


PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class InspectorWidgetPrivate : public QObject {
public:
    void init();
    void initController();
    void initStage(const Stage& stage);
    void initSelection();

public Q_SLOTS:
    void dataChanged(const QList<SdfPath>& paths);
    void selectionChanged();

public:
    struct Data {
        Stage stage;
        QPointer<Controller> controller;
        QPointer<Selection> selection;
        QPointer<InspectorWidget> widget;
    };
    Data d;
};

void
InspectorWidgetPrivate::init()
{}

void
InspectorWidgetPrivate::initController()
{
    connect(d.controller.data(), &Controller::dataChanged, this, &InspectorWidgetPrivate::dataChanged);
}

void
InspectorWidgetPrivate::initSelection()
{
    connect(d.selection.data(), &Selection::selectionChanged, this, &InspectorWidgetPrivate::selectionChanged);
}

void
InspectorWidgetPrivate::initStage(const Stage& stage)
{
    d.widget->clear();
    if (stage.isValid()) {
        d.stage = stage;

        UsdStageRefPtr stageptr = stage.stagePtr();
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

        addChild("metersPerUnit", QString::number(UsdGeomGetStageMetersPerUnit(stageptr)));
        addChild("upAxis", QString::fromStdString(UsdGeomGetStageUpAxis(stageptr).GetString()));
        addChild("timeCodesPerSecond", QString::number(stageptr->GetTimeCodesPerSecond()));
        addChild("startTimeCode", QString::number(stageptr->GetStartTimeCode()));
        addChild("endTimeCode", QString::number(stageptr->GetEndTimeCode()));

        std::string comment = stageptr->GetRootLayer()->GetComment();
        if (!comment.empty())
            addChild("comment", QString::fromStdString(comment));

        std::string filePath = stageptr->GetRootLayer()->GetRealPath();
        addChild("filePath", QFileInfo(QString::fromStdString(filePath)).fileName());
    }
}

void
InspectorWidgetPrivate::dataChanged(const QList<SdfPath>& paths)
{
    d.widget->update();
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
    QList<SdfPath> selectedPaths = d.selection->paths();
    d.widget->clear();

    if (!d.stage.isValid()) {
        return;
    }

    if (selectedPaths.isEmpty()) {
        initStage(d.stage);
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
    UsdStageRefPtr stageptr = d.stage.stagePtr();
    UsdPrim prim = stageptr->GetPrimAtPath(path);
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

Controller*
InspectorWidget::controller()
{
    return p->d.controller;
}

void
InspectorWidget::setController(Controller* controller)
{
    if (p->d.controller != controller) {
        p->d.controller = controller;
        p->initController();
        update();
    }
}

Selection*
InspectorWidget::selection()
{
    return p->d.selection;
}

void
InspectorWidget::setSelection(Selection* selection)
{
    if (p->d.selection != selection) {
        p->d.selection = selection;
        p->initSelection();
        update();
    }
}

Stage
InspectorWidget::stage() const
{
    Q_ASSERT("stage is not set" && p->d.stage.isValid());
    return p->d.stage;
}

bool
InspectorWidget::setStage(const Stage& stage)
{
    p->initStage(stage);
    update();
    return true;
}
}  // namespace usd
