// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "viewer.h"
#include "application.h"
#include "commanddispatcher.h"
#include "commandstack.h"
#include "datamodel.h"
#include "mouseevent.h"
#include "os.h"
#include "outlinerview.h"
#include "progressview.h"
#include "qtutils.h"
#include "renderview.h"
#include "selectionmodel.h"
#include "settings.h"
#include "stageutils.h"
#include "style.h"
#include <QActionGroup>
#include <QClipboard>
#include <QColorDialog>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QImageWriter>
#include <QMimeData>
#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QTimer>
#include <QToolButton>

// generated files
#include "ui_viewer.h"

namespace usdviewer {
class ViewerPrivate : public QObject {
    Q_OBJECT
public:
    ViewerPrivate();
    void init();
    void initRecentFiles();
    void initSettings();
    bool loadFile(const QString& filename);
    DockWidget* outlinerDock();
    DockWidget* progressDock();
    OutlinerView* outlinerView();
    ProgressView* progressView();
    RenderView* renderView();
    bool eventFilter(QObject* object, QEvent* event);
    void enable(bool enable);

public Q_SLOTS:
    void open();
    void save();
    void saveAs();
    void saveCopy();
    void reload();
    void close();
    void undo();
    void redo();
    void clear();
    void copyImage();
    void backgroundColor();
    void exportAll();
    void exportSelected();
    void exportImage();
    void saveSettings();
    void exit();
    void showSelected();
    void showRecursive();
    void hideSelected();
    void hideRecursive();
    void loadSelected();
    void loadRecursive();
    void loadVariant(int variant);
    void unloadSelected();
    void unloadRecursive();
    void isolate(bool checked);
    void frameAll();
    void frameSelected();
    void resetView();
    void collapse();
    void expand();
    void cameraLight(bool checked);
    void sceneLights(bool checked);
    void sceneShaders(bool checked);
    void renderShaded();
    void renderWireframe();
    void light();
    void dark();
    void toggleOutliner(bool checked);
    void toggleProgress(bool checked);
    void openGithubReadme();
    void openGithubIssues();

public Q_SLOTS:
    void boundingBoxChanged(const GfBBox3d& bbox);
    void selectionChanged(const QList<SdfPath>& paths) const;
    void stageChanged(UsdStageRefPtr stage, DataModel::LoadPolicy policy, DataModel::StageStatus status);
    void statusChanged(const QString& status);

public:
    void updateRecentFiles(const QString& filename);
    void updateStatus(const QString& message, bool error = false);
    struct Data {
        DataModel::LoadPolicy loadPolicy;
        bool stageInit;
        QStringList arguments;
        QStringList extensions;
        QStringList recentFiles;
        QColor backgroundColor;
        Qt::DockWidgetArea outlinerArea;
        Qt::DockWidgetArea progressArea;
        QScopedPointer<MouseEvent> backgroundColorFilter;
        QScopedPointer<Ui_Viewer> ui;
        QPointer<Viewer> viewer;
    };
    Data d;
};

ViewerPrivate::ViewerPrivate()
{
    d.loadPolicy = DataModel::LoadPolicy::LoadAll;
    d.stageInit = false;
    d.extensions = { "usd", "usda", "usdc", "usdz" };
}

void
ViewerPrivate::init()
{
    os::setDarkTheme();
    d.ui.reset(new Ui_Viewer());
    d.ui->setupUi(d.viewer.data());
    // background color
    d.backgroundColor = QColor(settings()->value("backgroundColor", "#4f4f4f").toString());
    d.ui->backgroundColor->setStyleSheet("background-color: " + d.backgroundColor.name() + ";");
    d.backgroundColorFilter.reset(new MouseEvent);
    d.ui->backgroundColor->installEventFilter(d.backgroundColorFilter.data());
    // event filter
    d.viewer->installEventFilter(this);
    // views
    d.outlinerArea = d.viewer->dockWidgetArea(d.ui->outlinerDock);
    outlinerView()->setAttribute(Qt::WA_DeleteOnClose, false);
    d.progressArea = d.viewer->dockWidgetArea(d.ui->progressDock);
    progressView()->setAttribute(Qt::WA_DeleteOnClose, false);
    renderView()->setBackgroundColor(d.backgroundColor);
    // connect
    connect(d.ui->fileOpen, &QAction::triggered, this, &ViewerPrivate::open);
    connect(d.ui->policyAll, &QAction::triggered, this, [this]() {
        d.loadPolicy = DataModel::LoadAll;
        settings()->setValue("loadType", "all");
    });
    connect(d.ui->policyPayload, &QAction::triggered, this, [this]() {
        d.loadPolicy = DataModel::LoadPayload;
        settings()->setValue("loadType", "payload");
    });
    {
        QActionGroup* actions = new QActionGroup(this);
        actions->setExclusive(true);
        {
            actions->addAction(d.ui->policyAll);
            actions->addAction(d.ui->policyPayload);
        }
    }
    connect(d.ui->fileSave, &QAction::triggered, this, &ViewerPrivate::save);
    connect(d.ui->fileSaveAs, &QAction::triggered, this, &ViewerPrivate::saveAs);
    connect(d.ui->fileSaveCopy, &QAction::triggered, this, &ViewerPrivate::saveCopy);
    connect(d.ui->fileReload, &QAction::triggered, this, &ViewerPrivate::reload);
    connect(d.ui->fileClose, &QAction::triggered, this, &ViewerPrivate::close);
    connect(d.ui->fileExportAll, &QAction::triggered, this, &ViewerPrivate::exportAll);
    connect(d.ui->fileExportSelected, &QAction::triggered, this, &ViewerPrivate::exportSelected);
    connect(d.ui->fileExportImage, &QAction::triggered, this, &ViewerPrivate::exportImage);
    connect(d.ui->fileSaveSettings, &QAction::triggered, this, &ViewerPrivate::saveSettings);
    connect(d.ui->fileExit, &QAction::triggered, this, &ViewerPrivate::exit);
    connect(d.ui->editUndo, &QAction::triggered, this, &ViewerPrivate::undo);
    connect(d.ui->editRedo, &QAction::triggered, this, &ViewerPrivate::redo);
    connect(d.ui->editClear, &QAction::triggered, this, &ViewerPrivate::clear);
    connect(d.ui->editCopyImage, &QAction::triggered, this, &ViewerPrivate::copyImage);
    connect(d.ui->editShowSelected, &QAction::triggered, this, &ViewerPrivate::showSelected);
    connect(d.ui->editShowRecursive, &QAction::triggered, this, &ViewerPrivate::showRecursive);
    connect(d.ui->editHideSelected, &QAction::triggered, this, &ViewerPrivate::hideSelected);
    connect(d.ui->editHideRecursive, &QAction::triggered, this, &ViewerPrivate::hideRecursive);
    connect(d.ui->editLoadSelected, &QAction::triggered, this, &ViewerPrivate::loadSelected);
    connect(d.ui->editLoadRecursive, &QAction::triggered, this, &ViewerPrivate::loadRecursive);
    connect(d.ui->editLoadVariant1, &QAction::triggered, this, [=]() { loadVariant(0); });
    connect(d.ui->editLoadVariant2, &QAction::triggered, this, [=]() { loadVariant(1); });
    connect(d.ui->editLoadVariant3, &QAction::triggered, this, [=]() { loadVariant(2); });
    connect(d.ui->editLoadVariant4, &QAction::triggered, this, [=]() { loadVariant(3); });
    connect(d.ui->editLoadVariant5, &QAction::triggered, this, [=]() { loadVariant(4); });
    connect(d.ui->editLoadVariant6, &QAction::triggered, this, [=]() { loadVariant(5); });
    connect(d.ui->editLoadVariant7, &QAction::triggered, this, [=]() { loadVariant(7); });
    connect(d.ui->editLoadVariant8, &QAction::triggered, this, [=]() { loadVariant(8); });
    connect(d.ui->editLoadVariant9, &QAction::triggered, this, [=]() { loadVariant(9); });
    connect(d.ui->editUnloadSelected, &QAction::triggered, this, &ViewerPrivate::hideSelected);
    connect(d.ui->editUnloadRecursive, &QAction::triggered, this, &ViewerPrivate::hideRecursive);
    connect(d.ui->displayIsolate, &QAction::toggled, this, &ViewerPrivate::isolate);
    connect(d.ui->displayCameraLight, &QAction::toggled, this, &ViewerPrivate::cameraLight);
    connect(d.ui->displaySceneLights, &QAction::toggled, this, &ViewerPrivate::sceneLights);
    connect(d.ui->displaySceneShaders, &QAction::toggled, this, &ViewerPrivate::sceneShaders);
    connect(d.ui->renderShaded, &QAction::triggered, this, &ViewerPrivate::renderShaded);
    connect(d.ui->renderWireframe, &QAction::triggered, this, &ViewerPrivate::renderWireframe);
    {
        QActionGroup* actions = new QActionGroup(this);
        actions->setExclusive(true);
        {
            actions->addAction(d.ui->renderShaded);
            actions->addAction(d.ui->renderWireframe);
        }
    }
    connect(d.ui->displayFrameAll, &QAction::triggered, this, &ViewerPrivate::frameAll);
    connect(d.ui->displayFrameSelected, &QAction::triggered, this, &ViewerPrivate::frameSelected);
    connect(d.ui->displayResetView, &QAction::triggered, this, &ViewerPrivate::resetView);
    connect(d.ui->displayCollapse, &QAction::triggered, this, &ViewerPrivate::collapse);
    connect(d.ui->displayExpand, &QAction::triggered, this, &ViewerPrivate::expand);
    connect(d.ui->helpGithubReadme, &QAction::triggered, this, &ViewerPrivate::openGithubReadme);
    connect(d.ui->helpGithubIssues, &QAction::triggered, this, &ViewerPrivate::openGithubIssues);
    connect(d.ui->open, &QToolButton::clicked, this, &ViewerPrivate::open);
    {
        d.ui->exportImage->setDefaultAction(d.ui->fileExportImage);
        d.ui->exportAll->setDefaultAction(d.ui->fileExportAll);
        d.ui->frameAll->setDefaultAction(d.ui->displayFrameAll);
        d.ui->wireframe->setDefaultAction(d.ui->renderShaded);
        d.ui->shaded->setDefaultAction(d.ui->renderWireframe);
    }
    connect(d.backgroundColorFilter.data(), &MouseEvent::pressed, this, &ViewerPrivate::backgroundColor);
    connect(d.ui->themeLight, &QAction::triggered, this, &ViewerPrivate::light);
    connect(d.ui->themeDark, &QAction::triggered, this, &ViewerPrivate::dark);
    {
        QActionGroup* actions = new QActionGroup(this);
        actions->setExclusive(true);
        {
            actions->addAction(d.ui->themeLight);
            actions->addAction(d.ui->themeDark);
        }
    }
    // models
    connect(selectionModel(), &SelectionModel::selectionChanged, this, &ViewerPrivate::selectionChanged);
    connect(dataModel(), &DataModel::boundingBoxChanged, this, &ViewerPrivate::boundingBoxChanged);
    connect(dataModel(), &DataModel::stageChanged, this, &ViewerPrivate::stageChanged);
    connect(dataModel(), &DataModel::statusChanged, this, &ViewerPrivate::statusChanged);
    // command stack
    connect(commandStack(), &CommandStack::canUndoChanged, d.ui->editUndo, &QAction::setEnabled);
    connect(commandStack(), &CommandStack::canRedoChanged, d.ui->editRedo, &QAction::setEnabled);
    connect(commandStack(), &CommandStack::canClearChanged, d.ui->editClear, &QAction::setEnabled);
    // views
    connect(d.ui->hudSceneTree, &QAction::toggled, this, [=](bool checked) { renderView()->enableSceneTree(checked); });
    connect(d.ui->hudGpuPerformance, &QAction::toggled, this,
            [=](bool checked) { renderView()->enableGpuPerformance(checked); });
    connect(d.ui->hudCameraAxis, &QAction::toggled, this,
            [=](bool checked) { renderView()->enableCameraAxis(checked); });
    connect(d.ui->outlinerDock, &QDockWidget::visibilityChanged, this,
            [=](bool visible) { d.ui->viewOutliner->setChecked(visible); });
    connect(d.ui->progressDock, &QDockWidget::visibilityChanged, this,
            [=](bool visible) { d.ui->viewProgress->setChecked(visible); });
    // docks
    connect(d.ui->outlinerDock, &QDockWidget::visibilityChanged, this,
            [=](bool visible) { d.ui->viewOutliner->setChecked(visible); });
    connect(d.ui->progressDock, &QDockWidget::visibilityChanged, this,
            [=](bool visible) { d.ui->viewProgress->setChecked(visible); });
    connect(d.ui->viewOutliner, &QAction::toggled, this, &ViewerPrivate::toggleOutliner);
    connect(d.ui->viewProgress, &QAction::toggled, this, &ViewerPrivate::toggleProgress);
    // setup
    progressDock()->hide();
    renderView()->setFocus();
    initSettings();
    enable(false);
}

void
ViewerPrivate::initRecentFiles()
{
    QMenu* recentMenu = d.ui->fileRecent;
    if (!recentMenu)
        return;

    recentMenu->clear();
    if (d.recentFiles.isEmpty()) {
        QAction* emptyAction = new QAction("No recent files", recentMenu);
        emptyAction->setEnabled(false);
        recentMenu->addAction(emptyAction);
        return;
    }

    for (const QString& file : d.recentFiles) {
        QString fileName = QFileInfo(file).fileName();
        QAction* action = new QAction(fileName, recentMenu);
        action->setToolTip(file);
        action->setData(file);
        connect(action, &QAction::triggered, this, [this, file]() { loadFile(file); });
        recentMenu->addAction(action);
    }
    recentMenu->addSeparator();
    QAction* clearAction = new QAction("Clear", recentMenu);
    connect(clearAction, &QAction::triggered, this, [this, recentMenu]() {
        d.recentFiles.clear();
        settings()->setValue("recentFiles", QStringList());
        initRecentFiles();
    });
    recentMenu->addAction(clearAction);
}

void
ViewerPrivate::initSettings()
{
    QString loadType = settings()->value("loadType", "all").toString();
    if (loadType == "all") {
        d.loadPolicy = DataModel::LoadAll;
        d.ui->policyAll->setChecked(true);
    }
    else {
        d.loadPolicy = DataModel::LoadPayload;
        d.ui->policyPayload->setChecked(true);
    }

    bool sceneTree = settings()->value("sceneTree", true).toBool();
    d.ui->hudSceneTree->setChecked(sceneTree);
    renderView()->enableSceneTree(sceneTree);

    bool gpuPerformance = settings()->value("gpuPerformance", false).toBool();
    d.ui->hudGpuPerformance->setChecked(gpuPerformance);
    renderView()->enableGpuPerformance(gpuPerformance);

    bool cameraAxis = settings()->value("cameraAxis", true).toBool();
    d.ui->hudCameraAxis->setChecked(cameraAxis);
    renderView()->enableCameraAxis(cameraAxis);

    QString theme = settings()->value("theme", "dark").toString();
    if (theme == "dark") {
        dark();
        d.ui->themeDark->setChecked(true);
    }
    else {
        light();
        d.ui->themeLight->setChecked(true);
    }
    d.recentFiles = settings()->value("recentFiles", QStringList()).toStringList();
    initRecentFiles();
}

bool
ViewerPrivate::loadFile(const QString& filename)
{
    QFileInfo fileInfo(filename);
    if (!d.extensions.contains(fileInfo.suffix().toLower())) {
        updateStatus(QString("unsupported file format: %1").arg(fileInfo.suffix()), true);
        return false;
    }
    QElapsedTimer timer;
    timer.start();

    dataModel()->loadFromFile(filename, d.loadPolicy);
    if (dataModel()->isLoaded()) {
        qint64 elapsedMs = timer.elapsed();
        double elapsedSec = elapsedMs / 1000.0;

        QString shortName = fileInfo.fileName();
        d.viewer->setWindowTitle(QString("%1: %2").arg(PROJECT_NAME).arg(shortName));
        settings()->setValue("openDir", fileInfo.absolutePath());
        updateRecentFiles(filename);
        updateStatus(QString("Loaded %1 in %2 seconds").arg(shortName).arg(QString::number(elapsedSec, 'f', 2)), false);
        return true;
    }
    else {
        updateStatus(QString("Failed to load file: %1").arg(fileInfo.fileName()), true);
        return false;
    }
}

DockWidget*
ViewerPrivate::outlinerDock()
{
    return d.ui->outlinerDock;
}

DockWidget*
ViewerPrivate::progressDock()
{
    return d.ui->progressDock;
}

OutlinerView*
ViewerPrivate::outlinerView()
{
    return d.ui->outlinerView;
}

ProgressView*
ViewerPrivate::progressView()
{
    return d.ui->progressView;
}

RenderView*
ViewerPrivate::renderView()
{
    return d.ui->renderView;
}

bool
ViewerPrivate::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange) {
        Qt::WindowStates state = d.viewer->windowState();
        if (!(state & Qt::WindowMinimized)) {
            QTimer::singleShot(0, d.viewer, [this]() {
                if (d.ui->viewOutliner->isChecked() && !d.ui->outlinerDock->isVisible())
                    d.ui->outlinerDock->show();
                if (d.ui->viewProgress->isChecked() && !d.ui->progressDock->isVisible())
                    d.ui->progressDock->show();
            });
        }
    }
    return QObject::eventFilter(object, event);
}

