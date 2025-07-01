// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdviewer.h"
#include "icctransform.h"
#include "mouseevent.h"
#include "platform.h"
#include "usdcontroller.h"
#include "usdinspectoritem.h"
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
#include <QToolButton>

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
    InspectorWidget* inspector();
    OutlinerWidget* outliner();
    Controller* controller();
    Selection* selection();
    QVariant settingsValue(const QString& key, const QVariant& defaultValue = QVariant());
    void setSettingsValue(const QString& key, const QVariant& value);
    bool eventFilter(QObject* object, QEvent* event);
    void enable(bool enable);
    void profile();
    void stylesheet();

public Q_SLOTS:
    void open();
    void ready();
    void copyImage();
    void clearColor();
    void exportAll();
    void exportSelected();
    void exportImage();
    void showSelected();
    void hideSelected();
    void deleteSelected();
    void frameAll();
    void frameSelected();
    void resetView();
    void clear();
    void expand();
    void collapse();
    void filterChanged(const QString& filter);
    void defaultCameraLightEnabled(bool checked);
    void sceneLightsEnabled(bool checked);
    void sceneMaterialsEnabled(bool checked);
    void drawModeChanged(int index);
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
        QScopedPointer<Controller> controller;
        QScopedPointer<Selection> selection;
        QScopedPointer<Ui_Usdviewer> ui;
        QPointer<Viewer> viewer;
    };
    Data d;
};

ViewerPrivate::ViewerPrivate() { d.extensions = { "usd", "usda", "usdc", "usdz" }; }

