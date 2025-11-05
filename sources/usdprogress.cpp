// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/specviz

#include "usdprogress.h"

#include <QPointer>

// generated files
#include "ui_usdprogress.h"

namespace usd {
class ProgressPrivate : public QObject {
    Q_OBJECT
public:
    ProgressPrivate();
    void init();

public:
    QPointer<Progress> dialog;
    QScopedPointer<Ui_Usdprogress> ui;
};

ProgressPrivate::ProgressPrivate() {}

void
ProgressPrivate::init()
{
    ui.reset(new Ui_Usdprogress());
    ui->setupUi(dialog);
}

#include "usdprogress.moc"

Progress::Progress(QWidget* parent)
    : QDialog(parent)
    , p(new ProgressPrivate())
{
    p->dialog = this;
    p->init();
}

Progress::~Progress() {}

void
Progress::loadPathsSubmitted(const QList<pxr::SdfPath>& paths)
{}

void
Progress::loadPathsCompleted(const QList<pxr::SdfPath>& loaded)
{}

void
Progress::onCancel()
{}
}  // namespace usd
