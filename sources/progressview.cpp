// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "progressview.h"
#include "application.h"
#include "qtutils.h"
#include "selectionlist.h"
#include "session.h"
#include "style.h"
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
    void progressBlockChanged(const QString& name, Session::ProgressMode mode);
    void progressNotifyChanged(const Session::Notify& notify, size_t completed, size_t expected);
    void selectionChanged(const QList<SdfPath>& paths);
    void stageChanged(UsdStageRefPtr stage, Session::LoadPolicy policy, Session::StageStatus status);

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
    progressTree()->setHeaderLabels(QStringList() << "Command"
                                                  << "Status");
    progressTree()->setIndentation(0);
    // connect
    connect(d.ui->clear, &QPushButton::clicked, this, &ProgressViewPrivate::clear);
    connect(session(), &Session::progressBlockChanged, this, &ProgressViewPrivate::progressBlockChanged);
    connect(session(), &Session::progressNotifyChanged, this, &ProgressViewPrivate::progressNotifyChanged);
    connect(session(), &Session::stageChanged, this, &ProgressViewPrivate::stageChanged);
    connect(session()->selectionList(), &SelectionList::selectionChanged, this, &ProgressViewPrivate::selectionChanged);
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
    session()->endProgressBlock();
}

void
ProgressViewPrivate::clear()
{
    progressTree()->clear();
}

void
ProgressViewPrivate::progressBlockChanged(const QString& name, Session::ProgressMode mode)
{
    if (mode == Session::ProgressMode::Running) {
        clear();
        d.ui->progress->setValue(0);
        d.timer.restart();
        d.running = true;
        d.ui->status->setText(QString("Running: %1").arg(name));
    }
    else {
        d.running = false;
        qint64 ms = d.timer.elapsed();
        QString timeStr = QTime(0, 0).addMSecs(static_cast<int>(ms)).toString("hh:mm:ss");
        d.ui->status->setText(QString("Finished: %1 (Time: %2)").arg(name).arg(timeStr));
    }
}

void
ProgressViewPrivate::progressNotifyChanged(const Session::Notify& notify, size_t completed, size_t expected)
{
    QTreeWidget* tree = progressTree();

    if (d.expectedCount == 0)
        d.expectedCount = int(expected);

    const int index = int(completed) - 1;
    if (index < 0)
        return;

    // Ensure enough rows
    while (tree->topLevelItemCount() <= index) {
        auto* newItem = new QTreeWidgetItem(tree);
        newItem->setText(0, "Pending...");
        newItem->setText(1, "");
    }

    QTreeWidgetItem* item = tree->topLevelItem(index);
    if (!item)
        return;

    item->setText(0, notify.message);
    if (!notify.paths.isEmpty()) {
        const QString first = StringToQString(notify.paths.first().GetName());
        const qsizetype count = notify.paths.size();
        item->setText(1, QString("%1 (%2)").arg(first).arg(count));
    }
    else {
        item->setText(1, QString());
    }
    item->setForeground(0, QBrush());
    item->setForeground(1, QBrush());
    item->setIcon(0, QIcon());

    switch (notify.status) {
    case Session::Notify::Status::Error: item->setForeground(0, style()->color(Style::ColorRole::Error)); break;

    case Session::Notify::Status::Warning: item->setForeground(0, style()->color(Style::ColorRole::Warning)); break;

    case Session::Notify::Status::Progress: item->setForeground(0, style()->color(Style::ColorRole::Progress)); break;

    case Session::Notify::Status::Info:
    default: break;
    }
    const int pct = int((double(completed) / std::max<size_t>(1, expected)) * 100.0);
    d.ui->progress->setValue(pct);
    d.ui->status->setText(updateStatus(completed, expected));
    tree->expandAll();
}

void
ProgressViewPrivate::stageChanged(UsdStageRefPtr stage, Session::LoadPolicy policy, Session::StageStatus status)
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
