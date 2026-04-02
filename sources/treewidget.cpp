// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "treewidget.h"
#include "application.h"
#include "style.h"
#include "treeitem.h"
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QStyledItemDelegate>

namespace usdviewer {

namespace {
    static constexpr const char* kDropItemPtrProperty = "_usdviewer_drop_item_ptr";
    static constexpr const char* kDropModeProperty = "_usdviewer_drop_mode";

    enum DropMode { DropNone = 0, DropAboveItem = 1, DropOnItem = 2, DropBelowItem = 3 };
}  // namespace

class TreeWidgetPrivate {
public:
    TreeWidgetPrivate();
    void init();
    bool hasSelectedChildren(QTreeWidgetItem* item) const;
    int visualRowIndex(const QModelIndex& index) const;
    QRect branchRect(const QRect& rect, const QModelIndex& index) const;

public:
    class ItemDelegate : public QStyledItemDelegate {
    public:
        struct Layout {
            QRect contentRect;
            QRect checkRect;
            QRect checkHitRect;
            QRect iconRect;
            QRect textRect;
            bool isCheckable = false;
        };

        ItemDelegate(QObject* parent = nullptr)
            : QStyledItemDelegate(parent)
        {}

        Layout layout(const QStyleOptionViewItem& option, const QModelIndex& index) const
        {
            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);

            const int leftMargin = 8;
            const int rightMargin = 8;
            const int iconSpacing = 6;
            const int textSpacing = 8;
            const int iconSize = style()->iconSize(Style::UIScale::Small);
            const int xOffset = 10;
            const int yOffset = 2;

            Layout l;
            l.contentRect = opt.rect.adjusted(leftMargin, 0, -rightMargin, 0);

            int x = l.contentRect.left() + xOffset;

            l.isCheckable = index.column() == 0 && (index.flags() & Qt::ItemIsUserCheckable)
                            && index.data(Qt::CheckStateRole).isValid();

            if (l.isCheckable) {
                l.checkRect = QRect(x, l.contentRect.center().y() - iconSize / 2 + yOffset, iconSize, iconSize);
                l.checkHitRect = l.checkRect.adjusted(3, 1, -2, -1);
                x = l.checkRect.right() + 1 + iconSpacing;
            }
            if (!opt.icon.isNull()) {
                l.iconRect = QRect(x, l.contentRect.center().y() - iconSize / 2 + yOffset, iconSize, iconSize);
                x = l.iconRect.right() + 1 + textSpacing;
            }
            l.textRect = l.contentRect;
            l.textRect.setLeft(x);
            return l;
        }

        bool hitCheckbox(const QStyleOptionViewItem& option, const QModelIndex& index, const QPoint& pos) const
        {
            const Layout l = layout(option, index);
            return l.isCheckable && l.checkHitRect.contains(pos);
        }

        bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                         const QModelIndex& index) override
        {
            Q_UNUSED(model);

            const bool isCheckable = index.column() == 0 && (index.flags() & Qt::ItemIsUserCheckable)
                                     && index.data(Qt::CheckStateRole).isValid();

            if (!isCheckable) {
                return QStyledItemDelegate::editorEvent(event, model, option, index);
            }

            if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
                auto* mouse = static_cast<QMouseEvent*>(event);

                if (mouse->button() == Qt::LeftButton && hitCheckbox(option, index, mouse->pos())) {
                    return true;
                }
                return false;
            }
            if (event->type() == QEvent::MouseButtonDblClick) {
                auto* mouse = static_cast<QMouseEvent*>(event);
                const Layout l = layout(option, index);

                if (!l.textRect.contains(mouse->pos())) {
                    return true;
                }
            }
            return QStyledItemDelegate::editorEvent(event, model, option, index);
        }

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);
            opt.features &= ~QStyleOptionViewItem::HasCheckIndicator;
            opt.backgroundBrush = Qt::NoBrush;
            opt.state &= ~QStyle::State_HasFocus;

            const Layout l = layout(option, index);
            const int iconSize = style()->iconSize(Style::UIScale::Small);

            painter->save();

            if (l.isCheckable) {
                painter->setBrush(style()->color(Style::ColorRole::BaseAlt));
                painter->setPen(style()->color(Style::ColorRole::BorderAlt));
                painter->drawRect(l.checkRect.adjusted(0, 0, -1, -1));

                const Qt::CheckState state = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
                if (state == Qt::Checked) {
                    const QIcon checked = style()->icon(Style::IconRole::Checked, Style::UIScale::Small);
                    painter->drawPixmap(l.checkRect.topLeft(), checked.pixmap(iconSize, iconSize));
                }
                else if (state == Qt::PartiallyChecked) {
                    const QIcon partial = style()->icon(Style::IconRole::PartiallyChecked, Style::UIScale::Small);
                    painter->drawPixmap(l.checkRect.topLeft(), partial.pixmap(iconSize, iconSize));
                }
            }

            if (!l.iconRect.isNull()) {
                opt.icon.paint(painter, l.iconRect, Qt::AlignCenter,
                               (opt.state & QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled);
            }

            painter->setPen(opt.palette.color(QPalette::Text));
            painter->setFont(opt.font);
            painter->drawText(l.textRect, opt.displayAlignment | Qt::AlignVCenter,
                              opt.fontMetrics.elidedText(opt.text, Qt::ElideRight, l.textRect.width()));

            painter->restore();
        }
    };

    struct Data {
        QPointer<ItemDelegate> delegate;
        QPointer<TreeWidget> tree;
        bool suppressNextSelection = false;
    };
    Data d;
};

