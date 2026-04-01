// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "stageutils.h"
#include "qtutils.h"
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/variantSets.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <stack>

namespace usdviewer {
namespace stage {
    QMap<QString, QList<QString>> findVariantSets(UsdStageRefPtr stage, const QList<SdfPath>& paths, bool recursive)
    {
        QMap<QString, QList<QString>> result;
        if (!stage || paths.isEmpty())
            return result;

        const QList<SdfPath> filtered = topLevelPaths(paths);

        std::vector<UsdPrim> prims;
        prims.reserve(filtered.size() * 4);

        for (const SdfPath& path : filtered) {
            UsdPrim root = stage->GetPrimAtPath(path);
            if (!root)
                continue;

            prims.push_back(root);

            if (recursive) {
                std::stack<UsdPrim> stack;
                stack.push(root);

                while (!stack.empty()) {
                    UsdPrim prim = stack.top();
                    stack.pop();

                    for (const UsdPrim& child : prim.GetAllChildren()) {
                        prims.push_back(child);
                        stack.push(child);
                    }
                }
            }
        }

        for (const UsdPrim& prim : prims) {
            if (!prim)
                continue;

            const std::vector<std::string> setNames = prim.GetVariantSets().GetNames();
            for (const std::string& setName : setNames) {
                UsdVariantSet variantSet = prim.GetVariantSet(setName);
                const std::vector<std::string> variantNames = variantSet.GetVariantNames();
                const QString key = QString::fromUtf8(setName.c_str());

                QList<QString>& bucket = result[key];
                bucket.reserve(bucket.size() + int(variantNames.size()));
                for (const std::string& value : variantNames)
                    bucket.append(QString::fromUtf8(value.c_str()));
            }
        }

        for (auto it = result.begin(); it != result.end(); ++it) {
            QList<QString>& list = it.value();
            std::sort(list.begin(), list.end());
            list.erase(std::unique(list.begin(), list.end()), list.end());
        }

        return result;
    }

    QList<SdfPath> payloadPaths(UsdStageRefPtr stage, const QList<SdfPath>& paths)
    {
        QList<SdfPath> result;
        if (!stage || paths.isEmpty())
            return result;

        QSet<QString> seen;
        for (const SdfPath& path : paths) {
            const UsdPrim prim = stage->GetPrimAtPath(path);
            if (!prim)
                continue;

            const SdfPath primPath = prim.GetPath();
            const QString key = qt::StringToQString(primPath.GetString());

            if (isPayload(stage, primPath) && !seen.contains(key)) {
                seen.insert(key);
                result.append(primPath);
            }
        }

        return result;
    }

    QList<SdfPath> ancestorPayloadPaths(UsdStageRefPtr stage, const QList<SdfPath>& paths)
    {
        QList<SdfPath> result;
        if (!stage || paths.isEmpty())
            return result;

        QSet<QString> seen;
        for (const SdfPath& path : paths) {
            UsdPrim prim = stage->GetPrimAtPath(path);
            while (prim) {
                const SdfPath primPath = prim.GetPath();
                if (isPayload(stage, primPath)) {
                    const QString key = qt::StringToQString(primPath.GetString());
                    if (!seen.contains(key)) {
                        seen.insert(key);
                        result.append(primPath);
                    }
                    break;
                }
                prim = prim.GetParent();
            }
        }

        return result;
    }

    QList<SdfPath> descendantsPayloadPaths(UsdStageRefPtr stage, const QList<SdfPath>& paths)
    {
        QList<SdfPath> result;
        if (!stage || paths.isEmpty())
            return result;

        QSet<QString> seen;
        std::function<void(const UsdPrim&)> collect = [&](const UsdPrim& prim) {
            if (!prim)
                return;

            const SdfPath primPath = prim.GetPath();
            const QString key = qt::StringToQString(primPath.GetString());

            if (isPayload(stage, primPath) && !seen.contains(key)) {
                seen.insert(key);
                result.append(primPath);
            }

            for (const UsdPrim& child : prim.GetAllChildren())
                collect(child);
        };

        for (const SdfPath& path : paths) {
            const UsdPrim prim = stage->GetPrimAtPath(path);
            if (prim)
                collect(prim);
        }

        return result;
    }

    QList<SdfPath>
    selectionPayloadPaths(UsdStageRefPtr stage, const QList<SdfPath>& paths)
    {
        QList<SdfPath> result;
        if (!stage || paths.isEmpty())
            return result;

        const QList<SdfPath> roots = stage::topLevelPaths(paths);

        for (const SdfPath& path : roots) {
            if (path.IsEmpty())
                continue;

            SdfPath payloadPath;
            SdfPath currentPath = path;

            while (!currentPath.IsEmpty() && currentPath != SdfPath::AbsoluteRootPath()) {
                if (stage::isPayload(stage, currentPath)) {
                    payloadPath = currentPath;
                    break;
                }
                currentPath = currentPath.GetParentPath();
            }

            if (!payloadPath.IsEmpty()) {
                result.append(payloadPath);
                continue;
            }

            const QList<SdfPath> descendantPayloads = stage::descendantsPayloadPaths(stage, { path });
            result.append(descendantPayloads);
        }

        return stage::topLevelPaths(result);
    }

