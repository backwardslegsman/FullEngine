#include "renderer/scene/Decal.hpp"
#include "renderer/scene/Frustum.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void expect(const bool condition, const char* message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

void makeIdentity(float out[16]) noexcept
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

full_renderer::DecalDesc makeDecal(
    const float x,
    const float y,
    const float z,
    const float extent,
    const std::uint32_t sortKey) noexcept
{
    full_renderer::DecalDesc desc;
    makeIdentity(desc.transform);
    desc.transform[12] = x;
    desc.transform[13] = y;
    desc.transform[14] = z;
    desc.halfExtentsMeters[0] = extent;
    desc.halfExtentsMeters[1] = extent;
    desc.halfExtentsMeters[2] = extent;
    desc.sortKey = sortKey;
    return desc;
}

void transformPoint(const float matrix[16], const float point[3], float out[3]) noexcept
{
    out[0] = matrix[0] * point[0] + matrix[4] * point[1] + matrix[8] * point[2] + matrix[12];
    out[1] = matrix[1] * point[0] + matrix[5] * point[1] + matrix[9] * point[2] + matrix[13];
    out[2] = matrix[2] * point[0] + matrix[6] * point[1] + matrix[10] * point[2] + matrix[14];
}

bool near(const float lhs, const float rhs, const float tolerance = 0.0001f) noexcept
{
    return std::abs(lhs - rhs) <= tolerance;
}

void defaultDisabledPlanIsNeutral(int& failures)
{
    const full_renderer::DecalSubmitDesc submit;
    expect(full_renderer::scene::isValidDecalSubmitDesc(submit), "disabled decal submit validates", failures);

    const full_renderer::scene::DecalRenderPlan plan = full_renderer::scene::buildDecalRenderPlan(&submit);
    expect(!plan.enabled, "disabled decal submit produces disabled plan", failures);
    expect(plan.activeCount == 0, "disabled decal submit has no active decals", failures);
    expect(!plan.projectionDeferred, "disabled decal submit does not report deferred projection", failures);

    const full_renderer::scene::DecalRenderPlan nullPlan = full_renderer::scene::buildDecalRenderPlan(nullptr);
    expect(!nullPlan.enabled, "null decal submit produces disabled plan", failures);
}

void descriptorValidationRejectsBadGeometry(int& failures)
{
    full_renderer::DecalDesc desc = makeDecal(0.0f, 0.0f, 0.0f, 1.0f, 0);
    expect(full_renderer::scene::isValidDecalDesc(desc), "valid decal descriptor validates", failures);

    desc.transform[0] = std::numeric_limits<float>::quiet_NaN();
    expect(!full_renderer::scene::isValidDecalDesc(desc), "non-finite transform is rejected", failures);

    desc = makeDecal(0.0f, 0.0f, 0.0f, 1.0f, 0);
    desc.transform[0] = 0.0f;
    desc.transform[5] = 0.0f;
    desc.transform[10] = 0.0f;
    expect(!full_renderer::scene::isValidDecalDesc(desc), "singular transform is rejected", failures);

    desc = makeDecal(0.0f, 0.0f, 0.0f, 0.0f, 0);
    expect(!full_renderer::scene::isValidDecalDesc(desc), "zero extent is rejected", failures);

    desc = makeDecal(0.0f, 0.0f, 0.0f, 1.0f, 0);
    desc.tintColorLinear[2] = 2.0f;
    expect(!full_renderer::scene::isValidDecalDesc(desc), "out-of-range tint color is rejected", failures);

    desc = makeDecal(0.0f, 0.0f, 0.0f, 1.0f, 0);
    desc.opacity = 8.0f;
    expect(full_renderer::scene::isValidDecalDesc(desc), "finite opacity outside range is accepted for clamping", failures);
    expect(full_renderer::scene::clampDecalOpacity(desc.opacity) == 1.0f, "high opacity clamps to one", failures);
    expect(full_renderer::scene::clampDecalOpacity(-2.0f) == 0.0f, "low opacity clamps to zero", failures);
    expect(
        full_renderer::scene::clampDecalProjectionDepthMeters(-1.0f) == 0.0f,
        "negative decal projection depth clamps to full-volume default",
        failures);
    expect(
        full_renderer::scene::clampDecalProjectionEdgeFadeMeters(std::numeric_limits<float>::infinity()) == 0.0f,
        "non-finite decal edge fade clamps to hard edge",
        failures);
}

