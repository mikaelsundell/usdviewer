// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#pragma once

#include <QList>
#include <QMap>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdviewer {
namespace stage {
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
    QMap<QString, QList<QString>> findVariantSets(UsdStageRefPtr stage, const QList<SdfPath>& paths,
                                                  bool recursive = false);

    /**
     * @brief Collects payload prim paths at the specified prim paths.
     *
     * Checks each path in @p paths and returns those whose corresponding
     * prim directly authors or contains a payload.
     *
     * @param stage USD stage to query.
     * @param paths Prim paths to test.
     *
     * @return List of payload prim paths found exactly at @p paths.
     */
    QList<SdfPath> payloadPaths(UsdStageRefPtr stage, const QList<SdfPath>& paths);

    /**
     * @brief Collects nearest payload ancestor prim paths for the specified prim paths.
     *
     * For each path in @p paths, walks upward through its parent hierarchy until
     * the nearest prim that directly authors or contains a payload is found.
     * At most one payload ancestor is returned per input path.
     *
     * @param stage USD stage to query.
     * @param paths Prim paths to resolve upward from.
     *
     * @return List of nearest payload ancestor prim paths.
     */
    QList<SdfPath> ancestorPayloadPaths(UsdStageRefPtr stage, const QList<SdfPath>& paths);

    /**
     * @brief Collects payload prim paths at and below the specified prim paths.
     *
     * Traverses each path in @p paths and recursively visits its descendants,
     * collecting prims that directly author or contain payloads.
     *
     * @param stage USD stage to query.
     * @param paths Root prim paths to traverse downward from.
     *
     * @return List of payload prim paths found in the descendant hierarchies.
     */
    QList<SdfPath> descendantsPayloadPaths(UsdStageRefPtr stage, const QList<SdfPath>& paths);

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
    QList<SdfPath> rootPaths(const QList<SdfPath>& paths);

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
    GfBBox3d boundingBox(UsdStageRefPtr stage, const QList<SdfPath>& paths);

    /**
     * @brief Checks whether a prim has authored specifications.
     *
     * Determines if the prim at the given path has any authored
     * @c SdfPrimSpec entries across the composed layer stack.
     *
     * @param stage USD stage containing the prim.
     * @param path  Prim path to evaluate.
     *
     * @return True if the prim has authored specs, false otherwise.
     */
    bool isAuthored(UsdStageRefPtr stage, const SdfPath& path);

    /**
     * @brief Checks whether a prim exists in the current edit target.
     *
     * Evaluates if the prim has a specification in the active edit
     * target layer of the stage. This indicates whether edits will
     * be authored directly in the current layer.
     *
     * @param stage USD stage containing the prim.
     * @param path  Prim path to evaluate.
     *
     * @return True if the prim exists in the edit target layer, false otherwise.
     */
    bool isEditTarget(UsdStageRefPtr stage, const SdfPath& path);

    /**
     * @brief Checks whether a prim has a payload.
     *
     * Determines if the prim at the given path has an authored
     * payload, either directly or through its composed prim stack.
     *
     * @param stage USD stage containing the prim.
     * @param path  Prim path to evaluate.
     *
     * @return True if the prim has a payload, false otherwise.
     */
    bool isPayload(UsdStageRefPtr stage, const SdfPath& path);

    /**
     * @brief Checks whether a prim is part of a payload hierarchy.
     *
     * Evaluates whether the prim or any of its ancestors has a payload.
     * This effectively determines if the prim is located under a payload
     * composition boundary.
     *
     * @param stage USD stage containing the prim.
     * @param path  Prim path to evaluate.
     *
     * @return True if the prim is under a payload hierarchy, false otherwise.
     */
    bool isPayloadHierarchy(UsdStageRefPtr stage, const SdfPath& path);

    /**
     * @brief Checks whether a prim is editable.
     *
     * Determines if the prim can be modified under the current editing
     * policy. A prim is considered editable if it:
     * - Is active and valid.
     * - Is not under a payload hierarchy.
     * - Has authored specifications.
     * - Exists in the current edit target layer.
     *
     * @param stage USD stage containing the prim.
     * @param path  Prim path to evaluate.
     *
     * @return True if the prim is editable, false otherwise.
     */
    bool isEditable(UsdStageRefPtr stage, const SdfPath& path);

    /**
     * @brief Returns the visibility state of a prim.
     *
     * Queries the authored visibility of the specified prim on the stage.
     * If no visibility is authored, the prim is considered visible (inherited).
     *
     * This function evaluates only the prim’s own visibility attribute and
     * does not account for inherited visibility from parent prims.
     *
     * @param stage USD stage containing the prim.
     * @param path Prim path to query.
     *
     * @return True if the prim is visible, false if it is explicitly invisible.
     */
    bool isVisible(UsdStageRefPtr stage, const SdfPath& path);

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
    void setVisible(UsdStageRefPtr stage, const QList<SdfPath>& paths, bool visible, bool recursive = false);

}  // namespace stage
}  // namespace usdviewer
