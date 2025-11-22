// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdviewer.h"
#include "commanddispatcher.h"
#include "commandstack.h"
#include "icctransform.h"
#include "mouseevent.h"
#include "platform.h"
#include "stylesheet.h"
#include "usdoutlinerview.h"
#include "usdpayloadview.h"
#include "usdrenderview.h"
#include "usdstagemodel.h"
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
#include <QTimer>
#include <QToolButton>

// generated files
#include "ui_usdviewer.h"

namespace usd {
class ViewerPrivate : public QObject {
    Q_OBJECT
public:
    ViewerPrivate();
    void init();
    void initRecentFiles();
    void initSettings();
    bool loadFile(const QString& filename);
    OutlinerView* outlinerView();
    PayloadView* payloadView();
    RenderView* renderView();
    bool eventFilter(QObject* object, QEvent* event);
    void enable(bool enable);
    void profile();
    void stylesheet();
    QVariant settingsValue(const QString& key, const QVariant& defaultValue = QVariant());
    void setSettingsValue(const QString& key, const QVariant& value);
    void loadSettings();
    void saveSettings();

public Q_SLOTS:
    void open();
    void save();
    void saveAs();
    void saveCopy();
    void reload();
    void close();
    void ready();
    void copyImage();
    void backgroundColor();
    void exportAll();
    void exportSelected();
    void exportImage();
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
    void defaultCameraLightEnabled(bool checked);
    void sceneLightsEnabled(bool checked);
    void sceneMaterialsEnabled(bool checked);
    void wireframeChanged(bool checked);
    void light();
    void dark();
    void openGithubReadme();
    void openGithubIssues();

public Q_SLOTS:
    void boundingBoxChanged(const GfBBox3d& bbox);
    void selectionChanged(const QList<SdfPath>& paths) const;
    void stageChanged(UsdStageRefPtr stage);

public:
    void updateRecentFiles(const QString& filename);
    void updateStatus(const QString& message, bool error = false);
    struct Data {
        bool loadInit;
        StageModel::load_policy loadPolicy;
        QStringList arguments;
        QStringList extensions;
        QStringList recentFiles;
        QColor backgroundColor;
        QScopedPointer<MouseEvent> backgroundColorFilter;
        QScopedPointer<StageModel> stageModel;
        QScopedPointer<SelectionModel> selectionModel;
        QScopedPointer<CommandStack> commandStack;
        QScopedPointer<Ui_UsdViewer> ui;
        QPointer<Viewer> viewer;
    };
    Data d;
};

ViewerPrivate::ViewerPrivate()
{
    d.loadInit = false;
    d.loadPolicy = StageModel::load_policy::load_all;
    d.extensions = { "usd", "usda", "usdc", "usdz" };
}

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
    d.ui.reset(new Ui_UsdViewer());
    d.ui->setupUi(d.viewer.data());
    // settings
    loadSettings();
    // background color
    d.backgroundColor = QColor(settingsValue("backgroundColor", "#4f4f4f").toString());
    d.ui->backgroundColor->setStyleSheet("background-color: " + d.backgroundColor.name() + ";");
    d.backgroundColorFilter.reset(new MouseEvent);
    d.ui->backgroundColor->installEventFilter(d.backgroundColorFilter.data());
    // event filter
    d.viewer->installEventFilter(this);
    // models
    d.stageModel.reset(new StageModel());
    d.selectionModel.reset(new SelectionModel());
    // command
    d.commandStack.reset(new CommandStack());
    d.commandStack->setStageModel(d.stageModel.data());
    d.commandStack->setSelectionModel(d.selectionModel.data());
    CommandDispatcher::setCommandStack(d.commandStack.data());
    // views
    outlinerView()->setStageModel(d.stageModel.data());
    outlinerView()->setSelectionModel(d.selectionModel.data());
    payloadView()->setStageModel(d.stageModel.data());
    payloadView()->setSelectionModel(d.selectionModel.data());
    renderView()->setBackgroundColor(d.backgroundColor);
    renderView()->setStageModel(d.stageModel.data());
    renderView()->setSelectionModel(d.selectionModel.data());
    // connect
    connect(renderView(), &RenderView::renderReady, this, &ViewerPrivate::ready);
    connect(d.ui->fileOpen, &QAction::triggered, this, &ViewerPrivate::open);
    connect(d.ui->policyAll, &QAction::triggered, this, [this]() {
        d.loadPolicy = StageModel::load_all;
        setSettingsValue("loadType", "all");
    });
    connect(d.ui->policyPayload, &QAction::triggered, this, [this]() {
        d.loadPolicy = StageModel::load_payload;
        setSettingsValue("loadType", "payload");
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
    connect(d.ui->displayIsolate, &QAction::toggled, this, &ViewerPrivate::isolate);
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
    connect(d.ui->enableDefaultCameraLight, &QCheckBox::toggled, this, &ViewerPrivate::defaultCameraLightEnabled);
    connect(d.ui->enableSceneLights, &QCheckBox::toggled, this, &ViewerPrivate::sceneLightsEnabled);
    connect(d.ui->enableSceneMaterials, &QCheckBox::toggled, this, &ViewerPrivate::sceneMaterialsEnabled);
    connect(d.ui->wireframe, &QToolButton::toggled, this, &ViewerPrivate::wireframeChanged);
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
    connect(d.stageModel.data(), &StageModel::boundingBoxChanged, this, &ViewerPrivate::boundingBoxChanged);
    connect(d.stageModel.data(), &StageModel::stageChanged, this, [=]() { enable(true); });
    // docks
    connect(d.ui->outlinerDock, &QDockWidget::visibilityChanged, this,
            [=](bool visible) { d.ui->viewOutliner->setChecked(visible); });
    connect(d.ui->viewOutliner, &QAction::toggled, this,
            [=](bool checked) { d.ui->outlinerDock->setVisible(checked); });
    connect(d.ui->payloadDock, &QDockWidget::visibilityChanged, this,
            [=](bool visible) { d.ui->viewPayload->setChecked(visible); });
    connect(d.ui->viewPayload, &QAction::toggled, this, [=](bool checked) { d.ui->payloadDock->setVisible(checked); });
    // settings
    initSettings();
    enable(false);
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
        setSettingsValue("recentFiles", QStringList());
        initRecentFiles();
    });
    recentMenu->addAction(clearAction);
}

