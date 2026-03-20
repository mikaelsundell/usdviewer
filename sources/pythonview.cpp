// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "pythonview.h"
#include "application.h"
#include "pythoninterpreter.h"
#include "style.h"
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
    // connect
    QObject::connect(d.ui->run, &QToolButton::clicked, this, &PythonViewPrivate::run);

    QObject::connect(d.ui->clear, &QToolButton::clicked, this, &PythonViewPrivate::clear);
}

void
PythonViewPrivate::run()
{
    auto* interpreter = pythonInterpreter();  // your accessor
    const QString code = d.ui->pythonEditor->toPlainText().trimmed();
    if (code.isEmpty())
        return;

    // Optional: echo command
    d.ui->log->appendPlainText(">>> " + code);

    const QString result = interpreter->executeScript(code);

    if (!result.isEmpty())
        d.ui->log->appendPlainText(result);

    d.ui->log->appendPlainText("");  // spacing
}

void
PythonViewPrivate::clear()
{
    d.ui->log->clear();
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
