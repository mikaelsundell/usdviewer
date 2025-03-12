// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdviewer.h"
#include "mouseevent.h"
#include "usdoutlineritem.h"
#include "usdstage.h"
#include <QActionGroup>
#include <QClipboard>
#include <QColorDialog>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QImageWriter>
#include <QMimeData>
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
        void initStage(const Stage& stage);
        ViewCamera camera();
        ImagingGLWidget* renderer();
        OutlinerWidget* outliner();
        Selection* selection();
        QVariant settingsValue(const QString& key, const QVariant& defaultValue = QVariant());
        void setSettingsValue(const QString& key, const QVariant& value);

    public Q_SLOTS:
        void open();
        void ready();
        void copyImage();
        void clearColor();
        void exportAll();
        void exportSelected();
        void exportImage();
        void frameAll();
        void frameSelected();
        void resetView();
        void aovChanged(int index);
        void asComplexityLow();
        void asComplexityMedium();
        void asComplexityHigh();
        void asComplexityVeryHigh();
        void openGithubReadme();
        void openGithubIssues();

    public:
        struct Data {
                Stage stage;
                QStringList arguments;
                QStringList extensions;
                QColor clearColor;
                QScopedPointer<MouseEvent> clearColorFilter;
                QScopedPointer<Selection> selection;
                QScopedPointer<Ui_Usdviewer> ui;
                QPointer<Viewer> viewer;
        };
        Data d;
};

ViewerPrivate::ViewerPrivate() { d.extensions = { ".usd", ".usda", ".usdz" }; }

void
ViewerPrivate::init()
{
    d.ui.reset(new Ui_Usdviewer());
    d.ui->setupUi(d.viewer.data());
    // clear color
    d.clearColor = QColor(settingsValue("clearColor", "#4f4f4f").toString());
    d.ui->clearcolor->setStyleSheet("background-color: " + d.clearColor.name() + ";");
    d.clearColorFilter.reset(new MouseEvent);
    d.ui->clearcolor->installEventFilter(d.clearColorFilter.data());
    // selection
    d.selection.reset(new Selection());
    // renderer
    renderer()->setClearColor(d.clearColor);
    renderer()->setSelection(d.selection.data());
    // outliner
    outliner()->setHeaderLabels(QStringList() << "Name"
                                              << "Type"
                                              << "Visibility");
    outliner()->setColumnWidth(OutlinerItem::Name, 100);
    outliner()->setColumnWidth(OutlinerItem::Type, 80);
    outliner()->setColumnWidth(OutlinerItem::Visible, 80);
    outliner()->setSelection(d.selection.data());
    // connect
    connect(d.ui->imagingglwidget, &ImagingGLWidget::rendererReady, this, &ViewerPrivate::ready);
    connect(d.ui->fileOpen, &QAction::triggered, this, &ViewerPrivate::open);
    connect(d.ui->fileExportAll, &QAction::triggered, this, &ViewerPrivate::exportAll);
    connect(d.ui->fileExportSelected, &QAction::triggered, this, &ViewerPrivate::exportSelected);
    connect(d.ui->fileExportImage, &QAction::triggered, this, &ViewerPrivate::exportImage);
    connect(d.ui->editCopyImage, &QAction::triggered, this, &ViewerPrivate::copyImage);
    connect(d.ui->asComplexityLow, &QAction::triggered, this, &ViewerPrivate::asComplexityLow);
    connect(d.ui->asComplexityMedium, &QAction::triggered, this,
            &ViewerPrivate::asComplexityMedium);
    connect(d.ui->asComplexityHigh, &QAction::triggered, this, &ViewerPrivate::asComplexityHigh);
    connect(d.ui->asComplexityVeryHigh, &QAction::triggered, this,
            &ViewerPrivate::asComplexityVeryHigh);
    {
        QActionGroup* actions = new QActionGroup(this);
        actions->setExclusive(true);
        {
            actions->addAction(d.ui->asComplexityLow);
            actions->addAction(d.ui->asComplexityMedium);
            actions->addAction(d.ui->asComplexityHigh);
            actions->addAction(d.ui->asComplexityVeryHigh);
        }
    }
    connect(d.ui->displayFrameAll, &QAction::triggered, this, &ViewerPrivate::frameAll);
    connect(d.ui->displayFrameSelected, &QAction::triggered, this, &ViewerPrivate::frameSelected);
    connect(d.ui->displayResetView, &QAction::triggered, this, &ViewerPrivate::resetView);
    connect(d.ui->helpGithubReadme, &QAction::triggered, this, &ViewerPrivate::openGithubReadme);
    connect(d.ui->helpGithubIssues, &QAction::triggered, this, &ViewerPrivate::openGithubIssues);
    connect(d.ui->open, &QPushButton::clicked, this, &ViewerPrivate::open);
    connect(d.ui->exportSelected, &QPushButton::clicked, this, &ViewerPrivate::exportSelected);
    connect(d.ui->exportImage, &QPushButton::clicked, this, &ViewerPrivate::exportImage);
    connect(d.ui->frameAll, &QPushButton::clicked, this, &ViewerPrivate::frameAll);
    connect(d.ui->frameSelected, &QPushButton::clicked, this, &ViewerPrivate::frameSelected);
    connect(d.ui->resetView, &QPushButton::clicked, this, &ViewerPrivate::resetView);
    connect(d.ui->aovs, &QComboBox::currentIndexChanged, this, &ViewerPrivate::aovChanged);
    connect(d.clearColorFilter.data(), &MouseEvent::pressed, this, &ViewerPrivate::clearColor);
    connect(d.selection.data(), &Selection::selectionChanged, d.ui->imagingglwidget,
            &ImagingGLWidget::updateSelection);
    connect(d.selection.data(), &Selection::selectionChanged, d.ui->outlinerwidget,
            &OutlinerWidget::updateSelection);
}

