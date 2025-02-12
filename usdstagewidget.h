// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2024 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>

class UsdStageWidgetPrivate;
class UsdStageWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    public:
        UsdStageWidget(QWidget* parent = nullptr);
        virtual ~UsdStageWidget();
        bool load_file(const QString& filename);

    protected:
        void initializeGL() override;
        void paintGL() override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
    
    private:
        QScopedPointer<UsdStageWidgetPrivate> p;
};
