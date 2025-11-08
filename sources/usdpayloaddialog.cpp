// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/specviz

#include "usdpayloaddialog.h"
#include <QFileInfo>
#include <QDir>
#include <QPointer>
#include <QTimer>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/usd/payloads.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/sdf/payload.h>
#include <pxr/usd/pcp/arc.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primCompositionQuery.h>

// generated files
#include "ui_usdpayloaddialog.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class PayloadDialogPrivate : public QObject {
    Q_OBJECT
public:
    PayloadDialogPrivate();
    void init();
    void payloadsRequested(const QList<SdfPath>& paths);
    void payloadsFailed(const SdfPath& paths);
    void payloadsLoaded(const SdfPath& paths);
    void payloadsUnloaded(const SdfPath& paths);

public:
    struct Data {
        qsizetype total = 0;
        qsizetype completed = 0;
        QPointer<StageModel> stageModel;
        QPointer<Selection> selection;
        QPointer<PayloadDialog> dialog;
        QScopedPointer<Ui_UsdPayloadDialog> ui;
    };
    Data d;
};

PayloadDialogPrivate::PayloadDialogPrivate() {}

void
PayloadDialogPrivate::init()
{
    d.ui.reset(new Ui_UsdPayloadDialog());
    d.ui->setupUi(d.dialog.data());
    d.ui->status->setColumnCount(2);
    d.ui->status->setHeaderLabels(QStringList() << "Name"
                                                << "Status"
                                                << "Size"
                                                << "Filename");
    d.ui->status->setColumnWidth(0, 400);
    d.ui->status->setColumnWidth(1, 80);
    d.ui->status->setColumnWidth(2, 80);
    d.ui->status->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    d.ui->progress->setValue(0);
    d.ui->label->setText("");
    // dialog
    d.dialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    // connect
    connect(d.ui->close, &QPushButton::clicked, this, [=]() { d.dialog->accept(); });
}

void
PayloadDialogPrivate::payloadsRequested(const QList<SdfPath>& paths)
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
    if (!d.dialog->isVisible()) {
        d.dialog->show();
    }
}

void
PayloadDialogPrivate::payloadsFailed(const SdfPath& path)
{
    QString str = QString::fromStdString(path.GetString());
    auto matches = d.ui->status->findItems(str, Qt::MatchExactly, 2);
    if (!matches.isEmpty()) {
        matches.first()->setText(1, "Failed");
    }
}

void
PayloadDialogPrivate::payloadsLoaded(const SdfPath& path)
{
    const QString pathStr = QString::fromStdString(path.GetString());
    auto matches = d.ui->status->findItems(pathStr, Qt::MatchExactly, 2);
    if (matches.isEmpty())
        return;

    QTreeWidgetItem* item = matches.first();
    item->setText(1, "Loaded");

    if (!d.stageModel)
        return;

    UsdStageRefPtr stage = d.stageModel->stage();
    if (!stage)
        return;

    UsdPrim prim = stage->GetPrimAtPath(path);
    if (!prim)
        return;

    UsdPrimCompositionQuery query(prim);
    for (const auto& arc : query.GetCompositionArcs()) {
        if (arc.GetArcType() == PcpArcTypePayload) {
            SdfPayloadEditorProxy editor;
            SdfPayload payload;

            if (arc.GetIntroducingListEditor(&editor, &payload)) {
                std::string assetPath = payload.GetAssetPath();
                std::string resolved = pxr::ArGetResolver().Resolve(assetPath);

                if (resolved.empty()) {
                    SdfLayerHandle introducingLayer = arc.GetIntroducingLayer();
                    if (introducingLayer && !introducingLayer->GetRealPath().empty()) {
                        QFileInfo baseInfo(QString::fromStdString(introducingLayer->GetRealPath()));
                        QString composed =
                            QDir(baseInfo.absolutePath()).filePath(QString::fromStdString(assetPath));
                        resolved = composed.toStdString();
                    }
                }

                QString displayPath =
                    QString::fromStdString(resolved.empty() ? assetPath : resolved);

                QFileInfo info(displayPath);
                if (info.exists()) {
                    item->setText(3, info.absoluteFilePath());

                    qint64 size = info.size();
                    QString sizeStr;
                    if (size < 1024)
                        sizeStr = QString("%1 B").arg(size);
                    else if (size < 1024 * 1024)
                        sizeStr = QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
                    else
                        sizeStr = QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);

                    item->setText(2, sizeStr);
                } else {
                    item->setText(3, displayPath);
                    item->setText(2, "—");
                }

                break;  // only the first payload shown
            }
        }
    }
    d.completed++;
    int progress = static_cast<int>(
        (float)d.completed / std::max<qsizetype>(1, d.total) * 100.0f);
    d.ui->progress->setValue(progress);

    QString name = QString::fromStdString(path.GetName());
    d.ui->label->setText(QString("Loaded: %1").arg(name));

    if (d.completed >= d.total) {
        d.ui->label->setText("All payloads loaded successfully");
    }
}


void
PayloadDialogPrivate::payloadsUnloaded(const SdfPath& path)
{
    QString str = QString::fromStdString(path.GetString());
    auto matches = d.ui->status->findItems(str, Qt::MatchExactly, 2);
    if (!matches.isEmpty()) {
        matches.first()->setText(1, "Unloaded");
    }
    d.completed++;
    int progress = static_cast<int>((float)d.completed / std::max(1, (int)d.total) * 100.0f);
    d.ui->progress->setValue(progress);

    QString name = QString::fromStdString(path.GetName());
    d.ui->label->setText(QString("Unloaded: %1").arg(name));

    if (d.completed >= d.total) {
        d.ui->label->setText("All payloads unloaded successfully");
    }
}

#include "usdpayloaddialog.moc"

PayloadDialog::PayloadDialog(QWidget* parent)
    : QDialog(parent)
    , p(new PayloadDialogPrivate())
{
    p->d.dialog = this;
    p->init();
}

PayloadDialog::~PayloadDialog() {}

Selection*
PayloadDialog::selection()
{
    return p->d.selection;
}

void
PayloadDialog::setSelection(Selection* selection)
{
    if (p->d.selection != selection) {
        p->d.selection = selection;
        update();
    }
}

StageModel*
PayloadDialog::stageModel() const
{
    return p->d.stageModel;
}

void
PayloadDialog::setStageModel(StageModel* stageModel)
{
    if (p->d.stageModel != stageModel) {
        p->d.stageModel = stageModel;
        update();
    }
}

void
PayloadDialog::payloadsRequested(const QList<SdfPath>& paths)
{
    p->payloadsRequested(paths);
}

void
PayloadDialog::payloadsFailed(const SdfPath& path)
{
    p->payloadsFailed(path);
}

void
PayloadDialog::payloadsLoaded(const SdfPath& path)
{
    p->payloadsLoaded(path);
}

void
PayloadDialog::payloadsUnloaded(const SdfPath& path)
{
    p->payloadsUnloaded(path);
}

void
PayloadDialog::cancel()
{
    reject();
}

}  // namespace usd