void
ViewerPrivate::initSettings()
{
    QString loadType = settingsValue("loadType", "all").toString();
    if (loadType == "all") {
        d.loadPolicy = StageModel::load_all;
        d.ui->policyAll->setChecked(true);
    }
    else {
        d.loadPolicy = StageModel::load_payload;
        d.ui->policyPayload->setChecked(true);
    }
    QString theme = settingsValue("theme", "dark").toString();
    if (theme == "dark") {
        dark();
        d.ui->themeDark->setChecked(true);
    }
    else {
        light();
        d.ui->themeLight->setChecked(true);
    }
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

    d.stageModel->loadFromFile(filename, d.loadPolicy);

    if (d.stageModel->isLoaded()) {
        qint64 elapsedMs = timer.elapsed();
        double elapsedSec = elapsedMs / 1000.0;

        QString shortName = fileInfo.fileName();
        d.viewer->setWindowTitle(QString("%1: %2").arg(PROJECT_NAME).arg(shortName));
        setSettingsValue("openDir", fileInfo.absolutePath());
        updateRecentFiles(filename);
        updateStatus(QString("Loaded %1 in %2 seconds").arg(shortName).arg(QString::number(elapsedSec, 'f', 2)), false);
        return true;
    }
    else {
        updateStatus(QString("Failed to load file: %1").arg(fileInfo.fileName()), true);
        return false;
    }
}

RenderView*
ViewerPrivate::renderView()
{
    return d.ui->renderView;
}

OutlinerView*
ViewerPrivate::outlinerView()
{
    return d.ui->outlinerView;
}

PayloadView*
ViewerPrivate::payloadView()
{
    return d.ui->payloadView;
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
    QList<QAction*> actions = { d.ui->fileReload,       d.ui->fileClose,       d.ui->fileSave,
                                d.ui->fileSaveCopy,     d.ui->fileExportAll,   d.ui->fileExportSelected,
                                d.ui->fileExportImage,  d.ui->editCopyImage,   d.ui->editDelete,
                                d.ui->displayIsolate,   d.ui->displayFrameAll, d.ui->displayFrameSelected,
                                d.ui->displayResetView, d.ui->displayExpand,   d.ui->displayCollapse };
    for (QAction* action : actions) {
        if (action)
            action->setEnabled(enable);
    }
    d.ui->editShow->setEnabled(enable);
    d.ui->editHide->setEnabled(enable);
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
    QString path = platform::getApplicationPath() + "/Resources/App.qss";
    auto ss = Stylesheet::instance();
    if (ss->loadQss(path)) {
        ss->applyQss(ss->compiled());
    }
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
ViewerPrivate::loadSettings()
{
    d.recentFiles = settingsValue("recentFiles", QStringList()).toStringList();
    initRecentFiles();
}

void
ViewerPrivate::saveSettings()
{
    setSettingsValue("recentFiles", d.recentFiles);
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
        loadFile(filename);
    }
}

void
ViewerPrivate::save()
{
    QString filename = d.stageModel->filename();
    if (filename.isEmpty()) {
        saveAs();
        return;
    }
    if (d.stageModel->saveToFile(filename)) {
        d.viewer->setWindowTitle(QString("%1: %2").arg(PROJECT_NAME).arg(filename));
    }
}

