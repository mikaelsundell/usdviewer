// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "pythonview.h"
#include "application.h"
#include "pythoninterpreter.h"
#include "session.h"
#include "settings.h"
#include "shelfwidget.h"
#include "style.h"
#include <QAction>
#include <QApplication>
#include <QDrag>
#include <QDropEvent>
#include <QFileDialog>
#include <QJsonDocument>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSaveFile>
#include <QStyle>
#include <QTabBar>
#include <QToolButton>
#include <QVariantMap>


#include <QTimer>

// generated files
#include "ui_pythonview.h"

namespace usdviewer {

namespace {

    constexpr auto kShelvesSettingsKey = "python/shelves";
    constexpr auto kScriptMimeType = "application/x-usdviewer-python-script";

    QString normalizedSelectedText(const QString& text)
    {
        QString out = text;
        out.replace(QChar(0x2029), '\n');
        out.replace(QChar(0x2028), '\n');
        return out;
    }

}  // namespace

class PythonViewPrivate : public QObject {
public:
    void init();
    bool eventFilter(QObject* object, QEvent* event) override;
    void createDefaultTabIfNeeded();
    int createShelfTab(const QString& name, const QVariantList& scripts = {});
    ShelfWidget* currentShelf() const;
    ShelfWidget* shelfAt(int index) const;
    void beginTabRename(int index);
    void commitTabRename();
    void cancelTabRename();
    void removeTab(int index);
    void exportTab(int index);
    void loadShelves();
    void saveShelves() const;
    void startScriptDrag(QPlainTextEdit* edit);
    void updateClearButton();

public Q_SLOTS:
    void run();
    void clear();
    void stageChanged(UsdStageRefPtr stage, Session::LoadPolicy policy, Session::StageStatus status);
    void showLogContextMenu(const QPoint& pos);
    void showTabContextMenu(const QPoint& pos);
    void newTab();

public:
    struct Data {
        QScopedPointer<Ui_PythonView> ui;
        QPointer<PythonView> view;
        QPoint dragStartPos;
        QPointer<QLineEdit> tabRenameEditor;
        int tabRenameIndex = -1;
        QPointer<QToolButton> addTabButton;
    };
    Data d;
};

void
PythonViewPrivate::init()
{
    d.ui.reset(new Ui_PythonView());
    d.ui->setupUi(d.view.data());
    d.ui->run->setIcon(style()->icon(Style::IconRole::Run));
    d.ui->clear->setIcon(style()->icon(Style::IconRole::Clear));
    d.ui->log->setReadOnly(true);
    d.ui->log->setContextMenuPolicy(Qt::CustomContextMenu);
    d.ui->log->setAcceptDrops(false);
    d.ui->editor->setAcceptDrops(true);
    d.ui->tabWidget->setTabsClosable(false);
    d.ui->tabWidget->setMovable(true);
    d.ui->tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    d.ui->tabWidget->tabBar()->installEventFilter(this);
    d.ui->tabWidget->setUsesScrollButtons(true);
    d.ui->tabWidget->setElideMode(Qt::ElideRight);

    if (QTabBar* bar = d.ui->tabWidget->tabBar()) {
        bar->setExpanding(false);
        bar->setUsesScrollButtons(true);
        bar->setElideMode(Qt::ElideRight);
        bar->setMinimumWidth(0);
        bar->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }

    d.ui->log->viewport()->installEventFilter(this);
    d.ui->editor->viewport()->installEventFilter(this);



    /*
    d.addTabButton = new QToolButton(d.ui->tabWidget);
    d.addTabButton->setText("+");
    d.addTabButton->setAutoRaise(true);
    d.addTabButton->setToolTip(tr("New Shelf"));
    d.ui->tabWidget->setCornerWidget(d.addTabButton, Qt::TopRightCorner);
    */


    QObject::connect(d.addTabButton, &QToolButton::clicked, this, &PythonViewPrivate::newTab);
    QObject::connect(d.ui->run, &QToolButton::clicked, this, &PythonViewPrivate::run);
    QObject::connect(d.ui->clear, &QToolButton::clicked, this, &PythonViewPrivate::clear);
    QObject::connect(d.ui->editor->document(), &QTextDocument::contentsChanged, this,
                     &PythonViewPrivate::updateClearButton);
    QObject::connect(d.ui->log, &QWidget::customContextMenuRequested, this, &PythonViewPrivate::showLogContextMenu);
    QObject::connect(d.ui->tabWidget->tabBar(), &QWidget::customContextMenuRequested, this,
                     &PythonViewPrivate::showTabContextMenu);
    QObject::connect(d.ui->tabWidget->tabBar(), &QTabBar::tabMoved, this, [this](int, int) { saveShelves(); });
    QObject::connect(session(), &Session::stageChanged, this, &PythonViewPrivate::stageChanged);


    loadShelves();
    createDefaultTabIfNeeded();
    updateClearButton();
}

bool
PythonViewPrivate::eventFilter(QObject* object, QEvent* event)
{
    if (object == d.ui->tabWidget->tabBar()) {
        QTabBar* tabBar = d.ui->tabWidget->tabBar();
        if (event->type() == QEvent::MouseButtonDblClick) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const int index = tabBar->tabAt(mouseEvent->pos());
            if (index >= 0) {
                beginTabRename(index);
                return true;
            }
        }
    }