void
ViewerPrivate::enable(bool enable)
{
    QList<QAction*> actions = { d.ui->fileReload,           d.ui->fileClose,        d.ui->fileSave,
                                d.ui->fileSaveAs,           d.ui->fileSaveCopy,     d.ui->fileExportAll,
                                d.ui->fileExportSelected,   d.ui->fileExportImage,  d.ui->editCopyImage,
                                d.ui->editDelete,           d.ui->displayIsolate,   d.ui->displayFrameAll,
                                d.ui->displayFrameSelected, d.ui->displayResetView, d.ui->displayExpand,
                                d.ui->displayCollapse,      d.ui->renderShaded,     d.ui->renderWireframe };
    for (QAction* action : actions) {
        if (action)
            action->setEnabled(enable);
    }
    d.ui->editShow->setEnabled(enable);
    d.ui->editHide->setEnabled(enable);
    d.ui->backgroundColor->setEnabled(enable);
}

void
ViewerPrivate::open()
{
    QString openDir = settings()->value("openDir", QDir::homePath()).toString();
    QStringList filters;
    for (const QString& ext : d.extensions) {
        filters.append("*." + ext);
    }
    QString filter = QString("USD Files (%1)").arg(filters.join(' '));
    QString filename = QFileDialog::getOpenFileName(d.viewer.data(), "Open USD File", openDir, filter);
    if (filename.size()) {
        loadFile(filename);
    }
}

