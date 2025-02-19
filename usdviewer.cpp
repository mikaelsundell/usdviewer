// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdviewer.h"
#include "usdstage.h"
#include "mouseevent.h"
#include <QColorDialog>
#include <QObject>
#include <QPointer>

// generated files
#include "ui_usdviewer.h"

namespace usd {
class ViewerPrivate : public QObject {
    Q_OBJECT
    public:
        ViewerPrivate();
        void init();
    public Q_SLOTS:
        void ready();
        void clearcolor();
        void aovChanged(int index);
    public:
        struct Data {
            QStringList arguments;
            QScopedPointer<MouseEvent> clearColorFilter;
            QScopedPointer<Ui_Usdviewer> ui;
            QPointer<Viewer> viewer;
        };
        Data d;
};

ViewerPrivate::ViewerPrivate()
{
}

void
ViewerPrivate::init()
{
    d.ui.reset(new Ui_Usdviewer());
    d.ui->setupUi(d.viewer.data());
    d.clearColorFilter.reset(new MouseEvent);
    d.ui->clearcolor->installEventFilter(d.clearColorFilter.data());
    // connect
    connect(d.ui->imagingglwidget, &ImagingGLWidget::rendererReady, this, &ViewerPrivate::ready);
    connect(d.ui->aovs, &QComboBox::currentIndexChanged, this, &ViewerPrivate::aovChanged);
    connect(d.clearColorFilter.data(), &MouseEvent::pressed, this, &ViewerPrivate::clearcolor);
}

void
ViewerPrivate::ready()
{
    for(QString aov : d.ui->imagingglwidget->rendererAovs()) {
        d.ui->aovs->addItem(aov, QVariant::fromValue(aov));
    }
}

void
ViewerPrivate::clearcolor()
{
    QColor color = QColorDialog::getColor(d.ui->imagingglwidget->clearColor(), d.viewer.data(), "Select Color");
    if (color.isValid()) {
        d.ui->imagingglwidget->setClearColor(color);
        d.ui->clearcolor->setStyleSheet("background-color: " + color.name() + ";");
    }
}

void
ViewerPrivate::aovChanged(int index)
{
    QString aov = d.ui->aovs->itemData(index, Qt::UserRole).value<QString>();
    d.ui->imagingglwidget->setRendererAov(aov);
}

#include "usdviewer.moc"

Viewer::Viewer(QWidget* parent)
: QMainWindow(parent)
, p(new ViewerPrivate())
{
    p->d.viewer = this;
    p->init();
}

Viewer::~Viewer()
{
}

void
Viewer::setArguments(const QStringList& arguments)
{
    p->d.arguments = arguments;
    for (int i = 0; i < arguments.size(); ++i) {
        if (arguments[i] == "--open" && i + 1 < arguments.size()) {
            QString filename = arguments[i + 1];
            if (!filename.isEmpty()) {
                Stage stage(filename);
                if (stage.isValid()) {
                    p->d.ui->imagingglwidget->setStage(stage);
                }
                else {
                    qWarning() << "could not load stage from filename: " << filename;
                }
            }
            break;
        }
    }
}
}

