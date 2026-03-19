// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "stagetree.h"
#include "application.h"
#include "command.h"
#include "commanddispatcher.h"
#include "primitem.h"
#include "qtutils.h"
#include "selectionmodel.h"
#include "signalguard.h"
#include "stageutils.h"
#include "style.h"
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

namespace usdviewer {
class StageTreePrivate : public QObject, public SignalGuard {
public:
    StageTreePrivate();
    void init();
    void initTree();
    void close();
    void collapse();
    void expand();
    void expandDepth(int targetDepth, const SdfPath& path);
    int maxDepth(const SdfPath& path) const;
    int depth(const SdfPath& path) const;
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
    int parentDepth(const SdfPath& path) const;
    QTreeWidgetItem* itemFromPath(const SdfPath& path) const;

    struct Data {
        int pending;
        bool payloadEnabled;
        QString filter;
        QList<SdfPath> loadPaths;
        QList<SdfPath> unloadPaths;
        UsdStageRefPtr stage;
        QPointer<StageTree> tree;
    };
    Data d;
};

StageTreePrivate::StageTreePrivate() { d.pending = 0; }

void
StageTreePrivate::init()
{
    attach(d.tree);
    attach(d.tree->selectionModel());
    int size = style()->iconSize(Style::UIScale::Small);
    d.tree->setIconSize(QSize(size, size));
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
    }
}

void
StageTreePrivate::close()
{
    QSignalBlocker blocker(d.tree);
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
        QTreeWidgetItem* parent = item->parent();
        while (parent) {
            parent->setExpanded(true);
            parent = parent->parent();
        }
    }
    if (!selected.isEmpty()) {
        QTreeWidgetItem* item = selected.first();
        QRect itemRect = d.tree->visualItemRect(item);
        QRect viewRect = d.tree->viewport()->rect();
        if (!viewRect.intersects(itemRect)) {
            d.tree->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        }
    }
}

void
StageTreePrivate::expandDepth(int targetDepth, const SdfPath& path)
{
    if (d.tree->topLevelItemCount() == 0)
        return;

    d.tree->setUpdatesEnabled(false);
    if (path.IsEmpty()) {
        QTreeWidgetItem* root = d.tree->topLevelItem(0);

        std::function<void(QTreeWidgetItem*, int)> expand = [&](QTreeWidgetItem* item, int depth) {
            item->setExpanded(depth < targetDepth);

            for (int i = 0; i < item->childCount(); ++i)
                expand(item->child(i), depth + 1);
        };
        expand(root, 0);
    }
    else {
        QTreeWidgetItem* item = itemFromPath(path);
        if (!item)
            return;

        int selectedDepth = depth(path);

        QTreeWidgetItem* parent = item->parent();
        int parentDepth = selectedDepth - 1;

        while (parent) {
            parent->setExpanded(parentDepth < targetDepth);
            parent = parent->parent();
            parentDepth--;
        }
        std::function<void(QTreeWidgetItem*, int)> expand = [&](QTreeWidgetItem* node, int depth) {
            node->setExpanded(depth < targetDepth);
            for (int i = 0; i < node->childCount(); ++i)
                expand(node->child(i), depth + 1);
        };
        expand(item, selectedDepth);
    }
    d.tree->setUpdatesEnabled(true);
}

int
StageTreePrivate::maxDepth(const SdfPath& path) const
{
    QTreeWidgetItem* root = nullptr;

    if (d.tree->topLevelItemCount() > 0)
        root = d.tree->topLevelItem(0);
    if (!root)
        return 0;
    std::function<int(QTreeWidgetItem*, int)> subtreeDepth = [&](QTreeWidgetItem* item, int d) {
        int max = d;

        for (int i = 0; i < item->childCount(); ++i)
            max = std::max(max, subtreeDepth(item->child(i), d + 1));

        return max;
    };
    if (path.IsEmpty())
        return subtreeDepth(root, 0);

    QTreeWidgetItem* item = itemFromPath(path);
    if (!item)
        return subtreeDepth(root, 0);

    int parentDepth = depth(path);
    int childDepth = subtreeDepth(item, 0);
    return parentDepth + childDepth;
}

