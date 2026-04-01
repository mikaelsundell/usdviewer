// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "viewer.h"
#include "application.h"
#include "commandstack.h"
#include "mouseevent.h"
#include "os.h"
#include "outlinerview.h"
#include "progressview.h"
#include "pythonview.h"
#include "qtutils.h"
#include "renderview.h"
#include "selectionlist.h"
#include "session.h"
#include "settings.h"
#include "stageutils.h"
#include "style.h"
#include "tracelocks.h"
#include <QActionGroup>
#include <QClipboard>
#include <QColorDialog>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QImageWriter>
#include <QMessageBox>
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
    bool loadFile(const QString& fileName);
    DockWidget* outlinerDock();
    DockWidget* progressDock();
    OutlinerView* outlinerView();
    ProgressView* progressView();
    PythonView* pythonView();
    RenderView* renderView();
    bool eventFilter(QObject* object, QEvent* event);
    void enable(bool enable);

public Q_SLOTS:
    void newFile();
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
    void stageUpY();
    void stageUpZ();
    void loadSelected();
    void loadRecursive();
    void loadVariant(int variant);
    void unloadSelected();
    void unloadRecursive();
    void deleteSelected();
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
    void togglePython(bool checked);
    void openGithubReadme();
    void openGithubIssues();

public Q_SLOTS:
    void boundingBoxChanged(const GfBBox3d& bbox);
    void selectionChanged(const QList<SdfPath>& paths);
    void primsChanged(const QList<SdfPath>& paths, const QList<SdfPath>& invalidated);
    void stageChanged(UsdStageRefPtr stage, Session::LoadPolicy policy, Session::StageStatus status);
    void stageUpChanged(Session::StageUp stageUp);
    void statusChanged(const QString& status);

public:
    void updateModified(bool modified);
    void updateRecentFiles(const QString& filename);
    void updateStatus(const QString& message, bool error = false);
    void updateWindowTitle();
    bool saveFile();
    bool saveChanges();
    void clearChanges();
    bool hasChanges() const;
    struct Data {
        Session::LoadPolicy loadPolicy;
        bool init;
        bool modified;
        int changes;
        QStringList arguments;
        QStringList extensions;
        QStringList recentFiles;
        QColor backgroundColor;
        Qt::DockWidgetArea outlinerArea;
        Qt::DockWidgetArea progressArea;
        Qt::DockWidgetArea pythonArea;
        QScopedPointer<MouseEvent> backgroundColorFilter;
        QScopedPointer<Ui_Viewer> ui;
        QPointer<Viewer> viewer;
    };
    Data d;
};