void
ViewerPrivate::save()
{
    QString filename = dataModel()->filename();
    if (filename.isEmpty()) {
        saveAs();
        return;
    }
    if (dataModel()->saveToFile(filename)) {
        d.viewer->setWindowTitle(QString("%1: %2").arg(PROJECT_NAME).arg(filename));
    }
}

void
ViewerPrivate::saveAs()
{
    QString saveDir = settings()->value("saveDir", QDir::homePath()).toString();
    QString currentFile = dataModel()->filename();
    QString defaultName;

    if (!currentFile.isEmpty()) {
        QFileInfo info(currentFile);
        defaultName = info.fileName();
        saveDir = info.absolutePath();
    }
    else {
        defaultName = "untitled.usd";
    }

    QStringList filters;
    for (const QString& ext : d.extensions)
        filters.append("*." + ext);

    QString filter = QString("USD files (%1)").arg(filters.join(' '));
    QString filename = QFileDialog::getSaveFileName(d.viewer.data(), "Save USD file as",
                                                    QDir(saveDir).filePath(defaultName), filter);

    if (filename.isEmpty())
        return;

    if (dataModel()->saveToFile(filename)) {
        settings()->setValue("saveDir", QFileInfo(filename).absolutePath());
        d.viewer->setWindowTitle(QString("%1: %2").arg(PROJECT_NAME).arg(filename));
        updateRecentFiles(filename);
    }
}

