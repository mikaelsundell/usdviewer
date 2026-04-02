// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "stagetree.h"
#include "application.h"
#include "command.h"
#include "primitem.h"
#include "qtutils.h"
#include "selectionlist.h"
#include "signalguard.h"
#include "style.h"
#include "tracelocks.h"
#include "usdutils.h"
#include "viewcontext.h"
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDrag>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QTimer>
#include <algorithm>
#include <pxr/usd/usd/prim.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdviewer {

namespace {
    static constexpr const char* kPrimPathMime = "application/x-usdviewer-prim-path";
    static constexpr const char* kDropItemPtrProperty = "_usdviewer_drop_item_ptr";
    static constexpr const char* kDropModeProperty = "_usdviewer_drop_mode";

    enum DropMode { DropNone = 0, DropAboveItem = 1, DropOnItem = 2, DropBelowItem = 3 };

    void clearDropIndicator(QWidget* widget)
    {
        if (!widget)
            return;

        widget->setProperty(kDropItemPtrProperty, QVariant::fromValue<qulonglong>(0));
        widget->setProperty(kDropModeProperty, DropNone);
        if (widget->updatesEnabled())
            widget->update();
    }

    void setDropIndicator(QWidget* widget, QTreeWidgetItem* item, int mode)
    {
        if (!widget)
            return;

        const qulonglong ptr = reinterpret_cast<qulonglong>(item);
        widget->setProperty(kDropItemPtrProperty, QVariant::fromValue(ptr));
        widget->setProperty(kDropModeProperty, mode);
        if (widget->updatesEnabled())
            widget->update();
    }

    QList<SdfPath> sortPathsByDepth(const QList<SdfPath>& paths)
    {
        QList<SdfPath> sorted = paths;
        std::sort(sorted.begin(), sorted.end(), [](const SdfPath& a, const SdfPath& b) {
            const size_t aCount = a.GetPathElementCount();
            const size_t bCount = b.GetPathElementCount();
            if (aCount != bCount)
                return aCount < bCount;
            return a.GetString() < b.GetString();
        });
        return sorted;
    }

}  // namespace

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

    void toggleVisible(PrimItem* item);
    void updateFilter();
    void itemCheckState(QTreeWidgetItem* item, bool checkable, bool recursive = false);
    void treeCheckState(QTreeWidgetItem* item);
    void contextMenuEvent(QContextMenuEvent* event);
    void updateStage(UsdStageRefPtr stage);
    void updatePrims(const QList<SdfPath>& paths, const QList<SdfPath>& invalidated);
    void updateSelection(const QList<SdfPath>& paths);

    void updatePrim(const SdfPath& path);
    void invalidatePrim(const SdfPath& path);
    void invalidatePrimImpl(const SdfPath& path, bool refreshParent);
    void invalidateSubtree(PrimItem* item, const UsdPrim& prim);
    void invalidateChildren(PrimItem* parentItem, const UsdPrim& prim);

    PrimItem* addItem(PrimItem* parent, const SdfPath& parentPath);
    void addChildren(PrimItem* parent, const SdfPath& parentPath);

public Q_SLOTS:
    void itemSelectionChanged();
    void checkStateChanged(PrimItem* item);
    void nameChanged(PrimItem* item);

public:
    int parentDepth(const SdfPath& path) const;
    PrimItem* itemFromPath(const SdfPath& path) const;

    struct Data {
        int pending;
        bool payloadEnabled;
        QString filter;
        QList<SdfPath> loadPaths;
        QList<SdfPath> unloadPaths;
        QList<SdfPath> maskPaths;
        UsdStageRefPtr stage;
        QPointer<ViewContext> context;
        QPointer<StageTree> tree;
    };
    Data d;
};

StageTreePrivate::StageTreePrivate()
{
    d.pending = 0;
    d.payloadEnabled = false;
    d.context = nullptr;
}

void
StageTreePrivate::init()
{
    attach(d.tree);
    attach(d.tree->selectionModel());
    int size = style()->iconSize(Style::UIScale::Small);
    d.tree->setIconSize(QSize(size, size));

    d.tree->setDragEnabled(true);
    d.tree->setAcceptDrops(true);
    d.tree->viewport()->setAcceptDrops(true);
    d.tree->setDropIndicatorShown(false);
    d.tree->setDragDropMode(QAbstractItemView::DragDrop);
    d.tree->setDefaultDropAction(Qt::MoveAction);

    connect(d.tree.data(), &StageTree::itemSelectionChanged, this, &StageTreePrivate::itemSelectionChanged);
    connect(d.tree.data(), &StageTree::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        if (column == PrimItem::Name) {
            PrimItem* primItem = static_cast<PrimItem*>(item);
            checkStateChanged(primItem);
            nameChanged(primItem);
        }
    });
}