    if (object == d.ui->log->viewport() || object == d.ui->editor->viewport()) {
        QPlainTextEdit* edit = nullptr;
        if (object == d.ui->log->viewport())
            edit = d.ui->log;
        else if (object == d.ui->editor->viewport())
            edit = d.ui->editor;

        if (!edit)
            return QObject::eventFilter(object, event);

        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton)
                d.dragStartPos = mouseEvent->pos();
            break;
        }
        case QEvent::MouseMove: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (!(mouseEvent->buttons() & Qt::LeftButton))
                break;
            if ((mouseEvent->pos() - d.dragStartPos).manhattanLength() < QApplication::startDragDistance())
                break;
            if (!edit->textCursor().hasSelection())
                break;

            startScriptDrag(edit);
            return true;
        }
        case QEvent::DragEnter: {
            auto* dragEvent = static_cast<QDragEnterEvent*>(event);
            if (object == d.ui->editor->viewport()
                && (dragEvent->mimeData()->hasFormat(kScriptMimeType) || dragEvent->mimeData()->hasText())) {
                dragEvent->acceptProposedAction();
                return true;
            }
            break;
        }
        case QEvent::DragMove: {
            auto* dragEvent = static_cast<QDragMoveEvent*>(event);
            if (object == d.ui->editor->viewport()
                && (dragEvent->mimeData()->hasFormat(kScriptMimeType) || dragEvent->mimeData()->hasText())) {
                dragEvent->acceptProposedAction();
                return true;
            }
            break;
        }
        case QEvent::Drop: {
            auto* dropEvent = static_cast<QDropEvent*>(event);
            if (object == d.ui->editor->viewport()
                && (dropEvent->mimeData()->hasFormat(kScriptMimeType) || dropEvent->mimeData()->hasText())) {
                QString code;
                if (dropEvent->mimeData()->hasFormat(kScriptMimeType))
                    code = QString::fromUtf8(dropEvent->mimeData()->data(kScriptMimeType));
                else
                    code = dropEvent->mimeData()->text();

                code = normalizedSelectedText(code).trimmed();
                if (!code.isEmpty()) {
                    d.ui->editor->setPlainText(code);
                    dropEvent->acceptProposedAction();
                    updateClearButton();
                    return true;
                }
            }
            break;
        }
        default: break;
        }
    }

    return QObject::eventFilter(object, event);
}

void
PythonViewPrivate::createDefaultTabIfNeeded()
{
    if (d.ui->tabWidget->count() == 0)
        createShelfTab(tr("Default"));
}

int
PythonViewPrivate::createShelfTab(const QString& name, const QVariantList& scripts)
{
    auto* shelf = new ShelfWidget(d.ui->tabWidget);
    shelf->fromVariantList(scripts);

    QObject::connect(shelf, &ShelfWidget::itemActivated, this, [this](const QString& code) {
        d.ui->editor->setPlainText(code);
        updateClearButton();
        run();
    });

    QObject::connect(shelf, &ShelfWidget::itemContextMenuRequested, this,
                     [this, shelf](const QPoint& pos, QListWidgetItem* item) {
                         QMenu menu(shelf);
                         QAction* loadAction = nullptr;
                         QAction* renameAction = nullptr;
                         QAction* exportAction = nullptr;
                         QAction* removeAction = nullptr;

                         if (item) {
                             loadAction = menu.addAction(tr("Load"));
                             renameAction = menu.addAction(tr("Rename"));
                             exportAction = menu.addAction(tr("Export"));
                             removeAction = menu.addAction(tr("Remove"));
                         }
                         else {
                             QAction* clearAction = menu.addAction(tr("Clear Shelf"));
                             clearAction->setEnabled(shelf->count() > 0);
                             QObject::connect(clearAction, &QAction::triggered, this, [this, shelf]() {
                                 shelf->clear();
                                 saveShelves();
                             });
                         }

                         QAction* chosen = menu.exec(shelf->mapToGlobal(pos));
                         if (!chosen || !item)
                             return;

                         const QString code = item->data(Qt::UserRole).toString();

                         if (chosen == loadAction) {
                             d.ui->editor->setPlainText(code);
                             updateClearButton();
                         }
                         else if (chosen == renameAction) {
                             shelf->editScript(item);
                         }
                         else if (chosen == exportAction) {
                             const QString fileName
                                 = QFileDialog::getSaveFileName(d.view.data(), tr("Export Script"),
                                                                item->text() + QStringLiteral(".py"),
                                                                tr("Python Files (*.py);;Text Files (*.txt)"));
                             if (!fileName.isEmpty()) {
                                 QSaveFile file(fileName);
                                 if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                                     file.write(code.toUtf8());
                                     file.commit();
                                 }
                             }
                         }
                         else if (chosen == removeAction) {
                             shelf->removeScript(item);
                             saveShelves();
                         }
                     });

    QObject::connect(shelf, &ShelfWidget::changed, this, [this]() { saveShelves(); });

    const int index = d.ui->tabWidget->addTab(shelf, name.trimmed().isEmpty() ? tr("Default") : name);
    d.ui->tabWidget->setCurrentIndex(index);
    return index;
}

