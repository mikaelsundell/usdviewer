// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "treewidget.h"
#include "application.h"
#include "style.h"
#include <QPainter>
#include <QPointer>

namespace usd {
class TreeWidgetPrivate {
public:
    TreeWidgetPrivate();
    bool hasSelectedChildren(QTreeWidgetItem* item) const;
    int visualRowIndex(const QModelIndex& index) const;
    struct Data {
        QPointer<TreeWidget> tree;
    };
    Data d;
};

TreeWidgetPrivate::TreeWidgetPrivate() {}

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

    setIndentation(20);
}

TreeWidget::~TreeWidget() = default;

void
TreeWidget::drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
    QRect tighterRect = rect;
    tighterRect.translate(4, 0);
    QTreeWidget::drawBranches(painter, tighterRect, index);
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
    QColor bg = alternatingRow ? app()->style()->color(Style::ColorBaseAlt) : app()->style()->color(Style::ColorBase);

    painter->fillRect(rowRect, bg);

    if (opt.state & QStyle::State_Selected) {
        painter->fillRect(rowRect, app()->style()->color(Style::ColorHighlight));
    }
    QTreeWidgetItem* item = itemFromIndex(index);
    if (item && p->hasSelectedChildren(item) && !(opt.state & QStyle::State_Selected)) {
        painter->fillRect(rowRect, app()->style()->color(Style::ColorHighlightAlt));
    }
    QTreeWidget::drawRow(painter, opt, index);
}

}  // namespace usd