void
ViewerPrivate::saveCopy()
{
    QString copyDir = settings()->value("copyDir", QDir::homePath()).toString();
    QString currentFile = dataModel()->filename();
    QString defaultName;

    if (!currentFile.isEmpty()) {
        QFileInfo info(currentFile);
        defaultName = QString("%1 (Copy).%2").arg(info.completeBaseName(), info.suffix());
        copyDir = info.absolutePath();
    }
    else {
        defaultName = "untitled.usd";
    }

    QStringList filters;
    for (const QString& ext : d.extensions)
        filters.append("*." + ext);

    QString filter = QString("USD Files (%1)").arg(filters.join(' '));
    QString filename = QFileDialog::getSaveFileName(d.viewer.data(), "Save copy of USD file",
                                                    QDir(copyDir).filePath(defaultName), filter);

    if (filename.isEmpty())
        return;

    if (dataModel()->exportToFile(filename)) {
        settings()->setValue("copyDir", QFileInfo(filename).absolutePath());
    }
}

void
ViewerPrivate::reload()
{
    if (dataModel()->isLoaded()) {
        commandStack()->clear();
        dataModel()->reload();
    }
}

void
ViewerPrivate::close()
{
    if (dataModel()->isLoaded()) {
        commandStack()->clear();
        dataModel()->close();
        d.viewer->setWindowTitle(QString("%1").arg(PROJECT_NAME));
        enable(false);
    }
}