TreeWidgetPrivate::TreeWidgetPrivate() {}

void
TreeWidgetPrivate::init()
{
    d.delegate = new ItemDelegate(d.tree.data());
    d.tree->setItemDelegate(d.delegate);
}

bool
TreeWidgetPrivate::hasSelectedChildren(QTreeWidgetItem* item) const
{
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem* child = item->child(i);
        if (child->isSelected() || hasSelectedChildren(child)) {
            return true;
        }
    }
    return false;
}

int
TreeWidgetPrivate::visualRowIndex(const QModelIndex& index) const
{
    int row = 0;
    QModelIndex current = index;
    while (true) {
        QModelIndex above = d.tree->indexAbove(current);
        if (!above.isValid())
            break;
        current = above;
        ++row;
    }
    return row;
}

QRect
TreeWidgetPrivate::branchRect(const QRect& rect, const QModelIndex& index) const
{
    Q_UNUSED(index);
    const int size = style()->iconSize(Style::UIScale::Medium) - 4;
    int x = 6;
    int y = (rect.center().y() - size / 2) + 1;
    return QRect(x, y, size, size);
}

TreeWidget::TreeWidget(QWidget* parent)
    : QTreeWidget(parent)
    , p(new TreeWidgetPrivate())
{
    p->d.tree = this;
    p->init();

    setIndentation(12);
}

TreeWidget::~TreeWidget() = default;

bool
TreeWidget::viewportEvent(QEvent* event)
{
    if (event->type() == QEvent::DragLeave) {
        setProperty(kDropItemPtrProperty, QVariant::fromValue<qulonglong>(0));
        setProperty(kDropModeProperty, DropNone);
        viewport()->update();
    }

    return QTreeWidget::viewportEvent(event);
}

void
TreeWidget::mousePressEvent(QMouseEvent* event)
{
    p->d.suppressNextSelection = false;

    if (event->button() == Qt::LeftButton) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid() && index.column() == 0) {
            const QRect vr = visualRect(index);
            const QRect br = p->branchRect(vr, index);

            if (br.contains(event->pos())) {
                p->d.suppressNextSelection = true;
                event->accept();
                return;
            }

            if (p->d.delegate) {
                QStyleOptionViewItem opt;
                initViewItemOption(&opt);
                opt.rect = vr;

                if (p->d.delegate->hitCheckbox(opt, index, event->pos())) {
                    p->d.suppressNextSelection = true;
                    event->accept();
                    return;
                }
            }
        }
    }

    QTreeWidget::mousePressEvent(event);
}

