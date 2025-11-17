// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdoutlinerwidget.h"
#include "usdoutlineritem.h"
#include "usdselectionmodel.h"
#include "usdutils.h"

#include <QApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QTimer>
#include <pxr/usd/usd/prim.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class OutlinerWidgetPrivate : public QObject {
public:
    OutlinerWidgetPrivate();
    void init();
    void initController();
    void initStageModel();
    void initSelection();
    void initTree();
    void collapse();
    void expand();
    void addItem(OutlinerItem* parent, const SdfPath& parentPath);
    void addChildren(OutlinerItem* parent, const SdfPath& parentPath);
    void toggleVisible(OutlinerItem* item);
    void updateFilter();
    void itemCheckState(QTreeWidgetItem* item, bool checkable, bool recursive = false);
    void treeCheckState(QTreeWidgetItem* item);

public Q_SLOTS:
    void updateSelection();
    void checkStateChanged(OutlinerItem* item);
    void selectionChanged(const QList<SdfPath>& paths);
    void primsChanged(const QList<SdfPath>& paths);
    void stageChanged();

public:
    class ItemDelegate : public QStyledItemDelegate {
    public:
        ItemDelegate(QObject* parent = nullptr)
            : QStyledItemDelegate(parent)
        {}
        bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                         const QModelIndex& index) override
        {
            if (!(index.flags() & Qt::ItemIsUserCheckable))
                return QStyledItemDelegate::editorEvent(event, model, option, index);

            if (event->type() != QEvent::MouseButtonRelease && event->type() != QEvent::MouseButtonDblClick
                && event->type() != QEvent::KeyPress)
                return QStyledItemDelegate::editorEvent(event, model, option, index);

            QStyleOptionViewItem opt = option;
            initStyleOption(&opt, index);
            const QWidget* w = option.widget;
            const QStyle* style = w ? w->style() : QApplication::style();
            QRect checkRect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &opt, w);

            if (auto* me = dynamic_cast<QMouseEvent*>(event)) {
                if (!checkRect.contains(me->pos()))
                    return QStyledItemDelegate::editorEvent(event, model, option, index);
            }
            Qt::CheckState s = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
            switch (s) {
            case Qt::Unchecked: s = Qt::Checked; break;
            case Qt::Checked: s = Qt::Unchecked; break;
            case Qt::PartiallyChecked: s = Qt::Unchecked; break;
            }

            model->setData(index, s, Qt::CheckStateRole);
            return true;
        }
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
        int pending;
        QList<SdfPath> load;
        QList<SdfPath> unload;
        QString filter;
        QPointer<ItemDelegate> delegate;
        QPointer<StageModel> stageModel;
        QPointer<SelectionModel> selectionModel;
        QPointer<OutlinerWidget> widget;
    };
    Data d;
};

OutlinerWidgetPrivate::OutlinerWidgetPrivate() { d.pending = 0; }

void
OutlinerWidgetPrivate::init()
{
    d.delegate = new ItemDelegate(d.widget.data());
    d.widget->setItemDelegate(d.delegate);
    connect(d.widget.data(), &OutlinerWidget::itemSelectionChanged, this, &OutlinerWidgetPrivate::updateSelection);
    connect(d.widget.data(), &OutlinerWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        if (column == OutlinerItem::Name) {
            OutlinerItem* outlinerItem = static_cast<OutlinerItem*>(item);
            checkStateChanged(outlinerItem);
        }
    });
}

void
OutlinerWidgetPrivate::itemCheckState(QTreeWidgetItem* item, bool checkable, bool recursive)
{
    Qt::ItemFlags f = item->flags();
    if (checkable) {
        f |= Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
        if (item->childCount() > 0)
            f |= Qt::ItemIsAutoTristate;
        item->setFlags(f);
        if (item->data(0, Qt::CheckStateRole).isNull())
            item->setCheckState(0, Qt::Unchecked);
    }
    else {
        f &= ~Qt::ItemIsUserCheckable;
        item->setFlags(f);
        item->setData(0, Qt::CheckStateRole, QVariant());
    }

    if (recursive) {
        for (int i = 0; i < item->childCount(); ++i)
            itemCheckState(item->child(i), checkable, recursive);
    }
}