void
StageTreePrivate::itemCheckState(QTreeWidgetItem* item, bool checkable, bool recursive)
{
    Qt::ItemFlags f = item->flags();
    if (checkable) {
        f |= Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
        f |= Qt::ItemIsAutoTristate;
        item->setFlags(f);
        if (item->data(0, Qt::CheckStateRole).isNull())
            item->setCheckState(0, Qt::Unchecked);
    }
    else {
        f &= ~(Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
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
    clearDropIndicator(d.tree);
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
        if (!viewRect.intersects(itemRect))
            d.tree->scrollToItem(item, QAbstractItemView::PositionAtCenter);
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
        if (!item) {
            d.tree->setUpdatesEnabled(true);
            return;
        }

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

PrimItem*
StageTreePrivate::addItem(PrimItem* parent, const SdfPath& path)
{
    UsdStageRefPtr stage;
    bool isPayload = false;
    bool isLoaded = false;
    {
        READ_LOCKER(locker, d.context->stageLock(), "stageLock");
        stage = d.stage;
        if (!stage)
            return nullptr;

        isPayload = d.payloadEnabled && stage::isPayload(stage, path);
        if (isPayload)
            isLoaded = stage->GetPrimAtPath(path).IsLoaded();
    }

    qDebug() << "StageTreePrivate::addItem:" << qt::StringToQString(path.GetString()) << "payloadEnabled"
             << d.payloadEnabled << "isPayload" << isPayload << "isLoaded" << isLoaded;

    PrimItem* item = new PrimItem(parent, stage, path);
    item->invalidate();

    Qt::ItemFlags flags = item->flags();
    flags |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    item->setFlags(flags);

    itemCheckState(item, d.payloadEnabled);
    parent->addChild(item);

    if (isPayload) {
        qDebug() << "StageTreePrivate::addItem: setCheckState" << qt::StringToQString(path.GetString())
                 << (isLoaded ? "Checked" : "Unchecked");

        item->setCheckState(0, isLoaded ? Qt::Checked : Qt::Unchecked);

        qDebug() << "StageTreePrivate::addItem: final checkState" << qt::StringToQString(path.GetString())
                 << item->checkState(0);
        return item;
    }

    addChildren(item, path);
    return item;
}

void
StageTreePrivate::addChildren(PrimItem* parent, const SdfPath& path)
{
    QList<SdfPath> childPaths;
    {
        READ_LOCKER(locker, d.context->stageLock(), "stageLock");
        if (!d.stage)
            return;

        UsdPrim prim = d.stage->GetPrimAtPath(path);
        if (!prim)
            return;

        for (const UsdPrim& child : prim.GetAllChildren())
            childPaths.append(child.GetPath());
    }

    for (const SdfPath& childPath : childPaths)
        addItem(parent, childPath);
}

void
StageTreePrivate::toggleVisible(PrimItem* item)
{
    const SdfPath path(QStringToString(item->data(0, PrimItem::Path).toString()));
    bool visible = false;
    {
        READ_LOCKER(locker, d.context->stageLock(), "stageLock");
        if (!d.stage)
            return;
        visible = stage::isVisible(d.stage, path);
    }
    if (visible)
        d.context->run(new Command(hidePaths(QList<SdfPath> { path }, false)));
    else
        d.context->run(new Command(showPaths(QList<SdfPath> { path }, false)));
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
        for (int i = 0; i < item->childCount(); ++i) {
            if (matchfilter(item->child(i)))
                childMatches = true;
        }

        const bool visible = matches || childMatches;
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
        const QString pathString = item->data(0, PrimItem::Path).toString();
        if (!pathString.isEmpty())
            paths.append(SdfPath(QStringToString(pathString)));
    }
    d.context->run(new Command(selectPaths(paths)));
}

void
StageTreePrivate::checkStateChanged(PrimItem* item)
{
    PrimItem* primItem = static_cast<PrimItem*>(item);
    const QString pathString = primItem->data(0, PrimItem::Path).toString();
    if (pathString.isEmpty())
        return;

    const SdfPath path(QStringToString(pathString));

    bool hasPayload = false;
    bool isLoaded = false;
    {
        READ_LOCKER(locker, d.context->stageLock(), "stageLock");
        if (!d.stage)
            return;

        const UsdPrim prim = d.stage->GetPrimAtPath(path);
        if (!prim)
            return;

        if (!stage::isPayload(d.stage, path))
            return;

        hasPayload = true;
        isLoaded = prim.IsLoaded();
    }

    if (!hasPayload)
        return;

    const Qt::CheckState state = item->checkState(PrimItem::Name);
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
                d.context->run(new Command(loadPayloads(d.loadPaths)));
                d.loadPaths.clear();
            }
            if (!d.unloadPaths.isEmpty()) {
                d.context->run(new Command(unloadPayloads(d.unloadPaths)));
                d.unloadPaths.clear();
            }
            d.pending = 0;
        }
    });
}