void
ViewerPrivate::saveAs()
{
    QString saveDir = settingsValue("saveDir", QDir::homePath()).toString();
    QString currentFile = d.stageModel->filename();
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

    if (d.stageModel->saveToFile(filename)) {
        setSettingsValue("saveDir", QFileInfo(filename).absolutePath());
        d.viewer->setWindowTitle(QString("%1: %2").arg(PROJECT_NAME).arg(filename));
        updateRecentFiles(filename);
    }
}

void
ViewerPrivate::saveCopy()
{
    QString copyDir = settingsValue("copyDir", QDir::homePath()).toString();
    QString currentFile = d.stageModel->filename();
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

    if (d.stageModel->exportToFile(filename)) {
        setSettingsValue("copyDir", QFileInfo(filename).absolutePath());
    }
}

void
ViewerPrivate::reload()
{
    if (d.stageModel->isLoaded()) {
        d.stageModel->reload();
    }
}

void
ViewerPrivate::close()
{
    if (d.stageModel->isLoaded()) {
        d.stageModel->close();
        d.viewer->setWindowTitle(QString("%1").arg(PROJECT_NAME));
        enable(false);
    }
}

void
ViewerPrivate::ready()
{}

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
        setSettingsValue("backgroundColor", color.name());
        d.backgroundColor = color;
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
        if (d.stageModel->exportToFile(filename)) {
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
        if (d.stageModel->exportPathsToFile(d.selectionModel->paths(), filename)) {
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
    if (d.selectionModel->paths().size()) {
        d.stageModel->setVisible(d.selectionModel->paths(), true);
    }
}

void
ViewerPrivate::showRecursive()
{
    if (d.selectionModel->paths().size()) {
        d.stageModel->setVisible(d.selectionModel->paths(), true, true);
    }
}

void
ViewerPrivate::hideSelected()
{
    if (d.selectionModel->paths().size()) {
        d.stageModel->setVisible(d.selectionModel->paths(), false);
    }
}

void
ViewerPrivate::hideRecursive()
{
    if (d.selectionModel->paths().size()) {
        d.stageModel->setVisible(d.selectionModel->paths(), false, true);
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
        if (d.selectionModel->paths().size()) {
            d.stageModel->setMask(d.selectionModel->paths());
        }
    }
    else {
        d.stageModel->setMask(QList<SdfPath>());
    }
}

void
ViewerPrivate::frameAll()
{
    if (d.stageModel->isLoaded()) {
        renderView()->frameAll();
    }
}

void
ViewerPrivate::frameSelected()
{
    if (d.selectionModel->paths().size()) {
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
    if (d.selectionModel->paths().size()) {
        outlinerView()->expand();
    }
}

void
ViewerPrivate::defaultCameraLightEnabled(bool checked)
{
    renderView()->setDefaultCameraLightEnabled(checked);
}

void
ViewerPrivate::sceneLightsEnabled(bool checked)
{
    renderView()->setSceneLightsEnabled(checked);
}

void
ViewerPrivate::sceneMaterialsEnabled(bool checked)
{
    renderView()->setSceneMaterialsEnabled(checked);
}

void
ViewerPrivate::wireframeChanged(bool checked)
{
    if (checked) {
        renderView()->setDrawMode(RenderView::render_mode::render_wireframe);
    }
    else {
        renderView()->setDrawMode(RenderView::render_mode::render_shaded);
    }
}

void
ViewerPrivate::light()
{
    Stylesheet::instance()->setTheme(Stylesheet::Light);
    setSettingsValue("theme", "light");
    stylesheet();
}

void
ViewerPrivate::dark()
{
    Stylesheet::instance()->setTheme(Stylesheet::Dark);
    setSettingsValue("theme", "dark");
    stylesheet();
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
    if (!d.loadInit) {
        frameAll();
        d.loadInit = true;
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
}

void
ViewerPrivate::stageChanged(UsdStageRefPtr stage)
{
    d.loadInit = false;
}

void
ViewerPrivate::updateRecentFiles(const QString& filename)
{
    d.recentFiles.removeAll(filename);
    d.recentFiles.prepend(filename);
    const int maxRecent = 10;
    while (d.recentFiles.size() > maxRecent)
        d.recentFiles.removeLast();
    setSettingsValue("recentFiles", d.recentFiles);
    initRecentFiles();
}

void
ViewerPrivate::updateStatus(const QString& message, bool error)
{
    QStatusBar* bar = d.ui->statusbar;
    QString text = error ? QString(" error: %1").arg(message) : QString(" %1").arg(message);
    int timeoutMs = 4000;
    bar->showMessage(text, timeoutMs);
    QTimer::singleShot(timeoutMs, bar, [bar]() { bar->showMessage(" Ready."); });
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
}  // namespace usd
