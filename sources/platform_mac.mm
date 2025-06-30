// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "platform.h"

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

#include <os/log.h>
#include <QApplication>
#include <QScreen>

namespace platform
{
    namespace {
        QPointF toNativeCursor(int x, int y)
        {
            QScreen* screen = QGuiApplication::primaryScreen();
            QPointF cursor = QPointF(x, y);
            qreal reverse = screen->geometry().height() - cursor.y();
            return QPointF(cursor.x(), reverse);
        }
    
        NSWindow* toNativeWindow(WId winId)
        {
            NSView *view = (NSView*)winId;
            return [view window];
        }
    
        NSScreen* toNativeScreen(WId winId)
        {
            NSWindow *window = toNativeWindow(winId);
            return [window screen];
        }
    
        struct ColorSyncProfile {
            uint32_t screenNumber;
            CFStringRef displayProfileUrl;
            ColorSyncProfile() : screenNumber(0), displayProfileUrl(nullptr) {}
            ColorSyncProfile(const ColorSyncProfile& other)
            : screenNumber(other.screenNumber) {
                displayProfileUrl = other.displayProfileUrl ? static_cast<CFStringRef>(CFRetain(other.displayProfileUrl)) : nullptr;
            }
            ColorSyncProfile& operator=(const ColorSyncProfile& other) {
                if (this != &other) {
                    screenNumber = other.screenNumber;
                    if (displayProfileUrl) CFRelease(displayProfileUrl);
                    displayProfileUrl = other.displayProfileUrl ? static_cast<CFStringRef>(CFRetain(other.displayProfileUrl)) : nullptr;
                }
                return *this;
            }
            ~ColorSyncProfile() {
                if (displayProfileUrl) CFRelease(displayProfileUrl);
            }
        };
        QMap<uint32_t, ColorSyncProfile> colorsynccache;
        ColorSyncProfile getColorSyncProfile(NSScreen* screen)
        {
            ColorSyncProfile colorSyncProfile;
            NSDictionary* screenDescription = [screen deviceDescription];
            NSNumber* screenID = [screenDescription objectForKey:@"NSScreenNumber"];
            colorSyncProfile.screenNumber = [screenID unsignedIntValue];
            ColorSyncProfileRef csProfileRef = ColorSyncProfileCreateWithDisplayID((CGDirectDisplayID)colorSyncProfile.screenNumber);
            if (csProfileRef) {
                CFURLRef iccURLRef = ColorSyncProfileGetURL(csProfileRef, NULL);
                if (iccURLRef) {
                    colorSyncProfile.displayProfileUrl = CFURLCopyFileSystemPath(iccURLRef, kCFURLPOSIXPathStyle);
                }
                CFRelease(csProfileRef);
            }
            return colorSyncProfile;
        }
    
        ColorSyncProfile getDisplayProfile(NSScreen* screen) {
            NSDictionary* screenDescription = [screen deviceDescription];
            CGDirectDisplayID displayId = [[screenDescription objectForKey:@"NSScreenNumber"] unsignedIntValue];
            if (colorsynccache.contains(displayId)) {
                return colorsynccache.value(displayId);
            }
            ColorSyncProfile colorSyncProfile = getColorSyncProfile(screen);
            colorsynccache.insert(displayId, colorSyncProfile);
            return colorSyncProfile;
        }
    }

    void setDarkTheme()
    {
        // we force dark aque no matter appearance set in system settings
        [NSApp setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
    }

    IccProfile getIccProfile(WId wid)
    {
        NSScreen* screen = toNativeScreen(wid);
        ColorSyncProfile colorsyncProfile = getDisplayProfile(screen);
        return IccProfile {
            int(colorsyncProfile.screenNumber),
            QString::fromCFString(colorsyncProfile.displayProfileUrl)
        };
    }

    QString getIccProfileUrl(WId wid)
    {
        return getIccProfile(wid).displayProfileUrl;
    }

    QString getApplicationPath()
    {
        return QApplication::applicationDirPath() + "/..";
    }

    QString resolveBookmark(const QString& bookmark)
    {
        if (bookmark.isEmpty()) {
            return QString();
        }
        QByteArray bookmarkData = QByteArray::fromBase64(bookmark.toUtf8());
        NSError* error = nil;
        BOOL isstale = NO;
        NSURL *url = [NSURL URLByResolvingBookmarkData:[NSData dataWithBytes:bookmarkData.data() length:bookmarkData.size()]
                       options:NSURLBookmarkResolutionWithSecurityScope
                 relativeToURL:nil
           bookmarkDataIsStale:&isstale
                         error:&error];

        if (url && !error) {
            if ([url startAccessingSecurityScopedResource]) {
                return QString::fromUtf8([[url path] UTF8String]);
            } else {
                QString();
            }
        } else {
            QString();
        }
        return QString();
    }

    QString saveBookmark(const QString& bookmark)
    {
        if (bookmark.isEmpty()) {
            return QString();
        }
        NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:bookmark.toUtf8().constData()]];
        NSError* error = nil;
        NSData* bookmarkData = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
            includingResourceValuesForKeys:nil
                             relativeToURL:nil
                                    error:&error];

        if (bookmarkData && !error) {
            QByteArray bookmark((const char *)[bookmarkData bytes], [bookmarkData length]);
            return QString::fromUtf8(bookmark.toBase64());
        } else {
            return QString();
        }
    }
}