ViewerPrivate::ViewerPrivate()
{
    d.loadPolicy = Session::LoadPolicy::All;
    d.init = false;
    d.modified = false;
    d.changes = 0;
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
    d.pythonArea = d.viewer->dockWidgetArea(d.ui->pythonDock);
    pythonView()->setAttribute(Qt::WA_DeleteOnClose, false);
    renderView()->setBackgroundColor(d.backgroundColor);
    // actions
    d.ui->fileOpen->setIcon(style()->icon(Style::IconRole::Open));
    d.ui->fileExportAll->setIcon(style()->icon(Style::IconRole::Export));
    d.ui->fileExportImage->setIcon(style()->icon(Style::IconRole::ExportImage));
    d.ui->displayFrameAll->setIcon(style()->icon(Style::IconRole::FrameAll));
    d.ui->renderWireframe->setIcon(style()->icon(Style::IconRole::Wireframe));
    d.ui->renderShaded->setIcon(style()->icon(Style::IconRole::Shaded));
    // connect
    connect(d.ui->fileOpen, &QAction::triggered, this, &ViewerPrivate::open);
    connect(d.ui->policyAll, &QAction::triggered, this, [this]() {
        d.loadPolicy = Session::LoadPolicy::All;
        settings()->setValue("loadType", "all");
    });
    connect(d.ui->policyPayload, &QAction::triggered, this, [this]() {
        d.loadPolicy = Session::LoadPolicy::None;
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
    connect(d.ui->fileNew, &QAction::triggered, this, &ViewerPrivate::newFile);
    connect(d.ui->fileOpen, &QAction::triggered, this, &ViewerPrivate::open);
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
    {
        QActionGroup* actions = new QActionGroup(this);
        actions->setExclusive(true);
        {
            actions->addAction(d.ui->stageUpY);
            actions->addAction(d.ui->stageUpZ);
        }
    }
    connect(d.ui->editLoadSelected, &QAction::triggered, this, &ViewerPrivate::loadSelected);
    connect(d.ui->editLoadRecursive, &QAction::triggered, this, &ViewerPrivate::loadRecursive);
    connect(d.ui->editDeleteSelected, &QAction::triggered, this, &ViewerPrivate::deleteSelected);
    connect(d.ui->editLoadVariant1, &QAction::triggered, this, [=]() { loadVariant(0); });
    connect(d.ui->editLoadVariant2, &QAction::triggered, this, [=]() { loadVariant(1); });
    connect(d.ui->editLoadVariant3, &QAction::triggered, this, [=]() { loadVariant(2); });
    connect(d.ui->editLoadVariant4, &QAction::triggered, this, [=]() { loadVariant(3); });
    connect(d.ui->editLoadVariant5, &QAction::triggered, this, [=]() { loadVariant(4); });
    connect(d.ui->editLoadVariant6, &QAction::triggered, this, [=]() { loadVariant(5); });
    connect(d.ui->editLoadVariant7, &QAction::triggered, this, [=]() { loadVariant(7); });
    connect(d.ui->editLoadVariant8, &QAction::triggered, this, [=]() { loadVariant(8); });
    connect(d.ui->editLoadVariant9, &QAction::triggered, this, [=]() { loadVariant(9); });
    connect(d.ui->editUnloadSelected, &QAction::triggered, this, &ViewerPrivate::unloadSelected);
    connect(d.ui->editUnloadRecursive, &QAction::triggered, this, &ViewerPrivate::unloadRecursive);
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
        d.ui->open->setDefaultAction(d.ui->fileOpen);
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
    connect(session(), &Session::boundingBoxChanged, this, &ViewerPrivate::boundingBoxChanged);
    connect(session(), &Session::primsChanged, this, &ViewerPrivate::primsChanged);
    connect(session(), &Session::stageChanged, this, &ViewerPrivate::stageChanged);
    connect(session(), &Session::stageUpChanged, this, &ViewerPrivate::stageUpChanged);
    connect(session(), &Session::statusChanged, this, &ViewerPrivate::statusChanged);
    connect(session()->selectionList(), &SelectionList::selectionChanged, this, &ViewerPrivate::selectionChanged);
    // command stack
    connect(session()->commandStack(), &CommandStack::canUndoChanged, d.ui->editUndo, &QAction::setEnabled);
    connect(session()->commandStack(), &CommandStack::canRedoChanged, d.ui->editRedo, &QAction::setEnabled);
    connect(session()->commandStack(), &CommandStack::canClearChanged, d.ui->editClear, &QAction::setEnabled);
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
    connect(d.ui->pythonDock, &QDockWidget::visibilityChanged, this,
            [=](bool visible) { d.ui->viewPython->setChecked(visible); });
    // docks
    connect(d.ui->outlinerDock, &QDockWidget::visibilityChanged, this,
            [=](bool visible) { d.ui->viewOutliner->setChecked(visible); });
    connect(d.ui->progressDock, &QDockWidget::visibilityChanged, this,
            [=](bool visible) { d.ui->viewProgress->setChecked(visible); });
    connect(d.ui->pythonDock, &QDockWidget::visibilityChanged, this,
            [=](bool visible) { d.ui->viewPython->setChecked(visible); });
    connect(d.ui->viewOutliner, &QAction::toggled, this, &ViewerPrivate::toggleOutliner);
    connect(d.ui->viewProgress, &QAction::toggled, this, &ViewerPrivate::toggleProgress);
    connect(d.ui->viewPython, &QAction::toggled, this, &ViewerPrivate::togglePython);
    // setup
    renderView()->setFocus();
    initSettings();
    newFile();
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
        connect(action, &QAction::triggered, this, [this, file]() {
            if (!saveChanges())
                return;
            loadFile(file);
        });
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
        d.loadPolicy = Session::LoadPolicy::All;
        d.ui->policyAll->setChecked(true);
    }
    else {
        d.loadPolicy = Session::LoadPolicy::None;
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
ViewerPrivate::loadFile(const QString& fileName)
{
    QFileInfo fileInfo(fileName);
    if (!d.extensions.contains(fileInfo.suffix().toLower())) {
        updateStatus(QString("Unsupported file format: %1").arg(fileInfo.suffix()), true);
        return false;
    }

    QElapsedTimer timer;
    timer.start();

    if (!session()->loadFromFile(fileName, d.loadPolicy)) {
        updateStatus(QString("Failed to load file: %1").arg(fileName), true);
        return false;
    }

    const qint64 elapsedMs = timer.elapsed();
    const double elapsedSec = elapsedMs / 1000.0;

    settings()->setValue("openDir", fileInfo.absolutePath());
    updateWindowTitle();
    updateRecentFiles(QFileInfo(fileName).absoluteFilePath());
    updateStatus(QString("Loaded %1 in %2 seconds").arg(fileName).arg(QString::number(elapsedSec, 'f', 2)), false);
    clearChanges();
    return true;
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

PythonView*
ViewerPrivate::pythonView()
{
    return d.ui->pythonView;
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
                if (d.ui->viewPython->isChecked() && !d.ui->pythonDock->isVisible())
                    d.ui->progressDock->show();
            });
        }
    }
    return QObject::eventFilter(object, event);
}

