// Copyright 2022-present Contributors to the jobman project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/mikaelsundell/jobman

#include "os.h"
#include <QApplication>
#include <QScreen>
#include <windows.h>

namespace usdviewer {
namespace os {
    void setDarkTheme() {}

    QString getApplicationPath() { return QApplication::applicationDirPath(); }

    QString restoreScopedPath(const QString& path)
    {
        return path;  // ignore on win32
    }

    QString persistScopedPath(const QString& path)
    {
        return path;  // ignore on win32
    }

    void console(const QString& message)
    {
        QString string = QStringLiteral("usdviewer: %1\n").arg(message);
        OutputDebugStringW(reinterpret_cast<const wchar_t*>(string.utf16()));
    }

}  // namespace os
}  // namespace usdviewer