void
ViewerPrivate::init()
{
    platform::setDarkTheme();
    // icc profile
    ICCTransform* transform = ICCTransform::instance();
    QDir resources(platform::getApplicationPath() + "/Resources");
    QString inputProfile = resources.filePath("sRGB2014.icc");  // built-in Qt input profile
    transform->setInputProfile(inputProfile);
    profile();
    d.ui.reset(new Ui_Usdviewer());
    d.ui->setupUi(d.viewer.data());
    // clear color
    d.clearColor = QColor(settingsValue("clearColor", "#4f4f4f").toString());
    d.ui->clearcolor->setStyleSheet("background-color: " + d.clearColor.name() + ";");
    d.clearColorFilter.reset(new MouseEvent);
    d.ui->clearcolor->installEventFilter(d.clearColorFilter.data());
    // event filter
    d.viewer->installEventFilter(this);
    // controller
    d.controller.reset(new Controller());
    // selection
    d.selection.reset(new Selection());
    // renderer
    renderer()->setClearColor(d.clearColor);
    renderer()->setController(d.controller.data());
    renderer()->setSelection(d.selection.data());
    // metadata
    inspector()->setHeaderLabels(QStringList() << "Key"
                                               << "Value");
    inspector()->setColumnWidth(InspectorItem::Key, 180);
    inspector()->setColumnWidth(InspectorItem::Value, 80);
    // outliner
    outliner()->setHeaderLabels(QStringList() << "Name"
                                              << "Type"
                                              << "Visibility");
    outliner()->setColumnWidth(OutlinerItem::Name, 180);
    outliner()->setColumnWidth(OutlinerItem::Type, 80);
    outliner()->setColumnWidth(OutlinerItem::Visible, 80);
    outliner()->setController(d.controller.data());
    outliner()->setSelection(d.selection.data());
    // connect
    connect(d.ui->imagingglWidget, &ImagingGLWidget::rendererReady, this, &ViewerPrivate::ready);
    connect(d.ui->fileOpen, &QAction::triggered, this, &ViewerPrivate::open);
    connect(d.ui->fileExportAll, &QAction::triggered, this, &ViewerPrivate::exportAll);
    connect(d.ui->fileExportSelected, &QAction::triggered, this, &ViewerPrivate::exportSelected);
    connect(d.ui->fileExportImage, &QAction::triggered, this, &ViewerPrivate::exportImage);
    connect(d.ui->editCopyImage, &QAction::triggered, this, &ViewerPrivate::copyImage);
    connect(d.ui->editShow, &QAction::triggered, this, &ViewerPrivate::showSelected);
    connect(d.ui->editHide, &QAction::triggered, this, &ViewerPrivate::hideSelected);
    connect(d.ui->editDelete, &QAction::triggered, this, &ViewerPrivate::deleteSelected);
    connect(d.ui->asComplexityLow, &QAction::triggered, this, &ViewerPrivate::asComplexityLow);
    connect(d.ui->asComplexityMedium, &QAction::triggered, this, &ViewerPrivate::asComplexityMedium);
    connect(d.ui->asComplexityHigh, &QAction::triggered, this, &ViewerPrivate::asComplexityHigh);
    connect(d.ui->asComplexityVeryHigh, &QAction::triggered, this, &ViewerPrivate::asComplexityVeryHigh);
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
    connect(d.ui->displayCollapse, &QAction::triggered, this, &ViewerPrivate::collapse);
    connect(d.ui->displayExpand, &QAction::triggered, this, &ViewerPrivate::expand);
    connect(d.ui->helpGithubReadme, &QAction::triggered, this, &ViewerPrivate::openGithubReadme);
    connect(d.ui->helpGithubIssues, &QAction::triggered, this, &ViewerPrivate::openGithubIssues);
    connect(d.ui->open, &QPushButton::clicked, this, &ViewerPrivate::open);
    connect(d.ui->exportSelected, &QPushButton::clicked, this, &ViewerPrivate::exportSelected);
    connect(d.ui->exportImage, &QPushButton::clicked, this, &ViewerPrivate::exportImage);
    connect(d.ui->frameAll, &QPushButton::clicked, this, &ViewerPrivate::frameAll);
    connect(d.ui->frameSelected, &QPushButton::clicked, this, &ViewerPrivate::frameSelected);
    connect(d.ui->resetView, &QPushButton::clicked, this, &ViewerPrivate::resetView);
    connect(d.ui->filter, &QLineEdit::textChanged, this, &ViewerPrivate::filterChanged);
    connect(d.ui->clear, &QPushButton::pressed, this, &ViewerPrivate::clear);
    connect(d.ui->collapse, &QPushButton::pressed, this, &ViewerPrivate::collapse);
    connect(d.ui->expand, &QPushButton::pressed, this, &ViewerPrivate::expand);
    connect(d.ui->enableDefaultCameraLight, &QCheckBox::toggled, this, &ViewerPrivate::defaultCameraLightEnabled);
    connect(d.ui->enableSceneLights, &QCheckBox::toggled, this, &ViewerPrivate::sceneLightsEnabled);
    connect(d.ui->enableSceneMaterials, &QCheckBox::toggled, this, &ViewerPrivate::sceneMaterialsEnabled);
    connect(d.ui->drawMode, &QComboBox::currentIndexChanged, this, &ViewerPrivate::drawModeChanged);
    connect(d.ui->aov, &QComboBox::currentIndexChanged, this, &ViewerPrivate::aovChanged);
    connect(d.clearColorFilter.data(), &MouseEvent::pressed, this, &ViewerPrivate::clearColor);
    // draw modes
    {
        d.ui->drawMode->addItem("Points", QVariant::fromValue(ImagingGLWidget::Points));
        d.ui->drawMode->addItem("Wireframe", QVariant::fromValue(ImagingGLWidget::Wireframe));
        d.ui->drawMode->addItem("Wireframe On Surface", QVariant::fromValue(ImagingGLWidget::WireframeOnSurface));
        d.ui->drawMode->addItem("Shaded Flat", QVariant::fromValue(ImagingGLWidget::ShadedFlat));
        d.ui->drawMode->addItem("Shaded Smooth", QVariant::fromValue(ImagingGLWidget::ShadedSmooth));
        d.ui->drawMode->addItem("Geom Only", QVariant::fromValue(ImagingGLWidget::GeomOnly));
        d.ui->drawMode->addItem("Geom Flat", QVariant::fromValue(ImagingGLWidget::GeomFlat));
        d.ui->drawMode->addItem("Geom Smooth", QVariant::fromValue(ImagingGLWidget::GeomSmooth));
        d.ui->drawMode->setCurrentIndex(d.ui->drawMode->findData(QVariant::fromValue(ImagingGLWidget::ShadedSmooth)));
    }
    // stylesheet
    stylesheet();
// debug
#ifdef QT_DEBUG
    QMenu* menu = d.ui->menubar->addMenu("Debug");
    {
        QAction* action = new QAction("Reload stylesheet...", this);
        action->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
        menu->addAction(action);
        connect(action, &QAction::triggered, [&]() { this->stylesheet(); });
    }
#endif
}

void
ViewerPrivate::initStage(const Stage& stage)
{
    controller()->setStage(stage);
    renderer()->setStage(stage);
    inspector()->setStage(stage);
    outliner()->setStage(stage);
    d.stage = stage;
    enable(true);
}

ViewCamera
ViewerPrivate::camera()
{
    return d.ui->imagingglWidget->viewCamera();
}


ImagingGLWidget*
ViewerPrivate::renderer()
{
    return d.ui->imagingglWidget;
}

InspectorWidget*
ViewerPrivate::inspector()
{
    return d.ui->inspectorWidget;
}

OutlinerWidget*
ViewerPrivate::outliner()
{
    return d.ui->outlinerWidget;
}

Controller*
ViewerPrivate::controller()
{
    return d.controller.data();
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

bool
ViewerPrivate::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::ScreenChangeInternal) {
        profile();
        stylesheet();
    }
    return QObject::eventFilter(object, event);
}

