// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdpayloaditem.h"
#include "usdpayloadwidget.h"
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
class PayloadWidgetPrivate : public QObject {
public:
    void init();
    void initStageModel();
    void initSelection();
    bool eventFilter(QObject* obj, QEvent* event);

public Q_SLOTS:
    void selectionChanged(const QList<SdfPath>& paths);
    void stageChanged();

public:
    struct Data {
        QPointer<StageModel> stageModel;
        QPointer<SelectionModel> selectionModel;
        QPointer<PayloadWidget> widget;
    };
    Data d;
};

void
PayloadWidgetPrivate::init()
{
    d.widget->installEventFilter(this);
}

void
PayloadWidgetPrivate::initStageModel()
{
    connect(d.stageModel.data(), &StageModel::stageChanged, this, &PayloadWidgetPrivate::stageChanged);
}

void
PayloadWidgetPrivate::initSelection()
{
    connect(d.selectionModel.data(), &SelectionModel::selectionChanged, this, &PayloadWidgetPrivate::selectionChanged);
}

bool
PayloadWidgetPrivate::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Show) {
        static bool init = false;
        if (!init) {
            init = true;
            d.widget->setColumnWidth(PayloadItem::Name, 200);
            d.widget->setColumnWidth(PayloadItem::Value, 80);
            d.widget->header()->setSectionResizeMode(PayloadItem::Value, QHeaderView::Stretch);
        }
    }
    return QObject::eventFilter(obj, event);
}

void
PayloadWidgetPrivate::stageChanged()
{
    d.widget->clear();
}

void
PayloadWidgetPrivate::selectionChanged(const QList<SdfPath>& paths)
{
}

PayloadWidget::PayloadWidget(QWidget* parent)
    : QTreeWidget(parent)
    , p(new PayloadWidgetPrivate())
{
    p->d.widget = this;
    p->init();
}

PayloadWidget::~PayloadWidget() {}

StageModel*
PayloadWidget::stageModel() const
{
    return p->d.stageModel;
}

void
PayloadWidget::setStageModel(StageModel* stageModel)
{
    if (p->d.stageModel != stageModel) {
        p->d.stageModel = stageModel;
        p->initStageModel();
        update();
    }
}

SelectionModel*
PayloadWidget::selectionModel()
{
    return p->d.selectionModel;
}

void
PayloadWidget::setSelectionModel(SelectionModel* selectionModel)
{
    if (p->d.selectionModel != selectionModel) {
        p->d.selectionModel = selectionModel;
        p->initSelection();
        update();
    }
}
}  // namespace usd
