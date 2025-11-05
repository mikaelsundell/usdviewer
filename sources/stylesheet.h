// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QScopedPointer>

class StylesheetPrivate;
class Stylesheet : public QObject {
    Q_OBJECT
public:
    enum ColorRole {
        Base,
        BaseAlt,
        Accent,
        AccentAlt,
        Text,
        TextDisabled,
        Highlight,
        Border,
        BorderAlt,
        Scrollbar,
        Progress,
        Button,
        ButtonAlt
    };
    Q_ENUM(ColorRole)

    enum FontRole {
        DefaultSize,
        SmallSize,
        LargeSize,
    };
    Q_ENUM(FontRole)
    
    enum Theme {
        Dark,
        Light
    };
    Q_ENUM(Theme)

    static Stylesheet* instance();
    void applyQss(const QString& qss);
    bool loadQss(const QString& path);
    QString compiled() const;
    
    void setTheme(Theme theme);
    Theme theme() const;

    void setColor(ColorRole role, const QColor& color);
    QColor color(ColorRole role) const;

    void setFontSize(FontRole role, int size);
    int fontSize(FontRole role) const;

private:
    Stylesheet();
    ~Stylesheet();
    Stylesheet(const Stylesheet&) = delete;
    Stylesheet& operator=(const Stylesheet&) = delete;

    class Deleter {
    public:
        static void cleanup(Stylesheet* pointer) { delete pointer; }
    };
    static QScopedPointer<Stylesheet, Deleter> pi;
    QScopedPointer<StylesheetPrivate> p;
};
