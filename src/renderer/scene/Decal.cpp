#include "renderer/scene/Decal.hpp"

#include "renderer/scene/Frustum.hpp"
#include "renderer/scene/Math.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace full_renderer::scene
{
namespace
{
constexpr float kMinimumExtentMeters = 0.001f;
constexpr float kMinimumDeterminant = 0.000001f;
constexpr float kMaximumProjectionDepthMeters = 10000.0f;
constexpr float kMaximumProjectionEdgeFadeMeters = 1000.0f;
constexpr float kMinimumCullPaddingMeters = 0.05f;
constexpr float kCullPaddingExtentFraction = 0.02f;

bool isUnitRangeColor4(const float values[4]) noexcept
{
    for (int index = 0; index < 4; ++index)
    {
        if (!std::isfinite(values[index]) || values[index] < 0.0f || values[index] > 1.0f)
        {
            return false;
        }
    }

    return true;
}

float determinant3x3ColumnMajor(const float m[16]) noexcept
{
    const float a00 = m[0];
    const float a01 = m[4];
    const float a02 = m[8];
    const float a10 = m[1];
    const float a11 = m[5];
    const float a12 = m[9];
    const float a20 = m[2];
    const float a21 = m[6];
    const float a22 = m[10];
    return a00 * (a11 * a22 - a12 * a21) -
        a01 * (a10 * a22 - a12 * a20) +
        a02 * (a10 * a21 - a11 * a20);
}

void identity(float out[16]) noexcept
{
    for (int index = 0; index < 16; ++index)
    {
        out[index] = 0.0f;
    }
    out[0] = 1.0f;
    out[5] = 1.0f;
    out[10] = 1.0f;
    out[15] = 1.0f;
}

float conservativeCullPaddingMeters(const DecalDesc& desc, const float edgeFadeMeters) noexcept
{
    const float largestExtent = std::max(
        desc.halfExtentsMeters[0],
        std::max(desc.halfExtentsMeters[1], desc.halfExtentsMeters[2]));
    const float extentPadding = largestExtent * kCullPaddingExtentFraction;
    return std::max(kMinimumCullPaddingMeters, extentPadding) + std::max(edgeFadeMeters, 0.0f);
}

void expandBounds(Aabb& bounds, const float paddingMeters) noexcept
{
    for (int axis = 0; axis < 3; ++axis)
    {
        bounds.min[axis] -= paddingMeters;
        bounds.max[axis] += paddingMeters;
    }
}
} // namespace

float clampDecalOpacity(const float opacity) noexcept
{
    if (!std::isfinite(opacity))
    {
        return 1.0f;
    }

    return std::min(std::max(opacity, 0.0f), 1.0f);
}

std::uint32_t clampDecalCount(const std::uint32_t decalCount) noexcept
{
    return std::min(decalCount, kMaxFrameDecals);
}

float clampDecalProjectionDepthMeters(const float depthMeters) noexcept
{
    if (!std::isfinite(depthMeters) || depthMeters <= 0.0f)
    {
        return 0.0f;
    }

    return std::min(depthMeters, kMaximumProjectionDepthMeters);
}

float clampDecalProjectionEdgeFadeMeters(const float fadeMeters) noexcept
{
    if (!std::isfinite(fadeMeters) || fadeMeters <= 0.0f)
    {
        return 0.0f;
    }

    return std::min(fadeMeters, kMaximumProjectionEdgeFadeMeters);
}

bool isValidDecalDesc(const DecalDesc& desc) noexcept
{
    if (!isFinite16(desc.transform) ||
        !isFinite3(desc.halfExtentsMeters) ||
        !isUnitRangeColor4(desc.tintColorLinear) ||
        !std::isfinite(desc.opacity))
    {
        return false;
    }

    for (int axis = 0; axis < 3; ++axis)
    {
        if (desc.halfExtentsMeters[axis] < kMinimumExtentMeters)
        {
            return false;
        }
    }

    return std::abs(determinant3x3ColumnMajor(desc.transform)) > kMinimumDeterminant;
}

bool isValidDecalSubmitDesc(const DecalSubmitDesc& desc) noexcept
{
    if (!desc.enabled)
    {
        return true;
    }

    if (desc.decalCount > 0 && desc.decals == nullptr)
    {
        return false;
    }

    return true;
}

Aabb buildDecalBounds(const DecalDesc& desc) noexcept
{
    Aabb bounds;
    const float center[3] = {
        desc.transform[12],
        desc.transform[13],
        desc.transform[14]};
    const float radius[3] = {
        std::abs(desc.transform[0]) * desc.halfExtentsMeters[0] +
            std::abs(desc.transform[4]) * desc.halfExtentsMeters[1] +
            std::abs(desc.transform[8]) * desc.halfExtentsMeters[2],
        std::abs(desc.transform[1]) * desc.halfExtentsMeters[0] +
            std::abs(desc.transform[5]) * desc.halfExtentsMeters[1] +
            std::abs(desc.transform[9]) * desc.halfExtentsMeters[2],
        std::abs(desc.transform[2]) * desc.halfExtentsMeters[0] +
            std::abs(desc.transform[6]) * desc.halfExtentsMeters[1] +
            std::abs(desc.transform[10]) * desc.halfExtentsMeters[2]};

    for (int axis = 0; axis < 3; ++axis)
    {
        bounds.min[axis] = center[axis] - radius[axis];
        bounds.max[axis] = center[axis] + radius[axis];
    }

    return bounds;
}

bool buildWorldToDecalMatrix(const DecalDesc& desc, float outWorldToDecal[16]) noexcept
{
    if (!isValidDecalDesc(desc) || outWorldToDecal == nullptr)
    {
        return false;
    }

    const float det = determinant3x3ColumnMajor(desc.transform);
    if (std::abs(det) <= kMinimumDeterminant)
    {
        return false;
    }

    const float invDet = 1.0f / det;
    const float a00 = desc.transform[0];
    const float a01 = desc.transform[4];
    const float a02 = desc.transform[8];
    const float a10 = desc.transform[1];
    const float a11 = desc.transform[5];
    const float a12 = desc.transform[9];
    const float a20 = desc.transform[2];
    const float a21 = desc.transform[6];
    const float a22 = desc.transform[10];

    identity(outWorldToDecal);
    outWorldToDecal[0] = (a11 * a22 - a12 * a21) * invDet;
    outWorldToDecal[4] = (a02 * a21 - a01 * a22) * invDet;
    outWorldToDecal[8] = (a01 * a12 - a02 * a11) * invDet;

    outWorldToDecal[1] = (a12 * a20 - a10 * a22) * invDet;
    outWorldToDecal[5] = (a00 * a22 - a02 * a20) * invDet;
    outWorldToDecal[9] = (a02 * a10 - a00 * a12) * invDet;

    outWorldToDecal[2] = (a10 * a21 - a11 * a20) * invDet;
    outWorldToDecal[6] = (a01 * a20 - a00 * a21) * invDet;
    outWorldToDecal[10] = (a00 * a11 - a01 * a10) * invDet;

    const float tx = desc.transform[12];
    const float ty = desc.transform[13];
    const float tz = desc.transform[14];
    outWorldToDecal[12] = -(outWorldToDecal[0] * tx + outWorldToDecal[4] * ty + outWorldToDecal[8] * tz);
    outWorldToDecal[13] = -(outWorldToDecal[1] * tx + outWorldToDecal[5] * ty + outWorldToDecal[9] * tz);
    outWorldToDecal[14] = -(outWorldToDecal[2] * tx + outWorldToDecal[6] * ty + outWorldToDecal[10] * tz);

    const float inverseExtents[3] = {
        1.0f / desc.halfExtentsMeters[0],
        1.0f / desc.halfExtentsMeters[1],
        1.0f / desc.halfExtentsMeters[2]};
    for (int column = 0; column < 4; ++column)
    {
        outWorldToDecal[column * 4 + 0] *= inverseExtents[0];
        outWorldToDecal[column * 4 + 1] *= inverseExtents[1];
        outWorldToDecal[column * 4 + 2] *= inverseExtents[2];
    }

    return true;
}

DecalRenderPlan buildDecalRenderPlan(const DecalSubmitDesc* desc) noexcept
{
    return buildDecalRenderPlan(desc, nullptr);
}

DecalRenderPlan buildDecalRenderPlan(const DecalSubmitDesc* desc, const Frustum* cameraFrustum) noexcept
{
    DecalRenderPlan plan;
    if (desc == nullptr || !desc->enabled)
    {
        return plan;
    }

    plan.enabled = true;
    plan.activeProjectionSupported = true;
    plan.debugDrawVolumes = desc->debugDrawVolumes;
    plan.frustumCullingEnabled = desc->cullAgainstViewFrustum && cameraFrustum != nullptr;
    plan.maxProjectionDepthMeters = clampDecalProjectionDepthMeters(desc->maxProjectionDepthMeters);
    plan.projectionEdgeFadeMeters = clampDecalProjectionEdgeFadeMeters(desc->projectionEdgeFadeMeters);
    plan.submittedCount = desc->decalCount;
    if (desc->decalCount > 0 && desc->decals == nullptr)
    {
        plan.rejectedCount = desc->decalCount;
        plan.invalidDescriptorRejectCount = desc->decalCount;
        return plan;
    }

    const std::uint32_t count = clampDecalCount(desc->decalCount);
    plan.maxCountRejectedCount = desc->decalCount - count;
    plan.rejectedCount = plan.maxCountRejectedCount;
    for (std::uint32_t sourceIndex = 0; sourceIndex < count; ++sourceIndex)
    {
        DecalDesc itemDesc = desc->decals[sourceIndex];
        if (!isValidDecalDesc(itemDesc))
        {
            ++plan.rejectedCount;
            ++plan.invalidDescriptorRejectCount;
            continue;
        }

        itemDesc.opacity = clampDecalOpacity(itemDesc.opacity);
        DecalRenderItem item;
        item.desc = itemDesc;
        item.bounds = buildDecalBounds(itemDesc);
        item.sourceIndex = sourceIndex;
        item.usesFallbackColor = !isValid(itemDesc.albedoTexture);
        if (!buildWorldToDecalMatrix(itemDesc, item.worldToDecal))
        {
            ++plan.rejectedCount;
            ++plan.invalidDescriptorRejectCount;
            continue;
        }

        if (plan.frustumCullingEnabled)
        {
            Aabb cullingBounds = item.bounds;
            expandBounds(cullingBounds, conservativeCullPaddingMeters(item.desc, plan.projectionEdgeFadeMeters));
            if (!intersects(*cameraFrustum, cullingBounds))
            {
                ++plan.culledCount;
                continue;
            }
        }

        std::uint32_t insertAt = plan.activeCount;
        while (insertAt > 0)
        {
            const DecalRenderItem& previous = plan.items[insertAt - 1U];
            if (previous.desc.sortKey < item.desc.sortKey ||
                (previous.desc.sortKey == item.desc.sortKey && previous.sourceIndex <= item.sourceIndex))
            {
                break;
            }
            plan.items[insertAt] = previous;
            --insertAt;
        }
        plan.items[insertAt] = item;
        ++plan.activeCount;
        if (item.usesFallbackColor)
        {
            ++plan.fallbackColorCount;
        }
    }

    plan.debugVolumeCount = plan.debugDrawVolumes ? plan.activeCount : 0;
    plan.projectionDeferred = plan.enabled && plan.activeCount > 0 && !plan.activeProjectionSupported;
    return plan;
}

DecalSceneTargetPlan makeDecalSceneTargetPlan(
    const std::uint32_t viewportWidth,
    const std::uint32_t viewportHeight,
    const DecalRenderPlan& plan) noexcept
{
    DecalSceneTargetPlan targetPlan;
    if (viewportWidth == 0 || viewportHeight == 0 || !plan.enabled || plan.activeCount == 0)
    {
        return targetPlan;
    }

    targetPlan.width = viewportWidth;
    targetPlan.height = viewportHeight;
    targetPlan.requiresSceneTarget = true;
    return targetPlan;
}
} // namespace full_renderer::scene
