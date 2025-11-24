// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "stylesheet.h"
#include "icctransform.h"
#include <QApplication>
#include <QFile>
#include <QMetaEnum>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>

QScopedPointer<Stylesheet, Stylesheet::Deleter> Stylesheet::pi;

class StylesheetPrivate : public QObject {
    Q_OBJECT
public:
    StylesheetPrivate();
    ~StylesheetPrivate();
    QString roleName(Stylesheet::ColorRole role) const;
    QString roleName(Stylesheet::FontRole role) const;
    QString path;
    QString compiled;
    Stylesheet::Theme theme;
    QHash<QString, QColor> palette;
    QHash<QString, int> fonts;
};

StylesheetPrivate::StylesheetPrivate() {}
StylesheetPrivate::~StylesheetPrivate() {}

QString
StylesheetPrivate::roleName(Stylesheet::ColorRole role) const
{
    const QMetaEnum me = QMetaEnum::fromType<Stylesheet::ColorRole>();
    return QString::fromLatin1(me.valueToKey(role)).toLower();
}

QString
StylesheetPrivate::roleName(Stylesheet::FontRole role) const
{
    const QMetaEnum me = QMetaEnum::fromType<Stylesheet::FontRole>();
    return QString::fromLatin1(me.valueToKey(role)).toLower();
}

#include "stylesheet.moc"

Stylesheet::Stylesheet()
    : p(new StylesheetPrivate())
{
    setTheme(Stylesheet::Light);
}

Stylesheet::~Stylesheet() {}

void
Stylesheet::applyQss(const QString& qss)
{
    qApp->setStyleSheet(qss);
}

bool
Stylesheet::loadQss(const QString& path)
{
    QFile file(path);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return false;
    }
    p->path = path;
    QString output = QString::fromUtf8(file.readAll());

    QRegularExpression regex(R"(\$([a-z0-9]+)(?:\.(lightness|saturation)\((\d+)\))?)",
                             QRegularExpression::CaseInsensitiveOption);

    QString result;
    qsizetype lastIndex = 0;
    QRegularExpressionMatchIterator it = regex.globalMatch(output);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        result.append(output.mid(lastIndex, match.capturedStart() - lastIndex));

        QString roleName = match.captured(1).toLower();
        QString modifier = match.captured(2).toLower();
        int factor = match.captured(3).isEmpty() ? 100 : match.captured(3).toInt();

        QColor color = p->palette.value(roleName, QColor());
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

            QString hsl = QString("hsl(%1, %2%, %3%)")
                              .arg(mapped.hue() == -1 ? 0 : mapped.hue())
                              .arg(int(mapped.hslSaturationF() * 100))
                              .arg(int(mapped.lightnessF() * 100));
            result.append(hsl);
        }

        lastIndex = match.capturedEnd();
    }
    result.append(output.mid(lastIndex));

    for (auto it = p->fonts.constBegin(); it != p->fonts.constEnd(); ++it) {
        QString placeholder = "$" + it.key();
        QString replacement = QString::number(it.value()) + "px";
        result.replace(placeholder, replacement, Qt::CaseInsensitive);
    }

    p->compiled = result;
    return true;
}

QString
Stylesheet::compiled() const
{
    return p->compiled;
}

void
Stylesheet::setTheme(Theme theme)
{
    ICCTransform* transform = ICCTransform::instance();

    auto map = [&](ColorRole role, QColor c) {
        QColor mapped = transform->map(c.rgb());
        setColor(role, mapped);
    };

    if (theme == Dark) {
        map(Base, QColor::fromHsl(220, 6, 48));
        map(BaseAlt, QColor::fromHsl(220, 6, 56));
        map(Dock, QColor::fromHsl(220, 6, 56));
        map(DockAlt, QColor::fromHsl(220, 6, 40));
        map(Accent, QColor::fromHsl(220, 6, 20));
        map(AccentAlt, QColor::fromHsl(220, 6, 24));
        map(Text, QColor::fromHsl(0, 0, 220));
        map(TextDisabled, QColor::fromHsl(0, 0, 80));
        map(Highlight, QColor::fromHsl(216, 82, 80));
        map(HighlightAlt, QColor::fromHsl(216, 10, 60));
        map(Border, QColor::fromHsl(220, 3, 32));
        map(BorderAlt, QColor::fromHsl(220, 3, 64));
        map(Scrollbar, QColor::fromHsl(0, 0, 70));
        map(Progress, QColor::fromHsl(216, 82, 20));
        map(Button, QColor::fromHsl(220, 6, 40));
        map(ButtonAlt, QColor::fromHsl(220, 6, 54));
    }
    else {
        map(Base, QColor::fromHsl(0, 0, 220));
        map(BaseAlt, QColor::fromHsl(0, 0, 180));
        map(Dock, QColor::fromHsl(0, 0, 210));
        map(DockAlt, QColor::fromHsl(0, 0, 180));
        map(Accent, QColor::fromHsl(210, 10, 92));
        map(AccentAlt, QColor::fromHsl(210, 10, 88));
        map(Text, QColor::fromHsl(0, 0, 15));
        map(TextDisabled, QColor::fromHsl(0, 0, 65));
        map(Highlight, QColor::fromHsl(210, 90, 180));
        map(HighlightAlt, QColor::fromHsl(210, 60, 220));
        map(Border, QColor::fromHsl(0, 0, 200));
        map(BorderAlt, QColor::fromHsl(0, 0, 220));
        map(Scrollbar, QColor::fromHsl(0, 0, 85));
        map(Progress, QColor::fromHsl(210, 90, 45));
        map(Button, QColor::fromHsl(0, 0, 180));
        map(ButtonAlt, QColor::fromHsl(0, 0, 160));
    }

    setFontSize(DefaultSize, 12);
    setFontSize(SmallSize, 10);
    setFontSize(LargeSize, 14);

    p->compiled.clear();
    p->theme = theme;
}

Stylesheet::Theme
Stylesheet::theme() const
{
    return p->palette.value("base").lightness() < 128 ? Dark : Light;
}

void
Stylesheet::setColor(ColorRole role, const QColor& color)
{
    p->palette[p->roleName(role)] = color;
}

QColor
Stylesheet::color(ColorRole role) const
{
    return p->palette.value(p->roleName(role), QColor());
}

void
Stylesheet::setFontSize(FontRole role, int size)
{
    p->fonts[p->roleName(role)] = size;
}

int
Stylesheet::fontSize(FontRole role) const
{
    return p->fonts.value(p->roleName(role), -1);
}

Stylesheet*
Stylesheet::instance()
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    if (!pi) {
        pi.reset(new Stylesheet());
    }
    return pi.data();
}