void
ViewerPrivate::enable(bool enable)
{
    QList<QAction*> actions
        = { d.ui->fileReload,           d.ui->fileClose,           d.ui->fileSave,           d.ui->fileSaveAs,
            d.ui->fileSaveCopy,         d.ui->fileExportAll,       d.ui->fileExportSelected, d.ui->fileExportImage,
            d.ui->editCopyImage,        d.ui->editDeleteSelected,  d.ui->editShowSelected,   d.ui->editShowRecursive,
            d.ui->editHideSelected,     d.ui->editHideRecursive,   d.ui->editLoadRecursive,  d.ui->editLoadRecursive,
            d.ui->editUnloadSelected,   d.ui->editUnloadRecursive, d.ui->displayIsolate,     d.ui->displayFrameAll,
            d.ui->displayFrameSelected, d.ui->displayResetView,    d.ui->displayExpand,      d.ui->displayCollapse,
            d.ui->renderShaded,         d.ui->renderWireframe,     d.ui->stageUpY,           d.ui->stageUpZ };
    for (QAction* action : actions) {
        if (action)
            action->setEnabled(enable);
    }
    d.ui->editShow->setEnabled(enable);
    d.ui->editHide->setEnabled(enable);
    d.ui->backgroundColor->setEnabled(enable);
}

void
ViewerPrivate::newFile()
{
    if (!saveChanges())
        return;

    session()->commandStack()->clear();
    if (!session()->newStage(d.loadPolicy)) {
        updateStatus("Failed to create new stage", true);
        return;
    }

    d.init = false;
    updateWindowTitle();
    clearChanges();
    enable(true);
}

void
ViewerPrivate::open()
{
    if (!saveChanges())
        return;

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
    saveFile();
}

void
ViewerPrivate::saveAs()
{
    QString saveDir = settings()->value("saveDir", QDir::homePath()).toString();
    QString currentFile = session()->filename();
    QString defaultName;

    if (!currentFile.isEmpty()) {
        QFileInfo info(currentFile);
        defaultName = info.fileName();
        saveDir = info.absolutePath();
    }
    else {
        defaultName = "Untitled.usd";
    }

    QStringList filters;
    for (const QString& ext : d.extensions)
        filters.append("*." + ext);

    const QString filter = QString("USD Files (%1)").arg(filters.join(' '));
    QString filename = QFileDialog::getSaveFileName(d.viewer.data(), "Save USD File As",
                                                    QDir(saveDir).filePath(defaultName), filter);

    if (filename.isEmpty())
        return;

    if (QFileInfo(filename).suffix().isEmpty())
        filename += ".usd";

    if (session()->saveToFile(filename)) {
        settings()->setValue("saveDir", QFileInfo(filename).absolutePath());
        updateWindowTitle();
        updateRecentFiles(QFileInfo(filename).absoluteFilePath());
        clearChanges();
        updateStatus(QString("Saved %1").arg(filename), false);
    }
    else {
        updateStatus(QString("Failed to save file: %1").arg(filename), true);
    }
}

