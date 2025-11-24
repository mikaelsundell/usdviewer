// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdpayloadview.h"
#include "usdqtutils.h"
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/usd/primCompositionQuery.h>

// generated files
#include "ui_usdpayloadview.h"

namespace usd {
class PayloadViewPrivate : public QObject {
public:
    void init();
    void initStageModel();
    void initSelection();
    QTreeWidget* payloadTree();
    bool eventFilter(QObject* obj, QEvent* event);

public Q_SLOTS:
    void cancel();
    void clear();
    void selectionChanged(const QList<SdfPath>& paths);
    void payloadsRequested(const QList<SdfPath>& paths, StageModel::payload_mode mode);
    void payloadChanged(const SdfPath& path, StageModel::payload_mode mode);
    void stageChanged(UsdStageRefPtr stage, StageModel::load_policy policy, StageModel::stage_status status);

public:
    QString updateStatus();
    struct Data {
        StageModel::payload_mode payloadMode;
        UsdStageRefPtr stage;
        qsizetype total = 0;
        qsizetype completed = 0;
        qsizetype totalsize = 0;
        QScopedPointer<Ui_UsdPayloadView> ui;
        QPointer<StageModel> stageModel;
        QPointer<SelectionModel> selectionModel;
        QPointer<PayloadView> view;
    };
    Data d;
};

void
PayloadViewPrivate::init()
{
    d.ui.reset(new Ui_UsdPayloadView());
    d.ui->setupUi(d.view.data());
    // event filter
    payloadTree()->installEventFilter(this);
    // payload tree
    d.ui->payloadTree->setHeaderLabels(QStringList() << "Name"
                                                     << "Filename");
    // connect
    connect(d.ui->clear, &QPushButton::clicked, this, &PayloadViewPrivate::clear);
}

void
PayloadViewPrivate::initStageModel()
{
    connect(d.stageModel.data(), &StageModel::payloadsRequested, this, &PayloadViewPrivate::payloadsRequested);
    connect(d.stageModel.data(), &StageModel::payloadChanged, this, &PayloadViewPrivate::payloadChanged);
    connect(d.stageModel.data(), &StageModel::stageChanged, this, &PayloadViewPrivate::stageChanged);
}

void
PayloadViewPrivate::initSelection()
{
    connect(d.selectionModel.data(), &SelectionModel::selectionChanged, this, &PayloadViewPrivate::selectionChanged);
}

QTreeWidget*
PayloadViewPrivate::payloadTree()
{
    return d.ui->payloadTree;
}

bool
PayloadViewPrivate::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Show) {
        static bool inittree = false;
        if (!inittree) {
            inittree = true;
            payloadTree()->setColumnWidth(0, 200);
            payloadTree()->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        }
    }
    return QObject::eventFilter(obj, event);
}

void
PayloadViewPrivate::cancel()
{
    d.stageModel->cancelPayloads();
}

void
PayloadViewPrivate::clear()
{
    payloadTree()->clear();
}

void
PayloadViewPrivate::payloadsRequested(const QList<SdfPath>& paths, StageModel::payload_mode mode)
{
    d.payloadMode = mode;
    d.total = paths.size();
    d.completed = 0;
    d.totalsize = 0;
    d.ui->status->clear();
    d.ui->progress->setValue(0);
    d.ui->status->setText(updateStatus());
    for (const SdfPath& path : paths) {
        auto* item = new QTreeWidgetItem(d.ui->payloadTree);
        item->setText(0, StringToQString(path.GetName()));
        item->setData(0, Qt::UserRole, StringToQString(path.GetString()));
        item->setText(1, "Queued");
    }
    clear();
}

void
PayloadViewPrivate::payloadChanged(const SdfPath& path, StageModel::payload_mode mode)
{
    QVariant target = StringToQString(path.GetString());
    QTreeWidget* tree = d.ui->payloadTree;
    const int role = Qt::UserRole;
    const int column = 0;
    QTreeWidgetItem* item = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        item = tree->topLevelItem(i);
        if (item->data(column, role) == target) {
            item->setText(1, "Completed");
            break;
        }
    }
    if (!item)
        return;
    UsdPrim prim = d.stage->GetPrimAtPath(path);
    UsdPrimCompositionQuery query(prim);
    for (const auto& arc : query.GetCompositionArcs()) {
        if (arc.GetArcType() == PcpArcTypePayload) {
            SdfPayloadEditorProxy editor;
            SdfPayload payload;
            if (arc.GetIntroducingListEditor(&editor, &payload)) {
                std::string assetPath = payload.GetAssetPath();
                std::string resolved = ArGetResolver().Resolve(assetPath);
                if (resolved.empty()) {
                    SdfLayerHandle introducingLayer = arc.GetIntroducingLayer();
                    if (introducingLayer && !introducingLayer->GetRealPath().empty()) {
                        QFileInfo baseInfo(StringToQString(introducingLayer->GetRealPath()));
                        QString composed = QDir(baseInfo.absolutePath()).filePath(StringToQString(assetPath));
                        resolved = composed.toStdString();
                    }
                }
                QString displayPath = StringToQString(resolved.empty() ? assetPath : resolved);
                QFileInfo info(displayPath);
                if (info.exists()) {
                    d.totalsize += info.size();
                }
                break;
            }
        }
    }
    d.completed++;
    int progress = static_cast<int>((float)d.completed / std::max<qsizetype>(1, d.total) * 100.0f);
    d.ui->progress->setValue(progress);
    d.ui->status->setText(updateStatus());
    if (d.completed >= d.total) {
        d.ui->status->setText("All payloads loaded successfully");
    }
}

void
PayloadViewPrivate::stageChanged(UsdStageRefPtr stage, StageModel::load_policy policy, StageModel::stage_status status)
{
    d.ui->payloadTree->clear();
    d.stage = stage;
}

void
PayloadViewPrivate::selectionChanged(const QList<SdfPath>& paths)
{}

QString
PayloadViewPrivate::updateStatus()
{
    QString sizeStr = QLocale().formattedDataSize(d.totalsize, 1, QLocale::DataSizeTraditionalFormat);
    return QString("Time: 00:00:00 (Files: %1/%2, %3 %4)")
        .arg(d.completed)
        .arg(d.total)
        .arg(sizeStr)
        .arg(d.payloadMode == StageModel::payload_loaded ? "loaded" : "unloaded");
}

PayloadView::PayloadView(QWidget* parent)
    : QWidget(parent)
    , p(new PayloadViewPrivate())
{
    p->d.view = this;
    p->init();
}

PayloadView::~PayloadView() {}

StageModel*
PayloadView::stageModel() const
{
    return p->d.stageModel;
}

void
PayloadView::setStageModel(StageModel* stageModel)
{
    if (p->d.stageModel != stageModel) {
        p->d.stageModel = stageModel;
        p->initStageModel();
        update();
    }
}

SelectionModel*
PayloadView::selectionModel()
{
    return p->d.selectionModel;
}

void
PayloadView::setSelectionModel(SelectionModel* selectionModel)
{
    if (p->d.selectionModel != selectionModel) {
        p->d.selectionModel = selectionModel;
        p->initSelection();
        update();
    }
}
}  // namespace usd
