// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "shelflist.h"
#include "application.h"
#include "mime.h"
#include "qtutils.h"
#include "roles.h"
#include "shelfwidget.h"
#include "style.h"
#include <QApplication>
#include <QBuffer>
#include <QDragEnterEvent>
#include <QImageReader>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QStyledItemDelegate>

namespace usdviewer {

class ShelfListPrivate {
public:
    ShelfListPrivate();
    ~ShelfListPrivate();
    void init();
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const;
    bool hasImageMime(const QMimeData* mime) const;
    QImage imageMimeData(const QMimeData* mime);
    QImage iconImage(const QImage& image);
    QImage centerCrop(const QImage& image);
    QString scriptTitle(const QString& code) const;

public:
    class ShelfItemDelegate : public QStyledItemDelegate {
    public:
        explicit ShelfItemDelegate(QObject* parent = nullptr)
            : QStyledItemDelegate(parent)
        {}
        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            painter->save();

            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);

            const QWidget* widget = opt.widget;
            QStyle* style = widget ? widget->style() : QApplication::style();
            const auto* list = qobject_cast<const ShelfList*>(widget);
            const bool isPressed = list && list->pressedIndex() == index;
            const bool isSelected = (opt.state & QStyle::State_Selected);
            const bool isEnabled = (opt.state & QStyle::State_Enabled);

            const QIcon icon = opt.icon;
            opt.icon = QIcon();
            opt.text.clear();
            if (isPressed)
                opt.state |= QStyle::State_Sunken;

            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

            QRect iconRect = opt.rect.adjusted(2, 2, -2, -2);
            if (isPressed)
                iconRect.translate(0, 1);

            icon.paint(painter, iconRect, Qt::AlignCenter, isEnabled ? QIcon::Normal : QIcon::Disabled,
                       (isSelected || isPressed) ? QIcon::On : QIcon::Off);
            painter->restore();
        }

        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            return QStyledItemDelegate::sizeHint(option, index);
        }
    };
    struct Data {
        QModelIndex pressedIndex;
        QPointer<ShelfList> list;
    };
    Data d;
};

ShelfListPrivate::ShelfListPrivate() {}

ShelfListPrivate::~ShelfListPrivate() {}

void
ShelfListPrivate::init()
{
    d.list->setViewMode(QListView::IconMode);
    d.list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    d.list->setFlow(QListView::LeftToRight);
    d.list->setWrapping(true);
    d.list->setResizeMode(QListView::Adjust);
    d.list->setMovement(QListView::Static);
    d.list->setSpacing(10);
    d.list->setUniformItemSizes(true);
    d.list->setItemDelegate(new ShelfItemDelegate(d.list.data()));
    d.list->setDragDropMode(QAbstractItemView::DragDrop);
    d.list->setDefaultDropAction(Qt::CopyAction);
    d.list->setDragEnabled(true);
    d.list->setAcceptDrops(true);

    d.list->setDropIndicatorShown(true);
    d.list->setSelectionMode(QAbstractItemView::SingleSelection);
    d.list->setContextMenuPolicy(Qt::CustomContextMenu);

    const int iconSize = usdviewer::style()->iconSize(Style::UIScale::Large);
    d.list->setIconSize(QSize(iconSize, iconSize));

    QFont f = d.list->font();
    f.setPointSizeF(usdviewer::style()->fontSize(Style::UIScale::Small) - 2);
    d.list->setFont(f);

    const int pad = 10;
    const int width = iconSize + pad * 2;
    const int height = iconSize + pad * 2;

    d.list->setGridSize(QSize(width, height));
    d.list->viewport()->setAcceptDrops(true);
}

QMimeData*
ShelfListPrivate::mimeData(const QList<QListWidgetItem*>& items) const
{
    if (items.isEmpty())
        return nullptr;

    const QString code = items.front()->data(Qt::UserRole).toString();
    if (code.isEmpty())
        return nullptr;

    auto* mime = new QMimeData();
    mime->setData(mime::script, code.toUtf8());
    mime->setText(code);
    return mime;
}

bool
ShelfListPrivate::hasImageMime(const QMimeData* mime) const
{
    if (!mime)
        return false;

    if (mime->hasImage())
        return true;

    if (mime->hasUrls()) {
        const QList<QUrl> urls = mime->urls();
        for (const QUrl& url : urls) {
            if (!url.isLocalFile())
                continue;

            QImageReader reader(url.toLocalFile());
            if (reader.canRead())
                return true;
        }
    }
    return false;
}

QImage
ShelfListPrivate::imageMimeData(const QMimeData* mime)
{
    if (!mime)
        return QImage();

    if (mime->hasImage()) {
        const QVariant imageData = mime->imageData();
        if (imageData.canConvert<QImage>())
            return qvariant_cast<QImage>(imageData);
    }

    if (mime->hasUrls()) {
        const QList<QUrl> urls = mime->urls();
        for (const QUrl& url : urls) {
            if (!url.isLocalFile())
                continue;

            QImageReader reader(url.toLocalFile());
            const QImage image = reader.read();
            if (!image.isNull())
                return image;
        }
    }

    return QImage();
}

