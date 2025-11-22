// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdpayloadview.h"
#include <QPointer>

// generated files
#include "ui_usdpayloadview.h"

namespace usd {
class PayloadViewPrivate : public QObject {
public:
    void init();
    void initStageModel();
    void initSelection();

public Q_SLOTS:
    void selectionChanged(const QList<SdfPath>& paths);
    void stageChanged(UsdStageRefPtr stage, StageModel::load_policy policy, StageModel::stage_status status);

public:
    struct Data {
        UsdStageRefPtr stage;
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
    // payload tree
    d.ui->payloadTree->setHeaderLabels(QStringList() << "Name"
                                                     << "Filename");
}

void
PayloadViewPrivate::initStageModel()
{
    connect(d.stageModel.data(), &StageModel::stageChanged, this, &PayloadViewPrivate::stageChanged);
}

void
PayloadViewPrivate::initSelection()
{
    connect(d.selectionModel.data(), &SelectionModel::selectionChanged, this, &PayloadViewPrivate::selectionChanged);
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
