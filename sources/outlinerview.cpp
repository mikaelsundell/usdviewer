// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "outlinerview.h"
#include "propertytree.h"
#include "signalguard.h"
#include "stagetree.h"
#include <QPointer>
#include <QTimer>
#include <pxr/usd/usd/prim.h>

PXR_NAMESPACE_USING_DIRECTIVE

// generated files
#include "ui_outlinerview.h"

namespace usd {
class OutlinerViewPrivate : public QObject, public SignalGuard {
public:
    OutlinerViewPrivate();
    void init();
    void initDataModel();
    void initSelection();
    void initDepth();
    PropertyTree* propertyTree();
    StageTree* stageTree();
    bool eventFilter(QObject* obj, QEvent* event);
public Q_SLOTS:
    void clearFilter();
    void clearDepth();
    void collapse();
    void expand();
    void filterChanged(const QString& filter);
    void primsChanged(const QList<SdfPath>& paths);
    void selectionChanged(const QList<SdfPath>& paths);
    void stageChanged(UsdStageRefPtr stage, DataModel::load_policy policy, DataModel::stage_status status);
    void depthChanged(int value);

public:
    void updateDepth(const SdfPath& path = SdfPath());
    struct Data {
        QScopedPointer<Ui_OutlinerView> ui;
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
    d.ui.reset(new Ui_OutlinerView());
    d.ui->setupUi(d.view.data());
    attach(d.ui->depth);
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
    connect(d.ui->clear, &QToolButton::clicked, this, &OutlinerViewPrivate::clearFilter);
    connect(d.ui->collapse, &QToolButton::clicked, this, &OutlinerViewPrivate::collapse);
    connect(d.ui->expand, &QToolButton::clicked, this, &OutlinerViewPrivate::expand);
    connect(d.ui->depth, &QSlider::valueChanged, this, &OutlinerViewPrivate::depthChanged);
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

void
OutlinerViewPrivate::initDepth()
{}

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
OutlinerViewPrivate::clearFilter()
{
    d.ui->filter->setText(QString());
}

void
OutlinerViewPrivate::clearDepth()
{
    d.ui->depth->setMinimum(0);
    d.ui->depth->setMaximum(10);
    d.ui->depth->setValue(0);
    d.ui->depth->setEnabled(false);
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
    if (filter.size()) {
        d.ui->clear->setEnabled(true);
    }
    else {
        d.ui->clear->setEnabled(false);
    }
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
    SignalGuard::Scope guard(this);
    propertyTree()->updateSelection(paths);
    stageTree()->updateSelection(paths);
    if (paths.size() == 1) {
        updateDepth(paths.first());
    }
    else {
        updateDepth();
    }
    d.ui->collapse->setEnabled(true);
    d.ui->expand->setEnabled(true);
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
        d.ui->filter->setEnabled(true);
        d.ui->depth->setEnabled(true);
        updateDepth();
    }
    else {
        propertyTree()->close();
        stageTree()->close();
        d.ui->clear->setEnabled(false);
        d.ui->filter->setEnabled(false);
        d.ui->depth->setEnabled(false);
        clearFilter();
        clearDepth();
    }
}

void
OutlinerViewPrivate::depthChanged(int value)
{
    QList<SdfPath> paths = d.selectionModel->paths();
    if (paths.size() == 1) {
        stageTree()->expandDepth(value, paths.first());
    }
    else {
        stageTree()->expandDepth(value);
    }
}

void
OutlinerViewPrivate::updateDepth(const SdfPath& path)
{
    SignalGuard::Scope guard(this);
    d.ui->depth->setEnabled(true);
    d.ui->depth->setMinimum(0);
    d.ui->depth->setMaximum(stageTree()->maxDepth(path));
    d.ui->depth->setValue(stageTree()->depth(path));
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
