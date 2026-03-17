// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "style.h"

#include <QApplication>
#include <QFile>
#include <QMetaEnum>
#include <QPixmap>
#include <QPointer>
#include <QRegularExpression>
#include <QScopedPointer>
#include <QSurfaceFormat>

namespace usd {

class StylePrivate {
public:
    StylePrivate();
    ~StylePrivate();

    void init();
    void updateTheme(Style::Theme theme);
    void updateColorSpace(const QColorSpace& colorSpace);
    void updateStylesheet();

    QColorSpace colorSpace() const;
    QColor color(Style::ColorRole role) const;
    int fontSize(Style::UIScale scale) const;
    int iconSize(Style::UIScale scale) const;
    QString iconPath(Style::IconRole role) const;

    QString roleName(Style::ColorRole role) const;
    QString roleName(Style::IconRole role) const;
    QString roleName(Style::UIScale scale) const;

public:
    struct Data {
        Style::Theme theme = Style::ThemeDark;
        QHash<QString, QColor> palette;
        QHash<QString, QString> icons;
        QHash<QString, int> fontSizes;
        QHash<QString, int> iconSizes;
        QHash<int, QPixmap> pixmaps;
        QPointer<QObject> object;
    };

    Data d;
};

StylePrivate::StylePrivate() = default;
StylePrivate::~StylePrivate() = default;

void
StylePrivate::init()
{
    updateTheme(d.theme);
    updateColorSpace(QColorSpace::SRgb);
    updateStylesheet();
}

void
StylePrivate::updateColorSpace(const QColorSpace& colorSpace)
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setColorSpace(colorSpace);
    QSurfaceFormat::setDefaultFormat(format);
}

void
StylePrivate::updateStylesheet()
{
    QFile file(":/style/resources/App.qss");
    if (!file.open(QFile::ReadOnly))
        return;

    QString styleSheet = QString::fromUtf8(file.readAll());

    QRegularExpression regex(
        R"(\$([a-z0-9_]+(?:\.(?!(?:lightness|saturation)\()[a-z0-9_]+){0,2})(?:\.(lightness|saturation)\((\d+)\))?)",
        QRegularExpression::CaseInsensitiveOption);

    QString result;
    result.reserve(styleSheet.size());

    qsizetype lastIndex = 0;
    QRegularExpressionMatchIterator it = regex.globalMatch(styleSheet);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();

        result.append(styleSheet.mid(lastIndex, match.capturedStart() - lastIndex));

        const QString path = match.captured(1).toLower();
        const QString modifier = match.captured(2).toLower();
        const int factor = match.captured(3).isEmpty() ? 100 : match.captured(3).toInt();

        const QStringList parts = path.split('.');
        const QString group = parts.value(0);
        const QString part1 = parts.value(1);
        const QString part2 = parts.value(2);

        bool replaced = false;
        if (group == "color" && !part1.isEmpty()) {
            QString key = "color" + part1;
            if (!part2.isEmpty())
                key += part2;

            QColor mapped = d.palette.value(key);
            if (mapped.isValid()) {
                if (modifier == "lightness") {
                    mapped = mapped.lighter(factor);
                }
                else if (modifier == "saturation") {
                    float h, s, l, a;
                    mapped.getHslF(&h, &s, &l, &a);
                    s = std::clamp(s * float(factor) / 100.0f, 0.0f, 1.0f);
                    mapped.setHslF(h, s, l, a);
                }

                result.append(QString("rgba(%1, %2, %3, %4)")
                                  .arg(mapped.red())
                                  .arg(mapped.green())
                                  .arg(mapped.blue())
                                  .arg(mapped.alpha()));
                replaced = true;
            }
        }
        if (!replaced && group == "font" && part1 == "size" && !part2.isEmpty()) {
            const QString key = "ui" + part2;
            const int size = d.fontSizes.value(key, -1);
            if (size > 0) {
                result.append(QString::number(size) + "px");
                replaced = true;
            }
        }
        if (!replaced && group == "icon" && !part1.isEmpty() && part2.isEmpty()) {
            const QString path = d.icons.value("icon" + part1);
            if (!path.isEmpty()) {
                result.append(path);
                replaced = true;
            }
        }

        if (!replaced)
            result.append(match.captured(0));

        lastIndex = match.capturedEnd();
    }
    result.append(styleSheet.mid(lastIndex));

    qDebug().noquote() << result;

    qApp->setStyleSheet(result);
}

