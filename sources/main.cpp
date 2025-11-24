// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "platform.h"
#include "test.h"
#include "usdviewer.h"
#include "usdqtutils.h"
#include <QApplication>
#include <QDir>
#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/setenv.h>

PXR_NAMESPACE_USING_DIRECTIVE

int
main(int argc, char* argv[])
{
    QApplication app(argc, argv);
#ifdef NDEBUG
    QStringList plugindirs;
    QString pluginusddir = platform::getApplicationPath() + "/plugin/usd";
    if (QDir(pluginusddir).exists()) {
        plugindirs << pluginusddir;
    }
    QString usddir = platform::getApplicationPath() + "/usd";
    if (QDir(usddir).exists()) {
        plugindirs << usddir;
    }
    if (!plugindirs.isEmpty()) {
        pxr::TfSetenv("PXR_DISABLE_STANDARD_PLUG_SEARCH_PATH", "1");
        std::vector<std::string> pluginPaths;
        for (const QString& dir : plugindirs) {
            pluginPaths.push_back(dir.toStdString());
        }
        pxr::PlugRegistry& registry = PlugRegistry::GetInstance();
        registry.RegisterPlugins(pluginPaths);
    }
#endif
    pxr::PlugRegistry& instance = pxr::PlugRegistry::GetInstance();
    pxr::PlugPluginPtrVector plugins = instance.GetAllPlugins();
#if defined(_DEBUG)
    platform::console("plugins");
    for (const auto& plugin : plugins) {
        platform::console(usd::StringToQString(plugin->GetPath()));
    }
    if (0) {
        test();
    }
#endif
    usd::Viewer viewer;
    viewer.setArguments(QCoreApplication::arguments());
    viewer.show();
    return app.exec();
}
