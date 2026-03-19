// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "treewidget.h"
#include "application.h"
#include "style.h"
#include "treeitem.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QStyledItemDelegate>

namespace usdviewer {
class TreeWidgetPrivate {
public:
    TreeWidgetPrivate();
    void init();
    bool hasSelectedChildren(QTreeWidgetItem* item) const;
    int visualRowIndex(const QModelIndex& index) const;
    QRect branchRect(const QRect& rect, const QModelIndex& index) const
    {
        const int indent = d.tree->indentation();
        const int depth = indexDepth(index);
        const int size = style()->iconSize(Style::UIScale::Medium) - 4;
        int x = 6;
        int y = (rect.center().y() - size / 2) + 1;
        return QRect(x, y, size, size);
    }
    int indexDepth(const QModelIndex& index) const
    {
        int depth = 0;
        QModelIndex p = index.parent();
        while (p.isValid()) {
            ++depth;
            p = p.parent();
        }
        return depth;
    }

public:
    class ItemDelegate : public QStyledItemDelegate {
    public:
        ItemDelegate(QObject* parent = nullptr)
            : QStyledItemDelegate(parent)
        {}

        QRect checkRect(const QStyleOptionViewItem& opt) const
        {
            const int indicatorSize = 16;
            const int leftMargin = 12;
            const int yOffset = 2;

            return QRect(opt.rect.left() + leftMargin, opt.rect.center().y() - indicatorSize / 2 + yOffset,
                         indicatorSize, indicatorSize);
        }

        bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                         const QModelIndex& index) override
        {
            // --- HANDLE CHECKBOX (existing behavior) ---
            if (index.column() == 0 && (index.flags() & Qt::ItemIsUserCheckable)
                && index.data(Qt::CheckStateRole).isValid()) {
                if (event->type() == QEvent::MouseButtonRelease) {
                    auto* mouse = static_cast<QMouseEvent*>(event);

                    if (mouse->button() == Qt::LeftButton && checkRect(option).contains(mouse->pos())) {
                        Qt::CheckState state = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());

                        switch (state) {
                        case Qt::Unchecked: state = Qt::Checked; break;
                        case Qt::Checked:
                        case Qt::PartiallyChecked: state = Qt::Unchecked; break;
                        }

                        model->setData(index, state, Qt::CheckStateRole);
                        return true;
                    }
                }
            }
            if (index.column() == 0 && event->type() == QEvent::MouseButtonDblClick) {
                auto* mouse = static_cast<QMouseEvent*>(event);

                QStyleOptionViewItem opt(option);
                initStyleOption(&opt, index);

                const int leftMargin = 8;
                const int rightMargin = 8;
                const int iconSpacing = 6;
                const int textSpacing = 8;
                const int iconSize = style()->iconSize(Style::UIScale::Small);
                const int xOffset = 10;

                QRect contentRect = opt.rect.adjusted(leftMargin, 0, -rightMargin, 0);
                int x = contentRect.left() + xOffset;

                const bool isCheckable = (index.flags() & Qt::ItemIsUserCheckable)
                                         && index.data(Qt::CheckStateRole).isValid();

                if (isCheckable) {
                    QRect cb(x, contentRect.center().y() - iconSize / 2, iconSize, iconSize);
                    x = cb.right() + 1 + iconSpacing;
                }

                if (!opt.icon.isNull()) {
                    QRect iconRect(x, contentRect.center().y() - iconSize / 2, iconSize, iconSize);
                    x = iconRect.right() + 1 + textSpacing;
                }

                QRect textRect = contentRect;
                textRect.setLeft(x);
                if (!textRect.contains(mouse->pos())) {
                    return true;  // swallow → no edit
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
            painter->save();

            const int leftMargin = 8;
            const int rightMargin = 8;
            const int iconSpacing = 6;
            const int textSpacing = 8;
            const int iconSize = style()->iconSize(Style::UIScale::Small);
            const int xOffset = 10;
            const int yOffset = 2;

            QRect contentRect = opt.rect.adjusted(leftMargin, 0, -rightMargin, 0);
            int x = contentRect.left() + xOffset;
            const bool isCheckable = index.column() == 0 && (index.flags() & Qt::ItemIsUserCheckable)
                                     && index.data(Qt::CheckStateRole).isValid();
            if (isCheckable) {
                const QRect cbRect(x, contentRect.center().y() - iconSize / 2 + yOffset, iconSize, iconSize);
                painter->setBrush(style()->color(Style::ColorRole::BaseAlt));
                painter->setPen(style()->color(Style::ColorRole::BorderAlt));
                painter->drawRect(cbRect.adjusted(0, 0, -1, -1));

                const Qt::CheckState state = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
                if (state == Qt::Checked) {
                    const QIcon checked = style()->icon(Style::IconRole::Checked, Style::UIScale::Small);
                    painter->drawPixmap(cbRect.topLeft(), checked.pixmap(iconSize, iconSize));
                }
                else if (state == Qt::PartiallyChecked) {
                    const QIcon partial = style()->icon(Style::IconRole::PartiallyChecked, Style::UIScale::Small);
                    painter->drawPixmap(cbRect.topLeft(), partial.pixmap(iconSize, iconSize));
                }
                x = cbRect.right() + 1 + iconSpacing;
            }
            if (!opt.icon.isNull()) {
                const QRect iconRect(x, contentRect.center().y() - iconSize / 2 + yOffset, iconSize, iconSize);
                opt.icon.paint(painter, iconRect, Qt::AlignCenter,
                               (opt.state & QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled);
                x = iconRect.right() + 1 + textSpacing;
            }
            QRect textRect = contentRect;
            textRect.setLeft(x);
            painter->setPen(opt.palette.color(QPalette::Text));
            painter->setFont(opt.font);
            painter->drawText(textRect, opt.displayAlignment | Qt::AlignVCenter,
                              opt.fontMetrics.elidedText(opt.text, Qt::ElideRight, textRect.width()));
            painter->restore();
        }
    };
    struct Data {
        QPointer<ItemDelegate> delegate;
        QPointer<TreeWidget> tree;
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
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* mouse = static_cast<QMouseEvent*>(event);

        if (mouse->button() != Qt::LeftButton)
            return QTreeWidget::viewportEvent(event);

        const QModelIndex index = indexAt(mouse->pos());
        if (!index.isValid())
            return QTreeWidget::viewportEvent(event);

        if (index.column() != 0)
            return QTreeWidget::viewportEvent(event);

        const QRect vr = visualRect(index);
        const QRect br = p->branchRect(vr, index);

        // 🔒 Strict hit test: branch ONLY
        if (br.contains(mouse->pos())) {
            // Avoid redundant work
            if (model()->hasChildren(index)) {
                const bool expanded = isExpanded(index);
                if (expanded)
                    collapse(index);
                else
                    expand(index);
            }
            event->accept();
            return true;
        }
    }
    return QTreeWidget::viewportEvent(event);
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

    QTreeWidget::drawRow(painter, opt, index);
}

}  // namespace usdviewer
