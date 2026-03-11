// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QColor>
#include <QColorSpace>
#include <QObject>
#include <QScopedPointer>

namespace usd {
class StylePrivate;
class Style : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Semantic color roles.
     */
    enum ColorRole {
        Base,
        BaseAlt,
        Dock,
        DockAlt,
        Accent,
        AccentAlt,
        Text,
        TextDisabled,
        Highlight,
        HighlightAlt,
        Border,
        BorderAlt,
        Handle,
        Progress,
        Button,
        ButtonAlt,
        Render,
        RenderAlt
    };
    Q_ENUM(ColorRole)

    /**
     * @brief Logical font size roles.
     */
    enum FontRole { DefaultSize, SmallSize, LargeSize };
    Q_ENUM(FontRole)

    /**
     * @brief Global theme modes.
     */
    enum Theme { Dark, Light };
    Q_ENUM(Theme)

    /**
     * @brief Constructs a Style instance.
     */
    Style();

    /**
     * @brief Destroys the Style instance.
     */
    ~Style() override;

    /** @name Theme */
    ///@{

    /**
     * @brief Sets the active theme.
     */
    void setTheme(Theme theme);

    /**
     * @brief Returns the active theme.
     */
    Theme theme() const;

    /**
     * @brief Sets color for a role.
     */
    void setColor(ColorRole role, const QColor& color);

    /**
     * @brief Returns color for a role.
     */
    QColor color(ColorRole role) const;

    ///@}

    /** @name Rendering */
    ///@{

    /**
     * @brief Sets rendering color space.
     */
    void setColorSpace(const QColorSpace& colorSpace);

    /**
     * @brief Returns rendering color space.
     */
    QColorSpace colorSpace() const;

    /**
     * @brief Sets font size for a role.
     */
    void setFontSize(FontRole role, int size);

    /**
     * @brief Returns font size for a role.
     */
    int fontSize(FontRole role) const;

    ///@}

Q_SIGNALS:

    /**
     * @brief Emitted when theme changes.
     */
    void themeChanged(Theme theme);

    /**
     * @brief Emitted when a color role changes.
     */
    void colorChanged(ColorRole role);

    /**
     * @brief Emitted when a font role changes.
     */
    void fontChanged(FontRole role);

private:
    Q_DISABLE_COPY_MOVE(Style)
    QScopedPointer<StylePrivate> p;
};

}  // namespace usd