    QList<SdfPath> topLevelPaths(const QList<SdfPath>& paths)
    {
        QList<SdfPath> result;
        result.reserve(paths.size());
        for (const SdfPath& path : paths) {
            bool isChild = false;
            for (const SdfPath& other : paths) {
                if (path != other && path.HasPrefix(other)) {
                    isChild = true;
                    break;
                }
            }
            if (!isChild)
                result.append(path);
        }
        return result;
    }

    GfBBox3d boundingBox(UsdStageRefPtr stage, const QList<SdfPath>& paths)
    {
        UsdGeomBBoxCache cache(UsdTimeCode::Default(), UsdGeomImageable::GetOrderedPurposeTokens(), true);
        GfBBox3d bbox;
        for (const SdfPath& path : paths) {
            UsdPrim prim = stage->GetPrimAtPath(path);
            if (!prim || !prim.IsA<UsdGeomImageable>())
                continue;

            bbox = GfBBox3d::Combine(bbox, cache.ComputeWorldBound(prim));
        }
        return bbox;
    }

    bool isAuthored(UsdStageRefPtr stage, const SdfPath& path)
    {
        if (!stage)
            return false;

        UsdPrim prim = stage->GetPrimAtPath(path);
        if (!prim)
            return false;

        return !prim.GetPrimStack().empty();
    }

    bool isEditTarget(UsdStageRefPtr stage, const SdfPath& path)
    {
        if (!stage)
            return false;

        SdfLayerHandle editLayer = stage->GetEditTarget().GetLayer();
        if (!editLayer)
            return false;

        return editLayer->GetPrimAtPath(path) != nullptr;
    }

    bool
    isLoaded(UsdStageRefPtr stage, const SdfPath& path)
    {
        if (!stage || path.IsEmpty())
            return false;

        const UsdPrim rootPrim = (path == SdfPath::AbsoluteRootPath()) ? stage->GetPseudoRoot() : stage->GetPrimAtPath(path);
        if (!rootPrim)
            return false;

        bool foundPayload = false;

        for (const UsdPrim& prim : UsdPrimRange(rootPrim)) {
            if (!prim)
                continue;

            if (!stage::isPayload(stage, prim.GetPath()))
                continue;

            foundPayload = true;

            if (!prim.IsLoaded())
                return false;
        }

        return foundPayload;
    }

    bool isPayload(UsdStageRefPtr stage, const SdfPath& path)
    {
        if (!stage)
            return false;

        UsdPrim prim = stage->GetPrimAtPath(path);
        if (!prim)
            return false;

        if (prim.HasPayload())
            return true;

        for (const SdfPrimSpecHandle& spec : prim.GetPrimStack()) {
            if (!spec)
                continue;

            auto payloads = spec->GetPayloadList();

            if (!payloads.GetExplicitItems().empty() || !payloads.GetAddedItems().empty()
                || !payloads.GetPrependedItems().empty())
                return true;
        }

        return false;
    }

    bool isPayloadHierarchy(UsdStageRefPtr stage, const SdfPath& path)
    {
        if (!stage)
            return false;

        UsdPrim prim = stage->GetPrimAtPath(path);
        if (!prim)
            return false;

        UsdPrim p = prim;
        while (p) {
            if (isPayload(stage, p.GetPath()))
                return true;
            p = p.GetParent();
        }

        return false;
    }

    bool isEditable(UsdStageRefPtr stage, const SdfPath& path)
    {
        if (!stage)
            return false;

        UsdPrim prim = stage->GetPrimAtPath(path);
        if (!prim || !prim.IsActive())
            return false;

        if (isPayloadHierarchy(stage, path))
            return false;

        if (!isAuthored(stage, path))
            return false;

        if (!isEditTarget(stage, path))
            return false;

        return true;
    }

    bool isVisible(UsdStageRefPtr stage, const SdfPath& path)
    {
        UsdPrim prim = stage->GetPrimAtPath(path);
        if (!prim)
            return true;

        UsdGeomImageable imageable(prim);
        if (!imageable)
            return true;

        TfToken vis;
        if (!imageable.GetVisibilityAttr().Get(&vis))
            return true;
        return vis != UsdGeomTokens->invisible;
    }

    void setVisible(UsdStageRefPtr stage, const QList<SdfPath>& paths, bool visible, bool recursive)
    {
        QList<SdfPath> affected;
        for (const SdfPath& path : paths) {
            UsdPrim prim = stage->GetPrimAtPath(path);
            if (!prim)
                continue;
            UsdGeomImageable imageable(prim);
            if (imageable) {
                if (visible)
                    imageable.MakeVisible();
                else
                    imageable.MakeInvisible();
                affected.append(path);
            }
            if (recursive) {
                for (const UsdPrim& child : prim.GetAllDescendants()) {
                    UsdGeomImageable childImageable(child);
                    if (!childImageable)
                        continue;
                    TfToken currentVis;
                    childImageable.GetVisibilityAttr().Get(&currentVis);
                    TfToken desiredVis = visible ? UsdGeomTokens->inherited : UsdGeomTokens->invisible;
                    if (currentVis != desiredVis) {
                        if (visible)
                            childImageable.MakeVisible();
                        else
                            childImageable.MakeInvisible();
                        affected.append(child.GetPath());
                    }
                }
            }
        }
    }
}  // namespace stage
}  // namespace usdviewer