void renderPlanBuildsBoundsAndFallbacks(int& failures)
{
    full_renderer::DecalDesc decals[2] = {
        makeDecal(2.0f, 3.0f, 4.0f, 1.5f, 4),
        makeDecal(-1.0f, 0.5f, 2.0f, 0.5f, 2),
    };
    decals[0].opacity = 2.0f;
    decals[1].albedoTexture = {7};

    full_renderer::DecalSubmitDesc submit;
    submit.enabled = true;
    submit.debugDrawVolumes = true;
    submit.decals = decals;
    submit.decalCount = 2;

    const full_renderer::scene::DecalRenderPlan plan = full_renderer::scene::buildDecalRenderPlan(&submit);
    expect(plan.enabled, "enabled decal submit produces enabled plan", failures);
    expect(plan.activeCount == 2, "valid decal submit accepts both decals", failures);
    expect(plan.rejectedCount == 0, "valid decal submit rejects none", failures);
    expect(plan.debugVolumeCount == 2, "debug volume count follows active decal count", failures);
    expect(plan.fallbackColorCount == 1, "zero decal texture is counted as fallback color", failures);
    expect(!plan.frustumCullingEnabled, "plan without camera frustum does not cull decals", failures);
    expect(!plan.projectionDeferred, "planned decals report active projection supported", failures);
    expect(plan.items[0].desc.sortKey == 2 && plan.items[1].desc.sortKey == 4,
        "decal render plan sorts by deterministic sort key",
        failures);
    expect(plan.items[1].desc.opacity == 1.0f, "planned opacity is clamped", failures);
    expect(plan.items[1].bounds.min[0] == 0.5f && plan.items[1].bounds.max[0] == 3.5f,
        "decal bounds include translation and half extent",
        failures);
}

void invalidAndClampedCountsAreReported(int& failures)
{
    full_renderer::DecalSubmitDesc missingPointer;
    missingPointer.enabled = true;
    missingPointer.decalCount = 3;
    expect(!full_renderer::scene::isValidDecalSubmitDesc(missingPointer),
        "enabled decal submit with missing pointer is invalid",
        failures);

    full_renderer::DecalDesc decals[full_renderer::kMaxFrameDecals + 2U] = {};
    for (std::uint32_t index = 0; index < full_renderer::kMaxFrameDecals + 2U; ++index)
    {
        decals[index] = makeDecal(static_cast<float>(index), 0.0f, 0.0f, 1.0f, index);
    }
    full_renderer::DecalSubmitDesc submit;
    submit.enabled = true;
    submit.decals = decals;
    submit.decalCount = full_renderer::kMaxFrameDecals + 2U;

    const full_renderer::scene::DecalRenderPlan plan = full_renderer::scene::buildDecalRenderPlan(&submit);
    expect(plan.submittedCount == full_renderer::kMaxFrameDecals + 2U,
        "decal plan records unclamped submitted count",
        failures);
    expect(plan.activeCount == full_renderer::kMaxFrameDecals,
        "decal plan clamps active decals to maximum",
        failures);
    expect(plan.rejectedCount == 2U,
        "decal plan reports count-clamped decals as rejected",
        failures);
    expect(plan.maxCountRejectedCount == 2U,
        "decal plan tracks count-clamped decals separately",
        failures);

    decals[0].halfExtentsMeters[0] = 0.0f;
    submit.decalCount = 2;
    const full_renderer::scene::DecalRenderPlan invalidPlan =
        full_renderer::scene::buildDecalRenderPlan(&submit);
    expect(invalidPlan.activeCount == 1 && invalidPlan.rejectedCount == 1,
        "invalid decal descriptors are rejected inside the plan",
        failures);
    expect(invalidPlan.invalidDescriptorRejectCount == 1,
        "invalid decal descriptor rejects are tracked separately",
        failures);
}

