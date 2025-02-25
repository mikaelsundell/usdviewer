// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include "usdstage.h"
#include "usdselection.h"
#include "usdviewcamera.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

namespace usd {
class ImagingGLWidgetPrivate;
class ImagingGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
    public:
        enum Complexity { Low, Medium, High, VeryHigh };
        ImagingGLWidget(QWidget* parent = nullptr);
        virtual ~ImagingGLWidget();
        ViewCamera viewCamera() const;
        QImage image();
    
        ImagingGLWidget::Complexity complexity() const;
        void setComplexity(ImagingGLWidget::Complexity complexity);
    
        QColor clearColor() const;
        void setClearColor(const QColor& color);
    
        QList<QString> rendererAovs() const;
        void setRendererAov(const QString& aov);
    
        Selection* selection();
        void setSelection(Selection* selection);

        Stage stage() const;
        bool setStage(const Stage& stage);
    
    public Q_SLOTS:
        void updateSelection();
    
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