ShelfWidget*
PythonViewPrivate::currentShelf() const
{
    return qobject_cast<ShelfWidget*>(d.ui->tabWidget->currentWidget());
}

ShelfWidget*
PythonViewPrivate::shelfAt(int index) const
{
    return qobject_cast<ShelfWidget*>(d.ui->tabWidget->widget(index));
}

void
PythonViewPrivate::beginTabRename(int index)
{
    cancelTabRename();

    QTabBar* tabBar = d.ui->tabWidget->tabBar();
    if (index < 0 || index >= tabBar->count())
        return;

    d.tabRenameIndex = index;
    d.tabRenameEditor = new QLineEdit(tabBar);
    d.tabRenameEditor->setText(tabBar->tabText(index));
    d.tabRenameEditor->setFrame(false);
    d.tabRenameEditor->setAlignment(Qt::AlignCenter);
    d.tabRenameEditor->setGeometry(tabBar->tabRect(index).adjusted(2, 2, -2, -2));
    d.tabRenameEditor->selectAll();
    d.tabRenameEditor->show();
    d.tabRenameEditor->setFocus();

    QObject::connect(d.tabRenameEditor, &QLineEdit::editingFinished, this, &PythonViewPrivate::commitTabRename);
}

void
PythonViewPrivate::commitTabRename()
{
    if (!d.tabRenameEditor)
        return;

    const QString name = d.tabRenameEditor->text().trimmed();
    if (d.tabRenameIndex >= 0 && d.tabRenameIndex < d.ui->tabWidget->count())
        d.ui->tabWidget->setTabText(d.tabRenameIndex, name.isEmpty() ? tr("Default") : name);

    d.tabRenameEditor->deleteLater();
    d.tabRenameEditor = nullptr;
    d.tabRenameIndex = -1;
    saveShelves();
}

void
PythonViewPrivate::cancelTabRename()
{
    if (!d.tabRenameEditor)
        return;

    d.tabRenameEditor->deleteLater();
    d.tabRenameEditor = nullptr;
    d.tabRenameIndex = -1;
}

void
PythonViewPrivate::removeTab(int index)
{
    if (index < 0 || index >= d.ui->tabWidget->count())
        return;

    QWidget* widget = d.ui->tabWidget->widget(index);
    d.ui->tabWidget->removeTab(index);

    if (widget)
        widget->deleteLater();

    createDefaultTabIfNeeded();
    saveShelves();
}

void
PythonViewPrivate::exportTab(int index)
{
    ShelfWidget* shelf = shelfAt(index);
    if (!shelf)
        return;

    QVariantMap tabData;
    tabData.insert("name", d.ui->tabWidget->tabText(index));
    tabData.insert("scripts", shelf->toVariantList());

    const QString defaultName = d.ui->tabWidget->tabText(index).trimmed().isEmpty()
                                    ? QStringLiteral("shelf.json")
                                    : d.ui->tabWidget->tabText(index).trimmed() + QStringLiteral(".json");

    const QString fileName = QFileDialog::getSaveFileName(d.view.data(), tr("Export Shelf"), defaultName,
                                                          tr("JSON Files (*.json)"));
    if (fileName.isEmpty())
        return;

    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
        return;

    const QJsonDocument json = QJsonDocument::fromVariant(tabData);
    file.write(json.toJson(QJsonDocument::Indented));
    file.commit();
}

void
PythonViewPrivate::loadShelves()
{
    d.ui->tabWidget->clear();

    const QVariant value = settings()->value(kShelvesSettingsKey);
    const QVariantList tabs = value.toList();
    for (const QVariant& tabValue : tabs) {
        const QVariantMap tabMap = tabValue.toMap();
        const QString name = tabMap.value("name").toString();
        const QVariantList scripts = tabMap.value("scripts").toList();
        createShelfTab(name, scripts);
    }
}