void
ViewerPrivate::initStage(const Stage& stage)
{
    renderer()->setStage(stage);
    outliner()->setStage(stage);
    d.stage = stage;
}

ViewCamera
ViewerPrivate::camera()
{
    return d.ui->imagingglwidget->viewCamera();
}

ImagingGLWidget*
ViewerPrivate::renderer()
{
    return d.ui->imagingglwidget;
}

OutlinerWidget*
ViewerPrivate::outliner()
{
    return d.ui->outlinerwidget;
}

Selection*
ViewerPrivate::selection()
{
    return d.selection.data();
}

QVariant
ViewerPrivate::settingsValue(const QString& key, const QVariant& defaultValue)
{
    QSettings settings(PROJECT_IDENTIFIER, PROJECT_NAME);
    return settings.value(key, defaultValue);
}

void
ViewerPrivate::setSettingsValue(const QString& key, const QVariant& value)
{
    QSettings settings(PROJECT_IDENTIFIER, PROJECT_NAME);
    settings.setValue(key, value);
}

void
ViewerPrivate::open()
{
    QString openDir = settingsValue("openDir", QDir::homePath()).toString();
    QStringList filters;
    for (const QString& ext : d.extensions) {
        filters.append("*" + ext);
    }
    QString filter = "USD Files (*.usd *.usda *.usdz)";
    QString filename = QFileDialog::getOpenFileName(d.viewer.data(), "Open USD File", openDir,
                                                    filter);
    if (filename.size()) {
        Stage stage(filename);
        if (stage.isValid()) {
            d.viewer->setWindowTitle(QString("%1: %2").arg(PROJECT_NAME).arg(filename));
            initStage(stage);
        }
        setSettingsValue("openDir", QFileInfo(filename).absolutePath());
    }
}

void
ViewerPrivate::ready()
{
    for (QString aov : d.ui->imagingglwidget->rendererAovs()) {
        d.ui->aovs->addItem(aov, QVariant::fromValue(aov));
    }
}

void
ViewerPrivate::copyImage()
{
    QImage image = d.ui->imagingglwidget->image();
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setImage(image);
}

void
ViewerPrivate::clearColor()
{
    QColor color = QColorDialog::getColor(d.clearColor, d.viewer.data(), "Select color");
    if (color.isValid()) {
        d.ui->imagingglwidget->setClearColor(color);
        d.ui->clearcolor->setStyleSheet("background-color: " + color.name() + ";");
        setSettingsValue("clearColor", color.name());
        d.clearColor = color;
    }
}

void
ViewerPrivate::exportAll()
{
    qDebug() << "todo: export all";
}

void
ViewerPrivate::exportSelected()
{
    qDebug() << "todo: export selected";
}