void
StageTreePrivate::nameChanged(PrimItem* item)
{
    const SdfPath oldPath(QStringToString(item->data(0, PrimItem::Path).toString()));
    if (oldPath.IsEmpty())
        return;

    const QString newName = item->data(PrimItem::Name, PrimItem::EditName).toString().trimmed();
    if (newName.isEmpty())
        return;

    const QString oldName = qt::StringToQString(oldPath.GetName());
    if (newName == oldName) {
        item->setData(PrimItem::Name, PrimItem::EditName, QString());
        return;
    }

    d.context->run(new Command(renamePath(oldPath, newName)));
}

void
StageTree::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);

    auto* item = static_cast<PrimItem*>(currentItem());
    if (!item)
        return;

    const QString pathString = item->data(0, PrimItem::Path).toString();
    if (pathString.isEmpty())
        return;

    auto* mime = new QMimeData();
    mime->setData(kPrimPathMime, pathString.toUtf8());

    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
}

void
StageTree::dragEnterEvent(QDragEnterEvent* event)
{
    if (!event->mimeData()->hasFormat(kPrimPathMime)) {
        clearDropIndicator(this);
        event->ignore();
        return;
    }

    event->acceptProposedAction();
}

void
StageTree::dragMoveEvent(QDragMoveEvent* event)
{
    if (!event->mimeData()->hasFormat(kPrimPathMime)) {
        clearDropIndicator(this);
        event->ignore();
        return;
    }

    QTreeWidgetItem* target = itemAt(event->position().toPoint());
    if (!target) {
        clearDropIndicator(this);
        event->ignore();
        return;
    }

    auto* primTarget = static_cast<PrimItem*>(target);
    const QString fromPathString = QString::fromUtf8(event->mimeData()->data(kPrimPathMime));
    const QString targetPathString = primTarget->data(0, PrimItem::Path).toString();

    if (fromPathString.isEmpty() || targetPathString.isEmpty()) {
        clearDropIndicator(this);
        event->ignore();
        return;
    }

    const SdfPath fromPath(QStringToString(fromPathString));
    const SdfPath targetPath(QStringToString(targetPathString));

    if (fromPath == targetPath || targetPath.HasPrefix(fromPath)) {
        clearDropIndicator(this);
        event->ignore();
        return;
    }

    const QRect rect = visualItemRect(target);
    const int y = event->position().toPoint().y() - rect.top();
    const int margin = std::max(4, rect.height() / 4);

    int mode = DropOnItem;
    if (y < margin)
        mode = DropAboveItem;
    else if (y > rect.height() - margin)
        mode = DropBelowItem;

    setDropIndicator(this, target, mode);
    event->acceptProposedAction();
}

void
StageTree::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()->hasFormat(kPrimPathMime)) {
        clearDropIndicator(this);
        event->ignore();
        return;
    }

    QTreeWidgetItem* targetItem = itemAt(event->position().toPoint());
    if (!targetItem) {
        clearDropIndicator(this);
        event->ignore();
        return;
    }

    const QString fromPathString = QString::fromUtf8(event->mimeData()->data(kPrimPathMime));
    const QString targetPathString = targetItem->data(0, PrimItem::Path).toString();

    if (fromPathString.isEmpty() || targetPathString.isEmpty()) {
        clearDropIndicator(this);
        event->ignore();
        return;
    }

    const SdfPath fromPath(QStringToString(fromPathString));
    const SdfPath targetPath(QStringToString(targetPathString));
    const int mode = property(kDropModeProperty).toInt();

    SdfPath newParentPath;
    int insertIndex = -1;

    if (mode == DropOnItem) {
        newParentPath = targetPath;
        insertIndex = targetItem->childCount();
    }
    else {
        newParentPath = targetPath.GetParentPath();

        QTreeWidgetItem* parentItem = targetItem->parent();
        if (!parentItem) {
            clearDropIndicator(this);
            event->ignore();
            return;
        }

        const int targetRow = parentItem->indexOfChild(targetItem);
        if (targetRow < 0) {
            clearDropIndicator(this);
            event->ignore();
            return;
        }

        insertIndex = (mode == DropAboveItem) ? targetRow : (targetRow + 1);
    }

    clearDropIndicator(this);

    if (newParentPath.IsEmpty() || newParentPath == SdfPath::AbsoluteRootPath()) {
        event->ignore();
        return;
    }

    if (fromPath == newParentPath || newParentPath.HasPrefix(fromPath)) {
        event->ignore();
        return;
    }

    // Adjust insertion index for same-parent reordering.
    if (fromPath.GetParentPath() == newParentPath) {
        QTreeWidgetItem* sourceItem = nullptr;

        std::function<QTreeWidgetItem*(QTreeWidgetItem*)> findItem = [&](QTreeWidgetItem* item) -> QTreeWidgetItem* {
            if (!item)
                return nullptr;

            if (item->data(0, PrimItem::Path).toString() == fromPathString)
                return item;

            for (int i = 0; i < item->childCount(); ++i) {
                if (QTreeWidgetItem* found = findItem(item->child(i)))
                    return found;
            }
            return nullptr;
        };

        for (int i = 0; i < topLevelItemCount() && !sourceItem; ++i)
            sourceItem = findItem(topLevelItem(i));

        if (sourceItem) {
            QTreeWidgetItem* oldParentItem = sourceItem->parent();
            if (oldParentItem) {
                const int oldRow = oldParentItem->indexOfChild(sourceItem);
                if (oldRow >= 0 && oldRow < insertIndex)
                    --insertIndex;
            }
        }
    }

    if (context())
        context()->run(new Command(movePath(fromPath, newParentPath, insertIndex)));

    event->acceptProposedAction();
}

