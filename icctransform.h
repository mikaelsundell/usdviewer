// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <lcms2.h>

#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QScopedPointer>

class ICCTransformPrivate;
class ICCTransform : public QObject {
    Q_OBJECT
public:
    static ICCTransform* instance();
    QString inputProfile() const;
    QString outputProfile() const;
    QRgb map(QRgb color);
    QImage map(const QImage& image);
    QRgb map(QRgb color, const QString& profile, const QString& outputProfile);
    QImage map(const QImage& image, const QString& profile, const QString& outputProfile);
    QRgb map(QRgb color, const QColorSpace& colorSpace, const QString& outputProfile);
    QImage map(const QImage& image, const QColorSpace& colorSpace, const QString& outputProfile);

public Q_SLOTS:
    void setInputProfile(const QString& inputProfile);
    void setOutputProfile(const QString& displayProfile);

Q_SIGNALS:
    void inputProfileChanged(const QString& inputProfile);
    void outputProfileChanged(const QString& outputProfile);

private:
    ICCTransform();
    ~ICCTransform();
    ICCTransform(const ICCTransform&) = delete;
    ICCTransform& operator=(const ICCTransform&) = delete;
    class Deleter {
    public:
        static void cleanup(ICCTransform* pointer) { delete pointer; }
    };
    static QScopedPointer<ICCTransform, Deleter> pi;
    QScopedPointer<ICCTransformPrivate> p;
};