void
PythonViewPrivate::saveShelves() const
{
    QVariantList tabs;
    for (int i = 0; i < d.ui->tabWidget->count(); ++i) {
        const ShelfWidget* shelf = shelfAt(i);
        if (!shelf)
            continue;

        QVariantMap tabMap;
        tabMap.insert("name", d.ui->tabWidget->tabText(i));
        tabMap.insert("scripts", shelf->toVariantList());
        tabs.append(tabMap);
    }

    settings()->setValue(kShelvesSettingsKey, tabs);
}

void
PythonViewPrivate::startScriptDrag(QPlainTextEdit* edit)
{
    QString code = normalizedSelectedText(edit->textCursor().selectedText()).trimmed();
    if (code.isEmpty())
        return;

    auto* mime = new QMimeData();
    mime->setData(kScriptMimeType, code.toUtf8());
    mime->setText(code);

    auto* drag = new QDrag(edit);
    drag->setMimeData(mime);
    drag->setPixmap(style()->icon(Style::IconRole::Code));
    drag->exec(Qt::CopyAction);
}

void
PythonViewPrivate::updateClearButton()
{
    const bool enabled = d.ui->editor->isEnabled() && !d.ui->editor->document()->isEmpty();
    d.ui->clear->setEnabled(enabled);
}

void
PythonViewPrivate::stageChanged(UsdStageRefPtr stage, Session::LoadPolicy policy, Session::StageStatus status)
{
    Q_UNUSED(stage);
    Q_UNUSED(policy);

    const bool enabled = (status == Session::StageStatus::Loaded);
    d.ui->run->setEnabled(enabled);
    d.ui->editor->setEnabled(enabled);
    d.ui->log->setEnabled(enabled);
    d.ui->tabWidget->setEnabled(enabled);
    if (d.addTabButton)
        d.addTabButton->setEnabled(enabled);
    updateClearButton();
}

void
PythonViewPrivate::showLogContextMenu(const QPoint& pos)
{
    QMenu* menu = d.ui->log->createStandardContextMenu();
    menu->addSeparator();

    QAction* clearAction = menu->addAction(style()->icon(Style::IconRole::Clear), tr("Clear"));
    clearAction->setEnabled(!d.ui->log->toPlainText().isEmpty());
    QObject::connect(clearAction, &QAction::triggered, this, [this]() { d.ui->log->clear(); });

    menu->exec(d.ui->log->viewport()->mapToGlobal(pos));
    delete menu;
}

void
PythonViewPrivate::showTabContextMenu(const QPoint& pos)
{
    QTabBar* tabBar = d.ui->tabWidget->tabBar();
    const int index = tabBar->tabAt(pos);

    QMenu menu(tabBar);
    QAction* newAction = menu.addAction(tr("New Shelf"));
    QAction* renameAction = menu.addAction(tr("Rename"));
    QAction* exportAction = menu.addAction(tr("Export"));
    QAction* removeAction = menu.addAction(tr("Remove"));

    renameAction->setEnabled(index >= 0);
    exportAction->setEnabled(index >= 0);
    removeAction->setEnabled(index >= 0 && d.ui->tabWidget->count() > 1);

    QAction* chosen = menu.exec(tabBar->mapToGlobal(pos));
    if (!chosen)
        return;

    if (chosen == newAction) {
        newTab();
    }
    else if (chosen == renameAction) {
        beginTabRename(index);
    }
    else if (chosen == exportAction) {
        exportTab(index);
    }
    else if (chosen == removeAction) {
        removeTab(index);
    }
}

void
PythonViewPrivate::newTab()
{
    const int index = createShelfTab(tr("Shelf"));
    saveShelves();
    beginTabRename(index);
}

void
PythonViewPrivate::run()
{
    auto* interpreter = pythonInterpreter();
    const QString code = d.ui->editor->toPlainText().trimmed();
    if (code.isEmpty())
        return;

    d.ui->log->appendPlainText(">>> " + code);

    const QString result = interpreter->executeScript(code);
    if (!result.isEmpty())
        d.ui->log->appendPlainText(result);

    d.ui->log->appendPlainText("");
}

void
PythonViewPrivate::clear()
{
    d.ui->editor->clear();
    updateClearButton();
}

PythonView::PythonView(QWidget* parent)
    : QWidget(parent)
    , p(new PythonViewPrivate())
{
    p->d.view = this;
    p->init();
}

PythonView::~PythonView()
{
    if (p)
        p->saveShelves();
}

}  // namespace usdviewer
