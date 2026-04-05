// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "consolewidget.h"
#include "application.h"
#include "console.h"
#include <QEvent>
#include <QMenu>
#include <QPointer>
#include <QScrollBar>
#include <QTextCursor>

PXR_NAMESPACE_USING_DIRECTIVE

// generated files
#include "ui_consolewidget.h"

namespace usdviewer {

class ConsoleWidgetPrivate : public QObject {
public:
    ConsoleWidgetPrivate();
    void init();
    bool eventFilter(QObject* object, QEvent* event) override;
    void appendText(const QString& text);
    void showLogContextMenu(const QPoint& pos);

public:
    struct Data {
        QScopedPointer<Ui_ConsoleWidget> ui;
        QPointer<ConsoleWidget> view;
    };
    Data d;
};

ConsoleWidgetPrivate::ConsoleWidgetPrivate() {}

void
ConsoleWidgetPrivate::init()
{
    d.ui.reset(new Ui_ConsoleWidget());
    d.ui->setupUi(d.view.data());
    d.ui->log->setReadOnly(true);
    d.ui->log->setPlainText(console()->text());
    d.ui->log->setContextMenuPolicy(Qt::CustomContextMenu);
    d.view->installEventFilter(this);

    connect(d.ui->log, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) { showLogContextMenu(pos); });

    connect(console(), &Console::textAppended, this, [this](const QString& text) { appendText(text); });
}

bool
ConsoleWidgetPrivate::eventFilter(QObject* object, QEvent* event)
{
    if (object == d.view) {
        switch (event->type()) {
        case QEvent::Show:
            if (d.view)
                Q_EMIT d.view->visibilityChanged(true);
            break;
        case QEvent::Hide:
            if (d.view)
                Q_EMIT d.view->visibilityChanged(false);
            break;
        default: break;
        }
    }
    return QObject::eventFilter(object, event);
}

void
ConsoleWidgetPrivate::showLogContextMenu(const QPoint& pos)
{
    if (!d.ui || !d.ui->log)
        return;

    QMenu* menu = d.ui->log->createStandardContextMenu();
    menu->addSeparator();

    QAction* clearAction = menu->addAction("Clear");
    connect(clearAction, &QAction::triggered, this, [this]() {
        if (d.ui && d.ui->log)
            d.ui->log->clear();
    });

    menu->exec(d.ui->log->mapToGlobal(pos));
    delete menu;
}

void
ConsoleWidgetPrivate::appendText(const QString& text)
{
    QScrollBar* scrollBar = d.ui->log->verticalScrollBar();
    const bool atBottom = !scrollBar || scrollBar->value() == scrollBar->maximum();
    d.ui->log->moveCursor(QTextCursor::End);
    d.ui->log->insertPlainText(text);
    if (atBottom)
        d.ui->log->moveCursor(QTextCursor::End);
}

ConsoleWidget::ConsoleWidget(QWidget* parent)
    : QWidget(parent)
    , p(new ConsoleWidgetPrivate())
{
    p->d.view = this;
    p->init();
}

ConsoleWidget::~ConsoleWidget() = default;

}  // namespace usdviewer