void cameraFrustumCullingFiltersOnlyOutsideDecals(int& failures)
{
    float identityViewProjection[16] = {};
    makeIdentity(identityViewProjection);
    const full_renderer::scene::Frustum frustum =
        full_renderer::scene::extractFrustumFromViewProjection(identityViewProjection);

    full_renderer::DecalDesc decals[4] = {
        makeDecal(0.0f, 0.0f, 0.0f, 0.25f, 3),
        makeDecal(1.1f, 0.0f, 0.0f, 0.25f, 1),
        makeDecal(4.0f, 0.0f, 0.0f, 0.25f, 2),
        makeDecal(-0.5f, 0.0f, 0.0f, 0.25f, 0),
    };

    full_renderer::DecalSubmitDesc submit;
    submit.enabled = true;
    submit.debugDrawVolumes = true;
    submit.decals = decals;
    submit.decalCount = 4;
    submit.maxProjectionDepthMeters = 0.45f;
    submit.projectionEdgeFadeMeters = 0.2f;

    const full_renderer::scene::DecalRenderPlan plan =
        full_renderer::scene::buildDecalRenderPlan(&submit, &frustum);
    expect(plan.frustumCullingEnabled, "camera frustum culling is active when a frustum is supplied", failures);
    expect(plan.submittedCount == 4, "frustum-culling plan records submitted decal count", failures);
    expect(plan.activeCount == 3, "inside and partially intersecting decals remain active", failures);
    expect(plan.culledCount == 1, "fully outside decal is culled", failures);
    expect(plan.debugVolumeCount == 3, "debug volume count follows post-cull active decals", failures);
    expect(plan.items[0].sourceIndex == 3 && plan.items[1].sourceIndex == 1 && plan.items[2].sourceIndex == 0,
        "post-cull decal order remains deterministic",
        failures);
    expect(plan.maxProjectionDepthMeters == 0.45f, "projection depth is preserved after clamping", failures);
    expect(plan.projectionEdgeFadeMeters == 0.2f, "projection edge fade is preserved after clamping", failures);

    submit.cullAgainstViewFrustum = false;
    const full_renderer::scene::DecalRenderPlan uncullledPlan =
        full_renderer::scene::buildDecalRenderPlan(&submit, &frustum);
    expect(!uncullledPlan.frustumCullingEnabled, "culling can be disabled per submission", failures);
    expect(uncullledPlan.activeCount == 4 && uncullledPlan.culledCount == 0,
        "disabled culling keeps outside but valid decals active",
        failures);
}

void conservativeCullingPaddingKeepsNearEdgeDecalsStable(int& failures)
{
    float identityViewProjection[16] = {};
    makeIdentity(identityViewProjection);
    const full_renderer::scene::Frustum frustum =
        full_renderer::scene::extractFrustumFromViewProjection(identityViewProjection);

    full_renderer::DecalDesc decal = makeDecal(1.26f, 0.0f, 0.0f, 0.25f, 0);
    full_renderer::DecalSubmitDesc submit;
    submit.enabled = true;
    submit.decals = &decal;
    submit.decalCount = 1;
    submit.cullAgainstViewFrustum = true;

    const full_renderer::scene::DecalRenderPlan plan =
        full_renderer::scene::buildDecalRenderPlan(&submit, &frustum);
    expect(plan.activeCount == 1, "near-frustum-edge decals are kept by conservative culling padding", failures);
    expect(plan.culledCount == 0, "near-frustum-edge decals are not reported as culled", failures);

    decal.transform[12] = 2.0f;
    const full_renderer::scene::DecalRenderPlan outsidePlan =
        full_renderer::scene::buildDecalRenderPlan(&submit, &frustum);
    expect(outsidePlan.activeCount == 0 && outsidePlan.culledCount == 1,
        "clearly outside decals are still culled",
        failures);
}

void worldToDecalMatrixIsStableForFixedInputs(int& failures)
{
    full_renderer::DecalDesc decal = makeDecal(2.0f, 3.0f, 4.0f, 1.0f, 0);
    decal.halfExtentsMeters[0] = 2.0f;
    decal.halfExtentsMeters[1] = 4.0f;
    decal.halfExtentsMeters[2] = 5.0f;

    float first[16] = {};
    float second[16] = {};
    expect(full_renderer::scene::buildWorldToDecalMatrix(decal, first),
        "valid decal builds a world-to-decal matrix",
        failures);
    expect(full_renderer::scene::buildWorldToDecalMatrix(decal, second),
        "repeated world-to-decal matrix build succeeds",
        failures);
    for (int index = 0; index < 16; ++index)
    {
        expect(near(first[index], second[index]), "world-to-decal matrix generation is deterministic", failures);
    }

    const float center[3] = {2.0f, 3.0f, 4.0f};
    float local[3] = {};
    transformPoint(first, center, local);
    expect(near(local[0], 0.0f) && near(local[1], 0.0f) && near(local[2], 0.0f),
        "decal center transforms to projector origin",
        failures);

    const float xEdge[3] = {4.0f, 3.0f, 4.0f};
    transformPoint(first, xEdge, local);
    expect(near(local[0], 1.0f) && near(local[1], 0.0f) && near(local[2], 0.0f),
        "world-to-decal matrix normalizes local X extent",
        failures);

    const float yEdge[3] = {2.0f, 7.0f, 4.0f};
    transformPoint(first, yEdge, local);
    expect(near(local[0], 0.0f) && near(local[1], 1.0f) && near(local[2], 0.0f),
        "world-to-decal matrix normalizes local projection depth extent",
        failures);
}

