// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdviewer.h"
#include "usdstage.h"
#include "mouseevent.h"
#include <QColorDialog>
#include <QFileDialog>
#include <QObject>
#include <QPointer>
#include <QSettings>

// generated files
#include "ui_usdviewer.h"

namespace usd {
class ViewerPrivate : public QObject {
    Q_OBJECT
    public:
        ViewerPrivate();
        void init();
        usd::ViewCamera camera();
        usd::ImagingGLWidget* renderer();
        QVariant settingsValue(const QString& key, const QVariant& defaultValue = QVariant());
        void setSettingsValue(const QString& key, const QVariant& value);
    public Q_SLOTS:
        void open();
        void ready();
        void clearcolor();
        void frameAll();
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
    // clear color
    QColor color(100, 150, 150);
    d.ui->imagingglwidget->setClearColor(color);
    d.ui->clearcolor->setStyleSheet("background-color: " + color.name() + ";");
    // connect
    connect(d.ui->imagingglwidget, &ImagingGLWidget::rendererReady, this, &ViewerPrivate::ready);
    connect(d.ui->aovs, &QComboBox::currentIndexChanged, this, &ViewerPrivate::aovChanged);
    connect(d.ui->open, &QPushButton::clicked, this, &ViewerPrivate::open);
    connect(d.ui->frameAll, &QPushButton::clicked, this, &ViewerPrivate::frameAll);
    connect(d.ui->fileOpen, &QAction::triggered, this, &ViewerPrivate::open);
    connect(d.ui->displayFrameAll, &QAction::triggered, this, &ViewerPrivate::frameAll);
    connect(d.clearColorFilter.data(), &MouseEvent::pressed, this, &ViewerPrivate::clearcolor);
}

usd::ViewCamera
ViewerPrivate::camera()
{
    return d.ui->imagingglwidget->viewCamera();
}

usd::ImagingGLWidget*
ViewerPrivate::renderer()
{
    return d.ui->imagingglwidget;
}

QVariant
ViewerPrivate::settingsValue(const QString& key, const QVariant& defaultValue) {
    QSettings settings(PROJECT_IDENTIFIER, PROJECT_NAME);
    return settings.value(key, defaultValue);
}

void
ViewerPrivate::setSettingsValue(const QString& key, const QVariant& value) {
    QSettings settings(PROJECT_IDENTIFIER, PROJECT_NAME);
    settings.setValue(key, value);
}

void
ViewerPrivate::open()
{
    QString openDir = settingsValue("openDir", QDir::homePath()).toString();
    QString filter = "USD Files (*.usd *.usda *.usdz)";
    QString filename = QFileDialog::getOpenFileName(d.viewer.data(), "Open USD File", QString(), filter);
    if (filename.size()) {
        Stage stage(filename);
        if (stage.isValid()) {
            d.viewer->setWindowTitle(QString("%1: %2").arg(PROJECT_NAME).arg(filename));
            renderer()->setStage(stage);
        }
        setSettingsValue("openDir", QFileInfo(filename).absolutePath());
    }
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
ViewerPrivate::frameAll()
{
    d.ui->imagingglwidget->viewCamera().frameAll();
    d.ui->imagingglwidget->update();
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
                    setWindowTitle(QString("%1: %2").arg(PROJECT_NAME).arg(filename));
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