void
ViewerPrivate::enable(bool enable)
{
    d.ui->collapse->setEnabled(enable);
    d.ui->expand->setEnabled(enable);
}

void
ViewerPrivate::profile()
{
    QString outputProfile = platform::getIccProfileUrl(d.viewer->winId());
    // icc profile
    ICCTransform* transform = ICCTransform::instance();
    transform->setOutputProfile(outputProfile);
}

void
ViewerPrivate::stylesheet()
{
    QFile stylesheet(platform::getApplicationPath() + "/Resources/App.css");
    stylesheet.open(QFile::ReadOnly);
    QString qss = stylesheet.readAll();
    QRegularExpression hslRegex("hsl\\(\\s*(\\d+)\\s*,\\s*(\\d+)%\\s*,\\s*(\\d+)%\\s*\\)");
    QString transformqss = qss;
    QRegularExpressionMatchIterator i = hslRegex.globalMatch(transformqss);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        if (match.hasMatch()) {
            if (!match.captured(1).isEmpty() && !match.captured(2).isEmpty() && !match.captured(3).isEmpty()) {
                int h = match.captured(1).toInt();
                int s = match.captured(2).toInt();
                int l = match.captured(3).toInt();
                QColor color = QColor::fromHslF(h / 360.0f, s / 100.0f, l / 100.0f);
                // icc profile
                ICCTransform* transform = ICCTransform::instance();
                color = transform->map(color.rgb());
                QString hsl = QString("hsl(%1, %2%, %3%)")
                                  .arg(color.hue() == -1 ? 0 : color.hue())
                                  .arg(static_cast<int>(color.hslSaturationF() * 100))
                                  .arg(static_cast<int>(color.lightnessF() * 100));

                transformqss.replace(match.captured(0), hsl);
            }
        }
    }
    qApp->setStyleSheet(transformqss);
}

void
ViewerPrivate::open()
{
    QString openDir = settingsValue("openDir", QDir::homePath()).toString();
    QStringList filters;
    for (const QString& ext : d.extensions) {
        filters.append("*." + ext);
    }
    QString filter = QString("USD Files (%1)").arg(filters.join(' '));
    QString filename = QFileDialog::getOpenFileName(d.viewer.data(), "Open USD File", openDir, filter);
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
    for (QString aov : renderer()->rendererAovs()) {
        d.ui->aov->addItem(aov, QVariant::fromValue(aov));  // can only be requested after renderer is ready
    }
}

void
ViewerPrivate::copyImage()
{
    QImage image = renderer()->image();
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setImage(image);
}

void
ViewerPrivate::clearColor()
{
    QColor color = QColorDialog::getColor(d.clearColor, d.viewer.data(), "Select color");
    if (color.isValid()) {
        renderer()->setClearColor(color);
        d.ui->clearcolor->setStyleSheet("background-color: " + color.name() + ";");
        setSettingsValue("clearColor", color.name());
        d.clearColor = color;
    }
}

void
ViewerPrivate::exportAll()
{
    QString exportDir = settingsValue("exportDir", QDir::homePath()).toString();
    QString defaultFormat = "usd";
    QString exportName = exportDir + "/all." + defaultFormat;
    QStringList filters;
    for (const QString& ext : d.extensions) {
        filters.append("*." + ext);
    }
    QString filter = QString("USD Files (%1)").arg(filters.join(' '));
    QString filename = QFileDialog::getSaveFileName(d.viewer.data(), "Export all ...", exportName, filter);
    if (!filename.isEmpty()) {
        if (d.stage.exportToFile(filename)) {
            setSettingsValue("exportDir", QFileInfo(filename).absolutePath());
        }
        else {
            qWarning() << "Failed to export stage to:" << filename;
        }
    }
}

void
ViewerPrivate::exportSelected()
{
    QString exportSelectedDir = settingsValue("exportSelectedDir", QDir::homePath()).toString();
    QString defaultFormat = "usd";
    QString exportName = exportSelectedDir + "/selected." + defaultFormat;
    QStringList filters;
    for (const QString& ext : d.extensions) {
        filters.append("*." + ext);
    }
    QString filter = QString("USD Files (%1)").arg(filters.join(' '));
    QString filename = QFileDialog::getSaveFileName(d.viewer.data(), "Export selected ...", exportName, filter);
    if (!filename.isEmpty()) {
        if (d.stage.exportPathsToFile(d.selection->paths(), filename)) {
            setSettingsValue("exportSelectedDir", QFileInfo(filename).absolutePath());
        }
        else {
            qWarning() << "Failed to export stage to:" << filename;
        }
    }
}