void planningIsDeterministic(int& failures)
{
    full_renderer::DecalDesc decals[3] = {
        makeDecal(0.0f, 0.0f, 0.0f, 1.0f, 3),
        makeDecal(1.0f, 0.0f, 0.0f, 1.0f, 1),
        makeDecal(2.0f, 0.0f, 0.0f, 1.0f, 1),
    };

    full_renderer::DecalSubmitDesc submit;
    submit.enabled = true;
    submit.decals = decals;
    submit.decalCount = 3;

    const full_renderer::scene::DecalRenderPlan first = full_renderer::scene::buildDecalRenderPlan(&submit);
    const full_renderer::scene::DecalRenderPlan second = full_renderer::scene::buildDecalRenderPlan(&submit);
    expect(first.activeCount == second.activeCount, "decal planning active count is deterministic", failures);
    expect(first.items[0].sourceIndex == 1 && first.items[1].sourceIndex == 2 && first.items[2].sourceIndex == 0,
        "decal planning sort order is stable for equal sort keys",
        failures);
    expect(first.items[0].sourceIndex == second.items[0].sourceIndex &&
            first.items[1].sourceIndex == second.items[1].sourceIndex &&
            first.items[2].sourceIndex == second.items[2].sourceIndex,
        "repeated decal planning produces the same order",
        failures);
}

void sceneTargetPlanTracksActiveDecalsAndViewport(int& failures)
{
    const full_renderer::scene::DecalRenderPlan disabledPlan;
    const full_renderer::scene::DecalSceneTargetPlan disabledTarget =
        full_renderer::scene::makeDecalSceneTargetPlan(640, 480, disabledPlan);
    expect(!disabledTarget.requiresSceneTarget, "disabled decal plan requires no scene target", failures);
    expect(disabledTarget.width == 0 && disabledTarget.height == 0,
        "disabled decal scene target dimensions are neutral",
        failures);

    full_renderer::DecalDesc decal = makeDecal(0.0f, 0.0f, 0.0f, 1.0f, 0);
    full_renderer::DecalSubmitDesc submit;
    submit.enabled = true;
    submit.decals = &decal;
    submit.decalCount = 1;
    const full_renderer::scene::DecalRenderPlan plan = full_renderer::scene::buildDecalRenderPlan(&submit);
    const full_renderer::scene::DecalSceneTargetPlan target =
        full_renderer::scene::makeDecalSceneTargetPlan(641, 479, plan);
    expect(target.requiresSceneTarget, "active decals require the backend scene target path", failures);
    expect(target.width == 641 && target.height == 479,
        "decal scene target follows viewport dimensions exactly",
        failures);

    const full_renderer::scene::DecalSceneTargetPlan zeroWidth =
        full_renderer::scene::makeDecalSceneTargetPlan(0, 479, plan);
    expect(!zeroWidth.requiresSceneTarget, "zero-sized viewport disables scene target planning", failures);
}
} // namespace

int main()
{
    int failures = 0;
    defaultDisabledPlanIsNeutral(failures);
    descriptorValidationRejectsBadGeometry(failures);
    renderPlanBuildsBoundsAndFallbacks(failures);
    invalidAndClampedCountsAreReported(failures);
    cameraFrustumCullingFiltersOnlyOutsideDecals(failures);
    conservativeCullingPaddingKeepsNearEdgeDecalsStable(failures);
    worldToDecalMatrixIsStableForFixedInputs(failures);
    planningIsDeterministic(failures);
    sceneTargetPlanTracksActiveDecalsAndViewport(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