void
ViewerPrivate::undo()
{
    commandStack()->undo();
}

void
ViewerPrivate::redo()
{
    commandStack()->redo();
}

void
ViewerPrivate::clear()
{
    commandStack()->clear();
}

void
ViewerPrivate::copyImage()
{
    QImage image = renderView()->captureImage();
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setImage(image);
}

void
ViewerPrivate::backgroundColor()
{
    QColor color = QColorDialog::getColor(d.backgroundColor, d.viewer.data(), "Select color");
    if (color.isValid()) {
        renderView()->setBackgroundColor(color);
        d.ui->backgroundColor->setStyleSheet("background-color: " + color.name() + ";");
        settings()->setValue("backgroundColor", color.name());
        d.backgroundColor = color;
    }
}

void
ViewerPrivate::exportAll()
{
    QString exportDir = settings()->value("exportDir", QDir::homePath()).toString();
    QString defaultFormat = "usd";
    QString exportName = exportDir + "/all." + defaultFormat;
    QStringList filters;
    for (const QString& ext : d.extensions) {
        filters.append("*." + ext);
    }
    QString filter = QString("USD Files (%1)").arg(filters.join(' '));
    QString filename = QFileDialog::getSaveFileName(d.viewer.data(), "Export all ...", exportName, filter);
    if (!filename.isEmpty()) {
        if (dataModel()->exportToFile(filename)) {
            settings()->setValue("exportDir", QFileInfo(filename).absolutePath());
        }
        else {
            qWarning() << "Failed to export stage to:" << filename;
        }
    }
}