void
StageTreePrivate::contextMenuEvent(QContextMenuEvent* event)
{
    QList<SdfPath> paths;
    QList<PrimItem*> selectedItems;
    selectedItems.reserve(d.tree->selectedItems().size());

    QStringList pathStrings;
    QStringList nameStrings;

    for (QTreeWidgetItem* selected : d.tree->selectedItems()) {
        PrimItem* primItem = static_cast<PrimItem*>(selected);
        const QString pathString = primItem->data(0, PrimItem::Path).toString();
        if (pathString.isEmpty())
            continue;

        const SdfPath path(qt::QStringToString(pathString));
        paths.append(path);
        selectedItems.append(primItem);
        pathStrings.append(pathString);
        nameStrings.append(qt::StringToQString(path.GetName()));
    }
    if (paths.isEmpty())
        return;

    const QList<SdfPath> topLevelPaths = path::topLevelPaths(paths);

    SdfPath createParentPath;
    if (!topLevelPaths.isEmpty())
        createParentPath = topLevelPaths.first();

    auto payloadPathsAtSelection = [&]() {
        QList<SdfPath> result;
        READ_LOCKER(locker, d.context->stageLock(), "stageLock");
        if (!d.stage)
            return result;
        return stage::payloadPaths(d.stage, topLevelPaths);
    };

    auto payloadPathsInSelection = [&]() {
        QList<SdfPath> result;
        READ_LOCKER(locker, d.context->stageLock(), "stageLock");
        if (!d.stage)
            return result;
        return stage::descendantsPayloadPaths(d.stage, topLevelPaths);
    };

    auto maskContainsCurrentSelection = [&](const QList<SdfPath>& maskPaths, const QList<SdfPath>& selectedPaths) {
        if (maskPaths.isEmpty() || selectedPaths.isEmpty())
            return false;

        for (const SdfPath& selectedPath : selectedPaths) {
            bool found = false;
            for (const SdfPath& maskPath : maskPaths) {
                if (selectedPath == maskPath) {
                    found = true;
                    break;
                }
            }
            if (!found)
                return false;
        }
        return true;
    };

    const bool isolateChecked = maskContainsCurrentSelection(d.maskPaths, paths);

    bool canLoadSelected = false;
    bool canUnloadSelected = false;

    if (d.payloadEnabled) {
        for (PrimItem* item : selectedItems) {
            const Qt::CheckState state = item->checkState(PrimItem::Name);

            if (state == Qt::Unchecked || state == Qt::PartiallyChecked)
                canLoadSelected = true;
            if (state == Qt::Checked || state == Qt::PartiallyChecked)
                canUnloadSelected = true;
        }
    }
    else {
        const QList<SdfPath> exactPayloadPaths = payloadPathsAtSelection();
        canLoadSelected = !exactPayloadPaths.isEmpty();
        canUnloadSelected = !exactPayloadPaths.isEmpty();
    }

    bool canShowSelected = false;
    bool canHideSelected = false;
    {
        READ_LOCKER(locker, d.context->stageLock(), "stageLock");
        if (d.stage) {
            for (const SdfPath& path : paths) {
                const bool visible = stage::isVisible(d.stage, path);
                if (visible)
                    canHideSelected = true;
                else
                    canShowSelected = true;

                if (canShowSelected && canHideSelected)
                    break;
            }
        }
    }

    auto setSelectedCheckState = [&](Qt::CheckState state) {
        for (PrimItem* item : selectedItems)
            item->setCheckState(PrimItem::Name, state);
    };

    auto copyToClipboard = [](const QString& text) {
        if (QClipboard* clipboard = QGuiApplication::clipboard())
            clipboard->setText(text);
    };

    QMenu menu(d.tree.data());
    struct VariantSelection {
        QString setName;
        QString value;
    };

    QMenu* loadVariantMenu = menu.addMenu("Load Variant");
    QMap<QAction*, VariantSelection> variantActions;
    {
        READ_LOCKER(locker, d.context->stageLock(), "stageLock");
        if (!d.stage)
            return;

        const QMap<QString, QList<QString>> variantSets = stage::findVariantSets(d.stage, paths, false);
        for (auto it = variantSets.begin(); it != variantSets.end(); ++it) {
            const QString& setName = it.key();
            const QList<QString>& values = it.value();

            QMenu* setMenu = loadVariantMenu->addMenu(setName);
            for (const QString& value : values) {
                QAction* action = setMenu->addAction(value);
                variantActions[action] = { setName, value };
            }
        }
        if (variantActions.isEmpty())
            loadVariantMenu->setEnabled(false);
    }

    menu.addSeparator();

    QAction* loadSelected = menu.addAction("Load");
    QAction* unloadSelected = menu.addAction("Unload");

    loadSelected->setEnabled(canLoadSelected);
    unloadSelected->setEnabled(canUnloadSelected);

    menu.addSeparator();
    QMenu* showMenu = menu.addMenu("Show");
    QAction* showSelected = showMenu->addAction("Selected");
    QAction* showRecursive = showMenu->addAction("Recursive");
    showSelected->setEnabled(canShowSelected);
    showRecursive->setEnabled(canShowSelected);

    QMenu* hideMenu = menu.addMenu("Hide");
    QAction* hideSelected = hideMenu->addAction("Selected");
    QAction* hideRecursive = hideMenu->addAction("Recursive");
    hideSelected->setEnabled(canHideSelected);
    hideRecursive->setEnabled(canHideSelected);

    menu.addSeparator();
    QAction* isolateAction = menu.addAction("Isolate");
    isolateAction->setCheckable(true);
    isolateAction->setChecked(isolateChecked);

    menu.addSeparator();
    QMenu* copyMenu = menu.addMenu("Copy");
    QAction* copyPath = copyMenu->addAction("Path");
    QAction* copyName = copyMenu->addAction("Name");
    QAction* copyPaths = nullptr;
    QAction* copyNames = nullptr;

    if (paths.size() > 1) {
        copyPaths = copyMenu->addAction("Paths");
        copyNames = copyMenu->addAction("Names");
    }
    
    menu.addSeparator();
    QAction* newXform = menu.addAction("New xform");
    QAction* deleteSelected = menu.addAction("Delete");
    

    QAction* chosen = menu.exec(d.tree->mapToGlobal(event->pos()));
    if (!chosen)
        return;

    if (chosen == copyPath) {
        copyToClipboard(pathStrings.first());
        return;
    }

    if (chosen == copyName) {
        copyToClipboard(nameStrings.first());
        return;
    }

    if (chosen == copyPaths) {
        copyToClipboard(pathStrings.join('\n'));
        return;
    }

    if (chosen == copyNames) {
        copyToClipboard(nameStrings.join('\n'));
        return;
    }

    if (chosen == loadSelected) {
        if (d.payloadEnabled) {
            setSelectedCheckState(Qt::Checked);
        }
        else {
            const QList<SdfPath> payloadPaths = payloadPathsAtSelection();
            if (!payloadPaths.isEmpty())
                d.context->run(new Command(loadPayloads(payloadPaths)));
        }
        return;
    }

    if (chosen == unloadSelected) {
        if (d.payloadEnabled) {
            setSelectedCheckState(Qt::Unchecked);
        }
        else {
            const QList<SdfPath> payloadPaths = payloadPathsAtSelection();
            if (!payloadPaths.isEmpty())
                d.context->run(new Command(unloadPayloads(payloadPaths)));
        }
        return;
    }

    if (variantActions.contains(chosen)) {
        const QList<SdfPath> payloadPaths = payloadPathsInSelection();
        if (!payloadPaths.isEmpty()) {
            const VariantSelection& sel = variantActions[chosen];
            d.context->run(new Command(loadPayloads(payloadPaths, sel.setName, sel.value)));
        }
        return;
    }

    if (chosen == newXform) {
        if (!createParentPath.IsEmpty())
            d.context->run(new Command(newXformPath(createParentPath, "Xform")));
        return;
    }

    if (chosen == showSelected)
        d.context->run(new Command(showPaths(paths, false)));
    else if (chosen == showRecursive)
        d.context->run(new Command(showPaths(paths, true)));
    else if (chosen == hideSelected)
        d.context->run(new Command(hidePaths(paths, false)));
    else if (chosen == hideRecursive)
        d.context->run(new Command(hidePaths(paths, true)));
    else if (chosen == isolateAction)
        d.context->run(new Command(isolatePaths(isolateAction->isChecked() ? paths : QList<SdfPath>())));
    else if (chosen == deleteSelected)
        d.context->run(new Command(deletePaths(paths)));
}