void
TreeWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid() && index.column() == 0) {
            const QRect vr = visualRect(index);
            const QRect br = p->branchRect(vr, index);

            if (br.contains(event->pos())) {
                if (model()->hasChildren(index)) {
                    if (isExpanded(index))
                        collapse(index);
                    else
                        expand(index);
                }
                p->d.suppressNextSelection = false;
                event->accept();
                return;
            }

            if (p->d.delegate) {
                QStyleOptionViewItem opt;
                initViewItemOption(&opt);
                opt.rect = vr;

                if (p->d.delegate->hitCheckbox(opt, index, event->pos())) {
                    Qt::CheckState state = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());

                    switch (state) {
                    case Qt::Unchecked: state = Qt::Checked; break;
                    case Qt::Checked:
                    case Qt::PartiallyChecked: state = Qt::Unchecked; break;
                    }

                    model()->setData(index, state, Qt::CheckStateRole);
                    p->d.suppressNextSelection = false;
                    event->accept();
                    return;
                }
            }
        }
    }
    p->d.suppressNextSelection = false;
    QTreeWidget::mouseReleaseEvent(event);
}

QItemSelectionModel::SelectionFlags
TreeWidget::selectionCommand(const QModelIndex& index, const QEvent* event) const
{
    if (event && index.isValid() && index.column() == 0) {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease
            || event->type() == QEvent::MouseButtonDblClick) {
            const auto* mouse = static_cast<const QMouseEvent*>(event);
            const QRect vr = visualRect(index);
            const QRect br = p->branchRect(vr, index);

            if (br.contains(mouse->pos())) {
                return QItemSelectionModel::NoUpdate;
            }

            if (p->d.delegate) {
                QStyleOptionViewItem opt;
                const_cast<TreeWidget*>(this)->initViewItemOption(&opt);
                opt.rect = vr;

                if (p->d.delegate->hitCheckbox(opt, index, mouse->pos())) {
                    return QItemSelectionModel::NoUpdate;
                }
            }
        }
    }
    return QTreeWidget::selectionCommand(index, event);
}

void
TreeWidget::drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
    Q_UNUSED(rect);

    const bool hasChildren = model()->hasChildren(index);
    if (!hasChildren)
        return;

    const bool expanded = isExpanded(index);
    QRect vr = visualRect(index);
    QRect r = p->branchRect(vr, index);
    QIcon icon = expanded ? app()->style()->icon(Style::IconRole::BranchOpen, Style::UIScale::Medium)
                          : app()->style()->icon(Style::IconRole::BranchClosed, Style::UIScale::Medium);
    icon.paint(painter, r, Qt::AlignCenter);
}

void
TreeWidget::drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt(option);
    opt.features &= ~QStyleOptionViewItem::Alternate;
    opt.backgroundBrush = Qt::NoBrush;

    QRect rowRect = visualRect(index.siblingAtColumn(0));
    rowRect.setLeft(0);
    rowRect.setRight(viewport()->width());

    bool alternatingRow = p->visualRowIndex(index) % 2 == 1;
    QColor bg = alternatingRow ? app()->style()->color(Style::ColorRole::BaseAlt)
                               : app()->style()->color(Style::ColorRole::Base);

    painter->fillRect(rowRect, bg);
    if (opt.state & QStyle::State_Selected) {
        painter->fillRect(rowRect, app()->style()->color(Style::ColorRole::Highlight));
    }
    QTreeWidgetItem* item = itemFromIndex(index);
    const bool selected = selectionModel() && selectionModel()->isSelected(index);
    if (selected) {
        painter->fillRect(rowRect, app()->style()->color(Style::ColorRole::Highlight));
    }
    else if (item && p->hasSelectedChildren(item)) {
        painter->fillRect(rowRect, app()->style()->color(Style::ColorRole::HighlightAlt));
    }

    const qulonglong dropItemPtr = property(kDropItemPtrProperty).toULongLong();
    const int dropMode = property(kDropModeProperty).toInt();

    if (item && dropItemPtr != 0 && reinterpret_cast<qulonglong>(item) == dropItemPtr) {
        painter->save();

        QPen pen(QColor(255, 80, 80));
        pen.setWidth(2);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        switch (dropMode) {
        case DropAboveItem: painter->drawLine(rowRect.left(), rowRect.top(), rowRect.right(), rowRect.top()); break;
        case DropBelowItem:
            painter->drawLine(rowRect.left(), rowRect.bottom(), rowRect.right(), rowRect.bottom());
            break;
        case DropOnItem: painter->drawRect(rowRect.adjusted(1, 1, -2, -2)); break;
        default: break;
        }

        painter->restore();
    }

    QTreeWidget::drawRow(painter, opt, index);
}

}  // namespace usdviewer