void
ViewerPrivate::exportSelected()
{
    QString exportSelectedDir = settings()->value("exportSelectedDir", QDir::homePath()).toString();
    QString defaultFormat = "usd";
    QString exportName = exportSelectedDir + "/selected." + defaultFormat;
    QStringList filters;
    for (const QString& ext : d.extensions) {
        filters.append("*." + ext);
    }
    QString filter = QString("USD Files (%1)").arg(filters.join(' '));
    QString filename = QFileDialog::getSaveFileName(d.viewer.data(), "Export selected ...", exportName, filter);
    if (!filename.isEmpty()) {
        if (dataModel()->exportPathsToFile(selectionModel()->paths(), filename)) {
            settings()->value("exportSelectedDir", QFileInfo(filename).absolutePath());
        }
        else {
            qWarning() << "Failed to export stage to:" << filename;
        }
    }
}

void
ViewerPrivate::exportImage()
{
    QString exportImageDir = settings()->value("exportImageDir", QDir::homePath()).toString();
    QImage image = renderView()->captureImage();
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
            settings()->setValue("exportImageDir", QFileInfo(filename).absolutePath());
        }
        else {
            qWarning() << "failed to save image: " << filename;
        }
    }
}

void
ViewerPrivate::saveSettings()
{
    settings()->setValue("recentFiles", d.recentFiles);
    settings()->setValue("sceneTree", d.ui->hudSceneTree->isChecked());
    settings()->setValue("gpuPerformance", d.ui->hudGpuPerformance->isChecked());
    settings()->setValue("cameraAxis", d.ui->hudCameraAxis->isChecked());
}