void
ViewerPrivate::exportImage()
{
    QString exportImageDir = settingsValue("exportImageDir", QDir::homePath()).toString();
    QImage image = d.ui->imagingglwidget->image();
    QStringList filters;
    QList<QByteArray> formats = QImageWriter::supportedImageFormats();
    QString defaultFormat = "png";
    filters.append("PNG Files (*.png)");
    for (const QByteArray& format : formats) {
        QString ext = QString(format).toLower();
        if (ext != defaultFormat) {
            filters.append(QString("%1 Files (*.%2)").arg(ext.toUpper(), ext));
        }
    }
    filters.append("All Files (*)");
    QString filter = filters.join(";;");
    QString exportName = exportImageDir + "/image." + defaultFormat;

    QString filename = QFileDialog::getSaveFileName(d.viewer.data(), "Export Image", exportName,
                                                    filter);
    if (!filename.isEmpty()) {
        QString extension = QFileInfo(filename).suffix().toLower();
        if (extension.isEmpty()) {
            filename += "." + defaultFormat;
            extension = defaultFormat;
        }
        if (!formats.contains(extension.toUtf8())) {
            qWarning() << "unsupported file format: " << extension;
            filename = QFileInfo(filename).completeBaseName() + ".png";
            extension = defaultFormat;
        }
        if (image.save(filename, extension.toUtf8().constData())) {
            setSettingsValue("exportImageDir", QFileInfo(filename).absolutePath());
        }
        else {
            qWarning() << "failed to save image: " << filename;
        }
    }
}

void
ViewerPrivate::frameAll()
{
    camera().setBoundingBox(d.stage.boundingBox());
    camera().frameAll();
    renderer()->update();
}

void
ViewerPrivate::frameSelected()
{
    camera().setBoundingBox(d.stage.boundingBox(selection()->paths()));
    camera().frameAll();
    renderer()->update();
}

void
ViewerPrivate::resetView()
{
    camera().resetView();
    renderer()->update();
}

void
ViewerPrivate::aovChanged(int index)
{
    QString aov = d.ui->aovs->itemData(index, Qt::UserRole).value<QString>();
    d.ui->imagingglwidget->setRendererAov(aov);
}

void
ViewerPrivate::asComplexityLow()
{
    d.ui->imagingglwidget->setComplexity(ImagingGLWidget::Low);
}

void
ViewerPrivate::asComplexityMedium()
{
    d.ui->imagingglwidget->setComplexity(ImagingGLWidget::Medium);
}

void
ViewerPrivate::asComplexityHigh()
{
    d.ui->imagingglwidget->setComplexity(ImagingGLWidget::High);
}

void
ViewerPrivate::asComplexityVeryHigh()
{
    d.ui->imagingglwidget->setComplexity(ImagingGLWidget::VeryHigh);
}

void
ViewerPrivate::openGithubReadme()
{
    QDesktopServices::openUrl(
        QUrl("https://github.com/mikaelsundell/usdviewer/blob/master/README.md"));
}

void
ViewerPrivate::openGithubIssues()
{
    QDesktopServices::openUrl(QUrl("https://github.com/mikaelsundell/usdviewer/issues"));
}

#include "usdviewer.moc"

Viewer::Viewer(QWidget* parent)
    : QMainWindow(parent)
    , p(new ViewerPrivate())
{
    p->d.viewer = this;
    p->init();
}

Viewer::~Viewer() {}

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
                    p->initStage(stage);
                }
                else {
                    qWarning() << "could not load stage from filename: " << filename;
                }
            }
            break;
        }
    }
}

void
Viewer::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (urls.size() == 1) {
            QString filename = urls.first().toLocalFile();
            QString extension = QFileInfo(filename).suffix().toLower();
            if (p->d.extensions.contains("." + extension)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void
Viewer::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.size() == 1) {
        QString filename = urls.first().toLocalFile();
        QString extension = QFileInfo(filename).suffix().toLower();
        if (p->d.extensions.contains("." + extension)) {
            Stage stage(filename);
            if (stage.isValid()) {
                setWindowTitle(QString("%1: %2").arg(PROJECT_NAME).arg(filename));
                p->initStage(stage);
            }
            else {
                qWarning() << "Could not load stage from filename: " << filename;
            }
        }
    }
}
}  // namespace usd