void
OutlinerWidgetPrivate::treeCheckState(QTreeWidgetItem* item)
{
    itemCheckState(item, true, true);
}

void
OutlinerWidgetPrivate::initStageModel()
{
    connect(d.stageModel.data(), &StageModel::stageChanged, this, &OutlinerWidgetPrivate::stageChanged);
    connect(d.stageModel.data(), &StageModel::primsChanged, this, &OutlinerWidgetPrivate::primsChanged);
}

void
OutlinerWidgetPrivate::initSelection()
{
    connect(d.selectionModel.data(), &SelectionModel::selectionChanged, this, &OutlinerWidgetPrivate::selectionChanged);
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
        for (int i = 0; i < item->childCount(); ++i)
            collapseItems(item->child(i));
    };
    for (int i = 0; i < d.widget->topLevelItemCount(); ++i)
        collapseItems(d.widget->topLevelItem(i));

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
    //d.widget->scrollToItem(selected.first(), QAbstractItemView::PositionAtCenter);
}

void
OutlinerWidgetPrivate::addItem(OutlinerItem* parent, const SdfPath& path)
{
    OutlinerItem* item = new OutlinerItem(parent, d.stageModel->stage(), path);
    itemCheckState(item, false);
    parent->addChild(item);
    addChildren(item, path);
}

void
OutlinerWidgetPrivate::addChildren(OutlinerItem* parent, const SdfPath& path)
{
    UsdStageRefPtr stage = d.stageModel->stage();
    UsdPrim prim = stage->GetPrimAtPath(path);
    for (const UsdPrim& child : prim.GetAllChildren())
        addItem(parent, child.GetPath());
}

void
OutlinerWidgetPrivate::toggleVisible(OutlinerItem* item)
{
    QList<SdfPath> paths;
    QString pathString = item->data(0, Qt::UserRole).toString();
    if (!pathString.isEmpty())
        paths.append(SdfPath(pathString.toStdString()));

    d.stageModel->setVisible(paths, !item->isVisible());
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
        for (int i = 0; i < item->childCount(); ++i)
            if (matchfilter(item->child(i)))
                childMatches = true;

        bool visible = matches || childMatches;
        item->setHidden(!visible);
        return visible;
    };

    for (int i = 0; i < d.widget->topLevelItemCount(); ++i)
        matchfilter(d.widget->topLevelItem(i));
}

void
OutlinerWidgetPrivate::updateSelection()
{
    QList<SdfPath> paths;
    for (QTreeWidgetItem* item : d.widget->selectedItems()) {
        QString pathString = item->data(0, Qt::UserRole).toString();
        if (!pathString.isEmpty())
            paths.append(SdfPath(pathString.toStdString()));
    }
    d.selectionModel->replacePaths(paths);
}

void
OutlinerWidgetPrivate::checkStateChanged(OutlinerItem* item)
{
    QString pathString = item->data(0, Qt::UserRole).toString();
    if (pathString.isEmpty())
        return;

    SdfPath path(pathString.toStdString());
    UsdStageRefPtr stage = d.stageModel->stage();
    UsdPrim prim = stage->GetPrimAtPath(path);
    if (!prim || !prim.HasPayload())
        return;

    bool isLoaded = prim.IsLoaded();
    Qt::CheckState state = item->checkState(OutlinerItem::Name);

    if ((state == Qt::Checked && isLoaded) || (state == Qt::Unchecked && !isLoaded))
        return;

    if (state == Qt::Checked)
        d.load.append(path);
    else if (state == Qt::Unchecked)
        d.unload.append(path);

    d.pending++;
    QTimer::singleShot(0, d.widget, [this]() {
        if (--d.pending <= 0) {
            if (!d.load.isEmpty()) {
                d.stageModel->loadPayloads(d.load);
                d.load.clear();
            }
            if (!d.unload.isEmpty()) {
                d.stageModel->unloadPayloads(d.unload);
                d.unload.clear();
            }
            d.pending = 0;
        }
    });
}