void
ViewerPrivate::exit()
{
    d.viewer->close();
}

void
ViewerPrivate::showSelected()
{
    QList<SdfPath> paths = selectionModel()->paths();
    if (paths.size()) {
        CommandDispatcher::run(new Command(showPaths(paths, false)));
    }
}

void
ViewerPrivate::showRecursive()
{
    QList<SdfPath> paths = selectionModel()->paths();
    if (paths.size()) {
        CommandDispatcher::run(new Command(showPaths(paths, true)));
    }
}

void
ViewerPrivate::hideSelected()
{
    QList<SdfPath> paths = selectionModel()->paths();
    if (paths.size()) {
        CommandDispatcher::run(new Command(hidePaths(paths, false)));
    }
}

void
ViewerPrivate::hideRecursive()
{
    QList<SdfPath> paths = selectionModel()->paths();
    if (paths.size()) {
        CommandDispatcher::run(new Command(hidePaths(paths, true)));
    }
}

void
ViewerPrivate::loadSelected()
{
    qDebug() << "loadSelected";
}

void
ViewerPrivate::loadRecursive()
{
    qDebug() << "loadRecursive";
}

void
ViewerPrivate::loadVariant(int variant)
{
    qDebug() << "loadVariant";
}

void
ViewerPrivate::unloadSelected()
{
    qDebug() << "unloadSelected";
}

void
ViewerPrivate::unloadRecursive()
{
    qDebug() << "unloadRecursive";
}

void
ViewerPrivate::isolate(bool checked)
{
    if (checked) {
        if (selectionModel()->paths().size()) {
            dataModel()->setMask(selectionModel()->paths());
        }
    }
    else {
        dataModel()->setMask(QList<SdfPath>());
    }
}

void
ViewerPrivate::frameAll()
{
    if (dataModel()->isLoaded()) {
        renderView()->frameAll();
    }
}

void
ViewerPrivate::frameSelected()
{
    if (selectionModel()->paths().size()) {
        renderView()->frameSelected();
    }
}

void
ViewerPrivate::resetView()
{
    renderView()->resetView();
}

void
ViewerPrivate::collapse()
{
    outlinerView()->collapse();
}

void
ViewerPrivate::expand()
{
    if (selectionModel()->paths().size()) {
        outlinerView()->expand();
    }
}

void
ViewerPrivate::cameraLight(bool checked)
{
    renderView()->setDefaultCameraLightEnabled(checked);
}

void
ViewerPrivate::sceneLights(bool checked)
{
    renderView()->setSceneLightsEnabled(checked);
}

void
ViewerPrivate::sceneShaders(bool checked)
{
    renderView()->setSceneMaterialsEnabled(checked);
}

void
ViewerPrivate::renderShaded()
{
    renderView()->setDrawMode(RenderView::render_mode::render_shaded);
}

void
ViewerPrivate::renderWireframe()
{
    renderView()->setDrawMode(RenderView::render_mode::render_wireframe);
}

void
ViewerPrivate::light()
{
    style()->setTheme(Style::ThemeLight);
    settings()->setValue("theme", "light");
}

void
ViewerPrivate::dark()
{
    style()->setTheme(Style::ThemeDark);
    settings()->setValue("theme", "dark");
}

void
ViewerPrivate::toggleOutliner(bool checked)
{
    if (checked) {
        if (!d.ui->outlinerDock->isVisible()) {
            d.ui->outlinerDock->setFloating(false);
            if (!d.ui->outlinerDock->parentWidget())
                d.viewer->addDockWidget(d.outlinerArea, d.ui->outlinerDock);
            d.ui->outlinerDock->show();
        }
    }
    else {
        if (d.ui->outlinerDock->isVisible())
            d.ui->outlinerDock->hide();
    }
}

