// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdstagetree.h"
#include "command.h"
#include "commanddispatcher.h"
#include "selectionmodel.h"
#include "stylesheet.h"
#include "usdprimitem.h"
#include "usdqtutils.h"
#include "usdstageutils.h"
#include <QApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QTimer>
#include <pxr/usd/usd/prim.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class StageTreePrivate : public QObject {
public:
    StageTreePrivate();
    void init();
    void initDataModel();
    void initSelection();
    void initTree();
    void close();
    void collapse();
    void expand();
    void addItem(PrimItem* parent, const SdfPath& parentPath);
    void addChildren(PrimItem* parent, const SdfPath& parentPath);
    void toggleVisible(PrimItem* item);
    void updateFilter();
    void itemCheckState(QTreeWidgetItem* item, bool checkable, bool recursive = false);
    void treeCheckState(QTreeWidgetItem* item);
    void contextMenuEvent(QContextMenuEvent* event);
    void updateStage(UsdStageRefPtr stage);
    void updatePrims(const QList<SdfPath>& paths);
    void updateSelection(const QList<SdfPath>& paths);

public Q_SLOTS:
    void itemSelectionChanged();
    void checkStateChanged(PrimItem* item);

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

            if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseButtonDblClick) {
                auto* mouseEvent = dynamic_cast<QMouseEvent*>(event);
                if (mouseEvent) {
                    if (mouseEvent->button() != Qt::LeftButton)
                        return false;
                    if (!checkRect.contains(mouseEvent->pos()))
                        return QStyledItemDelegate::editorEvent(event, model, option, index);
                }
                Qt::CheckState s = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
                switch (s) {
                case Qt::Unchecked: s = Qt::Checked; break;
                case Qt::Checked: s = Qt::Unchecked; break;
                case Qt::PartiallyChecked: s = Qt::Unchecked; break;
                }
                model->setData(index, s, Qt::CheckStateRole);
            }
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
            opt.state &= ~QStyle::State_HasFocus;
            auto ss = Stylesheet::instance();
            if (opt.state & QStyle::State_Selected) {
                painter->fillRect(opt.rect, ss->color(Stylesheet::ColorRole::Highlight));
            }
            if (hasSelectedChildren(item)) {
                opt.font.setBold(true);
                opt.font.setItalic(true);
                painter->save();
                painter->fillRect(opt.rect, ss->color(Stylesheet::ColorRole::HighlightAlt));
                painter->restore();
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
        bool payloadEnabled;
        QString filter;
        QList<SdfPath> loadPaths;
        QList<SdfPath> unloadPaths;
        UsdStageRefPtr stage;
        QPointer<ItemDelegate> delegate;
        QPointer<StageTree> tree;
    };
    Data d;
};

StageTreePrivate::StageTreePrivate() { d.pending = 0; }

void
StageTreePrivate::init()
{
    d.delegate = new ItemDelegate(d.tree.data());
    d.tree->setItemDelegate(d.delegate);
    connect(d.tree.data(), &StageTree::itemSelectionChanged, this, &StageTreePrivate::itemSelectionChanged);
    connect(d.tree.data(), &StageTree::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        if (column == PrimItem::Name) {
            PrimItem* primItem = static_cast<PrimItem*>(item);
            checkStateChanged(primItem);
        }
    });
}

void
StageTreePrivate::itemCheckState(QTreeWidgetItem* item, bool checkable, bool recursive)
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
StageTreePrivate::treeCheckState(QTreeWidgetItem* item)
{
    itemCheckState(item, true, true);
}

void
StageTreePrivate::initTree()
{
    int topLevelCount = d.tree->topLevelItemCount();
    for (int i = 0; i < topLevelCount; ++i) {
        QTreeWidgetItem* topItem = d.tree->topLevelItem(i);
        d.tree->expandItem(topItem);
        for (int j = 0; j < topItem->childCount(); ++j) {
            d.tree->expandItem(topItem->child(j));
        }
    }
}

void
StageTreePrivate::close()
{
    d.stage = nullptr;
    d.tree->clear();
}

void
StageTreePrivate::collapse()
{
    std::function<void(QTreeWidgetItem*)> collapseItems = [&](QTreeWidgetItem* item) {
        item->setExpanded(false);
        for (int i = 0; i < item->childCount(); ++i)
            collapseItems(item->child(i));
    };
    for (int i = 0; i < d.tree->topLevelItemCount(); ++i)
        collapseItems(d.tree->topLevelItem(i));

    initTree();
}

