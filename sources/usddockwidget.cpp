// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usddockwidget.h"
#include <QMouseEvent>
#include <QPointer>

namespace usd {
class DockWidgetPrivate : public QObject {
public:
    void init();
    struct Data {
        QPoint dragStart;
        QPointer<DockWidget> widget;
    };
    Data d;
};

void
DockWidgetPrivate::init()
{
    QWidget* title = new QWidget;
    title->setFixedHeight(0);
    title->setStyleSheet("background: transparent;");
    d.widget->setTitleBarWidget(title);
    QPointer<QDockWidget> dock = d.widget;
    QObject::connect(dock, &QDockWidget::topLevelChanged, [dock, title](bool floating) {
        if (floating) {
            dock->setTitleBarWidget(nullptr);
        }
        else {
            dock->setTitleBarWidget(title);
        }
    });
}

DockWidget::DockWidget(QWidget* parent)
    : QDockWidget(parent)
    , p(new DockWidgetPrivate())
{
    p->d.widget = this;
    p->init();
}

DockWidget::~DockWidget() {}

void
DockWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        p->d.dragStart = event->pos();
    }
}

void
DockWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        return;
    }
    if (!isFloating()) {
        setFloating(true);
        move(mapToGlobal(p->d.dragStart));
    }
    move(event->globalPosition().toPoint() - p->d.dragStart);
}
}  // namespace usd