void
ViewerPrivate::saveCopy()
{
    QString copyDir = settings()->value("copyDir", QDir::homePath()).toString();
    QString currentFile = session()->filename();
    QString defaultName;

    if (!currentFile.isEmpty()) {
        QFileInfo info(currentFile);
        defaultName = QString("%1 (Copy).%2").arg(info.completeBaseName(), info.suffix());
        copyDir = info.absolutePath();
    }
    else {
        defaultName = "Untitled.usd";
    }

    QStringList filters;
    for (const QString& ext : d.extensions)
        filters.append("*." + ext);

    const QString filter = QString("USD Files (%1)").arg(filters.join(' '));
    QString filename = QFileDialog::getSaveFileName(d.viewer.data(), "Save Copy of USD File",
                                                    QDir(copyDir).filePath(defaultName), filter);

    if (filename.isEmpty())
        return;

    if (QFileInfo(filename).suffix().isEmpty())
        filename += ".usd";

    if (session()->copyToFile(filename)) {
        settings()->setValue("copyDir", QFileInfo(filename).absolutePath());
        updateStatus(QString("Saved copy %1").arg(filename), false);
    }
    else {
        updateStatus(QString("Failed to save copy: %1").arg(filename), true);
    }
}

void
ViewerPrivate::reload()
{
    if (!session()->isLoaded())
        return;
    if (!saveChanges())
        return;

    if (!session()->reload()) {
        updateStatus("Failed to reload stage", true);
        return;
    }

    session()->commandStack()->clear();
    clearChanges();
    updateStatus("Reloaded stage", false);
}

void
ViewerPrivate::close()
{
    if (session()->isLoaded()) {
        if (!saveChanges())
            return;
        session()->commandStack()->clear();
        session()->close();
        d.init = false;
        updateWindowTitle();
        clearChanges();
        enable(false);
    }
}

void
ViewerPrivate::undo()
{
    session()->commandStack()->undo();
}

void
ViewerPrivate::redo()
{
    session()->commandStack()->redo();
}

void
ViewerPrivate::clear()
{
    session()->commandStack()->clear();
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
    QString exportDir = settings()->value("exportAllDir", QDir::homePath()).toString();
    QString currentFile = session()->filename();
    QString defaultName;

    if (!currentFile.isEmpty()) {
        QFileInfo info(currentFile);
        defaultName = QString("%1 (Export all).%2").arg(info.completeBaseName(), info.suffix());
        exportDir = info.absolutePath();
    }
    else {
        defaultName = "Untitled.usd";
    }

    QStringList filters;
    for (const QString& ext : d.extensions)
        filters.append("*." + ext);

    const QString filter = QString("USD Files (%1)").arg(filters.join(' '));
    QString fileName = QFileDialog::getSaveFileName(d.viewer.data(), "Export All",
                                                    QDir(exportDir).filePath(defaultName), filter);

    if (fileName.isEmpty())
        return;

    if (QFileInfo(fileName).suffix().isEmpty())
        fileName += ".usd";

    QElapsedTimer timer;
    timer.start();

    if (session()->flattenToFile(fileName)) {
        const qint64 elapsedMs = timer.elapsed();
        const double elapsedSec = elapsedMs / 1000.0;
        settings()->setValue("exportAllDir", QFileInfo(fileName).absolutePath());
        updateStatus(QString("Exported all to %1 in %2 seconds").arg(fileName).arg(QString::number(elapsedSec, 'f', 2)),
                     false);
    }
    else {
        updateStatus(QString("Failed to export all: %1").arg(fileName), true);
    }
}