void
StageTreePrivate::expand()
{
    const QList<QTreeWidgetItem*> selected = d.tree->selectedItems();
    for (QTreeWidgetItem* item : selected) {
        item->setExpanded(true);
        QTreeWidgetItem* parent = item->parent();
        while (parent) {
            parent->setExpanded(true);
            parent = parent->parent();
        }
    }
    if (selected.size()) {
        d.tree->scrollToItem(selected.first(), QAbstractItemView::PositionAtCenter);
    }
}

void
StageTreePrivate::addItem(PrimItem* parent, const SdfPath& path)
{
    PrimItem* item = new PrimItem(parent, d.stage, path);
    itemCheckState(item, false);
    parent->addChild(item);
    addChildren(item, path);
}

void
StageTreePrivate::addChildren(PrimItem* parent, const SdfPath& path)
{
    UsdPrim prim = d.stage->GetPrimAtPath(path);
    for (const UsdPrim& child : prim.GetAllChildren())
        addItem(parent, child.GetPath());
}

void
StageTreePrivate::toggleVisible(PrimItem* item)
{
    QList<SdfPath> paths;
    QString pathString = item->data(0, Qt::UserRole).toString();
    if (!pathString.isEmpty())
        paths.append(SdfPath(QStringToString(pathString)));
    if (item->isVisible()) {
        CommandDispatcher::run(new Command(hide(paths, false)));
    }
    else {
        CommandDispatcher::run(new Command(show(paths, false)));
    }
}

