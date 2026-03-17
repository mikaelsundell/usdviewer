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
     * @brief Global theme modes.
     */
    enum Theme { ThemeDark, ThemeLight };
    Q_ENUM(Theme)

    /**
     * @brief Semantic color roles.
     */
    enum ColorRole {
        ColorBase,
        ColorBaseAlt,
        ColorDock,
        ColorDockAlt,
        ColorAccent,
        ColorAccentAlt,
        ColorText,
        ColorTextDisabled,
        ColorHighlight,
        ColorHighlightAlt,
        ColorBorder,
        ColorBorderAlt,
        ColorHandle,
        ColorProgress,
        ColorButton,
        ColorButtonAlt,
        ColorRender,
        ColorRenderAlt
    };
    Q_ENUM(ColorRole)

    /**
     * @brief Semantic icon roles.
     */
    enum IconRole {
        IconBranchOpen,
        IconBranchClosed,
        IconHidden,
        IconVisible,
        IconChecked,
        IconDropdown,
        IconLeft,
        IconMaterial,
        IconMesh,
        IconPartiallyChecked,
        IconPrim,
        IconRight
    };
    Q_ENUM(IconRole)

    /**
     * @brief Logical UI scale levels.
     */
    enum UIScale { UISmall, UIMedium, UILarge };
    Q_ENUM(UIScale)

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
     * @brief Returns color for a role.
     */
    QColor color(ColorRole role) const;

    /**
     * @brief Returns icon for a role and size.
     */
    QPixmap icon(IconRole role, UIScale scale = UIScale::UIMedium) const;

    /**
     * @brief Returns font size for a scale.
     */
    int fontSize(UIScale scale) const;

    /**
     * @brief Sets the font size for a scale.
     */
    void setFontSize(UIScale scale, int size);

    /**
     * @brief Returns icon size for a scale.
     */
    int iconSize(UIScale scale) const;

    /**
     * @brief Sets the icon size for a scale.
     */
    void setIconSize(UIScale scale, int size);

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

private:
    Q_DISABLE_COPY_MOVE(Style)
    QScopedPointer<StylePrivate> p;
};

}  // namespace usd