void
ViewerPrivate::toggleProgress(bool checked)
{
    if (checked) {
        if (!d.ui->progressDock->isVisible()) {
            d.ui->progressDock->setFloating(false);
            if (!d.ui->progressDock->parentWidget())
                d.viewer->addDockWidget(d.progressArea, d.ui->progressDock);
            d.ui->progressDock->show();
        }
    }
    else {
        if (d.ui->progressDock->isVisible())
            d.ui->progressDock->hide();
    }
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

void
ViewerPrivate::boundingBoxChanged(const GfBBox3d& bbox)
{
    if (!d.stageInit && !bbox.GetRange().IsEmpty()) {
        frameAll();
        d.stageInit = true;
    }
}

void
ViewerPrivate::selectionChanged(const QList<SdfPath>& paths) const
{
    if (paths.size()) {
        d.ui->displayExpand->setEnabled(true);
        d.ui->displayIsolate->setEnabled(true);
    }
    else {
        d.ui->displayExpand->setEnabled(false);
        d.ui->displayIsolate->setEnabled(false);
    }
    for (QObject* child : d.ui->editLoad->children()) {
        QMenu* m = qobject_cast<QMenu*>(child);
        if (m && m->property("variantMenu").toBool()) {
            delete m;
        }
    }
    if (!paths.isEmpty()) {
        QMap<QString, QList<QString>> variantSets = stage::findVariantSets(dataModel()->stage(), paths, true);

        if (!variantSets.isEmpty()) {
            d.ui->editLoad->addSeparator();
            int index = 0;
            for (auto it = variantSets.begin(); it != variantSets.end(); ++it) {
                const QString& setName = it.key();
                const QList<QString>& values = it.value();

                QMenu* setMenu = d.ui->editLoad->addMenu(setName);
                setMenu->setProperty("variantMenu", true);

                for (const QString& value : values) {
                    QAction* action = setMenu->addAction(value);
                    if (index < 10) {
                        int key = (index == 9) ? Qt::Key_0 : (Qt::Key_1 + index);
                        action->setShortcut(QKeySequence(key));
                    }
                    QObject::connect(action, &QAction::triggered, d.viewer, [this, paths, setName, value]() {
                        QList<SdfPath> payloadPaths = stage::payloadPaths(dataModel()->stage(),
                                                                          stage::rootPaths(paths));

                        CommandDispatcher::run(new Command(loadPayloads(payloadPaths, setName, value)));
                    });
                    index++;
                }
            }
        }
    }
}

void
ViewerPrivate::stageChanged(UsdStageRefPtr stage, DataModel::LoadPolicy policy, DataModel::StageStatus status)
{
    d.stageInit = false;
    if (status == DataModel::StageLoaded) {
        enable(true);
    }
}

void
ViewerPrivate::statusChanged(const QString& status)
{
    updateStatus(status);
}

void
ViewerPrivate::updateRecentFiles(const QString& filename)
{
    d.recentFiles.removeAll(filename);
    d.recentFiles.prepend(filename);
    const int maxRecent = 10;
    while (d.recentFiles.size() > maxRecent)
        d.recentFiles.removeLast();
    settings()->setValue("recentFiles", d.recentFiles);
    initRecentFiles();
}

void
ViewerPrivate::updateStatus(const QString& message, bool error)
{
    QStatusBar* bar = d.ui->statusbar;
    QString text = error ? QString(" error: %1").arg(message) : QString(" %1").arg(message);
    int timeoutMs = 6000;
    bar->showMessage(text, timeoutMs);
    QTimer::singleShot(timeoutMs, bar, [bar]() { bar->showMessage(" Ready."); });
}

#include "viewer.moc"

Viewer::Viewer(QWidget* parent)
    : QMainWindow(parent)
    , p(new ViewerPrivate())
{
    p->d.viewer = this;
    p->init();
}

Viewer::~Viewer() = default;

void
Viewer::setArguments(const QStringList& arguments)
{
    p->d.arguments = arguments;
    for (int i = 0; i < arguments.size(); ++i) {
        if (arguments[i] == "--open" && i + 1 < arguments.size()) {
            QString filename = arguments[i + 1];
            if (!filename.isEmpty()) {
                p->loadFile(filename);
                return;
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
            p->loadFile(decodedPath);
            return;
        }
        p->loadFile(arg);
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
        p->loadFile(filename);
    }
}
}  // namespace usdviewer
