// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "style.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMetaEnum>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRegularExpression>
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
    QColor color(const QString& name);
    int fontSize(const QString& name);
    QString roleName(Style::ColorRole role) const;
    QString roleName(Style::FontRole role) const;
    struct Data {
        QString path;
        QString compiled;
        Style::Theme theme;
        QHash<QString, QColor> palette;
        QHash<QString, int> fonts;
        QPointer<QObject> object;
    };
    Data d;
};

StylePrivate::StylePrivate() { d.theme = Style::Dark; }
StylePrivate::~StylePrivate() {}

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
    QSurfaceFormat format;
    format.setColorSpace(colorSpace);
    // applying this ensures all new widgets/windows are tagged for
    // the system compositor's color management pipeline.
    QSurfaceFormat::setDefaultFormat(format);
}

void
StylePrivate::updateStylesheet()
{
    QFile file(":/style/resources/App.qss");
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());
        QRegularExpression regex(R"(\$([a-z0-9]+)(?:\.(lightness|saturation)\((\d+)\))?)",
                                 QRegularExpression::CaseInsensitiveOption);
        QString result;
        qsizetype lastIndex = 0;
        QRegularExpressionMatchIterator it = regex.globalMatch(styleSheet);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            result.append(styleSheet.mid(lastIndex, match.capturedStart() - lastIndex));

            QString roleName = match.captured(1).toLower();
            QString modifier = match.captured(2).toLower();
            int factor = match.captured(3).isEmpty() ? 100 : match.captured(3).toInt();

            QColor color = d.palette.value(roleName, QColor());
            if (!color.isValid()) {
                result.append(match.captured(0));
            }
            else {
                QColor mapped = color;
                if (modifier == "lightness") {
                    mapped = mapped.lighter(factor);
                }
                else if (modifier == "saturation") {
                    float h, s, l, a;
                    mapped.getHslF(&h, &s, &l, &a);
                    s = std::clamp(s * factor / 100.0, 0.0, 1.0);
                    mapped.setHslF(h, s, l, a);
                }
                float h, s, l;
                mapped.getHslF(&h, &s, &l);
                QString hsl = QString("hsl(%1, %2%, %3%)")
                                  .arg(h < 0.0f ? 0.0f : h * 360.0f, 0, 'f', 6)
                                  .arg(s * 100.0f, 0, 'f', 6)
                                  .arg(l * 100.0f, 0, 'f', 6);

                result.append(hsl);
            }
            lastIndex = match.capturedEnd();
        }
        result.append(styleSheet.mid(lastIndex));
        for (auto it = d.fonts.constBegin(); it != d.fonts.constEnd(); ++it) {
            QString placeholder = "$" + it.key();
            QString replacement = QString::number(it.value()) + "px";
            result.replace(placeholder, replacement, Qt::CaseInsensitive);
        }
        qApp->setStyleSheet(result);
    }
}

void
StylePrivate::updateTheme(Style::Theme theme)
{
    auto map = [&](Style::ColorRole role, QColor color) { d.palette[roleName(role)] = color; };
    if (theme == Style::Dark) {
        map(Style::Base, QColor::fromHsl(220, 6, 42));
        map(Style::BaseAlt, QColor::fromHsl(220, 6, 48));
        map(Style::Dock, QColor::fromHsl(220, 6, 56));
        map(Style::DockAlt, QColor::fromHsl(220, 6, 40));
        map(Style::Accent, QColor::fromHsl(220, 6, 20));
        map(Style::AccentAlt, QColor::fromHsl(220, 6, 24));
        map(Style::Text, QColor::fromHsl(0, 0, 220));
        map(Style::TextDisabled, QColor::fromHsl(0, 0, 80));
        map(Style::Highlight, QColor::fromHsl(216, 82, 80));
        map(Style::HighlightAlt, QColor::fromHsl(216, 10, 60));
        map(Style::Border, QColor::fromHsl(220, 3, 32));
        map(Style::BorderAlt, QColor::fromHsl(220, 3, 64));
        map(Style::Handle, QColor::fromHsl(0, 0, 150));
        map(Style::Progress, QColor::fromHsl(216, 82, 20));
        map(Style::Button, QColor::fromHsl(220, 6, 40));
        map(Style::ButtonAlt, QColor::fromHsl(220, 6, 54));
        map(Style::Render, QColor::fromHsl(220, 6, 25));
        map(Style::RenderAlt, QColor::fromHsl(220, 6, 40));
    }
    else {
        map(Style::Base, QColor::fromHsl(0, 0, 210));
        map(Style::BaseAlt, QColor::fromHsl(0, 0, 208));
        map(Style::Dock, QColor::fromHsl(0, 0, 210));
        map(Style::DockAlt, QColor::fromHsl(0, 0, 180));
        map(Style::Accent, QColor::fromHsl(210, 10, 92));
        map(Style::AccentAlt, QColor::fromHsl(210, 10, 88));
        map(Style::Text, QColor::fromHsl(0, 0, 15));
        map(Style::TextDisabled, QColor::fromHsl(0, 0, 65));
        map(Style::Highlight, QColor::fromHsl(210, 90, 180));
        map(Style::HighlightAlt, QColor::fromHsl(210, 60, 220));
        map(Style::Border, QColor::fromHsl(0, 0, 200));
        map(Style::BorderAlt, QColor::fromHsl(0, 0, 180));
        map(Style::Handle, QColor::fromHsl(0, 0, 120));
        map(Style::Progress, QColor::fromHsl(210, 90, 45));
        map(Style::Button, QColor::fromHsl(0, 0, 180));
        map(Style::ButtonAlt, QColor::fromHsl(0, 0, 160));
        map(Style::Render, QColor::fromHsl(220, 6, 25));
        map(Style::RenderAlt, QColor::fromHsl(220, 6, 40));
    }
    d.fonts[roleName(Style::LargeSize)] = 14;
    d.fonts[roleName(Style::DefaultSize)] = 12;
    d.fonts[roleName(Style::SmallSize)] = 10;
}

QColorSpace
StylePrivate::colorSpace() const
{
    return QSurfaceFormat::defaultFormat().colorSpace();
}

QColor
StylePrivate::color(const QString& name)
{
    return d.palette.value(name, QColor());
}

int
StylePrivate::fontSize(const QString& name)
{
    return d.fonts.value(name, -1);
}

QString
StylePrivate::roleName(Style::ColorRole role) const
{
    const QMetaEnum me = QMetaEnum::fromType<Style::ColorRole>();
    return QString::fromLatin1(me.valueToKey(role)).toLower();
}

QString
StylePrivate::roleName(Style::FontRole role) const
{
    const QMetaEnum me = QMetaEnum::fromType<Style::FontRole>();
    return QString::fromLatin1(me.valueToKey(role)).toLower();
}

Style::Style()
    : p(new StylePrivate())
{
    p->d.object = this;
    p->init();
}

Style::~Style() = default;

void
Style::setTheme(Style::Theme theme)
{
    p->updateTheme(theme);
}

Style::Theme
Style::theme() const
{
    return p->d.palette.value("base").lightness() < 128 ? Dark : Light;
}

void
Style::setColor(ColorRole role, const QColor& color)
{
    p->d.palette[p->roleName(role)] = color;
}

QColor
Style::color(ColorRole role) const
{
    return p->color(p->roleName(role));
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

void
Style::setFontSize(FontRole role, int size)
{
    return p->d.fonts.value(p->roleName(role), -1);
}

int
Style::fontSize(FontRole role) const
{
    return p->fontSize(p->roleName(role));
}
}  // namespace usd