void
StageTreePrivate::updateStage(UsdStageRefPtr stage)
{
    SignalGuard::Scope guard(this);
    close();
    d.stage = stage;
    if (!stage)
        return;

    UsdPrim prim;
    {
        READ_LOCKER(locker, d.context->stageLock(), "stageLock");
        prim = stage->GetPseudoRoot();
    }

    PrimItem* rootItem = new PrimItem(d.tree.data(), stage, prim.GetPath());

    Qt::ItemFlags flags = rootItem->flags();
    flags |= Qt::ItemIsDropEnabled;
    rootItem->setFlags(flags);

    itemCheckState(rootItem, false, false);
    addChildren(rootItem, prim.GetPath());
    initTree();

    if (d.payloadEnabled)
        itemCheckState(rootItem, true, true);
}

namespace {

    QString pathListString(const QList<SdfPath>& paths)
    {
        QStringList values;
        values.reserve(paths.size());
        for (const SdfPath& path : paths)
            values.append(qt::StringToQString(path.GetName()));
        return QString("[%1]").arg(values.join(", "));
    }

    QString childListString(PrimItem* parentItem)
    {
        if (!parentItem)
            return "[]";

        QStringList values;
        values.reserve(parentItem->childCount());

        for (int i = 0; i < parentItem->childCount(); ++i) {
            auto* child = static_cast<PrimItem*>(parentItem->child(i));
            if (!child) {
                values.append("<null>");
                continue;
            }

            const QString path = child->data(0, PrimItem::Path).toString();
            values.append(SdfPath(QStringToString(path)).GetName().c_str());
        }

        return QString("[%1]").arg(values.join(", "));
    }

}  // namespace