void
ViewerPrivate::exportImage()
{
    QString exportImageDir = settingsValue("exportImageDir", QDir::homePath()).toString();
    QImage image = renderer()->image();
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
    QString filename = QFileDialog::getSaveFileName(d.viewer.data(), "Export Image", exportName, filter);
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
ViewerPrivate::showSelected()
{
    if (selection()->paths().size()) {
        d.controller->visiblePaths(d.selection->paths(), true);
    }
}

void
ViewerPrivate::hideSelected()
{
    if (selection()->paths().size()) {
        d.controller->visiblePaths(d.selection->paths(), false);
    }
}

void
ViewerPrivate::deleteSelected()
{
    if (selection()->paths().size()) {
        d.controller->removePaths(d.selection->paths());
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
    if (selection()->paths().size()) {
        camera().setBoundingBox(d.stage.boundingBox(selection()->paths()));
        camera().frameAll();
        renderer()->update();
    }
}

void
ViewerPrivate::resetView()
{
    camera().resetView();
    renderer()->update();
}

void
ViewerPrivate::clear()
{
    d.ui->clear->setText(QString());
}

void
ViewerPrivate::expand()
{
    if (selection()->paths().size()) {
        outliner()->expand();
    }
}

void
ViewerPrivate::collapse()
{
    outliner()->collapse();
}

void
ViewerPrivate::filterChanged(const QString& filter)
{
    outliner()->setFilter(filter);
    d.ui->filter->setEnabled(filter.size());
}

void
ViewerPrivate::defaultCameraLightEnabled(bool checked)
{
    renderer()->setDefaultCameraLightEnabled(checked);
}

void
ViewerPrivate::sceneLightsEnabled(bool checked)
{
    renderer()->setSceneLightsEnabled(checked);
}

void
ViewerPrivate::sceneMaterialsEnabled(bool checked)
{
    renderer()->setSceneMaterialsEnabled(checked);
}

void
ViewerPrivate::drawModeChanged(int index)
{
    QVariant data = d.ui->drawMode->itemData(index);
    ImagingGLWidget::DrawMode mode = static_cast<ImagingGLWidget::DrawMode>(data.toInt());
    renderer()->setDrawMode(mode);
}

void
ViewerPrivate::aovChanged(int index)
{
    QString aov = d.ui->aov->itemData(index, Qt::UserRole).value<QString>();
    renderer()->setRendererAov(aov);
}

void
ViewerPrivate::asComplexityLow()
{
    renderer()->setComplexity(ImagingGLWidget::Low);
}

void
ViewerPrivate::asComplexityMedium()
{
    renderer()->setComplexity(ImagingGLWidget::Medium);
}

void
ViewerPrivate::asComplexityHigh()
{
    renderer()->setComplexity(ImagingGLWidget::High);
}

void
ViewerPrivate::asComplexityVeryHigh()
{
    renderer()->setComplexity(ImagingGLWidget::VeryHigh);
}

void
ViewerPrivate::openGithubReadme()
{
    QDesktopServices::openUrl(QUrl("https://github.com/mikaelsundell/usdviewer/blob/master/README.md"));
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
                QFileInfo fileInfo(filename);
                if (p->d.extensions.contains(fileInfo.suffix().toLower())) {
                    Stage stage(filename);
                    if (stage.isValid()) {
                        setWindowTitle(QString("%1: %2").arg(PROJECT_NAME).arg(filename));
                        p->initStage(stage);
                    }
                    else {
                        qWarning() << "Could not load stage from filename: " << filename;
                    }
                    return;
                }
            }
        }
    }
    if (arguments.size() == 2) {
        QString arg = arguments[1];
        const QString protocolPrefix = "usdviewer://";

        if (arg.startsWith(protocolPrefix, Qt::CaseInsensitive)) {
            QString pathEncoded = arg.mid(protocolPrefix.length());
            QString decodedPath = QUrl::fromPercentEncoding(pathEncoded.toUtf8());
#ifdef Q_OS_WIN
            decodedPath = QDir::fromNativeSeparators(decodedPath);
#endif
            QFileInfo fileInfo(decodedPath);
            if (p->d.extensions.contains(fileInfo.suffix().toLower())) {
                Stage stage(decodedPath);
                if (stage.isValid()) {
                    setWindowTitle(QString("%1: %2").arg(PROJECT_NAME).arg(decodedPath));
                    p->initStage(stage);
                }
            }
            return;
        }

        QFileInfo fileInfo(arg);
        if (p->d.extensions.contains(fileInfo.suffix().toLower())) {
            Stage stage(arg);
            if (stage.isValid()) {
                setWindowTitle(QString("%1: %2").arg(PROJECT_NAME).arg(arg));
                p->initStage(stage);
            }
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
            if (p->d.extensions.contains(extension)) {
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
        if (p->d.extensions.contains(extension)) {
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
