// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/specviz

#include "usdprogress.h"

#include <QFileInfo>
#include <QPointer>
#include <QTimer>

// generated files
#include "ui_usdprogress.h"

namespace usd {
class ProgressPrivate : public QObject {
    Q_OBJECT
public:
    ProgressPrivate();
    void init();
    void loadPathsSubmitted(const QList<SdfPath>& paths);
    void loadPathFailed(const SdfPath& path);
    void loadPathCompleted(const SdfPath& path);

public:
    struct Data {
        qsizetype total = 0;
        qsizetype completed = 0;
        QPointer<Progress> dialog;
        QScopedPointer<Ui_Usdprogress> ui;
    };
    Data d;
};

ProgressPrivate::ProgressPrivate() {}

void
ProgressPrivate::init()
{
    d.ui.reset(new Ui_Usdprogress());
    d.ui->setupUi(d.dialog.data());
    d.ui->status->setColumnCount(2);
    d.ui->status->setHeaderLabels(QStringList() << "Name"
                                                << "Status"
                                                << "Path");
    d.ui->status->setColumnWidth(0, 300);
    d.ui->status->setColumnWidth(1, 60);
    d.ui->status->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    d.ui->progress->setValue(0);
    d.ui->label->setText("");
    // connect
    connect(d.ui->close, &QPushButton::clicked, this, [=]() { d.dialog->accept(); });
}

void
ProgressPrivate::loadPathsSubmitted(const QList<SdfPath>& paths)
{
    d.total = paths.size();
    d.completed = 0;

    d.ui->status->clear();
    d.ui->progress->setValue(0);
    d.ui->label->setText(QString("Loading %1 prim(s)...").arg(d.total));

    for (const SdfPath& path : paths) {
        auto* item = new QTreeWidgetItem(d.ui->status);
        item->setText(0, QString::fromStdString(path.GetName()));
        item->setText(1, "Queued");
        item->setText(2, QString::fromStdString(path.GetString()));
    }
    d.dialog->show();
}

void
ProgressPrivate::loadPathCompleted(const SdfPath& path)
{
    QString str = QString::fromStdString(path.GetString());
    auto matches = d.ui->status->findItems(str, Qt::MatchExactly, 2);
    if (!matches.isEmpty()) {
        matches.first()->setText(1, "Loaded");
    }
    d.completed++;
    int progress = static_cast<int>((float)d.completed / std::max(1, (int)d.total) * 100.0f);
    d.ui->progress->setValue(progress);

    QString name = QString::fromStdString(path.GetName());
    d.ui->label->setText(QString("Loaded: %1").arg(name));

    if (d.completed >= d.total) {
        d.ui->label->setText("All paths loaded successfully");
        //QTimer::singleShot(1000, this, [this]() { d.dialog->accept(); });
    }
}

void
ProgressPrivate::loadPathFailed(const SdfPath& path)
{
    QString str = QString::fromStdString(path.GetString());
    auto matches = d.ui->status->findItems(str, Qt::MatchExactly, 2);
    if (!matches.isEmpty()) {
        matches.first()->setText(1, "Fauked");
    }
}

#include "usdprogress.moc"

Progress::Progress(QWidget* parent)
    : QDialog(parent)
    , p(new ProgressPrivate())
{
    p->d.dialog = this;
    p->init();
}

Progress::~Progress() {}

void
Progress::loadPathsSubmitted(const QList<SdfPath>& paths)
{
    p->loadPathsSubmitted(paths);
}

void
Progress::loadPathCompleted(const SdfPath& path)
{
    p->loadPathCompleted(path);
}

void
Progress::loadPathFailed(const SdfPath& path)
{
    p->loadPathFailed(path);
}

void
Progress::cancel()
{
    reject();
}

}  // namespace usd
