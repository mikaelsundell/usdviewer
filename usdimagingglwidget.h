// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "usdstage.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

#include <QResizeEvent>

namespace usd {
class ImagingGLWidgetPrivate;
class ImagingGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
    public:
        ImagingGLWidget(QWidget* parent = nullptr);
        virtual ~ImagingGLWidget();
        Stage stage() const;
        bool setStage(const Stage& stage);
        float complexity() const;
        void setComplexity(float complexity);
        QColor clearColor() const;
        void setClearColor(const QColor& color);
        QList<QString> rendererAovs() const;
        void setRendererAov(const QString& aov);
        
    Q_SIGNALS:
        void rendererReady();
        
    protected:
        void initializeGL() override;
        void paintGL() override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;

    private:
        QScopedPointer<ImagingGLWidgetPrivate> p;
};
}
