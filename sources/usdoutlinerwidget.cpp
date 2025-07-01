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
#include <QStyledItemDelegate>
#include <pxr/usd/usd/prim.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class OutlinerWidgetPrivate : public QObject {
public:
    void init();
    void initController();
    void initSelection();
    void initStage(const Stage& stage);
    void initTree();
    void collapse();
    void expand();
    void addItem(const UsdPrim& prim, OutlinerItem* parent);
    void addChildren(const UsdPrim& prim, OutlinerItem* parent);
    void toggleVisible(OutlinerItem* item);
    void updateFilter();
    void updateSelection();
    void dataChanged(const QList<SdfPath>& paths);
    void selectionChanged();

    class ItemDelegate : public QStyledItemDelegate {
    public:
        ItemDelegate(QObject* parent = nullptr)
            : QStyledItemDelegate(parent)
        {}
        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QSize size = QStyledItemDelegate::sizeHint(option, index);
            size.setHeight(30);
            return size;
        }
        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);
            QTreeWidgetItem* item = static_cast<const QTreeWidget*>(opt.widget)->itemFromIndex(index);
            std::function<bool(QTreeWidgetItem*)> hasSelectedChildren = [&](QTreeWidgetItem* parentItem) -> bool {
                for (int i = 0; i < parentItem->childCount(); ++i) {
                    QTreeWidgetItem* child = parentItem->child(i);
                    if (child->isSelected() || hasSelectedChildren(child)) {
                        return true;
                    }
                }
                return false;
            };
            if (hasSelectedChildren(item)) {
                opt.font.setBold(true);
                opt.font.setItalic(true);
            }
            QStyledItemDelegate::paint(painter, opt, index);
        }
        bool hasSelectedChildren(QTreeWidgetItem* item) const
        {
            for (int i = 0; i < item->childCount(); ++i) {
                QTreeWidgetItem* child = item->child(i);
                if (child->isSelected() || hasSelectedChildren(child)) {
                    return true;
                }
            }
            return false;
        }
    };

    struct Data {
        Stage stage;
        QString filter;
        QPointer<Controller> controller;
        QPointer<Selection> selection;
        QPointer<OutlinerWidget> widget;
    };
    Data d;
};

void
OutlinerWidgetPrivate::init()
{
    ItemDelegate* delegate = new ItemDelegate(d.widget.data());
    d.widget->setItemDelegate(delegate);
    connect(d.widget.data(), &OutlinerWidget::itemSelectionChanged, this, &OutlinerWidgetPrivate::updateSelection);
}

void
OutlinerWidgetPrivate::initController()
{
    connect(d.controller.data(), &Controller::dataChanged, this, &OutlinerWidgetPrivate::dataChanged);
}

void
OutlinerWidgetPrivate::initSelection()
{
    connect(d.selection.data(), &Selection::selectionChanged, this, &OutlinerWidgetPrivate::selectionChanged);
}

void
OutlinerWidgetPrivate::initStage(const Stage& stage)
{
    d.widget->clear();
    UsdPrim prim = stage.stagePtr()->GetPseudoRoot();
    OutlinerItem* item = new OutlinerItem(d.widget.data(), prim);
    addChildren(prim, item);
    initTree();
}

void
OutlinerWidgetPrivate::initTree()
{
    int topLevelCount = d.widget->topLevelItemCount();
    for (int i = 0; i < topLevelCount; ++i) {
        QTreeWidgetItem* topItem = d.widget->topLevelItem(i);
        d.widget->expandItem(topItem);
        for (int j = 0; j < topItem->childCount(); ++j) {
            d.widget->expandItem(topItem->child(j));
        }
    }
}

void
OutlinerWidgetPrivate::collapse()
{
    std::function<void(QTreeWidgetItem*)> collapseItems = [&](QTreeWidgetItem* item) {
        item->setExpanded(false);
        for (int i = 0; i < item->childCount(); ++i) {
            collapseItems(item->child(i));
        }
    };
    for (int i = 0; i < d.widget->topLevelItemCount(); ++i) {
        collapseItems(d.widget->topLevelItem(i));
    }
    initTree();
}

void
OutlinerWidgetPrivate::expand()
{
    collapse();
    const QList<QTreeWidgetItem*> selected = d.widget->selectedItems();
    for (QTreeWidgetItem* item : selected) {
        item->setExpanded(true);
        QTreeWidgetItem* parent = item->parent();
        while (parent) {
            parent->setExpanded(true);
            parent = parent->parent();
        }
    }
    d.widget->scrollToItem(selected.first(), QAbstractItemView::PositionAtCenter);
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
OutlinerWidgetPrivate::toggleVisible(OutlinerItem* item)
{
    QList<SdfPath> paths;
    QString pathString = item->data(0, Qt::UserRole).toString();
    if (!pathString.isEmpty()) {
        paths.append(SdfPath(pathString.toStdString()));
    }
    d.controller->visiblePaths(paths, !item->isVisible());
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
OutlinerWidgetPrivate::updateSelection()
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
OutlinerWidgetPrivate::dataChanged(const QList<SdfPath>& paths)
{
    d.widget->update();
}

void
OutlinerWidgetPrivate::selectionChanged()
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
    d.widget->update();
}

OutlinerWidget::OutlinerWidget(QWidget* parent)
    : QTreeWidget(parent)
    , p(new OutlinerWidgetPrivate())
{
    p->d.widget = this;
    p->init();
}

OutlinerWidget::~OutlinerWidget() {}

void
OutlinerWidget::collapse()
{
    p->collapse();
}

void
OutlinerWidget::expand()
{
    p->expand();
}

Controller*
OutlinerWidget::controller()
{
    return p->d.controller;
}

void
OutlinerWidget::setController(Controller* controller)
{
    if (p->d.controller != controller) {
        p->d.controller = controller;
        p->initController();
        update();
    }
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
        p->initSelection();
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
OutlinerWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
        for (int i = 0; i < topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = topLevelItem(i);
            item->setSelected(true);
        }
    }
    else if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
        QTreeWidgetItem* top = topLevelItem(0);
        QTreeWidgetItem* end = topLevelItem(topLevelItemCount() - 1);
        if (top && end) {
            setCurrentItem(top);
            scrollToItem(end);
            QItemSelectionModel* sel = selectionModel();
            QModelIndex topLeft = indexFromItem(top);
            QModelIndex bottomRight = indexFromItem(end);
            QItemSelection selection(topLeft, bottomRight);
            sel->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Rows);
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