void
StylePrivate::updateTheme(Style::Theme theme)
{
    d.theme = theme;
    d.palette.clear();

    auto map = [&](Style::ColorRole role, QColor color) { d.palette[roleName(role)] = color; };
    if (theme == Style::ThemeDark) {
        map(Style::ColorBase, QColor::fromHsl(220, 6, 42));
        map(Style::ColorBaseAlt, QColor::fromHsl(220, 6, 48));
        map(Style::ColorDock, QColor::fromHsl(220, 6, 56));
        map(Style::ColorDockAlt, QColor::fromHsl(220, 6, 40));
        map(Style::ColorAccent, QColor::fromHsl(220, 6, 20));
        map(Style::ColorAccentAlt, QColor::fromHsl(220, 6, 24));
        map(Style::ColorText, QColor::fromHsl(0, 0, 220));
        map(Style::ColorTextDisabled, QColor::fromHsl(0, 0, 160));
        map(Style::ColorHighlight, QColor::fromHsl(216, 82, 80));
        map(Style::ColorHighlightAlt, QColor::fromHsl(216, 60, 60));
        map(Style::ColorBorder, QColor::fromHsl(220, 3, 32));
        map(Style::ColorBorderAlt, QColor::fromHsl(220, 3, 64));
        map(Style::ColorHandle, QColor::fromHsl(0, 0, 150));
        map(Style::ColorProgress, QColor::fromHsl(216, 82, 20));
        map(Style::ColorButton, QColor::fromHsl(220, 6, 40));
        map(Style::ColorButtonAlt, QColor::fromHsl(220, 6, 54));
        map(Style::ColorRender, QColor::fromHsl(220, 6, 25));
        map(Style::ColorRenderAlt, QColor::fromHsl(220, 6, 40));
    }
    else {
        map(Style::ColorBase, QColor::fromHsl(0, 0, 210));
        map(Style::ColorBaseAlt, QColor::fromHsl(0, 0, 208));
        map(Style::ColorDock, QColor::fromHsl(0, 0, 210));
        map(Style::ColorDockAlt, QColor::fromHsl(0, 0, 180));
        map(Style::ColorAccent, QColor::fromHsl(210, 10, 92));
        map(Style::ColorAccentAlt, QColor::fromHsl(210, 10, 88));
        map(Style::ColorText, QColor::fromHsl(0, 0, 15));
        map(Style::ColorTextDisabled, QColor::fromHsl(0, 0, 65));
        map(Style::ColorHighlight, QColor::fromHsl(210, 90, 180));
        map(Style::ColorHighlightAlt, QColor::fromHsl(210, 40, 220));
        map(Style::ColorBorder, QColor::fromHsl(0, 0, 200));
        map(Style::ColorBorderAlt, QColor::fromHsl(0, 0, 180));
        map(Style::ColorHandle, QColor::fromHsl(0, 0, 120));
        map(Style::ColorProgress, QColor::fromHsl(210, 90, 45));
        map(Style::ColorButton, QColor::fromHsl(0, 0, 180));
        map(Style::ColorButtonAlt, QColor::fromHsl(0, 0, 160));
        map(Style::ColorRender, QColor::fromHsl(220, 6, 25));
        map(Style::ColorRenderAlt, QColor::fromHsl(220, 6, 40));
    }
    d.icons[roleName(Style::IconBranchOpen)] = ":/icons/resources/BranchOpen.png";
    d.icons[roleName(Style::IconBranchClosed)] = ":/icons/resources/BranchClosed.png";
    d.icons[roleName(Style::IconVisible)] = ":/icons/resources/Visible.png";
    d.icons[roleName(Style::IconHidden)] = ":/icons/resources/Hidden.png";
    d.icons[roleName(Style::IconChecked)] = ":/icons/resources/Checked.png";
    d.icons[roleName(Style::IconDropdown)] = ":/icons/resources/Dropdown.png";
    d.icons[roleName(Style::IconLeft)] = ":/icons/resources/Left.png";
    d.icons[roleName(Style::IconMaterial)] = ":/icons/resources/Material.png";
    d.icons[roleName(Style::IconMesh)] = ":/icons/resources/Mesh.png";
    d.icons[roleName(Style::IconPartiallyChecked)] = ":/icons/resources/PartiallyChecked.png";
    d.icons[roleName(Style::IconPrim)] = ":/icons/resources/Prim.png";
    d.icons[roleName(Style::IconRight)] = ":/icons/resources/Right.png";

    d.fontSizes[roleName(Style::UISmall)] = 10;
    d.fontSizes[roleName(Style::UIMedium)] = 12;
    d.fontSizes[roleName(Style::UILarge)] = 14;

    d.iconSizes[roleName(Style::UISmall)] = 16;
    d.iconSizes[roleName(Style::UIMedium)] = 24;
    d.iconSizes[roleName(Style::UILarge)] = 32;
}

