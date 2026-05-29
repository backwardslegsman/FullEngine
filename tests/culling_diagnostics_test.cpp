#include "renderer/scene/CullingDiagnostics.hpp"

#include <cstdlib>
#include <iostream>

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

void identity(float out[16])
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

full_renderer::Aabb bounds(
    const float minX,
    const float minY,
    const float minZ,
    const float maxX,
    const float maxY,
    const float maxZ)
{
    full_renderer::Aabb result;
    result.min[0] = minX;
    result.min[1] = minY;
    result.min[2] = minZ;
    result.max[0] = maxX;
    result.max[1] = maxY;
    result.max[2] = maxZ;
    return result;
}

full_renderer::RenderViewDesc wideView()
{
    full_renderer::RenderViewDesc view;
    identity(view.view);
    identity(view.projection);
    view.projection[0] = 0.1f;
    view.projection[5] = 0.1f;
    view.projection[10] = 0.1f;
    return view;
}

full_renderer::DirectionalLightDesc light()
{
    full_renderer::DirectionalLightDesc desc;
    desc.directionWorld[0] = 0.0f;
    desc.directionWorld[1] = 1.0f;
    desc.directionWorld[2] = 0.0f;
    desc.colorLinear[0] = 1.0f;
    desc.colorLinear[1] = 1.0f;
    desc.colorLinear[2] = 1.0f;
    desc.intensity = 1.0f;
    return desc;
}

full_renderer::DirectionalShadowDesc singleCascadeShadow()
{
    full_renderer::DirectionalShadowDesc desc;
    desc.enabled = true;
    desc.cascadeCount = 1;
    desc.mapResolution = 1024;
    desc.extentMeters = 40.0f;
    desc.centerWorld[0] = 0.0f;
    desc.centerWorld[1] = 0.0f;
    desc.centerWorld[2] = 0.0f;
    return desc;
}

full_renderer::DrawItem staticDraw(const full_renderer::Aabb& drawBounds)
{
    full_renderer::DrawItem draw;
    draw.mesh.id = 1;
    draw.material.id = 2;
    identity(draw.model);
    draw.bounds = drawBounds;
    draw.castsShadow = true;
    return draw;
}

full_renderer::InstancedDrawDesc instancedDraw(const full_renderer::Aabb& batchBounds, const float* matrices)
{
    full_renderer::InstancedDrawDesc batch;
    batch.mesh.id = 3;
    batch.material.id = 4;
    batch.modelMatrices = matrices;
    batch.instanceCount = 2;
    batch.bounds = batchBounds;
    batch.castsShadow = true;
    return batch;
}

full_renderer::AnimatedDrawItem skinnedDraw(const full_renderer::Aabb& drawBounds, const float* palette)
{
    full_renderer::AnimatedDrawItem draw;
    draw.mesh.id = 5;
    draw.material.id = 6;
    identity(draw.model);
    draw.bounds = drawBounds;
    draw.palette.skinningMatrices = palette;
    draw.palette.matrixCount = 1;
    draw.castsShadow = true;
    return draw;
}

full_renderer::RenderPacket basePacket()
{
    full_renderer::RenderPacket packet;
    packet.view = wideView();
    packet.directionalLight = light();
    return packet;
}

void sharedCullingStatsAggregateRenderableCategories(int& failures)
{
    float matrices[2][16] = {};
    identity(matrices[0]);
    identity(matrices[1]);
    float palette[16] = {};
    identity(palette);

    const full_renderer::DrawItem staticDraws[] = {
        staticDraw(bounds(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f)),
        staticDraw(bounds(30.0f, -1.0f, -1.0f, 32.0f, 1.0f, 1.0f)),
    };
    const full_renderer::InstancedDrawDesc instancedDraws[] = {
        instancedDraw(bounds(-2.0f, -1.0f, -2.0f, 2.0f, 1.0f, 2.0f), &matrices[0][0]),
        instancedDraw(bounds(-32.0f, -1.0f, -1.0f, -30.0f, 1.0f, 1.0f), &matrices[0][0]),
    };
    const full_renderer::AnimatedDrawItem skinnedDraws[] = {
        skinnedDraw(bounds(-0.5f, -1.0f, 3.0f, 0.5f, 1.0f, 4.0f), palette),
        skinnedDraw(bounds(0.0f, -1.0f, 36.0f, 1.0f, 1.0f, 38.0f), palette),
    };

    full_renderer::RenderPacket packet = basePacket();
    packet.directionalShadow = singleCascadeShadow();
    packet.drawItems = staticDraws;
    packet.drawItemCount = 2;
    packet.instancedDraws = instancedDraws;
    packet.instancedDrawCount = 2;
    packet.animatedDraws = skinnedDraws;
    packet.animatedDrawCount = 2;

    const full_renderer::scene::SharedCullingDiagnosticsPlan plan =
        full_renderer::scene::buildSharedCullingDiagnosticsPlan(packet);

    expect(plan.staticMeshes.submittedCount == 2, "static diagnostics count submitted draws", failures);
    expect(plan.staticMeshes.visibleCount == 1, "static diagnostics count camera-visible draws", failures);
    expect(plan.staticMeshes.frustumCulledCount == 1, "static diagnostics count culled draws", failures);
    expect(plan.staticMeshes.approximateBoundsCount == 2, "static diagnostics count usable bounds", failures);
    expect(plan.staticMeshes.shadowCasterCount >= 1, "static diagnostics count shadow casters", failures);
    expect(plan.staticMeshes.offCameraShadowCasterCount >= 1,
        "static diagnostics count off-camera shadow casters",
        failures);

    expect(plan.instancedMeshes.submittedCount == 2, "instanced diagnostics count submitted batches", failures);
    expect(plan.instancedMeshes.visibleCount == 1, "instanced diagnostics count visible batches", failures);
    expect(plan.instancedMeshes.frustumCulledCount == 1, "instanced diagnostics count culled batches", failures);
    expect(plan.instancedMeshes.drawSubmissionCount == 2,
        "instanced diagnostics count backend-facing batches",
        failures);

    expect(plan.skinnedMeshes.submittedCount == 2, "skinned diagnostics count submitted draws", failures);
    expect(plan.skinnedMeshes.visibleCount == 1, "skinned diagnostics count visible draws", failures);
    expect(plan.skinnedMeshes.frustumCulledCount == 1, "skinned diagnostics count culled draws", failures);
    expect(plan.skinnedMeshes.shadowCasterCount >= 1, "skinned diagnostics count shadow casters", failures);
}

