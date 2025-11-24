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
    void initDataModel();
    void initSelection();
    PropertyTree* propertyTree();
    StageTree* stageTree();
    void updateFilter();
    bool eventFilter(QObject* obj, QEvent* event);

public Q_SLOTS:
    void collapse();
    void expand();
    void filterChanged(const QString& filter);
    void primsChanged(const QList<SdfPath>& paths);
    void selectionChanged(const QList<SdfPath>& paths);
    void stageChanged(UsdStageRefPtr stage, DataModel::load_policy policy, DataModel::stage_status status);

public:
    struct Data {
        QScopedPointer<Ui_UsdOutlinerView> ui;
        QPointer<DataModel> dataModel;
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
    stageTree()->setHeaderLabels(QStringList() << "Name"
                                               << "Type"
                                               << "Vis");
    propertyTree()->setHeaderLabels(QStringList() << "Name"
                                                  << "Value");
    // event filter
    stageTree()->installEventFilter(this);
    propertyTree()->installEventFilter(this);
    // connect
    connect(d.ui->filter, &QLineEdit::textChanged, this, &OutlinerViewPrivate::filterChanged);
}

void
OutlinerViewPrivate::initDataModel()
{
    connect(d.dataModel.data(), &DataModel::stageChanged, this, &OutlinerViewPrivate::stageChanged);
    connect(d.dataModel.data(), &DataModel::primsChanged, this, &OutlinerViewPrivate::primsChanged);
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

bool
OutlinerViewPrivate::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Show) {
        if (auto* tree = qobject_cast<QTreeWidget*>(obj)) {
            if (tree == stageTree()) {
                tree->setColumnWidth(0, 180);
                tree->setColumnWidth(1, 80);
                tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
            }
            else if (tree == propertyTree()) {
                tree->setColumnWidth(0, 180);
                tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
            }
        }
    }
    return QObject::eventFilter(obj, event);
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
OutlinerViewPrivate::filterChanged(const QString& filter)
{
    stageTree()->setFilter(filter);
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
OutlinerViewPrivate::stageChanged(UsdStageRefPtr stage, DataModel::load_policy policy, DataModel::stage_status status)
{
    if (status == DataModel::stage_loaded) {
        if (policy == DataModel::load_payload) {
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

DataModel*
OutlinerView::dataModel() const
{
    return p->d.dataModel;
}

void
OutlinerView::setDataModel(DataModel* dataModel)
{
    if (p->d.dataModel != dataModel) {
        p->d.dataModel = dataModel;
        p->initDataModel();
        update();
    }
}
}  // namespace usd