void
ViewerPrivate::exportSelected()
{
    QString exportDir = settings()->value("exportSelectedDir", QDir::homePath()).toString();
    QString currentFile = session()->filename();
    QString defaultName;

    if (!currentFile.isEmpty()) {
        QFileInfo info(currentFile);
        defaultName = QString("%1 (Export selected).%2").arg(info.completeBaseName(), info.suffix());
        exportDir = info.absolutePath();
    }
    else {
        defaultName = "Untitled.usd";
    }

    QStringList filters;
    for (const QString& ext : d.extensions)
        filters.append("*." + ext);

    const QString filter = QString("USD Files (%1)").arg(filters.join(' '));
    QString fileName = QFileDialog::getSaveFileName(d.viewer.data(), "Export Selected",
                                                    QDir(exportDir).filePath(defaultName), filter);

    if (fileName.isEmpty())
        return;

    if (QFileInfo(fileName).suffix().isEmpty())
        fileName += ".usd";

    QElapsedTimer timer;
    timer.start();

    if (session()->flattenPathsToFile(session()->selectionList()->paths(), fileName)) {
        const qint64 elapsedMs = timer.elapsed();
        const double elapsedSec = elapsedMs / 1000.0;
        settings()->setValue("exportSelectedDir", QFileInfo(fileName).absolutePath());
        updateStatus(
            QString("Exported selected to %1 in %2 seconds").arg(fileName).arg(QString::number(elapsedSec, 'f', 2)),
            false);
    }
    else {
        updateStatus(QString("Failed to export selected: %1").arg(fileName), true);
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
    QList<SdfPath> paths = session()->selectionList()->paths();
    if (paths.size()) {
        session()->commandStack()->run(new Command(showPaths(paths, false)));
    }
}

void
ViewerPrivate::showRecursive()
{
    QList<SdfPath> paths = session()->selectionList()->paths();
    if (paths.size()) {
        session()->commandStack()->run(new Command(showPaths(paths, true)));
    }
}

void
ViewerPrivate::hideSelected()
{
    QList<SdfPath> paths = session()->selectionList()->paths();
    if (paths.size()) {
        session()->commandStack()->run(new Command(hidePaths(paths, false)));
    }
}

void
ViewerPrivate::hideRecursive()
{
    QList<SdfPath> paths = session()->selectionList()->paths();
    if (paths.size()) {
        session()->commandStack()->run(new Command(hidePaths(paths, true)));
    }
}

void
ViewerPrivate::stageUpY()
{
    session()->commandStack()->run(new Command(stageUp(Session::StageUp::Y)));
}

void
ViewerPrivate::stageUpZ()
{
    session()->commandStack()->run(new Command(stageUp(Session::StageUp::Z)));
}

void
ViewerPrivate::loadSelected()
{
    const QList<SdfPath> selectedPaths = session()->selectionList()->paths();
    if (selectedPaths.isEmpty())
        return;
    const QList<SdfPath> rootPaths = stage::rootPaths(selectedPaths);
    QList<SdfPath> payloadPaths;
    {
        READ_LOCKER(locker, session()->stageLock(), "stageLock");
        const UsdStageRefPtr stage = session()->stageUnsafe();
        if (!stage)
            return;

        payloadPaths = stage::ancestorPayloadPaths(stage, rootPaths);
    }

    if (!payloadPaths.isEmpty())
        session()->commandStack()->run(new Command(loadPayloads(payloadPaths)));
}

void
ViewerPrivate::loadRecursive()
{
    const QList<SdfPath> selectedPaths = session()->selectionList()->paths();
    if (selectedPaths.isEmpty())
        return;
    const QList<SdfPath> rootPaths = stage::rootPaths(selectedPaths);
    QList<SdfPath> payloadPaths;
    {
        READ_LOCKER(locker, session()->stageLock(), "stageLock");
        const UsdStageRefPtr stage = session()->stageUnsafe();
        if (!stage)
            return;
        payloadPaths = stage::ancestorPayloadPaths(stage, rootPaths);
    }
    if (!payloadPaths.isEmpty())
        session()->commandStack()->run(new Command(loadPayloads(payloadPaths)));
}

void
ViewerPrivate::loadVariant(int variant)
{
    const QList<SdfPath> selectedPaths = session()->selectionList()->paths();
    if (selectedPaths.isEmpty() || variant < 0)
        return;

    const QList<SdfPath> rootPaths = stage::rootPaths(selectedPaths);

    QList<SdfPath> payloadPaths;
    QMap<QString, QList<QString>> variantSets;
    {
        READ_LOCKER(locker, session()->stageLock(), "stageLock");
        const UsdStageRefPtr stage = session()->stageUnsafe();
        if (!stage)
            return;

        payloadPaths = stage::ancestorPayloadPaths(stage, rootPaths);
        variantSets = stage::findVariantSets(stage, selectedPaths, true);
    }

    if (payloadPaths.isEmpty() || variantSets.isEmpty())
        return;

    int index = 0;
    for (auto it = variantSets.cbegin(); it != variantSets.cend(); ++it) {
        const QString& setName = it.key();
        const QList<QString>& values = it.value();

        for (const QString& value : values) {
            if (index == variant) {
                session()->commandStack()->run(new Command(loadPayloads(payloadPaths, setName, value)));
                return;
            }
            ++index;
        }
    }
}

void
ViewerPrivate::unloadSelected()
{
    const QList<SdfPath> selectedPaths = session()->selectionList()->paths();
    qDebug() << "unloadSelected: selected paths =" << selectedPaths.size();
    for (const SdfPath& path : selectedPaths)
        qDebug() << "unloadSelected: selected" << qt::StringToQString(path.GetString());

    if (selectedPaths.isEmpty()) {
        qDebug() << "unloadSelected: no selected paths";
        return;
    }

    const QList<SdfPath> rootPaths = stage::rootPaths(selectedPaths);
    qDebug() << "unloadSelected: root paths =" << rootPaths.size();
    for (const SdfPath& path : rootPaths)
        qDebug() << "unloadSelected: root" << qt::StringToQString(path.GetString());

    QList<SdfPath> payloadPaths;
    {
        READ_LOCKER(locker, session()->stageLock(), "stageLock");
        const UsdStageRefPtr stage = session()->stageUnsafe();
        if (!stage) {
            qDebug() << "unloadSelected: missing stage";
            return;
        }

        payloadPaths = stage::ancestorPayloadPaths(stage, rootPaths);
        qDebug() << "unloadSelected: payload paths =" << payloadPaths.size();
        for (const SdfPath& path : payloadPaths)
            qDebug() << "unloadSelected: payload" << qt::StringToQString(path.GetString());
    }

    if (payloadPaths.isEmpty()) {
        qDebug() << "unloadSelected: no payload paths found";
        return;
    }

    qDebug() << "unloadSelected: running unloadPayloads";
    session()->commandStack()->run(new Command(unloadPayloads(payloadPaths)));
}

void
ViewerPrivate::unloadRecursive()
{
    const QList<SdfPath> selectedPaths = session()->selectionList()->paths();
    if (selectedPaths.isEmpty())
        return;

    const QList<SdfPath> rootPaths = stage::rootPaths(selectedPaths);

    QList<SdfPath> payloadPaths;
    {
        READ_LOCKER(locker, session()->stageLock(), "stageLock");
        const UsdStageRefPtr stage = session()->stageUnsafe();
        if (!stage)
            return;

        payloadPaths = stage::ancestorPayloadPaths(stage, rootPaths);
    }

    if (!payloadPaths.isEmpty())
        session()->commandStack()->run(new Command(unloadPayloads(payloadPaths)));
}

void
ViewerPrivate::deleteSelected()
{
    session()->commandStack()->run(new Command(deletePaths(session()->selectionList()->paths())));
}

void
ViewerPrivate::isolate(bool checked)
{
    const QList<SdfPath> paths = (checked && !session()->selectionList()->paths().isEmpty())
                                     ? session()->selectionList()->paths()
                                     : QList<SdfPath>();
    session()->commandStack()->run(new Command(isolatePaths(paths)));
}

void
ViewerPrivate::frameAll()
{
    if (session()->isLoaded()) {
        renderView()->frameAll();
    }
}

void
ViewerPrivate::frameSelected()
{
    if (session()->selectionList()->paths().size()) {
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
    if (session()->selectionList()->paths().size()) {
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
    renderView()->setRenderMode(RenderView::RenderMode::Shaded);
}

void
ViewerPrivate::renderWireframe()
{
    renderView()->setRenderMode(RenderView::RenderMode::Wireframe);
}

void
ViewerPrivate::light()
{
    style()->setTheme(Style::Theme::Light);
    settings()->setValue("theme", "light");
}

void
ViewerPrivate::dark()
{
    style()->setTheme(Style::Theme::Dark);
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
ViewerPrivate::togglePython(bool checked)
{
    if (checked) {
        if (!d.ui->pythonDock->isVisible()) {
            d.ui->pythonDock->setFloating(false);
            if (!d.ui->pythonDock->parentWidget())
                d.viewer->addDockWidget(d.pythonArea, d.ui->pythonDock);
            d.ui->pythonDock->show();
        }
    }
    else {
        if (d.ui->pythonDock->isVisible())
            d.ui->pythonDock->hide();
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
    if (!d.init && !bbox.GetRange().IsEmpty()) {
        frameAll();
        d.init = true;
    }
}

void
ViewerPrivate::selectionChanged(const QList<SdfPath>& paths)
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
        QMap<QString, QList<QString>> variantSets = stage::findVariantSets(session()->stage(), paths, true);

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
                        QList<SdfPath> payloadPaths = stage::ancestorPayloadPaths(session()->stage(), stage::rootPaths(paths));

                        session()->commandStack()->run(new Command(loadPayloads(payloadPaths, setName, value)));
                    });
                    index++;
                }
            }
        }
    }
}