void
StageTreePrivate::updatePrims(const QList<SdfPath>& paths, const QList<SdfPath>& invalidated)
{
    SignalGuard::Scope guard(this);
    if (!d.stage || !d.tree)
        return;

    qDebug() << "StageTreePrivate::updatePrims:"
             << "paths" << paths.size() << pathListString(paths) << "invalidated" << invalidated.size()
             << pathListString(invalidated);

    const QList<SdfPath> sortedChanged = sortPathsByDepth(paths);
    const QList<SdfPath> sortedInvalidated = sortPathsByDepth(invalidated);

    d.tree->setUpdatesEnabled(false);

    for (const SdfPath& path : sortedChanged)
        updatePrim(path);

    for (const SdfPath& path : sortedInvalidated)
        invalidatePrim(path);

    d.tree->setUpdatesEnabled(true);
    d.tree->update();
}

void
StageTreePrivate::updatePrim(const SdfPath& path)
{
    const SdfPath primPath = path.IsPropertyPath() ? path.GetPrimPath() : path;
    PrimItem* primItem = itemFromPath(primPath);
    if (!primItem)
        return;

    UsdPrim prim;
    bool isPayload = false;

    {
        READ_LOCKER(locker, d.context->stageLock(), "stageLock");
        if (!d.stage)
            return;

        prim = d.stage->GetPrimAtPath(primPath);
        if (!prim)
            return;

        if (d.payloadEnabled)
            isPayload = stage::isPayload(d.stage, primPath);
    }

    qDebug() << "StageTreePrivate::updatePrim:" << qt::StringToQString(primPath.GetString());

    primItem->invalidate();
    itemCheckState(primItem, d.payloadEnabled, false);

    if (isPayload) {
        const Qt::CheckState want = prim.IsLoaded() ? Qt::Checked : Qt::Unchecked;
        if (primItem->checkState(0) != want)
            primItem->setCheckState(0, want);

        qDebug() << "StageTreePrivate::updatePrim: payload node, clearing children for"
                 << qt::StringToQString(primPath.GetString()) << "loaded" << prim.IsLoaded();

        while (primItem->childCount() > 0)
            delete primItem->child(0);
    }
}