QColorSpace
StylePrivate::colorSpace() const
{
    return QSurfaceFormat::defaultFormat().colorSpace();
}

QColor
StylePrivate::color(Style::ColorRole role) const
{
    return d.palette.value(roleName(role), QColor());
}

int
StylePrivate::fontSize(Style::UIScale scale) const
{
    return d.fontSizes.value(roleName(scale), -1);
}

int
StylePrivate::iconSize(Style::UIScale scale) const
{
    return d.iconSizes.value(roleName(scale), -1);
}

QString
StylePrivate::iconPath(Style::IconRole role) const
{
    return d.icons.value(roleName(role));
}

QString
StylePrivate::roleName(Style::ColorRole role) const
{
    const QMetaEnum me = QMetaEnum::fromType<Style::ColorRole>();
    return QString::fromLatin1(me.valueToKey(role)).toLower();
}

QString
StylePrivate::roleName(Style::IconRole role) const
{
    const QMetaEnum me = QMetaEnum::fromType<Style::IconRole>();
    return QString::fromLatin1(me.valueToKey(role)).toLower();
}

QString
StylePrivate::roleName(Style::UIScale scale) const
{
    const QMetaEnum me = QMetaEnum::fromType<Style::UIScale>();
    return QString::fromLatin1(me.valueToKey(scale)).toLower();
}

Style::Style()
    : p(new StylePrivate())
{
    p->d.object = this;
    p->init();
}

Style::~Style() = default;

void
Style::setTheme(Theme theme)
{
    if (p->d.theme == theme)
        return;

    p->updateTheme(theme);
    Q_EMIT themeChanged(theme);
}

Style::Theme
Style::theme() const
{
    return p->d.theme;
}

QColor
Style::color(ColorRole role) const
{
    return p->color(role);
}

QPixmap
Style::icon(IconRole role, UIScale scale) const
{
    const int size = iconSize(scale);
    if (size <= 0)
        return QPixmap();

    const int roleKey = int(role);
    auto it = p->d.pixmaps.constFind(roleKey);
    if (it == p->d.pixmaps.constEnd()) {
        const QString path = p->iconPath(role);
        if (path.isEmpty())
            return QPixmap();

        QPixmap loaded(path);
        if (loaded.isNull())
            return QPixmap();

        it = p->d.pixmaps.insert(roleKey, loaded);
    }

    return it.value().scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

int
Style::fontSize(UIScale scale) const
{
    return p->fontSize(scale);
}

void
Style::setFontSize(UIScale scale, int size)
{
    if (size <= 0)
        return;

    const QString key = p->roleName(scale);
    if (p->d.fontSizes.value(key) == size)
        return;

    p->d.fontSizes[key] = size;
}

int
Style::iconSize(UIScale scale) const
{
    return p->iconSize(scale);
}

void
Style::setIconSize(UIScale scale, int size)
{
    if (size <= 0)
        return;

    const QString key = p->roleName(scale);
    if (p->d.iconSizes.value(key) == size)
        return;
}

void
Style::setColorSpace(const QColorSpace& colorSpace)
{
    p->updateColorSpace(colorSpace);
}

QColorSpace
Style::colorSpace() const
{
    return p->colorSpace();
}

}  // namespace usd
