// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QList>
#include <QMap>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {
QMap<QString, QList<QString>>
findVariantSets(UsdStageRefPtr stage, const QList<SdfPath>& paths, bool recursive = false);

void
setVisibility(UsdStageRefPtr stage, const QList<SdfPath>& paths, bool visible, bool recursive = false);
}  // namespace usd
