// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once
#include <QProcess>
#include <QWidget>

namespace platform {
struct IccProfile {
    int screenNumber;
    QString displayProfileUrl;
};
IccProfile
getIccProfile(WId wid);
void
setDarkTheme();
QString
getIccProfileUrl(WId wid);
QString
getApplicationPath();
QString
restoreScopedPath(const QString& bookmark);
QString
persistScopedPath(const QString& bookmark);
void
console(const QString& message);
}  // namespace platform