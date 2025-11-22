// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdoutlinerview.h"
#include "usdpropertytree.h"
#include "usdstagetree.h"
#include <QPointer>

// generated files
#include "ui_usdoutlinerview.h"

namespace usd {
class OutlinerViewPrivate : public QObject {
public:
    OutlinerViewPrivate();
    void init();
    void initStageModel();
    void initSelection();
    PropertyTree* propertyTree();
    StageTree* stageTree();

public Q_SLOTS:
    void collapse();
    void expand();
    void primsChanged(const QList<SdfPath>& paths);
    void selectionChanged(const QList<SdfPath>& paths);
    void stageChanged(UsdStageRefPtr stage, StageModel::load_policy policy, StageModel::stage_status status);

public:
    struct Data {
        QScopedPointer<Ui_UsdOutlinerView> ui;
        QPointer<StageModel> stageModel;
        QPointer<SelectionModel> selectionModel;
        QPointer<OutlinerView> view;
    };
    Data d;
};

OutlinerViewPrivate::OutlinerViewPrivate() {}

void
OutlinerViewPrivate::init()
{
    d.ui.reset(new Ui_UsdOutlinerView());
    d.ui->setupUi(d.view.data());
    d.ui->stageTree->setHeaderLabels(QStringList() << "Name"
                                                   << "Type"
                                                   << "Vis");
    d.ui->propertyTree->setHeaderLabels(QStringList() << "Name"
                                                      << "Value");
}

void
OutlinerViewPrivate::initStageModel()
{
    connect(d.stageModel.data(), &StageModel::stageChanged, this, &OutlinerViewPrivate::stageChanged);
    connect(d.stageModel.data(), &StageModel::primsChanged, this, &OutlinerViewPrivate::primsChanged);
}

void
OutlinerViewPrivate::initSelection()
{
    connect(d.selectionModel.data(), &SelectionModel::selectionChanged, this, &OutlinerViewPrivate::selectionChanged);
}

PropertyTree*
OutlinerViewPrivate::propertyTree()
{
    return d.ui->propertyTree;
}

StageTree*
OutlinerViewPrivate::stageTree()
{
    return d.ui->stageTree;
}

void
OutlinerViewPrivate::collapse()
{
    if (d.selectionModel->paths().size()) {
        d.ui->stageTree->collapse();
    }
}

void
OutlinerViewPrivate::expand()
{
    d.ui->stageTree->expand();
}

void
OutlinerViewPrivate::primsChanged(const QList<SdfPath>& paths)
{
    propertyTree()->updatePrims(paths);
    stageTree()->updatePrims(paths);
}

void
OutlinerViewPrivate::selectionChanged(const QList<SdfPath>& paths)
{
    propertyTree()->updateSelection(paths);
    stageTree()->updateSelection(paths);
}

void
OutlinerViewPrivate::stageChanged(UsdStageRefPtr stage, StageModel::load_policy policy, StageModel::stage_status status)
{
    if (status == StageModel::stage_loaded) {
        if (policy == StageModel::load_payload) {
            stageTree()->setPayloadEnabled(true);
        }
        else {
            stageTree()->setPayloadEnabled(false);
        }
        stageTree()->updateStage(stage);
        propertyTree()->updateStage(stage);
    }
    else {
        propertyTree()->clear();
        stageTree()->clear();
    }
}

OutlinerView::OutlinerView(QWidget* parent)
    : QWidget(parent)
    , p(new OutlinerViewPrivate())
{
    p->d.view = this;
    p->init();
}

OutlinerView::~OutlinerView() = default;

void
OutlinerView::collapse()
{
    p->collapse();
}
void
OutlinerView::expand()
{
    p->expand();
}

SelectionModel*
OutlinerView::selectionModel()
{
    return p->d.selectionModel;
}

void
OutlinerView::setSelectionModel(SelectionModel* selectionModel)
{
    if (p->d.selectionModel != selectionModel) {
        p->d.selectionModel = selectionModel;
        p->initSelection();
        update();
    }
}

StageModel*
OutlinerView::stageModel() const
{
    return p->d.stageModel;
}

void
OutlinerView::setStageModel(StageModel* stageModel)
{
    if (p->d.stageModel != stageModel) {
        p->d.stageModel = stageModel;
        p->initStageModel();
        update();
    }
}
}  // namespace usd
