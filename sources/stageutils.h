// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QList>
#include <QMap>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usd {

/**
 * @brief Finds variant sets for the specified prim paths.
 *
 * Traverses the provided paths and collects variant sets available
 * on the corresponding prims. The result maps each prim path to the
 * list of variant set names found on that prim.
 *
 * If @p recursive is enabled, child prims are also traversed and
 * included in the result.
 *
 * @param stage USD stage to query.
 * @param paths Prim paths to inspect.
 * @param recursive If true, traverse child prims recursively.
 *
 * @return Map of prim path strings to variant set names.
 */
QMap<QString, QList<QString>>
findVariantSets(UsdStageRefPtr stage, const QList<SdfPath>& paths, bool recursive = false);

/**
 * @brief Collects payload prim paths under the specified prim paths.
 *
 * Traverses the given prim paths and collects all prims that contain
 * payloads. When @p recursive is enabled, all descendants are also
 * traversed.
 *
 * @param stage USD stage to query.
 * @param paths Root prim paths.
 * @param recursive Traverse children recursively.
 *
 * @return List of payload prim paths.
 */
QList<SdfPath>
payloadPaths(UsdStageRefPtr stage, const QList<SdfPath>& paths, bool recursive = true);

/**
 * @brief Filters a list of prim paths to only top-most paths.
 *
 * Removes paths that are descendants of other paths in the list.
 * This prevents redundant traversal when operating recursively
 * on hierarchies.
 *
 * Example:
 *   /A
 *   /A/B
 *   /A/B/C
 *
 * Result:
 *   /A
 *
 * @param paths Prim paths to filter.
 *
 * @return List of top-most prim paths.
 */
QList<SdfPath>
rootPaths(const QList<SdfPath>& paths);

/**
 * @brief Computes the bounding box for the specified prim paths.
 *
 * Evaluates the world-space bounding box of the given prims
 * within the USD stage. The result combines the bounds of
 * all provided prims into a single @c GfBBox3d.
 *
 * @param stage USD stage containing the prims.
 * @param paths Prim paths to include in the bounding box calculation.
 *
 * @return Combined bounding box of the prims.
 */
GfBBox3d
boundingBox(UsdStageRefPtr stage, const QList<SdfPath>& paths);

/**
 * @brief Sets visibility for the specified prim paths.
 *
 * Updates the visibility state of the given prims on the stage.
 * When @p recursive is enabled, the visibility state is also
 * applied to all descendant prims.
 *
 * @param stage USD stage containing the prims.
 * @param paths Prim paths to update.
 * @param visible Visibility state to apply.
 * @param recursive If true, apply visibility recursively.
 */
void
setVisibility(UsdStageRefPtr stage, const QList<SdfPath>& paths, bool visible, bool recursive = false);

}  // namespace usd
