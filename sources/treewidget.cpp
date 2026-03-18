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
            if (index.column() != 0)
                return false;
            if (!(index.flags() & Qt::ItemIsUserCheckable))
                return false;
            if (!index.data(Qt::CheckStateRole).isValid())
                return false;
            if (event->type() != QEvent::MouseButtonRelease)
                return false;

            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() != Qt::LeftButton)
                return false;

            if (!checkRect(option).contains(mouse->pos()))
                return false;

            Qt::CheckState state = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());

            switch (state) {
            case Qt::Unchecked: state = Qt::Checked; break;
            case Qt::Checked:
            case Qt::PartiallyChecked: state = Qt::Unchecked; break;
            }

            model->setData(index, state, Qt::CheckStateRole);
            return true;
        }

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);
            opt.features &= ~QStyleOptionViewItem::HasCheckIndicator;
            opt.backgroundBrush = Qt::NoBrush;
            opt.state &= ~QStyle::State_HasFocus;
            painter->save();

            if (opt.state & QStyle::State_Selected)
                painter->fillRect(opt.rect, style()->color(Style::ColorHighlight));

            const int leftMargin = 8;
            const int rightMargin = 8;
            const int iconSpacing = 6;
            const int textSpacing = 8;
            const int iconSize = style()->iconSize(Style::UISmall);
            const int y = 2;

            QRect contentRect = opt.rect.adjusted(leftMargin, 0, -rightMargin, 0);
            int x = contentRect.left();
            const bool isCheckable = index.column() == 0 && (index.flags() & Qt::ItemIsUserCheckable)
                                     && index.data(Qt::CheckStateRole).isValid();
            if (isCheckable) {
                const QRect cbRect(x, contentRect.center().y() - iconSize / 2 + y, iconSize, iconSize);
                painter->setBrush(style()->color(Style::ColorBaseAlt));
                painter->setPen(style()->color(Style::ColorBorderAlt));
                painter->drawRect(cbRect.adjusted(0, 0, -1, -1));

                const Qt::CheckState state = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
                if (state == Qt::Checked) {
                    const QIcon checked = style()->icon(Style::IconChecked, Style::UISmall);
                    painter->drawPixmap(cbRect.topLeft(), checked.pixmap(iconSize, iconSize));
                }
                else if (state == Qt::PartiallyChecked) {
                    const QIcon partial = style()->icon(Style::IconPartiallyChecked, Style::UISmall);
                    painter->drawPixmap(cbRect.topLeft(), partial.pixmap(iconSize, iconSize));
                }
                x = cbRect.right() + 1 + iconSpacing;
            }

            if (!opt.icon.isNull()) {
                const QRect iconRect(x, contentRect.center().y() - iconSize / 2 + y, iconSize, iconSize);
                opt.icon.paint(painter, iconRect, Qt::AlignCenter,
                               (opt.state & QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled);

                x = iconRect.right() + 1 + textSpacing;
            }
            QRect textRect = contentRect;
            textRect.setLeft(x);

            const bool active = index.data(TreeItem::ItemActive).toBool();
            QColor textColor = style()->color(Style::ColorText);
            if (!active)
                textColor = style()->color(Style::ColorTextDisabled);

            painter->setPen(textColor);
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
}

TreeWidget::~TreeWidget() = default;

void
TreeWidget::drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
    bool alternatingRow = p->visualRowIndex(index) % 2 == 1;
    QColor bg = alternatingRow ? app()->style()->color(Style::ColorBaseAlt) : app()->style()->color(Style::ColorBase);
    if (selectionModel() && selectionModel()->isSelected(index)) {
        bg = app()->style()->color(Style::ColorHighlight);
    }
    else {
        QTreeWidgetItem* item = itemFromIndex(index);
        if (item && p->hasSelectedChildren(item)) {
            bg = app()->style()->color(Style::ColorHighlightAlt);
        }
    }
    painter->fillRect(rect, bg);
    QTreeWidget::drawBranches(painter, rect, index);
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

}  // namespace usdviewer