void
OutlinerWidgetPrivate::selectionChanged(const QList<SdfPath>& paths)
{
    QSignalBlocker blocker(d.widget);
    std::function<void(QTreeWidgetItem*)> selectItems = [&](QTreeWidgetItem* item) {
        QString pathString = item->data(0, Qt::UserRole).toString();
        if (!pathString.isEmpty()) {
            SdfPath path(pathString.toStdString());
            item->setSelected(paths.contains(path));
        }
        for (int i = 0; i < item->childCount(); ++i)
            selectItems(item->child(i));
    };
    for (int i = 0; i < d.widget->topLevelItemCount(); ++i)
        selectItems(d.widget->topLevelItem(i));
    d.widget->update();
}

void OutlinerWidgetPrivate::primsChanged(const QList<SdfPath>& paths)
{
    QSignalBlocker blocker(d.widget);
    std::function<void(QTreeWidgetItem*)> updateItem =
        [&](QTreeWidgetItem* item)
    {
        QString itemPath = item->data(0, Qt::UserRole).toString();
        if (!itemPath.isEmpty()) {
            for (const SdfPath& changed : paths) {
                if (itemPath == QString::fromStdString(changed.GetString())) {
                    UsdPrim prim = d.stageModel->stage()->GetPrimAtPath(changed);
                    if (prim && prim.HasPayload()) {
                        Qt::CheckState want = prim.IsLoaded()
                                              ? Qt::Checked
                                              : Qt::Unchecked;

                        if (item->checkState(0) != want)
                            item->setCheckState(0, want);
                    }
                }
            }
        }
        for (int i = 0; i < item->childCount(); ++i)
            updateItem(item->child(i));
    };
    for (int i = 0; i < d.widget->topLevelItemCount(); ++i)
        updateItem(d.widget->topLevelItem(i));
    d.widget->update();
}

void
OutlinerWidgetPrivate::stageChanged()
{
    d.widget->clear();
    if (d.stageModel->isLoaded()) {
        UsdPrim prim = d.stageModel->stage()->GetPseudoRoot();
        OutlinerItem* rootItem = new OutlinerItem(d.widget.data(), d.stageModel->stage(), prim.GetPath());
        itemCheckState(rootItem, false, true);
        addChildren(rootItem, prim.GetPath());
        initTree();

        if (d.stageModel->loadMode() == StageModel::Payload)
            itemCheckState(rootItem, true, true);
    }
}

OutlinerWidget::OutlinerWidget(QWidget* parent)
    : QTreeWidget(parent)
    , p(new OutlinerWidgetPrivate())
{
    p->d.widget = this;
    p->init();
}

OutlinerWidget::~OutlinerWidget() = default;

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

SelectionModel*
OutlinerWidget::selectionModel()
{
    return p->d.selectionModel;
}

void
OutlinerWidget::setSelectionModel(SelectionModel* selectionModel)
{
    if (p->d.selectionModel != selectionModel) {
        p->d.selectionModel = selectionModel;
        p->initSelection();
        update();
    }
}

StageModel*
OutlinerWidget::stageModel() const
{
    return p->d.stageModel;
}

void
OutlinerWidget::setStageModel(StageModel* stageModel)
{
    if (p->d.stageModel != stageModel) {
        p->d.stageModel = stageModel;
        p->initStageModel();
        update();
    }
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
        for (int i = 0; i < topLevelItemCount(); ++i)
            topLevelItem(i)->setSelected(true);
    }
    else {
        QTreeWidget::keyPressEvent(event);
    }
}

void OutlinerWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QTreeWidgetItem* item = itemAt(event->pos());
    if (!item)
        return;

    QList<SdfPath> paths;
    for (QTreeWidgetItem* selected : selectedItems()) {
        QString pathString = selected->data(0, Qt::UserRole).toString();
        if (!pathString.isEmpty())
            paths.append(SdfPath(pathString.toStdString()));
    }

    if (paths.isEmpty())
        return;

    std::vector<SdfPath> filtered;
    for (const SdfPath& p : paths) {
        bool isChild = false;
        for (const SdfPath& other : paths) {
            if (p == other) continue;
            if (p.HasPrefix(other)) {
                isChild = true;
                break;
            }
        }
        if (!isChild)
            filtered.push_back(p);
    }
    
    QList<SdfPath> payloadPaths;
    UsdStageRefPtr stage = p->d.stageModel->stage();

    std::function<void(const UsdPrim&)> collectPayloads =
        [&](const UsdPrim& prim)
    {
        if (!prim)
            return;

        if (prim.HasPayload())
            payloadPaths.append(prim.GetPath());

        for (const UsdPrim& child : prim.GetAllChildren())
            collectPayloads(child);
    };

    for (const SdfPath& rootPath : filtered) {
        UsdPrim root = stage->GetPrimAtPath(rootPath);
        if (root)
            collectPayloads(root);
    }
    auto variantSets = p->d.stageModel->variantSets(paths, true);
    QMenu menu(this);
    
    QMenu* loadMenu = menu.addMenu("Load");

    QAction* loadSelected = loadMenu->addAction("Selected");
    QAction* loadRecursive = loadMenu->addAction("Recursive");

    loadMenu->addSeparator();

    struct VariantSelection { std::string setName; std::string value; };
    QMap<QAction*, VariantSelection> variantActions;

    for (const auto& it : variantSets) {
        const std::string& setName = it.first;
        const auto& values = it.second;

        QString submenuName = QString("Recursive (%1)")
                                  .arg(QString::fromStdString(setName));
        QMenu* variantMenu = loadMenu->addMenu(submenuName);

        for (const std::string& value : values) {
            QAction* action = variantMenu->addAction(QString::fromStdString(value));
            variantActions[action] = { setName, value };
        }
    }

    menu.addSeparator();

    QMenu* showMenu = menu.addMenu("Show");
    QAction* showSelected = showMenu->addAction("Selected");
    QAction* showRecursive = showMenu->addAction("Recursively");

    QMenu* hideMenu = menu.addMenu("Hide");
    QAction* hideSelected = hideMenu->addAction("Selected");
    QAction* hideRecursive = hideMenu->addAction("Recursively");

    menu.addSeparator();
    QAction* isolate = menu.addAction("Isolate Selection");
    QAction* clearIsolation = menu.addAction("Clear Isolation");

    QAction* chosen = menu.exec(mapToGlobal(event->pos()));
    if (!chosen)
        return;

    if (chosen == loadSelected) {
        p->d.stageModel->loadPayloads(payloadPaths);
        return;
    }

    if (chosen == loadRecursive) {
        p->d.stageModel->loadPayloads(payloadPaths, "", "");
        return;
    }

    if (variantActions.contains(chosen)) {
        auto sel = variantActions[chosen];
        p->d.stageModel->loadPayloads(payloadPaths, sel.setName, sel.value);
        return;
    }

    if (chosen == showSelected) {
        p->d.stageModel->setVisible(paths, true);
    }
    else if (chosen == showRecursive) {
        p->d.stageModel->setVisible(paths, true, true);
    }
    else if (chosen == hideSelected) {
        p->d.stageModel->setVisible(paths, false);
    }
    else if (chosen == hideRecursive) {
        p->d.stageModel->setVisible(paths, false, true);
    }
    else if (chosen == isolate) {
        p->d.stageModel->setMask(paths);
    }
    else if (chosen == clearIsolation) {
        p->d.stageModel->setMask(QList<SdfPath>());
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
        p->toggleVisible(static_cast<OutlinerItem*>(item));
        event->accept();
        return;
    }
    QTreeWidget::mousePressEvent(event);
}

void
OutlinerWidget::mouseMoveEvent(QMouseEvent* event)
{
    event->accept();
}
}  // namespace usd
