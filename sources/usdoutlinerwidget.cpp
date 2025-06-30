// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdoutlinerwidget.h"
#include "usdoutlineritem.h"
#include "usdselection.h"
#include "usdutils.h"
#include <QHeaderView>
#include <QKeyEvent>
#include <QPointer>
#include <pxr/usd/usd/prim.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class OutlinerWidgetPrivate : public QObject {
public:
    void init();
    void initStage(const Stage& stage);
    void addItem(const UsdPrim& prim, OutlinerItem* parent);
    void addChildren(const UsdPrim& prim, OutlinerItem* parent);
    void toggleVisible(OutlinerItem* item);
    void updateFilter();
    void selectionChanged();
    void updateSelection();
    struct Data {
        Stage stage;
        QString filter;
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
    d.widget->expandItem(item);
    for (int i = 0; i < item->childCount(); ++i) {
        d.widget->expandItem(item->child(i));
    }
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
    UsdPrimSiblingRange children = prim.GetAllChildren();  // todo: add filters for children
    for (const UsdPrim& child : children) {
        addItem(child, parent);
    }
}

void
OutlinerWidgetPrivate::toggleVisible(OutlinerItem* parent)
{
    parent->setVisible(!parent->isVisible());
    d.selection->selectionChanged();
}

void
OutlinerWidgetPrivate::updateFilter()
{
    std::function<bool(QTreeWidgetItem*)> matchfilter = [&](QTreeWidgetItem* item) -> bool {
        bool matches = false;
        for (int col = 0; col < d.widget->columnCount(); ++col) {
            if (item->text(col).contains(d.filter, Qt::CaseInsensitive)) {
                matches = true;
                break;
            }
        }
        bool childMatches = false;
        for (int i = 0; i < item->childCount(); ++i) {
            QTreeWidgetItem* child = item->child(i);
            if (matchfilter(child)) {
                childMatches = true;
            }
        }

        bool visible = matches || childMatches;
        item->setHidden(!visible);
        return visible;
    };
    for (int i = 0; i < d.widget->topLevelItemCount(); ++i) {
        matchfilter(d.widget->topLevelItem(i));
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
                if (!item->isSelected()) {  // todo: needed, handled in setSelected()?
                    item->setSelected(true);
                }
            }
            else {
                if (item->isSelected()) {  // todo: needed, handled in setSelected()?
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

OutlinerWidget::OutlinerWidget(QWidget* parent)
    : QTreeWidget(parent)
    , p(new OutlinerWidgetPrivate())
{
    p->d.widget = this;
    p->init();
}

OutlinerWidget::~OutlinerWidget() {}

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

QString
OutlinerWidget::filter() const
{
    return p->d.filter;
}

void
OutlinerWidget::setFilter(const QString& filter)
{
    if (filter != p->d.filter) {
        p->d.filter = filter;
        p->updateFilter();
    }
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
    }
    else {
        QTreeWidget::keyPressEvent(event);
    }
}

void
OutlinerWidget::mousePressEvent(QMouseEvent* event)
{
    QTreeWidgetItem* item = itemAt(event->pos());
    int column = columnAt(event->pos().x());
    if (!item) {
        clearSelection();
        itemSelectionChanged();
        return;
    }
    if (column == OutlinerItem::Visible) {
        p->toggleVisible(static_cast<OutlinerItem*>(item));  // no qobject inheritance
        event->accept();
        return;
    }
    QTreeWidget::mousePressEvent(event);
}

void
OutlinerWidget::mouseMoveEvent(QMouseEvent* event)
{
    event->accept();  // avoid drag selection
}
}  // namespace usd