void
ViewerPrivate::primsChanged(const QList<SdfPath>& paths, const QList<SdfPath>& invalidated)
{
    if (paths.isEmpty() && invalidated.isEmpty())
        return;
    ++d.changes;
    updateModified(true);
}

void
ViewerPrivate::stageChanged(UsdStageRefPtr stage, Session::LoadPolicy policy, Session::StageStatus status)
{
    d.init = false;
    if (status == Session::StageStatus::Loaded) {
        enable(true);
    }
}

void
ViewerPrivate::stageUpChanged(Session::StageUp stageUp)
{
    d.ui->stageUpY->setChecked(stageUp == Session::StageUp::Y);
    d.ui->stageUpZ->setChecked(stageUp == Session::StageUp::Z);
}

void
ViewerPrivate::statusChanged(const QString& status)
{
    updateStatus(status);
}

void
ViewerPrivate::updateModified(bool modified)
{
    if (d.modified == modified)
        return;

    d.modified = modified;
    updateWindowTitle();
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

void
ViewerPrivate::updateWindowTitle()
{
    const QString title = QStringLiteral("%1 build:%2 (%3)").arg(PROJECT_NAME, PROJECT_BUILD_DATE, PROJECT_BUILD_CONFIG);
    if (!session()->isLoaded()) {
        d.viewer->setWindowTitle(title);
        return;
    }
    const QString filename = session()->filename();
    QString name = filename.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(filename).fileName();
    if (d.modified)
        name.prepend(QLatin1Char('*'));
    d.viewer->setWindowTitle(QStringLiteral("%1: %2").arg(title, name));
}

bool
ViewerPrivate::saveFile()
{
    const QString filename = session()->filename();
    if (filename.isEmpty()) {
        saveAs();
        return !hasChanges();
    }

    if (session()->saveToFile(filename)) {
        updateWindowTitle();
        clearChanges();
        updateStatus(QString("Saved %1").arg(filename), false);
        return true;
    }

    updateStatus(QString("Failed to save file: %1").arg(filename), true);
    return false;
}

bool
ViewerPrivate::saveChanges()
{
    if (!hasChanges())
        return true;

    QString name = session()->filename().isEmpty() ? "Untitled" : QFileInfo(session()->filename()).fileName();

    QMessageBox::StandardButton button
        = QMessageBox::warning(d.viewer.data(), PROJECT_NAME, QString("Save changes to %1?").arg(name),
                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);

    switch (button) {
    case QMessageBox::Save: return saveFile();
    case QMessageBox::Discard: return true;
    case QMessageBox::Cancel:
    default: return false;
    }
}

void
ViewerPrivate::clearChanges()
{
    d.changes = 0;
    updateModified(false);
}

bool
ViewerPrivate::hasChanges() const
{
    return d.changes > 0;
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
Viewer::closeEvent(QCloseEvent* event)
{
    if (!p->saveChanges()) {
        event->ignore();
        return;
    }
    p->saveSettings();
    event->accept();
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
        if (!p->saveChanges()) {
            event->ignore();
            return;
        }
        QString filename = urls.first().toLocalFile();
        p->loadFile(filename);
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}
}  // namespace usdviewer