void
StageTreePrivate::invalidatePrimImpl(const SdfPath& path, bool refreshParent)
{
    const SdfPath primPath = path.IsPropertyPath() ? path.GetPrimPath() : path;
    const SdfPath parentPath = primPath.GetParentPath();

    UsdPrim prim;
    PrimItem* primItem = itemFromPath(primPath);

    {
        READ_LOCKER(locker, d.context->stageLock(), "stageLock");
        if (!d.stage)
            return;
        prim = d.stage->GetPrimAtPath(primPath);
    }

    qDebug() << "StageTreePrivate::invalidatePrimImpl:" << qt::StringToQString(primPath.GetString()) << "existsInStage"
             << static_cast<bool>(prim) << "existsInTree" << static_cast<bool>(primItem) << "refreshParent"
             << refreshParent;

    if (!prim) {
        PrimItem* parentItem = itemFromPath(parentPath);

        if (primItem) {
            qDebug() << "StageTreePrivate::invalidatePrimImpl: deleting tree item"
                     << qt::StringToQString(primPath.GetString());
            delete primItem;
        }

        if (refreshParent && parentItem) {
            UsdPrim parentPrim;
            {
                READ_LOCKER(locker, d.context->stageLock(), "stageLock");
                if (d.stage)
                    parentPrim = d.stage->GetPrimAtPath(parentPath);
            }
            if (parentPrim) {
                qDebug() << "StageTreePrivate::invalidatePrimImpl: refreshing parent after delete"
                         << qt::StringToQString(parentPath.GetString());
                invalidateChildren(parentItem, parentPrim);
                parentItem->invalidate();
            }
        }
        return;
    }

    if (!primItem) {
        PrimItem* parentItem = itemFromPath(parentPath);
        if (!parentItem) {
            qDebug() << "StageTreePrivate::invalidatePrimImpl: missing parent item for"
                     << qt::StringToQString(parentPath.GetString());
            return;
        }

        primItem = addItem(parentItem, primPath);
        if (!primItem) {
            qDebug() << "StageTreePrivate::invalidatePrimImpl: failed to add item"
                     << qt::StringToQString(primPath.GetString());
            return;
        }

        qDebug() << "StageTreePrivate::invalidatePrimImpl: added item" << qt::StringToQString(primPath.GetString());

        if (refreshParent) {
            qDebug() << "StageTreePrivate::invalidatePrimImpl: refreshing parent after add"
                     << qt::StringToQString(parentPath.GetString());
            invalidateChildren(parentItem, prim.GetParent());
            parentItem->invalidate();
        }
    }

    invalidateSubtree(primItem, prim);
}

void
StageTreePrivate::invalidateSubtree(PrimItem* item, const UsdPrim& prim)
{
    if (!item || !prim)
        return;

    const SdfPath primPath = prim.GetPath();

    bool isPayload = false;
    {
        READ_LOCKER(locker, d.context->stageLock(), "stageLock");
        if (d.stage && d.payloadEnabled)
            isPayload = stage::isPayload(d.stage, primPath);
    }

    item->invalidate();
    itemCheckState(item, d.payloadEnabled, false);

    if (isPayload) {
        const Qt::CheckState want = prim.IsLoaded() ? Qt::Checked : Qt::Unchecked;
        if (item->checkState(0) != want)
            item->setCheckState(0, want);

        qDebug() << "StageTreePrivate::invalidateSubtree: payload node, clearing children for"
                 << qt::StringToQString(primPath.GetString()) << "loaded" << prim.IsLoaded();

        while (item->childCount() > 0)
            delete item->child(0);

        return;
    }

    invalidateChildren(item, prim);
}

void
StageTreePrivate::invalidatePrim(const SdfPath& path)
{
    invalidatePrimImpl(path, true);
}

