// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdinspectorwidget.h"
#include "usdinspectoritem.h"
#include "usdselection.h"
#include <QFileInfo>
#include <QPointer>
#include <pxr/usd/usdGeom/metrics.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class InspectorWidgetPrivate : public QObject {
public:
    void init();
    void initStage(const Stage& stage);
    void updateSelection();
    struct Data {
        Stage stage;
        QPointer<Selection> selection;
        QPointer<InspectorWidget> widget;
    };
    Data d;
};

void
InspectorWidgetPrivate::init()
{}

void
InspectorWidgetPrivate::initStage(const Stage& stage)
{
    d.widget->clear();

    UsdStageRefPtr stageptr = stage.stagePtr();
    double metersPerUnit = UsdGeomGetStageMetersPerUnit(stageptr);
    InspectorItem* metersItem = new InspectorItem(d.widget.data());
    metersItem->setText(InspectorItem::Key, "metersPerUnit");
    metersItem->setText(InspectorItem::Value, QString::number(metersPerUnit));
    d.widget->addTopLevelItem(metersItem);

    TfToken upAxis = UsdGeomGetStageUpAxis(stageptr);
    InspectorItem* upAxisItem = new InspectorItem(d.widget.data());
    upAxisItem->setText(InspectorItem::Key, "upAxis");
    upAxisItem->setText(InspectorItem::Value, QString::fromStdString(upAxis.GetString()));
    d.widget->addTopLevelItem(upAxisItem);

    double tcps = stageptr->GetTimeCodesPerSecond();
    InspectorItem* tcpsItem = new InspectorItem(d.widget.data());
    tcpsItem->setText(InspectorItem::Key, "timeCodesPerSecond");
    tcpsItem->setText(InspectorItem::Value, QString::number(tcps));
    d.widget->addTopLevelItem(tcpsItem);

    double startTime = stageptr->GetStartTimeCode();
    InspectorItem* startTimeItem = new InspectorItem(d.widget.data());
    startTimeItem->setText(InspectorItem::Key, "startTimeCode");
    startTimeItem->setText(InspectorItem::Value, QString::number(startTime));
    d.widget->addTopLevelItem(startTimeItem);

    double endTime = stageptr->GetEndTimeCode();
    InspectorItem* endTimeItem = new InspectorItem(d.widget.data());
    endTimeItem->setText(InspectorItem::Key, "endTimeCode");
    endTimeItem->setText(InspectorItem::Value, QString::number(endTime));
    d.widget->addTopLevelItem(endTimeItem);

    std::string comment = stageptr->GetRootLayer()->GetComment();
    if (!comment.empty()) {
        InspectorItem* commentItem = new InspectorItem(d.widget.data());
        commentItem->setText(InspectorItem::Key, "comment");
        commentItem->setText(InspectorItem::Value, QString::fromStdString(comment));
        d.widget->addTopLevelItem(commentItem);
    }

    std::string filePath = stageptr->GetRootLayer()->GetRealPath();
    InspectorItem* filePathItem = new InspectorItem(d.widget.data());
    filePathItem->setText(InspectorItem::Key, "filePath");
    filePathItem->setText(InspectorItem::Value, QFileInfo(QString::fromStdString(filePath)).fileName());
    d.widget->addTopLevelItem(filePathItem);

    for (int i = 0; i < d.widget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = d.widget->topLevelItem(i);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    }
}

void
InspectorWidgetPrivate::updateSelection()
{}

InspectorWidget::InspectorWidget(QWidget* parent)
    : QTreeWidget(parent)
    , p(new InspectorWidgetPrivate())
{
    p->d.widget = this;
    p->init();
}

InspectorWidget::~InspectorWidget() {}

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
