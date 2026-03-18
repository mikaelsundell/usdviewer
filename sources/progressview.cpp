// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "progressview.h"
#include "application.h"
#include "qtutils.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QPointer>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/usd/primCompositionQuery.h>

// generated files
#include "ui_progressview.h"

namespace usdviewer {
class ProgressViewPrivate : public QObject {
public:
    void init();
    QTreeWidget* progressTree();
    bool eventFilter(QObject* obj, QEvent* event);

public Q_SLOTS:
    void cancel();
    void clear();
    void progressBlockChanged(const QString& name, DataModel::ProgressMode mode);
    void progressNotifyChanged(const DataModel::Notify& notify, size_t completed, size_t expected);
    void selectionChanged(const QList<SdfPath>& paths);
    void stageChanged(UsdStageRefPtr stage, DataModel::LoadPolicy policy, DataModel::StageStatus status);

public:
    QString updateStatus(size_t completed, size_t expected);
    struct Data {
        UsdStageRefPtr stage;
        int expectedCount = 0;
        bool running = false;
        QElapsedTimer timer;
        QScopedPointer<Ui_ProgressView> ui;
        QPointer<ProgressView> view;
    };
    Data d;
};

void
ProgressViewPrivate::init()
{
    d.ui.reset(new Ui_ProgressView());
    d.ui->setupUi(d.view.data());
    // event filter
    progressTree()->installEventFilter(this);
    // payload tree
    progressTree()->setHeaderLabels(QStringList() << "Name"
                                                  << "Paths");
    // connect
    connect(d.ui->clear, &QPushButton::clicked, this, &ProgressViewPrivate::clear);
    connect(dataModel(), &DataModel::progressBlockChanged, this, &ProgressViewPrivate::progressBlockChanged);
    connect(dataModel(), &DataModel::progressNotifyChanged, this, &ProgressViewPrivate::progressNotifyChanged);
    connect(dataModel(), &DataModel::stageChanged, this, &ProgressViewPrivate::stageChanged);
    connect(selectionModel(), &SelectionModel::selectionChanged, this, &ProgressViewPrivate::selectionChanged);
}

QTreeWidget*
ProgressViewPrivate::progressTree()
{
    return d.ui->progressTree;
}

bool
ProgressViewPrivate::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Show) {
        static bool inittree = false;
        if (!inittree) {
            inittree = true;
            progressTree()->setColumnWidth(0, 180);
            progressTree()->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        }
    }
    return QObject::eventFilter(obj, event);
}

void
ProgressViewPrivate::cancel()
{
    dataModel()->endProgressBlock();
}

void
ProgressViewPrivate::clear()
{
    progressTree()->clear();
}

void
ProgressViewPrivate::progressBlockChanged(const QString& name, DataModel::ProgressMode mode)
{
    if (mode == DataModel::ProgressMode::ProgressRunning) {
        qDebug() << "RUNNING:" << name;

        clear();
        d.ui->progress->setValue(0);
        d.timer.restart();
        d.running = true;
        d.ui->status->setText(QString("Running: %1").arg(name));
    }
    else {
        qDebug() << "IDLE:" << name;

        d.running = false;
        qint64 ms = d.timer.elapsed();
        QString timeStr = QTime(0, 0).addMSecs(static_cast<int>(ms)).toString("hh:mm:ss");
        d.ui->status->setText(QString("Finished: %1 (Time: %2)").arg(name).arg(timeStr));
    }
}

void
ProgressViewPrivate::progressNotifyChanged(const DataModel::Notify& notify, size_t completed, size_t expected)
{
    QTreeWidget* tree = progressTree();
    if (d.expectedCount == 0)
        d.expectedCount = int(expected);

    int index = int(completed) - 1;
    if (index < 0)
        return;

    while (tree->topLevelItemCount() <= index) {
        auto* newItem = new QTreeWidgetItem(tree);
        newItem->setText(0, "Pending ...");
        newItem->setText(1, "");
    }

    QTreeWidgetItem* item = tree->topLevelItem(index);
    if (item) {
        qDebug() << "notify.message: " << notify.message;

        item->setText(0, notify.message);
        QString col1;
        if (!notify.paths.isEmpty()) {
            QString first = StringToQString(notify.paths.first().GetName());
            qsizetype count = notify.paths.size();
            col1 = QString("%1 (%2)").arg(first).arg(count);
        }
        else {
            col1 = QString();
        }
        item->setText(1, col1);

        QString msg = notify.message.toLower();
        if (msg.contains("failed")) {
            item->setForeground(0, QBrush(Qt::red));
        }
        else if (msg.contains("loaded") || msg.contains("done") || msg.contains("complete")) {
            item->setForeground(0, QBrush(Qt::green));
        }
        else if (msg.contains("loading") || msg.contains("working") || msg.contains("processing")) {
            item->setForeground(0, QBrush(Qt::yellow));
        }
        else {
            item->setForeground(0, QBrush(Qt::transparent));
        }
    }

    int pct = int((double(completed) / std::max<size_t>(1, expected)) * 100.0);
    d.ui->progress->setValue(pct);

    d.ui->status->setText(updateStatus(completed, expected));
    tree->expandAll();
}

void
ProgressViewPrivate::stageChanged(UsdStageRefPtr stage, DataModel::LoadPolicy policy, DataModel::StageStatus status)
{
    progressTree()->clear();
    d.stage = stage;
}

void
ProgressViewPrivate::selectionChanged(const QList<SdfPath>& paths)
{}

QString
ProgressViewPrivate::updateStatus(size_t completed, size_t expected)
{
    qint64 ms = d.timer.elapsed();
    QString timeStr = QTime(0, 0).addMSecs(static_cast<int>(ms)).toString("hh:mm:ss");
    return QString("Time: %1 (%2/%3)").arg(timeStr).arg(completed).arg(expected);
}

ProgressView::ProgressView(QWidget* parent)
    : QWidget(parent)
    , p(new ProgressViewPrivate())
{
    p->d.view = this;
    p->init();
}

ProgressView::~ProgressView() = default;

}  // namespace usdviewer