void
StageTreePrivate::invalidateChildren(PrimItem* parentItem, const UsdPrim& prim)
{
    if (!parentItem || !prim)
        return;

    const QString parentPathString = parentItem->data(0, PrimItem::Path).toString();

    qDebug() << "StageTreePrivate::invalidateChildren: parent" << parentPathString << "tree before"
             << childListString(parentItem);

    QHash<QString, PrimItem*> existing;
    existing.reserve(parentItem->childCount());

    for (int i = 0; i < parentItem->childCount(); ++i) {
        auto* child = static_cast<PrimItem*>(parentItem->child(i));
        if (!child)
            continue;
        existing.insert(child->data(0, PrimItem::Path).toString(), child);
    }

    QList<SdfPath> ordered;
    QSet<QString> stageSet;

    {
        READ_LOCKER(locker, d.context->stageLock(), "stageLock");
        for (const UsdPrim& childPrim : prim.GetAllChildren()) {
            const SdfPath childPath = childPrim.GetPath();
            const QString key = qt::StringToQString(childPath.GetString());
            ordered.append(childPath);
            stageSet.insert(key);
        }
    }

    qDebug() << "StageTreePrivate::invalidateChildren: stage order for" << parentPathString << pathListString(ordered);

    for (auto it = existing.begin(); it != existing.end(); ++it) {
        if (!stageSet.contains(it.key())) {
            qDebug() << "StageTreePrivate::invalidateChildren: removing stale child" << it.key();
            delete it.value();
        }
    }

    existing.clear();
    existing.reserve(parentItem->childCount());

    for (int i = 0; i < parentItem->childCount(); ++i) {
        auto* child = static_cast<PrimItem*>(parentItem->child(i));
        if (!child)
            continue;
        existing.insert(child->data(0, PrimItem::Path).toString(), child);
    }

    for (const SdfPath& childPath : ordered) {
        const QString key = qt::StringToQString(childPath.GetString());
        if (!existing.contains(key)) {
            qDebug() << "StageTreePrivate::invalidateChildren: adding missing child"
                     << qt::StringToQString(childPath.GetString());
            addItem(parentItem, childPath);
        }
    }

    existing.clear();
    existing.reserve(parentItem->childCount());

    for (int i = 0; i < parentItem->childCount(); ++i) {
        auto* child = static_cast<PrimItem*>(parentItem->child(i));
        if (!child)
            continue;
        existing.insert(child->data(0, PrimItem::Path).toString(), child);
    }

    bool orderMatches = (parentItem->childCount() == ordered.size());
    if (orderMatches) {
        for (int i = 0; i < ordered.size(); ++i) {
            auto* child = static_cast<PrimItem*>(parentItem->child(i));
            const SdfPath treePath = child ? SdfPath(QStringToString(child->data(0, PrimItem::Path).toString()))
                                           : SdfPath();

            if (!child || treePath != ordered[i]) {
                qDebug() << "StageTreePrivate::invalidateChildren: mismatch at index" << i << "tree"
                         << (child ? qt::StringToQString(treePath.GetString()) : QString("<null>")) << "stage"
                         << qt::StringToQString(ordered[i].GetString());
                orderMatches = false;
                break;
            }
        }
    }

    if (!orderMatches) {
        qDebug() << "StageTreePrivate::invalidateChildren: reordering children for" << parentPathString
                 << "tree before reorder" << childListString(parentItem);

        for (int i = 0; i < ordered.size(); ++i) {
            const QString key = qt::StringToQString(ordered[i].GetString());
            PrimItem* childItem = existing.value(key, nullptr);
            if (!childItem)
                continue;

            if (parentItem->child(i) != childItem) {
                qDebug() << "StageTreePrivate::invalidateChildren: move child" << key << "to index" << i;
                parentItem->removeChild(childItem);
                parentItem->insertChild(i, childItem);
            }
        }

        qDebug() << "StageTreePrivate::invalidateChildren: tree after reorder" << childListString(parentItem);
    }
    else {
        qDebug() << "StageTreePrivate::invalidateChildren: order already matches for" << parentPathString;
    }

    for (int i = 0; i < parentItem->childCount(); ++i) {
        auto* childItem = static_cast<PrimItem*>(parentItem->child(i));
        if (!childItem)
            continue;

        const QString childPathString = childItem->data(0, PrimItem::Path).toString();
        if (childPathString.isEmpty())
            continue;

        const SdfPath childPath(QStringToString(childPathString));

        UsdPrim childPrim;
        {
            READ_LOCKER(locker, d.context->stageLock(), "stageLock");
            if (!d.stage)
                return;
            childPrim = d.stage->GetPrimAtPath(childPath);
        }

        if (!childPrim) {
            qDebug() << "StageTreePrivate::invalidateChildren: removing vanished child during subtree refresh"
                     << childPathString;
            delete childItem;
            --i;
            continue;
        }

        invalidateSubtree(childItem, childPrim);
    }
}

void
StageTreePrivate::updateSelection(const QList<SdfPath>& paths)
{
    SignalGuard::Scope guard(this);
    const QSet<SdfPath> selectedSet(paths.begin(), paths.end());

    std::function<void(QTreeWidgetItem*)> selectItems = [&](QTreeWidgetItem* baseItem) {
        PrimItem* primItem = static_cast<PrimItem*>(baseItem);
        const QString pathString = primItem->data(0, PrimItem::Path).toString();
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

PrimItem*
StageTreePrivate::itemFromPath(const SdfPath& path) const
{
    const QString target = qt::StringToQString(path.GetString());

    std::function<PrimItem*(PrimItem*)> find = [&](PrimItem* item) -> PrimItem* {
        if (!item)
            return nullptr;

        if (item->data(0, PrimItem::Path).toString() == target)
            return item;

        for (int i = 0; i < item->childCount(); ++i) {
            if (PrimItem* found = find(static_cast<PrimItem*>(item->child(i))))
                return found;
        }

        return nullptr;
    };

    for (int i = 0; i < d.tree->topLevelItemCount(); ++i) {
        if (PrimItem* found = find(static_cast<PrimItem*>(d.tree->topLevelItem(i))))
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

ViewContext*
StageTree::context() const
{
    return p->d.context;
}

void
StageTree::setContext(ViewContext* context)
{
    if (p->d.context != context)
        p->d.context = context;
}

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
    if (p->d.payloadEnabled != enabled)
        p->d.payloadEnabled = enabled;
}

void
StageTree::updateMask(const QList<SdfPath>& paths)
{
    if (p->d.maskPaths != paths)
        p->d.maskPaths = paths;
}

void
StageTree::updateStage(UsdStageRefPtr stage)
{
    p->updateStage(stage);
}

void
StageTree::updatePrims(const QList<SdfPath>& paths, const QList<SdfPath>& invalidated)
{
    p->updatePrims(paths, invalidated);
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
    QTreeWidget::mouseMoveEvent(event);
}

}  // namespace usdviewer
