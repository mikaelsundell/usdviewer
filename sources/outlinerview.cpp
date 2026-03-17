// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "outlinerview.h"
#include "application.h"
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
    PropertyTree* propertyTree();
    StageTree* stageTree();
    bool eventFilter(QObject* obj, QEvent* event);
public Q_SLOTS:
    void clearFilter();
    void clearDepth();
    void collapse();
    void expand();
    void follow(bool enabled);
    void filterChanged(const QString& filter);
    void primsChanged(const QList<SdfPath>& paths);
    void selectionChanged(const QList<SdfPath>& paths);
    void stageChanged(UsdStageRefPtr stage, DataModel::LoadPolicy policy, DataModel::StageStatus status);
    void depthChanged(int value);

public:
    void updateDepth(const SdfPath& path = SdfPath());
    struct Data {
        bool followEnabled;
        QScopedPointer<Ui_OutlinerView> ui;
        QPointer<OutlinerView> view;
    };
    Data d;
};

OutlinerViewPrivate::OutlinerViewPrivate() { d.followEnabled = true; }

void
OutlinerViewPrivate::init()
{
    d.ui.reset(new Ui_OutlinerView());
    d.ui->setupUi(d.view.data());
    attach(d.ui->depth);
    stageTree()->setHeaderLabels(QStringList() << "Name"
                                               << "");
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
    connect(d.ui->follow, &QToolButton::toggled, this, &OutlinerViewPrivate::follow);
    connect(d.ui->depth, &QSlider::valueChanged, this, &OutlinerViewPrivate::depthChanged);
    // models
    connect(dataModel(), &DataModel::stageChanged, this, &OutlinerViewPrivate::stageChanged);
    connect(dataModel(), &DataModel::primsChanged, this, &OutlinerViewPrivate::primsChanged);
    connect(selectionModel(), &SelectionModel::selectionChanged, this, &OutlinerViewPrivate::selectionChanged);
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
                auto* header = tree->header();
                header->setStretchLastSection(false);
                header->setSectionResizeMode(0, QHeaderView::Stretch);
                header->setSectionResizeMode(1, QHeaderView::Fixed);
                tree->setColumnWidth(1, 60);
            }
            else if (tree == propertyTree()) {
                tree->setColumnWidth(0, 200);
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
    if (selectionModel()->paths().size()) {
        d.ui->stageTree->collapse();
    }
}

void
OutlinerViewPrivate::expand()
{
    d.ui->stageTree->expand();
}

void
OutlinerViewPrivate::follow(bool enabled)
{
    if (enabled)
        expand();
    d.followEnabled = enabled;
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
    if (paths.size() > 0 && d.followEnabled) {
        expand();
    }
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
OutlinerViewPrivate::stageChanged(UsdStageRefPtr stage, DataModel::LoadPolicy policy, DataModel::StageStatus status)
{
    if (status == DataModel::StageLoaded) {
        if (policy == DataModel::LoadPayload) {
            stageTree()->setPayloadEnabled(true);
        }
        else {
            stageTree()->setPayloadEnabled(false);
        }
        stageTree()->updateStage(stage);
        propertyTree()->updateStage(stage);
        d.ui->filter->setEnabled(true);
        d.ui->depth->setEnabled(true);
        d.ui->follow->setEnabled(true);
        updateDepth();
    }
    else {
        propertyTree()->close();
        stageTree()->close();
        d.ui->clear->setEnabled(false);
        d.ui->filter->setEnabled(false);
        d.ui->depth->setEnabled(false);
        d.ui->follow->setEnabled(false);
        clearFilter();
        clearDepth();
    }
}

void
OutlinerViewPrivate::depthChanged(int value)
{
    QList<SdfPath> paths = selectionModel()->paths();
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

bool
OutlinerView::followEnabled()
{
    return p->d.followEnabled;
}

void
OutlinerView::enableFollow(bool enable)
{
    p->follow(enable);
}

}  // namespace usd
