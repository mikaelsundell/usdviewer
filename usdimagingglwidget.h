// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>

class UsdImagingGLWidgetPrivate;
class UsdImagingGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    public:
        UsdImagingGLWidget(QWidget* parent = nullptr);
        virtual ~UsdImagingGLWidget();
    
        bool load_file(const QString& filename);
    
        float complexity() const;
        void set_complexity(float complexity);
    
        QColor clearcolor() const;
        void set_clearcolor(const QColor& color);
    
        QList<QString> rendereraovs() const;
        void set_rendereraov(const QString& aov);

    protected:
        void initializeGL() override;
        void paintGL() override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
    
    private:
        QScopedPointer<UsdImagingGLWidgetPrivate> p;
};