int
StageTreePrivate::depth(const SdfPath& path) const
{
    if (path.IsEmpty())
        return 0;

    QTreeWidgetItem* item = itemFromPath(path);
    if (!item)
        return 0;

    int d = 0;
    while (item->parent()) {
        d++;
        item = item->parent();
    }

    return d;
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
    const SdfPath path(QStringToString(item->data(0, PrimItem::PrimPath).toString()));
    if (stage::isVisible(d.stage, path)) {
        CommandDispatcher::run(new Command(hidePaths(QList<SdfPath> { path }, false)));
    }
    else {
        CommandDispatcher::run(new Command(showPaths(QList<SdfPath> { path }, false)));
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
    for (QTreeWidgetItem* baseItem : d.tree->selectedItems()) {
        PrimItem* item = static_cast<PrimItem*>(baseItem);
        const QString pathString = item->data(0, PrimItem::PrimPath).toString();
        if (!pathString.isEmpty())
            paths.append(SdfPath(QStringToString(pathString)));
    }
    CommandDispatcher::run(new Command(selectPaths(paths)));
}

void
StageTreePrivate::checkStateChanged(PrimItem* item)
{
    PrimItem* primItem = static_cast<PrimItem*>(item);
    const QString pathString = primItem->data(0, PrimItem::PrimPath).toString();
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
    QList<SdfPath> paths;
    for (QTreeWidgetItem* selected : d.tree->selectedItems()) {
        PrimItem* primItem = static_cast<PrimItem*>(selected);
        const QString pathString = primItem->data(0, PrimItem::PrimPath).toString();
        if (!pathString.isEmpty())
            paths.append(SdfPath(qt::QStringToString(pathString)));
    }
    if (paths.isEmpty())
        return;

    const QList<SdfPath> rootPaths = stage::rootPaths(paths);
    const QList<SdfPath> payloadPaths = stage::payloadPaths(d.stage, rootPaths);
    const QMap<QString, QList<QString>> variantSets = stage::findVariantSets(d.stage, paths, true);

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
                const QString& setName = it.key();
                const QList<QString>& values = it.value();

                QMenu* setMenu = loadMenu->addMenu(setName);
                for (const QString& value : values) {
                    QAction* action = setMenu->addAction(value);
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
    QMenu* showMenu = menu.addMenu("Show");
    QAction* showSelected = showMenu->addAction("Selected");
    QAction* showRecursive = showMenu->addAction("Recursive");

    QMenu* hideMenu = menu.addMenu("Hide");
    QAction* hideSelected = hideMenu->addAction("Selected");
    QAction* hideRecursive = hideMenu->addAction("Recursive");

    menu.addSeparator();
    QAction* isolateSelected = menu.addAction("Isolate");
    QAction* isolateClear = menu.addAction("Clear");

    menu.addSeparator();
    QAction* deleteSelected = menu.addAction("Delete");

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
        const VariantSelection& sel = variantActions[chosen];
        CommandDispatcher::run(new Command(loadPayloads(payloadPaths, sel.setName, sel.value)));
        return;
    }
    if (chosen == showSelected)
        CommandDispatcher::run(new Command(showPaths(paths, false)));
    else if (chosen == showRecursive)
        CommandDispatcher::run(new Command(showPaths(paths, true)));
    else if (chosen == hideSelected)
        CommandDispatcher::run(new Command(hidePaths(paths, false)));
    else if (chosen == hideRecursive)
        CommandDispatcher::run(new Command(hidePaths(paths, true)));
    else if (chosen == isolateSelected)
        CommandDispatcher::run(new Command(isolatePaths(paths)));
    else if (chosen == isolateClear)
        CommandDispatcher::run(new Command(isolatePaths(QList<SdfPath>())));
    else if (chosen == deleteSelected)
        CommandDispatcher::run(new Command(deletePaths(paths)));
}

void
StageTreePrivate::updateStage(UsdStageRefPtr stage)
{
    SignalGuard::Scope guard(this);
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
    beginGuard();
    {
        std::function<void(QTreeWidgetItem*)> updateItem = [&](QTreeWidgetItem* baseItem) {
            PrimItem* primItem = static_cast<PrimItem*>(baseItem);
            const QString pathString = primItem->data(0, PrimItem::PrimPath).toString();
            if (!pathString.isEmpty()) {
                for (const SdfPath& changed : paths) {
                    if (pathString == StringToQString(changed.GetString())) {
                        UsdPrim prim = d.stage->GetPrimAtPath(changed);
                        if (prim && prim.HasPayload()) {
                            Qt::CheckState want = prim.IsLoaded() ? Qt::Checked : Qt::Unchecked;
                            if (primItem->checkState(0) != want)
                                primItem->setCheckState(0, want);
                        }
                    }
                }
            }
            for (int i = 0; i < primItem->childCount(); ++i)
                updateItem(primItem->child(i));
        };
        for (int i = 0; i < d.tree->topLevelItemCount(); ++i)
            updateItem(d.tree->topLevelItem(i));
        d.tree->update();
    }
    endGuard();
}

void
StageTreePrivate::updateSelection(const QList<SdfPath>& paths)
{
    SignalGuard::Scope guard(this);
    const QSet<SdfPath> selectedSet(paths.begin(), paths.end());
    std::function<void(QTreeWidgetItem*)> selectItems = [&](QTreeWidgetItem* baseItem) {
        PrimItem* primItem = static_cast<PrimItem*>(baseItem);
        const QString pathString = primItem->data(0, PrimItem::PrimPath).toString();
        if (!pathString.isEmpty()) {
            const SdfPath itemPath(QStringToString(pathString));
            bool isSelected = selectedSet.contains(itemPath);
            if (!isSelected && d.payloadEnabled && primItem->childCount() == 0) {
                for (const SdfPath& path : selectedSet) {
                    if (path.HasPrefix(itemPath) && path != itemPath) {
                        isSelected = true;
                        break;
                    }
                }
            }
            primItem->setSelected(isSelected);
        }
        for (int i = 0; i < primItem->childCount(); ++i)
            selectItems(primItem->child(i));
    };
    for (int i = 0; i < d.tree->topLevelItemCount(); ++i)
        selectItems(d.tree->topLevelItem(i));

    d.tree->update();
}

QTreeWidgetItem*
StageTreePrivate::itemFromPath(const SdfPath& path) const
{
    QString target = qt::StringToQString(path.GetString());
    std::function<QTreeWidgetItem*(QTreeWidgetItem*)> find = [&](QTreeWidgetItem* baseItem) -> QTreeWidgetItem* {
        PrimItem* primItem = static_cast<PrimItem*>(baseItem);
        const QString pathString = primItem->data(0, PrimItem::PrimPath).toString();
        if (!pathString.isEmpty() && pathString == target)
            return primItem;
        for (int i = 0; i < primItem->childCount(); ++i) {
            if (QTreeWidgetItem* found = find(primItem->child(i)))
                return found;
        }
        return nullptr;
    };
    for (int i = 0; i < d.tree->topLevelItemCount(); ++i) {
        if (QTreeWidgetItem* found = find(d.tree->topLevelItem(i)))
            return found;
    }
    return nullptr;
}

StageTree::StageTree(QWidget* parent)
    : TreeWidget(parent)
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

void
StageTree::expandDepth(int targetDepth, const SdfPath& path)
{
    p->expandDepth(targetDepth, path);
}

int
StageTree::maxDepth(const SdfPath& path) const
{
    return p->maxDepth(path);
}

int
StageTree::depth(const SdfPath& path) const
{
    return p->depth(path);
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

}  // namespace usdviewer
