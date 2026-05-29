#include "renderer/scene/CullingDiagnostics.hpp"

#include "renderer/scene/Frustum.hpp"
#include "renderer/scene/Math.hpp"

namespace full_renderer::scene
{
namespace
{
Frustum cameraFrustumFromView(const RenderViewDesc& view) noexcept
{
    float viewProjection[16] = {};
    multiplyColumnMajor4x4(view.projection, view.view, viewProjection);
    return extractFrustumFromViewProjection(viewProjection);
}

bool hasUsableBounds(const Aabb& bounds) noexcept
{
    return isValidAabb(bounds);
}

bool isCameraVisible(const Frustum& frustum, const Aabb& bounds, CullingCategoryStats& stats) noexcept
{
    if (!hasUsableBounds(bounds))
    {
        return true;
    }

    ++stats.approximateBoundsCount;
    return intersects(frustum, bounds);
}

void recordCameraVisibility(
    const Frustum& frustum,
    const Aabb& bounds,
    CullingCategoryStats& stats) noexcept
{
    if (isCameraVisible(frustum, bounds, stats))
    {
        ++stats.visibleCount;
    }
    else
    {
        ++stats.frustumCulledCount;
    }
}

void recordShadowCaster(
    const bool cameraVisible,
    CullingCategoryStats& stats) noexcept
{
    ++stats.shadowCasterCount;
    if (!cameraVisible)
    {
        ++stats.offCameraShadowCasterCount;
    }
}

void accumulateStaticMeshStats(
    const RenderPacket& packet,
    const Frustum& cameraFrustum,
    const DirectionalShadowCascadeSet* cascadeSet,
    CullingCategoryStats& stats) noexcept
{
    stats.submittedCount = packet.drawItemCount;
    stats.residentCount = packet.drawItemCount;
    for (std::uint32_t drawIndex = 0; drawIndex < packet.drawItemCount; ++drawIndex)
    {
        const DrawItem& draw = packet.drawItems[drawIndex];
        if (!isValid(draw.mesh) || !isValid(draw.material))
        {
            ++stats.invalidResourceCount;
            ++stats.rejectedCount;
            continue;
        }

        ++stats.validResourceCount;
        ++stats.drawSubmissionCount;
        const bool cameraVisible = isCameraVisible(cameraFrustum, draw.bounds, stats);
        if (cameraVisible)
        {
            ++stats.visibleCount;
        }
        else
        {
            ++stats.frustumCulledCount;
        }

        if (!draw.castsShadow || cascadeSet == nullptr || !hasUsableBounds(draw.bounds))
        {
            continue;
        }

        for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeSet->cascadeCount; ++cascadeIndex)
        {
            const Frustum lightFrustum =
                extractFrustumFromViewProjection(cascadeSet->splits[cascadeIndex].matrices.viewProjection);
            if (intersects(lightFrustum, draw.bounds))
            {
                recordShadowCaster(cameraVisible, stats);
            }
        }
    }
}

void accumulateInstancedMeshStats(
    const RenderPacket& packet,
    const Frustum& cameraFrustum,
    const DirectionalShadowCascadeSet* cascadeSet,
    CullingCategoryStats& stats) noexcept
{
    stats.submittedCount = packet.instancedDrawCount;
    stats.residentCount = packet.instancedDrawCount;
    for (std::uint32_t batchIndex = 0; batchIndex < packet.instancedDrawCount; ++batchIndex)
    {
        const InstancedDrawDesc& batch = packet.instancedDraws[batchIndex];
        if (!isValid(batch.mesh) || !isValid(batch.material) || batch.modelMatrices == nullptr || batch.instanceCount == 0)
        {
            ++stats.invalidResourceCount;
            ++stats.rejectedCount;
            continue;
        }

        ++stats.validResourceCount;
        ++stats.drawSubmissionCount;
        const bool cameraVisible = isCameraVisible(cameraFrustum, batch.bounds, stats);
        if (cameraVisible)
        {
            ++stats.visibleCount;
        }
        else
        {
            ++stats.frustumCulledCount;
        }

        if (!batch.castsShadow || cascadeSet == nullptr || !hasUsableBounds(batch.bounds))
        {
            continue;
        }

        for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeSet->cascadeCount; ++cascadeIndex)
        {
            const Frustum lightFrustum =
                extractFrustumFromViewProjection(cascadeSet->splits[cascadeIndex].matrices.viewProjection);
            if (intersects(lightFrustum, batch.bounds))
            {
                recordShadowCaster(cameraVisible, stats);
            }
        }
    }
}

