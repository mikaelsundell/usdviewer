// Copyright 2024-present Rapid Images AB
// https://gitlab.rapidimages.se/one-cx/pipeline/usdviewer

#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>

class HydraRendererPrivate;
class HydraRenderer : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
    public:
        explicit HydraRenderer(QWidget* parent = nullptr);
        virtual ~HydraRenderer();
        void load_file(const QString& filename);

    protected:
        void initializeGL() override;
        void paintGL() override;

    private:
        HydraRendererPrivate* p;
};
