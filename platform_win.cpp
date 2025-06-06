// Copyright 2022-present Contributors to the jobman project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/mikaelsundell/jobman

#include "platform.h"
#include <QApplication>
#include <QScreen>
#include <windows.h>

namespace platform {
namespace utils {
    QPointF toNativeCursor(int x, int y)
    {
        Q_UNUSED(x);
        Q_UNUSED(y);
        QPoint globalPos = QCursor::pos();
        return QPointF(globalPos.x(), globalPos.y());
    }
    struct IccProfileData {
        QString profilePath;
        IccProfileData()
            : profilePath()
        {}
    };
    QMap<uint32_t, IccProfileData> iccCache;

    IccProfileData grabIccProfileData()
    {
        IccProfileData iccProfile;
        HDC hdc = GetDC(nullptr);
        if (hdc) {
            DWORD pathSize = MAX_PATH;
            WCHAR iccPath[MAX_PATH];
            BOOL result = GetICMProfileW(hdc, &pathSize, iccPath);
            if (result) {
                iccProfile.profilePath = QString::fromWCharArray(iccPath);
            }
            ReleaseDC(nullptr, hdc);
        }
        return iccProfile;
    }

    IccProfileData grabDisplayProfile()
    {
        uint32_t displayId = 0;
        if (iccCache.contains(displayId)) {
            return iccCache.value(displayId);
        }
        IccProfileData iccProfile = grabIccProfileData();
        iccCache.insert(displayId, iccProfile);
        return iccProfile;
    }
}  // namespace utils

void
setDarkTheme()
{}

IccProfile
grabIccProfile(WId wid)
{
    Q_UNUSED(wid);
    utils::IccProfileData iccData = utils::grabDisplayProfile();
    return IccProfile { 0,  // Screen number (always 0 for primary display)
                        iccData.profilePath };
}

QString
getIccProfileUrl(WId wid)
{
    return grabIccProfile(wid).displayProfileUrl;
}

QString
getApplicationPath()
{
    return QApplication::applicationDirPath();
}

QString
restoreScopedPath(const QString& path)
{
    return path;  // ignore on win32
}

QString
persistScopedPath(const QString& path)
{
    return path;  // ignore on win32
}

void
console(const QString& message)
{
    QString string = QStringLiteral("usdviewer: %1\n").arg(message);
    OutputDebugStringW(reinterpret_cast<const wchar_t*>(string.utf16()));
}

}  // namespace platform