// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "pythonview.h"
#include "application.h"
#include "pythoninterpreter.h"
#include "session.h"
#include "style.h"
#include <QMenu>
#include <QPointer>

// generated files
#include "ui_pythonview.h"

namespace usdviewer {

class PythonViewPrivate : public QObject {
public:
    void init();

public Q_SLOTS:
    void run();
    void clear();
    void stageChanged(UsdStageRefPtr stage, Session::LoadPolicy policy, Session::StageStatus status);
    void showLogContextMenu(const QPoint& pos);

public:
    struct Data {
        QScopedPointer<Ui_PythonView> ui;
        QPointer<PythonView> view;
    };
    Data d;
};

void
PythonViewPrivate::init()
{
    d.ui.reset(new Ui_PythonView());
    d.ui->setupUi(d.view.data());
    // actions
    d.ui->run->setIcon(style()->icon(Style::IconRole::Run));
    d.ui->clear->setIcon(style()->icon(Style::IconRole::Clear));
    d.ui->log->setContextMenuPolicy(Qt::CustomContextMenu);
    // connect
    QObject::connect(d.ui->run, &QToolButton::clicked, this, &PythonViewPrivate::run);
    QObject::connect(d.ui->clear, &QToolButton::clicked, this, &PythonViewPrivate::clear);
    QObject::connect(d.ui->pythonEditor->document(), &QTextDocument::contentsChanged, this,
                     [this]() { d.ui->clear->setEnabled(!d.ui->pythonEditor->document()->isEmpty()); });
    QObject::connect(d.ui->log, &QWidget::customContextMenuRequested, this, &PythonViewPrivate::showLogContextMenu);
    QObject::connect(session(), &Session::stageChanged, this, &PythonViewPrivate::stageChanged);
}

void
PythonViewPrivate::stageChanged(UsdStageRefPtr stage, Session::LoadPolicy policy, Session::StageStatus status)
{
    Q_UNUSED(stage);
    Q_UNUSED(policy);
    const bool enabled = (status == Session::StageStatus::Loaded);
    d.ui->run->setEnabled(enabled);
    d.ui->pythonEditor->setEnabled(enabled);
    d.ui->log->setEnabled(enabled);
    d.ui->clear->setEnabled(enabled && !d.ui->pythonEditor->toPlainText().isEmpty());
}

void
PythonViewPrivate::showLogContextMenu(const QPoint& pos)
{
    QMenu* menu = d.ui->log->createStandardContextMenu();
    menu->addSeparator();
    QAction* clearAction = menu->addAction(style()->icon(Style::IconRole::Clear), tr("Clear"));
    clearAction->setEnabled(!d.ui->log->toPlainText().isEmpty());
    QObject::connect(clearAction, &QAction::triggered, this, [this]() { d.ui->log->clear(); });
    menu->exec(d.ui->log->viewport()->mapToGlobal(pos));
    delete menu;
}

void
PythonViewPrivate::run()
{
    auto* interpreter = pythonInterpreter();
    const QString code = d.ui->pythonEditor->toPlainText().trimmed();
    if (code.isEmpty())
        return;
    d.ui->log->appendPlainText(">>> " + code);

    const QString result = interpreter->executeScript(code);
    if (!result.isEmpty())
        d.ui->log->appendPlainText(result);
    d.ui->log->appendPlainText("");
    d.ui->clear->setEnabled(!d.ui->log->toPlainText().isEmpty());
}

void
PythonViewPrivate::clear()
{
    d.ui->pythonEditor->clear();
    d.ui->clear->setEnabled(false);
}

PythonView::PythonView(QWidget* parent)
    : QWidget(parent)
    , p(new PythonViewPrivate())
{
    p->d.view = this;
    p->init();
}

PythonView::~PythonView() = default;

}  // namespace usdviewer
