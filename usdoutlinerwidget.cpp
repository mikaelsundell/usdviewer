// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdoutlinerwidget.h"
#include "usdoutlineritem.h"
#include "usdutils.h"
#include "usdselection.h"
#include <pxr/usd/usd/prim.h>
#include <QHeaderView>
#include <QKeyEvent>
#include <QPointer>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class OutlinerWidgetPrivate : public QObject {
public:
    void init();
    void initStage(const Stage& stage);
    void addItem(const UsdPrim& prim, OutlinerItem* parent);
    void addChildren(const UsdPrim& prim, OutlinerItem* parent);
    void selectionChanged();
    void updateSelection();
    struct Data {
        Stage stage;
        QPointer<Selection> selection;
        QPointer<OutlinerWidget> widget;
    };
    Data d;
};

void
OutlinerWidgetPrivate::init()
{
    connect(d.widget.data(), &OutlinerWidget::itemSelectionChanged, this, &OutlinerWidgetPrivate::selectionChanged);
}

void
OutlinerWidgetPrivate::initStage(const Stage& stage)
{
    d.widget->clear();
    UsdPrim prim = stage.stagePtr()->GetPseudoRoot();
    OutlinerItem* item = new OutlinerItem(d.widget.data(), prim);
    addChildren(prim, item);
}

void
OutlinerWidgetPrivate::addItem(const UsdPrim& prim, OutlinerItem* parent)
{
    OutlinerItem* item = new OutlinerItem(parent, prim);
    parent->addChild(item);
    addChildren(prim, item);
}

void
OutlinerWidgetPrivate::addChildren(const UsdPrim& prim, OutlinerItem* parent)
{
    UsdPrimSiblingRange children = prim.GetAllChildren(); // todo: add filters for children
    for (const UsdPrim& child : children) {
        addItem(child, parent);
    }
}

void
OutlinerWidgetPrivate::selectionChanged()
{
    QList<SdfPath> paths;
    QList<QTreeWidgetItem*> selectedpaths = d.widget->selectedItems();
    for (QTreeWidgetItem* item : selectedpaths) {
        QString pathString = item->data(0, Qt::UserRole).toString();
        if (!pathString.isEmpty()) {
            paths.append(SdfPath(pathString.toStdString()));
        }
    }
    d.selection->replacePaths(paths);
}

void
OutlinerWidgetPrivate::updateSelection()
{
    QList<SdfPath> selectedPaths = d.selection->paths();
    std::function<void(QTreeWidgetItem*)> selectItems = [&](QTreeWidgetItem* item) {
        QString pathString = item->data(0, Qt::UserRole).toString();
        if (!pathString.isEmpty()) {
            SdfPath path(pathString.toStdString());
            if (selectedPaths.contains(path)) {
                if (!item->isSelected()) { // todo: needed, handled in setSelected()?
                    item->setSelected(true);
                }
            } else {
                if (item->isSelected()) { // todo: needed, handled in setSelected()?
                    item->setSelected(false);
                }
            }
        }
        for (int i = 0; i < item->childCount(); ++i) {
            selectItems(item->child(i));
        }
    };
    for (int i = 0; i < d.widget->topLevelItemCount(); ++i) {
        selectItems(d.widget->topLevelItem(i));
    }
}

//#include "usdoutlinerwidget.moc"

OutlinerWidget::OutlinerWidget(QWidget* parent)
: QTreeWidget(parent)
, p(new OutlinerWidgetPrivate())
{
    p->d.widget = this;
    p->init();
}

OutlinerWidget::~OutlinerWidget()
{
}

Selection*
OutlinerWidget::selection()
{
    return p->d.selection;
}

void
OutlinerWidget::setSelection(Selection* selection)
{
    if (p->d.selection != selection) {
        p->d.selection = selection;
        update();
    }
}

Stage
OutlinerWidget::stage() const
{
    Q_ASSERT("stage is not set" && p->d.stage.isValid());
    return p->d.stage;
}

bool
OutlinerWidget::setStage(const Stage& stage)
{
    p->initStage(stage);
    update();
    return true;
}

void
OutlinerWidget::updateSelection()
{
    p->updateSelection();
}

void
OutlinerWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
        for (int i = 0; i < topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = topLevelItem(i);
            item->setSelected(true);
        }
    } else {
        QTreeWidget::keyPressEvent(event);
    }
}

void
OutlinerWidget::mousePressEvent(QMouseEvent *event)
{
    QTreeWidget::mousePressEvent(event);
    if (itemAt(event->pos()) == nullptr) {
        clearSelection();
        itemSelectionChanged();
    }
}
}