void
StageTreePrivate::updateFilter()
{
    std::function<bool(QTreeWidgetItem*)> matchfilter = [&](QTreeWidgetItem* item) -> bool {
        bool matches = false;
        for (int col = 0; col < d.tree->columnCount(); ++col) {
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

    for (int i = 0; i < d.tree->topLevelItemCount(); ++i)
        matchfilter(d.tree->topLevelItem(i));
}

void
StageTreePrivate::itemSelectionChanged()
{
    QList<SdfPath> paths;
    for (QTreeWidgetItem* item : d.tree->selectedItems()) {
        QString pathString = item->data(0, Qt::UserRole).toString();
        if (!pathString.isEmpty())
            paths.append(SdfPath(QStringToString(pathString)));
    }
    CommandDispatcher::run(new Command(select(paths)));
}

void
StageTreePrivate::checkStateChanged(PrimItem* item)
{
    QString pathString = item->data(0, Qt::UserRole).toString();
    if (pathString.isEmpty())
        return;

    SdfPath path(QStringToString(pathString));
    UsdPrim prim = d.stage->GetPrimAtPath(path);
    if (!prim || !prim.HasPayload())
        return;

    bool isLoaded = prim.IsLoaded();
    Qt::CheckState state = item->checkState(PrimItem::Name);

    if ((state == Qt::Checked && isLoaded) || (state == Qt::Unchecked && !isLoaded))
        return;

    if (state == Qt::Checked)
        d.loadPaths.append(path);
    else if (state == Qt::Unchecked)
        d.unloadPaths.append(path);

    d.pending++;
    QTimer::singleShot(0, d.tree, [this]() {
        if (--d.pending <= 0) {
            if (!d.loadPaths.isEmpty()) {
                CommandDispatcher::run(new Command(loadPayloads(d.loadPaths)));
                d.loadPaths.clear();
            }
            if (!d.unloadPaths.isEmpty()) {
                CommandDispatcher::run(new Command(unloadPayloads(d.unloadPaths)));
                d.unloadPaths.clear();
            }
            d.pending = 0;
        }
    });
}

void
StageTreePrivate::contextMenuEvent(QContextMenuEvent* event)
{
    QTreeWidgetItem* item = d.tree->itemAt(event->pos());
    QList<SdfPath> paths;
    for (QTreeWidgetItem* selected : d.tree->selectedItems()) {
        QString p = selected->data(0, Qt::UserRole).toString();
        if (!p.isEmpty())
            paths.append(SdfPath(usd::QStringToString(p)));
    }
    if (paths.isEmpty())
        return;

    std::vector<SdfPath> filtered;
    filtered.reserve(paths.size());
    for (const SdfPath& p : paths) {
        bool isChild = false;
        for (const SdfPath& other : paths) {
            if (p != other && p.HasPrefix(other)) {
                isChild = true;
                break;
            }
        }
        if (!isChild)
            filtered.push_back(p);
    }
    QList<SdfPath> payloadPaths;
    std::function<void(const UsdPrim&)> collectPayloads = [&](const UsdPrim& prim) {
        if (!prim)
            return;
        if (prim.HasPayload())
            payloadPaths.append(prim.GetPath());
        for (const UsdPrim& c : prim.GetAllChildren())
            collectPayloads(c);
    };

    for (const SdfPath& rootPath : filtered) {
        UsdPrim root = d.stage->GetPrimAtPath(rootPath);
        if (root)
            collectPayloads(root);
    }
    QMap<QString, QList<QString>> variantSets = findVariantSets(d.stage, paths, true);

    QMenu menu(d.tree.data());
    struct VariantSelection {
        QString setName;
        QString value;
    };
    QAction* loadSelected = nullptr;
    QAction* unloadSelected = nullptr;
    QMap<QAction*, VariantSelection> variantActions;

    if (!payloadPaths.isEmpty()) {
        if (!variantSets.isEmpty()) {
            QMenu* loadMenu = menu.addMenu("Load");
            QMenu* unloadMenu = menu.addMenu("Unload");

            loadSelected = loadMenu->addAction("Selected");
            unloadSelected = unloadMenu->addAction("Selected");
            loadMenu->addSeparator();

            for (auto it = variantSets.begin(); it != variantSets.end(); ++it) {
                QString setName = it.key();
                const QList<QString>& values = it.value();

                QMenu* variantMenu = loadMenu->addMenu(QString("%1").arg(setName));

                for (const QString& value : values) {
                    QAction* action = variantMenu->addAction(value);
                    variantActions[action] = { setName, value };
                }
            }
        }
        else {
            loadSelected = menu.addAction("Load");
            unloadSelected = menu.addAction("Unload");
        }
    }
    menu.addSeparator();

    QMenu* menuShow = menu.addMenu("Show");
    QAction* showSelected = menuShow->addAction("Selected");
    QAction* showRecursive = menuShow->addAction("Recursively");

    QMenu* menuHide = menu.addMenu("Hide");
    QAction* hideSelected = menuHide->addAction("Selected");
    QAction* hideRecursive = menuHide->addAction("Recursively");
    menu.addSeparator();

    QAction* isolateSelected = menu.addAction("Isolate");
    QAction* isolateClear = menu.addAction("Clear");

    QAction* chosen = menu.exec(d.tree->mapToGlobal(event->pos()));
    if (!chosen)
        return;

    if (chosen == loadSelected) {
        CommandDispatcher::run(new Command(loadPayloads(payloadPaths)));
        return;
    }

    if (chosen == unloadSelected) {
        CommandDispatcher::run(new Command(unloadPayloads(payloadPaths)));
        return;
    }

    if (variantActions.contains(chosen)) {
        VariantSelection sel = variantActions[chosen];
        CommandDispatcher::run(new Command(loadPayloads(payloadPaths, sel.setName, sel.value)));
        return;
    }

    if (chosen == showSelected)
        CommandDispatcher::run(new Command(show(paths, false)));
    else if (chosen == showRecursive)
        CommandDispatcher::run(new Command(show(paths, true)));
    else if (chosen == hideSelected)
        CommandDispatcher::run(new Command(hide(paths, false)));
    else if (chosen == hideRecursive)
        CommandDispatcher::run(new Command(hide(paths, true)));
    else if (chosen == isolateSelected)
        CommandDispatcher::run(new Command(isolate(paths)));
    else if (chosen == isolateClear)
        CommandDispatcher::run(new Command(isolate(QList<SdfPath>())));
}

void
StageTreePrivate::updateStage(UsdStageRefPtr stage)
{
    close();
    d.stage = stage;
    UsdPrim prim = stage->GetPseudoRoot();
    PrimItem* rootItem = new PrimItem(d.tree.data(), stage, prim.GetPath());
    itemCheckState(rootItem, false, true);
    addChildren(rootItem, prim.GetPath());
    initTree();
    if (d.payloadEnabled)
        itemCheckState(rootItem, true, true);
}

void
StageTreePrivate::updatePrims(const QList<SdfPath>& paths)
{
    QSignalBlocker blocker(d.tree);
    std::function<void(QTreeWidgetItem*)> updateItem = [&](QTreeWidgetItem* item) {
        QString itemPath = item->data(0, Qt::UserRole).toString();
        if (!itemPath.isEmpty()) {
            for (const SdfPath& changed : paths) {
                if (itemPath == StringToQString(changed.GetString())) {
                    UsdPrim prim = d.stage->GetPrimAtPath(changed);
                    if (prim && prim.HasPayload()) {
                        Qt::CheckState want = prim.IsLoaded() ? Qt::Checked : Qt::Unchecked;
                        if (item->checkState(0) != want)
                            item->setCheckState(0, want);
                    }
                }
            }
        }
        for (int i = 0; i < item->childCount(); ++i)
            updateItem(item->child(i));
    };
    for (int i = 0; i < d.tree->topLevelItemCount(); ++i)
        updateItem(d.tree->topLevelItem(i));
    d.tree->update();
}

void
StageTreePrivate::updateSelection(const QList<SdfPath>& paths)
{
    QSignalBlocker blocker(d.tree);
    QSet<SdfPath> selectedSet = QSet<SdfPath>(paths.begin(), paths.end());
    std::function<void(QTreeWidgetItem*)> selectItems = [&](QTreeWidgetItem* item) {
        QString itemData = item->data(0, Qt::UserRole).toString();
        if (!itemData.isEmpty()) {
            SdfPath itemPath(QStringToString(itemData));
            bool isSelected = false;
            if (selectedSet.contains(itemPath))
                isSelected = true;
            else if (d.payloadEnabled && !item->childCount()) {
                for (const SdfPath& path : selectedSet) {
                    if (path.HasPrefix(itemPath) && path != itemPath) {
                        isSelected = true;
                        break;
                    }
                }
            }
            item->setSelected(isSelected);
        }
        for (int i = 0; i < item->childCount(); ++i)
            selectItems(item->child(i));
    };
    for (int i = 0; i < d.tree->topLevelItemCount(); ++i)
        selectItems(d.tree->topLevelItem(i));
    d.tree->update();
}

StageTree::StageTree(QWidget* parent)
    : QTreeWidget(parent)
    , p(new StageTreePrivate())
{
    p->d.tree = this;
    p->init();
}

StageTree::~StageTree() = default;

void
StageTree::close()
{
    p->close();
}

void
StageTree::collapse()
{
    p->collapse();
}
void
StageTree::expand()
{
    p->expand();
}


QString
StageTree::filter() const
{
    return p->d.filter;
}

void
StageTree::setFilter(const QString& filter)
{
    if (filter != p->d.filter) {
        p->d.filter = filter;
        p->updateFilter();
    }
}

bool
StageTree::payloadEnabled() const
{
    return p->d.payloadEnabled;
}

void
StageTree::setPayloadEnabled(bool enabled)
{
    if (enabled != p->d.payloadEnabled) {
        p->d.payloadEnabled = enabled;
    }
}

void
StageTree::updateStage(UsdStageRefPtr stage)
{
    p->updateStage(stage);
}

void
StageTree::updatePrims(const QList<SdfPath>& paths)
{
    p->updatePrims(paths);
}

void
StageTree::updateSelection(const QList<SdfPath>& paths)
{
    p->updateSelection(paths);
}

void
StageTree::contextMenuEvent(QContextMenuEvent* event)
{
    p->contextMenuEvent(event);
}

void
StageTree::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
        for (int i = 0; i < topLevelItemCount(); ++i)
            topLevelItem(i)->setSelected(true);
    }
    QTreeWidget::keyPressEvent(event);
}

void
StageTree::mousePressEvent(QMouseEvent* event)
{
    QTreeWidgetItem* item = itemAt(event->pos());
    int column = columnAt(event->pos().x());
    if (!item) {
        clearSelection();
        Q_EMIT itemSelectionChanged();
        return;
    }
    if (column == PrimItem::Vis) {
        p->toggleVisible(static_cast<PrimItem*>(item));
        event->accept();
        return;
    }
    QTreeWidget::mousePressEvent(event);
}

void
StageTree::mouseMoveEvent(QMouseEvent* event)
{
    event->accept();
}
}  // namespace usd