void partialBoundsRemainVisibleAndInvalidOptionalBoundsAreNeutral(int& failures)
{
    full_renderer::DrawItem partial = staticDraw(bounds(9.8f, -0.5f, -0.5f, 12.0f, 0.5f, 0.5f));
    full_renderer::DrawItem unbounded = staticDraw(bounds(2.0f, 0.0f, 0.0f, -2.0f, 1.0f, 1.0f));
    unbounded.castsShadow = false;
    const full_renderer::DrawItem draws[] = {partial, unbounded};

    full_renderer::RenderPacket packet = basePacket();
    packet.drawItems = draws;
    packet.drawItemCount = 2;

    const full_renderer::scene::SharedCullingDiagnosticsPlan plan =
        full_renderer::scene::buildSharedCullingDiagnosticsPlan(packet);
    expect(plan.staticMeshes.visibleCount == 2,
        "partially intersecting and unbounded optional-bounds draws remain visible diagnostically",
        failures);
    expect(plan.staticMeshes.frustumCulledCount == 0,
        "partial intersection is not diagnosed as culled",
        failures);
    expect(plan.staticMeshes.approximateBoundsCount == 1,
        "invalid optional bounds are not used for diagnostic culling",
        failures);
}

void invalidResourcesAreRejectedByDiagnostics(int& failures)
{
    full_renderer::DrawItem invalid = staticDraw(bounds(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f));
    invalid.mesh = {};
    const full_renderer::DrawItem draws[] = {invalid};

    float matrices[16] = {};
    identity(matrices);
    full_renderer::InstancedDrawDesc badBatch = instancedDraw(
        bounds(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f),
        matrices);
    badBatch.modelMatrices = nullptr;
    const full_renderer::InstancedDrawDesc batches[] = {badBatch};

    full_renderer::RenderPacket packet = basePacket();
    packet.drawItems = draws;
    packet.drawItemCount = 1;
    packet.instancedDraws = batches;
    packet.instancedDrawCount = 1;

    const full_renderer::scene::SharedCullingDiagnosticsPlan plan =
        full_renderer::scene::buildSharedCullingDiagnosticsPlan(packet);
    expect(plan.staticMeshes.invalidResourceCount == 1, "static invalid handles are diagnosed", failures);
    expect(plan.staticMeshes.rejectedCount == 1, "static invalid handles are rejected in diagnostics", failures);
    expect(plan.instancedMeshes.invalidResourceCount == 1, "instanced invalid buffers are diagnosed", failures);
    expect(plan.instancedMeshes.drawSubmissionCount == 0,
        "invalid instanced batches produce no diagnostic draw submissions",
        failures);
}

void repeatedPlanningIsDeterministic(int& failures)
{
    const full_renderer::DrawItem draws[] = {
        staticDraw(bounds(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f)),
        staticDraw(bounds(20.0f, -1.0f, -1.0f, 21.0f, 1.0f, 1.0f)),
    };

    full_renderer::RenderPacket packet = basePacket();
    packet.directionalShadow = singleCascadeShadow();
    packet.drawItems = draws;
    packet.drawItemCount = 2;

    const full_renderer::scene::SharedCullingDiagnosticsPlan first =
        full_renderer::scene::buildSharedCullingDiagnosticsPlan(packet);
    const full_renderer::scene::SharedCullingDiagnosticsPlan second =
        full_renderer::scene::buildSharedCullingDiagnosticsPlan(packet);
    expect(first.staticMeshes.visibleCount == second.staticMeshes.visibleCount &&
            first.staticMeshes.frustumCulledCount == second.staticMeshes.frustumCulledCount &&
            first.staticMeshes.shadowCasterCount == second.staticMeshes.shadowCasterCount,
        "shared culling diagnostics are deterministic for repeated inputs",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    sharedCullingStatsAggregateRenderableCategories(failures);
    partialBoundsRemainVisibleAndInvalidOptionalBoundsAreNeutral(failures);
    invalidResourcesAreRejectedByDiagnostics(failures);
    repeatedPlanningIsDeterministic(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
