// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QList>
#include <QObject>
#include <QScopedPointer>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
class SelectionPrivate;
class Selection : public QObject {
    Q_OBJECT
public:
    Selection(QObject* parent = nullptr);
    ~Selection();
    bool isSelected(const SdfPath& path) const;
    void addPath(const SdfPath& path);
    void replacePaths(const QList<SdfPath>& paths);
    void removePath(const SdfPath& path);
    QList<SdfPath> paths() const;
    void clear();
    bool isValid() const;

Q_SIGNALS:
    void selectionChanged() const;

private:
    QScopedPointer<SelectionPrivate> p;
};
}  // namespace usd
