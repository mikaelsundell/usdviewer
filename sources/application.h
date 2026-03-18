// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2024 - present Mikael Sundell
// https://github.com/mikaelsundell/flipman

#pragma once

#include <QApplication>
#include <QScopedPointer>

namespace usdviewer {
class CommandStack;
class DataModel;
class SelectionModel;
class Style;
class Settings;
class ApplicationPrivate;

/**
 * @class Application
 * @brief Main application object for usdviewer.
 *
 * Extends QApplication and provides access to global subsystems
 * used throughout the viewer such as settings and style.
 */
class Application : public QApplication {
    Q_OBJECT
public:
    /**
     * @brief Constructs the usdviewer application.
     *
     * @param argc Argument count.
     * @param argv Argument vector.
     */
    Application(int& argc, char** argv);

    /**
     * @brief Destroys the application.
     */
    ~Application() override;

    /** @name Subsystems */
    ///@{

    /**
     * @brief Returns the data model subsystem.
     */
    DataModel* dataModel() const;

    /**
     * @brief Returns the selection model subsystem.
     */
    SelectionModel* selectionModel() const;

    /**
     * @brief Returns the command stack subsystem.
     */
    CommandStack* commandStack() const;

    /**
     * @brief Returns the settings subsystem.
     */
    Settings* settings() const;

    /**
     * @brief Returns the style subsystem.
     */
    Style* style() const;

    ///@}

    /**
     * @brief Returns the global Application instance.
     *
     * Asserts if the active QCoreApplication is not an
     * usdviewer::Application instance.
     */
    static Application* instance();

private:
    Q_DISABLE_COPY_MOVE(Application)
    QScopedPointer<ApplicationPrivate> p;
};

/**
 * @brief Returns the global Application instance.
 */
inline Application*
app()
{
    return Application::instance();
}

/**
 * @brief Returns the global data model subsystem.
 */
inline DataModel*
dataModel()
{
    auto* a = app();
    return a ? a->dataModel() : nullptr;
}

/**
 * @brief Returns the global selection model subsystem.
 */
inline SelectionModel*
selectionModel()
{
    auto* a = app();
    return a ? a->selectionModel() : nullptr;
}

/**
 * @brief Returns the global selection model subsystem.
 */
inline CommandStack*
commandStack()
{
    auto* a = app();
    return a ? a->commandStack() : nullptr;
}

/**
 * @brief Returns the global style subsystem.
 */
inline Style*
style()
{
    auto* a = app();
    return a ? a->style() : nullptr;
}

/**
 * @brief Returns the global settings subsystem.
 */
inline Settings*
settings()
{
    auto* a = app();
    return a ? a->settings() : nullptr;
}

}  // namespace usdviewer
