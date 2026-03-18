// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2022 - present Mikael Sundell.
// https://github.com/mikaelsundell/flipman

#include "application.h"
#include "commanddispatcher.h"
#include "commandstack.h"
#include "datamodel.h"
#include "os.h"
#include "qtutils.h"
#include "selectionmodel.h"
#include "settings.h"
#include "style.h"
#include <QApplication>
#include <QDir>
#include <QPointer>
#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/setenv.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdviewer {
class ApplicationPrivate {
public:
    ApplicationPrivate();
    ~ApplicationPrivate();
    void init();

public:
    struct Data {
        QScopedPointer<DataModel> dataModel;
        QScopedPointer<SelectionModel> selectionModel;
        QScopedPointer<CommandStack> commandStack;
        QScopedPointer<Style> style;
        QScopedPointer<Settings> settings;
    };
    Data d;
};

ApplicationPrivate::ApplicationPrivate() {}

ApplicationPrivate::~ApplicationPrivate() {}

void
ApplicationPrivate::init()
{
    d.dataModel.reset(new DataModel());
    d.selectionModel.reset(new SelectionModel());
    d.commandStack.reset(new CommandStack());
    CommandDispatcher::setCommandStack(d.commandStack.data());
    // settings
    d.settings.reset(new Settings());
    d.style.reset(new Style());
#ifdef NDEBUG
    QStringList plugindirs;
    QString pluginusddir = os::getApplicationPath() + "/plugin/usd";
    if (QDir(pluginusddir).exists()) {
        plugindirs << pluginusddir;
    }
    QString usddir = os::getApplicationPath() + "/usd";
    if (QDir(usddir).exists()) {
        plugindirs << usddir;
    }
    if (!plugindirs.isEmpty()) {
        TfSetenv("PXR_DISABLE_STANDARD_PLUG_SEARCH_PATH", "1");
        std::vector<std::string> pluginPaths;
        for (const QString& dir : plugindirs) {
            pluginPaths.push_back(qt::QStringToString(dir));
        }
        PlugRegistry& registry = PlugRegistry::GetInstance();
        registry.RegisterPlugins(pluginPaths);
    }
#endif
    PlugRegistry& instance = PlugRegistry::GetInstance();
    PlugPluginPtrVector plugins = instance.GetAllPlugins();
#if defined(_DEBUG)
    os::console("plugins");
    for (const auto& plugin : plugins) {
        os::console(qt::StringToQString(plugin->GetPath()));
    }
#endif
}

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
    , p(new ApplicationPrivate())
{
    p->init();
}

Application::~Application() {}

DataModel*
Application::dataModel() const
{
    return p->d.dataModel.data();
}

SelectionModel*
Application::selectionModel() const
{
    return p->d.selectionModel.data();
}

CommandStack*
Application::commandStack() const
{
    return p->d.commandStack.data();
}

Settings*
Application::settings() const
{
    return p->d.settings.data();
}

Style*
Application::style() const
{
    return p->d.style.data();
}

Application*
Application::instance()
{
    auto* app = qobject_cast<Application*>(QCoreApplication::instance());
    if (!app) {
        qFatal("Fatal Error in [Application::instance()]:\n"
               "The global application instance is missing or is not a core::Application. "
               "Ensure you have instantiated usdviewer::Application in your main() function.");
    }
    return app;
}
}  // namespace usdviewer