QImage
ShelfListPrivate::iconImage(const QImage& image)
{
    if (image.isNull())
        return QImage();

    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    const int iconSize = usdviewer::style()->iconSize(Style::UIScale::Large);
    const int tilePadding = 10;
    const int logicalSize = iconSize + tilePadding * 2;
    if (logicalSize <= 0)
        return QImage();

    const int physicalSize = qMax(1, qRound(logicalSize * dpr));
    const QImage cropped = centerCrop(image).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage scaled = cropped.scaled(physicalSize, physicalSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    scaled.setDevicePixelRatio(dpr);
    return scaled;
}

QImage
ShelfListPrivate::centerCrop(const QImage& image)
{
    if (image.isNull())
        return QImage();

    const int side = qMin(image.width(), image.height());
    const int x = (image.width() - side) / 2;
    const int y = (image.height() - side) / 2;
    return image.copy(x, y, side, side);
}

QString
ShelfListPrivate::scriptTitle(const QString& code) const
{
    const QStringList lines = code.split('\n');
    for (QString line : lines) {
        line = line.trimmed();
        if (!line.isEmpty()) {
            if (line.startsWith(">>>"))
                line = line.mid(3).trimmed();
            if (line.length() > 12)
                line = line.left(12).trimmed() + "...";
            return line.isEmpty() ? QStringLiteral("Script") : line;
        }
    }
    return QStringLiteral("Script");
}

ShelfList::ShelfList(QWidget* parent)
    : QListWidget(parent)
    , p(new ShelfListPrivate())
{
    p->d.list = this;
    p->init();
}

ShelfList::~ShelfList() {}

QModelIndex
ShelfList::pressedIndex() const
{
    return p->d.pressedIndex;
}

QStringList
ShelfList::mimeTypes() const
{
    return { QString::fromLatin1(mime::script), QStringLiteral("text/plain") };
}

QMimeData*
ShelfList::mimeData(const QList<QListWidgetItem*>& items) const
{
    return p->mimeData(items);
}

Qt::DropActions
ShelfList::supportedDropActions() const
{
    return Qt::CopyAction;
}

void
ShelfList::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(mime::script) || event->mimeData()->hasText()
        || p->hasImageMime(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QListWidget::dragEnterEvent(event);
}

void
ShelfList::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat(mime::script) || event->mimeData()->hasText()
        || p->hasImageMime(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QListWidget::dragMoveEvent(event);
}

void
ShelfList::dropEvent(QDropEvent* event)
{
    p->d.pressedIndex = QModelIndex();
    viewport()->update();

    if (p->hasImageMime(event->mimeData())) {
        QListWidgetItem* targetItem = itemAt(event->position().toPoint());
        if (targetItem) {
            const QImage droppedImage = p->imageMimeData(event->mimeData());
            const QImage normalizedImage = p->iconImage(droppedImage);
            const QByteArray iconBytes = qt::imageToPngBytes(normalizedImage);
            if (!iconBytes.isEmpty()) {
                targetItem->setData(roles::shelf::scriptIcon, iconBytes);
                targetItem->setIcon(qt::pngBytesToIcon(iconBytes));
                viewport()->update();

                if (auto* widget = qobject_cast<ShelfWidget*>(parentWidget()))
                    Q_EMIT widget->changed();
                event->acceptProposedAction();
                return;
            }
        }
    }
    QString code;
    if (event->mimeData()->hasFormat(mime::script))
        code = QString::fromUtf8(event->mimeData()->data(mime::script));
    else if (event->mimeData()->hasText())
        code = event->mimeData()->text();

    code = qt::normalizeNewlines(code).trimmed();
    if (!code.isEmpty()) {
        auto* widget = qobject_cast<ShelfWidget*>(parent());
        widget->addScript(code);
        event->acceptProposedAction();
        return;
    }
    QListWidget::dropEvent(event);
}

void
ShelfList::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        p->d.pressedIndex = indexAt(event->pos());
        viewport()->update();
    }

    QListWidget::mousePressEvent(event);
}

void
ShelfList::mouseMoveEvent(QMouseEvent* event)
{
    if (p->d.pressedIndex.isValid()) {
        const QModelIndex current = indexAt(event->pos());
        if (current != p->d.pressedIndex)
            viewport()->update();
    }
    QListWidget::mouseMoveEvent(event);
}

void
ShelfList::mouseReleaseEvent(QMouseEvent* event)
{
    const QModelIndex pressed = p->d.pressedIndex;
    const QModelIndex released = indexAt(event->pos());

    p->d.pressedIndex = QModelIndex();
    viewport()->update();

    QListWidget::mouseReleaseEvent(event);

    if (event->button() != Qt::LeftButton)
        return;

    if (!pressed.isValid() || pressed != released)
        return;

    QListWidgetItem* item = itemFromIndex(released);
    if (!item)
        return;

    setCurrentItem(item);

    if (auto* widget = qobject_cast<ShelfWidget*>(parentWidget()))
        Q_EMIT widget->itemActivated(item->data(Qt::UserRole).toString());

    event->accept();
}

void
ShelfList::leaveEvent(QEvent* event)
{
    if (p->d.pressedIndex.isValid()) {
        p->d.pressedIndex = QModelIndex();
        viewport()->update();
    }
    QListWidget::leaveEvent(event);
}

}  // namespace usdviewer
