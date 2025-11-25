// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025 - present Mikael Sundell
// https://github.com/mikaelsundell/usdviewer

#include "usdstageutils.h"
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/variantSets.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <stack>

namespace usd {
QMap<QString, QList<QString>>
findVariantSets(UsdStageRefPtr stage, const QList<SdfPath>& paths, bool recursive)
{
    QMap<QString, QList<QString>> result;
    std::vector<SdfPath> filtered;
    filtered.reserve(paths.size());
    for (const SdfPath& p : paths) {
        bool isChild = false;
        for (const SdfPath& other : paths) {
            if (p == other)
                continue;
            if (p.HasPrefix(other)) {
                isChild = true;
                break;
            }
        }
        if (!isChild)
            filtered.push_back(p);
    }
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
                UsdPrim p = stack.top();
                stack.pop();

                for (const UsdPrim& child : p.GetAllChildren()) {
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
            UsdVariantSet vs = prim.GetVariantSet(setName);
            const std::vector<std::string> variantNames = vs.GetVariantNames();
            QString key = QString::fromUtf8(setName.c_str());

            QList<QString>& bucket = result[key];
            bucket.reserve(bucket.size() + int(variantNames.size()));
            for (const std::string& v : variantNames) {
                bucket.append(QString::fromUtf8(v.c_str()));
            }
        }
    }
    for (auto it = result.begin(); it != result.end(); ++it) {
        QList<QString>& list = it.value();

        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
    }
    return result;
}

GfBBox3d
boundingBox(UsdStageRefPtr stage, const QList<SdfPath>& paths)
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

void
setVisibility(UsdStageRefPtr stage, const QList<SdfPath>& paths, bool visible, bool recursive)
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
}  // namespace usd