void accumulateSkinnedMeshStats(
    const RenderPacket& packet,
    const Frustum& cameraFrustum,
    const DirectionalShadowCascadeSet* cascadeSet,
    CullingCategoryStats& stats) noexcept
{
    stats.submittedCount = packet.animatedDrawCount;
    stats.residentCount = packet.animatedDrawCount;
    for (std::uint32_t drawIndex = 0; drawIndex < packet.animatedDrawCount; ++drawIndex)
    {
        const AnimatedDrawItem& draw = packet.animatedDraws[drawIndex];
        if (!isValid(draw.mesh) ||
            !isValid(draw.material) ||
            draw.palette.skinningMatrices == nullptr ||
            draw.palette.matrixCount == 0)
        {
            ++stats.invalidResourceCount;
            ++stats.rejectedCount;
            continue;
        }

        ++stats.validResourceCount;
        ++stats.drawSubmissionCount;
        const bool cameraVisible = isCameraVisible(cameraFrustum, draw.bounds, stats);
        if (cameraVisible)
        {
            ++stats.visibleCount;
        }
        else
        {
            ++stats.frustumCulledCount;
        }

        if (!draw.castsShadow || cascadeSet == nullptr || !hasUsableBounds(draw.bounds))
        {
            continue;
        }

        for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeSet->cascadeCount; ++cascadeIndex)
        {
            const Frustum lightFrustum =
                extractFrustumFromViewProjection(cascadeSet->splits[cascadeIndex].matrices.viewProjection);
            if (intersects(lightFrustum, draw.bounds))
            {
                recordShadowCaster(cameraVisible, stats);
            }
        }
    }
}
} // namespace

SharedCullingDiagnosticsPlan buildSharedCullingDiagnosticsPlan(const RenderPacket& packet) noexcept
{
    SharedCullingDiagnosticsPlan plan;
    const Frustum cameraFrustum = cameraFrustumFromView(packet.view);

    DirectionalShadowCascadeSet cascadeSet;
    DirectionalShadowCascadeSet* cascadeSetPtr = nullptr;
    if (packet.directionalShadow.enabled &&
        clampShadowCascadeCount(packet.directionalShadow.cascadeCount) == 1U)
    {
        DirectionalShadowSplit split;
        if (buildDirectionalShadowSplit(packet.directionalLight, packet.directionalShadow, split))
        {
            cascadeSet.cascadeCount = 1;
            cascadeSet.splits[0] = split;
            cascadeSet.splits[0].splitIndex = 0;
            plan.shadowCascadeCount = cascadeSet.cascadeCount;
            cascadeSetPtr = &cascadeSet;
        }
    }
    else if (packet.directionalShadow.enabled &&
        buildDirectionalShadowCascadeSet(packet.directionalLight, packet.directionalShadow, packet.view, cascadeSet))
    {
        plan.shadowCascadeCount = cascadeSet.cascadeCount;
        cascadeSetPtr = &cascadeSet;
    }

    if (packet.drawItems != nullptr || packet.drawItemCount == 0)
    {
        accumulateStaticMeshStats(packet, cameraFrustum, cascadeSetPtr, plan.staticMeshes);
    }

    if (packet.instancedDraws != nullptr || packet.instancedDrawCount == 0)
    {
        accumulateInstancedMeshStats(packet, cameraFrustum, cascadeSetPtr, plan.instancedMeshes);
    }

    if (packet.animatedDraws != nullptr || packet.animatedDrawCount == 0)
    {
        accumulateSkinnedMeshStats(packet, cameraFrustum, cascadeSetPtr, plan.skinnedMeshes);
    }

    return plan;
}

void copySharedCullingDiagnosticsToStats(
    const SharedCullingDiagnosticsPlan& plan,
    RendererStats& stats) noexcept
{
    stats.staticMeshCulling = plan.staticMeshes;
    stats.instancedMeshCulling = plan.instancedMeshes;
    stats.skinnedMeshCulling = plan.skinnedMeshes;
}
} // namespace full_renderer::scene
