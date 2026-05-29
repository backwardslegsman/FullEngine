#include "renderer/debug/TerrainDebug.hpp"
#include "renderer/debug/CsmDebug.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

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

full_renderer::Aabb makeBounds()
{
    full_renderer::Aabb bounds;
    bounds.min[0] = -1.0f;
    bounds.min[1] = 0.0f;
    bounds.min[2] = -2.0f;
    bounds.max[0] = 1.0f;
    bounds.max[1] = 2.0f;
    bounds.max[2] = 2.0f;
    return bounds;
}

full_renderer::TerrainChunkDebugInfo visibleChunk(
    const std::uint32_t lod,
    const bool hasSplatMap,
    const bool hasTerrainMaterial)
{
    full_renderer::TerrainChunkDebugInfo info;
    info.handle = {1, 1};
    info.bounds = makeBounds();
    info.cullResult = full_renderer::TerrainCullResult::Visible;
    info.selectedLod = lod;
    info.hasSplatMap = hasSplatMap;
    info.hasTerrainMaterial = hasTerrainMaterial;
    return info;
}

void disabledOptionsProduceNoLines(int& failures)
{
    const full_renderer::TerrainChunkDebugInfo chunk = visibleChunk(0, true, true);
    full_renderer::TerrainDebugOptions options;
    std::vector<full_renderer::debug::DebugLineVertex> lines;

    expect(!full_renderer::debug::hasTerrainGpuDebugOverlay(options),
        "disabled options do not request overlay",
        failures);
    expect(full_renderer::debug::buildTerrainDebugLines(options, &chunk, 1, lines) == 0,
        "disabled options produce no debug lines",
        failures);
    expect(lines.empty(), "disabled options clear output lines", failures);
}

void chunkBoundsProduceWireBox(int& failures)
{
    const full_renderer::TerrainChunkDebugInfo chunk = visibleChunk(0, true, true);
    full_renderer::TerrainDebugOptions options;
    options.drawChunkBounds = true;
    std::vector<full_renderer::debug::DebugLineVertex> lines;

    expect(full_renderer::debug::hasTerrainGpuDebugOverlay(options),
        "chunk bounds request overlay",
        failures);
    expect(full_renderer::debug::buildTerrainDebugLines(options, &chunk, 1, lines) == 24,
        "one AABB emits twelve line segments",
        failures);
    expect(lines.size() == 24, "line output contains 24 vertices", failures);
}

void lodOverlaySkipsCulledChunks(int& failures)
{
    full_renderer::TerrainChunkDebugInfo chunks[2] = {
        visibleChunk(2, true, true),
        visibleChunk(full_renderer::kInvalidTerrainLodIndex, true, true),
    };
    chunks[1].cullResult = full_renderer::TerrainCullResult::OutsideFrustum;

    full_renderer::TerrainDebugOptions options;
    options.drawLodOverlay = true;
    std::vector<full_renderer::debug::DebugLineVertex> lines;

    expect(full_renderer::debug::buildTerrainDebugLines(options, chunks, 2, lines) == 24,
        "LOD overlay emits only visible chunk lines",
        failures);
}

void combinedOverlayIncludesCulledAndFallbackState(int& failures)
{
    full_renderer::TerrainChunkDebugInfo chunks[2] = {
        visibleChunk(1, false, true),
        visibleChunk(full_renderer::kInvalidTerrainLodIndex, true, true),
    };
    chunks[1].cullResult = full_renderer::TerrainCullResult::OutsideFrustum;

    full_renderer::TerrainDebugOptions options;
    options.drawCombinedOverlay = true;
    std::vector<full_renderer::debug::DebugLineVertex> lines;

    expect(full_renderer::debug::buildTerrainDebugLines(options, chunks, 2, lines) == 48,
        "combined overlay emits visible and culled chunk lines",
        failures);
    expect(lines[0].colorLinear[0] > 0.9f && lines[0].colorLinear[2] > 0.8f,
        "combined overlay highlights splat fallback in magenta",
        failures);
}

void invalidHandlesDoNotProduceBounds(int& failures)
{
    full_renderer::TerrainChunkDebugInfo chunk = visibleChunk(0, true, true);
    chunk.cullResult = full_renderer::TerrainCullResult::InvalidHandle;

    full_renderer::TerrainDebugOptions options;
    options.drawCombinedOverlay = true;
    std::vector<full_renderer::debug::DebugLineVertex> lines;

    expect(full_renderer::debug::buildTerrainDebugLines(options, &chunk, 1, lines) == 0,
        "invalid handles do not emit boxes without bounds",
        failures);
}

void shadowFrustumCornersProduceTwelveEdges(int& failures)
{
    const float corners[8][3] = {
        {-1.0f, -1.0f, 0.0f},
        {1.0f, -1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f},
        {-2.0f, -2.0f, 4.0f},
        {2.0f, -2.0f, 4.0f},
        {2.0f, 2.0f, 4.0f},
        {-2.0f, 2.0f, 4.0f},
    };
    std::vector<full_renderer::debug::DebugLineVertex> lines;

    expect(full_renderer::debug::appendShadowFrustumDebugLines(corners, lines) == 24,
        "shadow frustum emits twelve line segments",
        failures);
    expect(lines.size() == 24, "shadow frustum line output contains 24 vertices", failures);
    expect(lines[0].position[0] == -1.0f && lines[1].position[0] == 1.0f,
        "first shadow frustum edge follows documented corner ordering",
        failures);
}

void coloredShadowFrustaAppendWithoutClearing(int& failures)
{
    const float corners[8][3] = {
        {-1.0f, -1.0f, 0.0f},
        {1.0f, -1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f},
        {-1.0f, -1.0f, 2.0f},
        {1.0f, -1.0f, 2.0f},
        {1.0f, 1.0f, 2.0f},
        {-1.0f, 1.0f, 2.0f},
    };
    const float firstColor[4] = {0.1f, 0.2f, 0.3f, 1.0f};
    const float secondColor[4] = {0.7f, 0.6f, 0.5f, 1.0f};
    std::vector<full_renderer::debug::DebugLineVertex> lines;

    expect(full_renderer::debug::appendShadowFrustumDebugLines(corners, firstColor, lines) == 24,
        "first colored frustum emits lines",
        failures);
    expect(full_renderer::debug::appendShadowFrustumDebugLines(corners, secondColor, lines) == 24,
        "second colored frustum appends lines",
        failures);
    expect(lines.size() == 48, "two cascade frusta append without clearing", failures);
    expect(lines[0].colorLinear[0] == firstColor[0], "first frustum keeps requested color", failures);
    expect(lines[24].colorLinear[0] == secondColor[0], "second frustum keeps requested color", failures);
}

void invalidShadowFrustumCornersAreSkipped(int& failures)
{
    float corners[8][3] = {
        {-1.0f, -1.0f, 0.0f},
        {1.0f, -1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f},
        {-1.0f, -1.0f, 2.0f},
        {1.0f, -1.0f, 2.0f},
        {1.0f, 1.0f, 2.0f},
        {-1.0f, 1.0f, 2.0f},
    };
    corners[3][1] = std::numeric_limits<float>::infinity();

    std::vector<full_renderer::debug::DebugLineVertex> lines;
    expect(full_renderer::debug::appendShadowFrustumDebugLines(corners, lines) == 0,
        "non-finite shadow frustum corners are skipped",
        failures);
    expect(lines.empty(), "invalid shadow frustum does not emit debug line vertices", failures);
}

void csmValidationDefaultsAreUsefulAndResettable(int& failures)
{
    const full_renderer::debug::CsmValidationSceneConfig config =
        full_renderer::debug::makeDefaultCsmValidationSceneConfig();
    expect(config.terrainGridRadius >= 8,
        "validation scene default contains a large terrain grid",
        failures);
    expect(config.staticCasterCount > 1 && config.instancedCasterCount > 1,
        "validation scene default contains mesh and instanced casters",
        failures);

    full_renderer::DirectionalShadowDesc shadow =
        full_renderer::debug::makeDefaultCsmValidationShadowDesc();
    expect(shadow.enabled,
        "validation CSM defaults enable shadows",
        failures);
    expect(shadow.cascadeCount == full_renderer::kMaxDirectionalShadowCascades,
        "validation CSM defaults use all cascades",
        failures);
    expect(shadow.stableCascadeProjection,
        "validation CSM defaults enable stable projection",
        failures);
    expect(shadow.cascadeBlendEnabled && shadow.cascadeBlendFraction > 0.0f,
        "validation CSM defaults enable blend bands",
        failures);
    expect(shadow.cascadeShadowDistanceMeters > 100.0f,
        "validation CSM defaults cover multiple visible regions",
        failures);

    full_renderer::TerrainDebugOptions terrainDebug;
    full_renderer::debug::applyCsmValidationDebugOverlayPreset(terrainDebug, shadow);
    expect(shadow.debugDrawCascadeFrusta && shadow.debugDrawCascadeCasters,
        "validation overlay preset enables cascade diagnostics",
        failures);
    expect(!terrainDebug.drawChunkBounds && !terrainDebug.drawCombinedOverlay,
        "validation overlay preset leaves noisy terrain overlays disabled",
        failures);

    shadow.cascadeCount = 1;
    shadow.mapResolution = 4096;
    shadow = full_renderer::debug::makeDefaultCsmValidationShadowDesc();
    expect(shadow.cascadeCount == full_renderer::kMaxDirectionalShadowCascades &&
            shadow.mapResolution == 1024,
        "resetting CSM settings restores validation defaults",
        failures);
}

void shadowMapPreviewSettingsClampAndGateAvailability(int& failures)
{
    full_renderer::debug::ShadowMapPreviewSettings settings =
        full_renderer::debug::makeDefaultShadowMapPreviewSettings();
    expect(!settings.enabled,
        "shadow preview is disabled by default",
        failures);
    expect(full_renderer::debug::clampShadowMapPreviewSize(1) == 128,
        "shadow preview size clamps to minimum",
        failures);
    expect(full_renderer::debug::clampShadowMapPreviewSize(256) == 256,
        "valid shadow preview size is preserved",
        failures);
    expect(full_renderer::debug::clampShadowMapPreviewSize(4096) == 512,
        "shadow preview size clamps to maximum",
        failures);

    float blackDepth = std::numeric_limits<float>::quiet_NaN();
    float whiteDepth = -1.0f;
    full_renderer::debug::normalizeShadowMapPreviewRemap(blackDepth, whiteDepth);
    expect(blackDepth >= 0.0f && blackDepth <= 1.0f &&
            whiteDepth > blackDepth && whiteDepth <= 1.0f,
        "shadow preview remap normalizes non-finite and inverted ranges",
        failures);

    settings.enabled = false;
    full_renderer::debug::ShadowMapPreviewRequest request =
        full_renderer::debug::makeShadowMapPreviewRequest(settings, 4, true);
    expect(!request.enabled && !request.available,
        "disabled shadow preview produces no available request",
        failures);

    settings.enabled = true;
    settings.cascadeIndex = 99;
    settings.previewSize = 9999;
    settings.blackDepth = 0.8f;
    settings.whiteDepth = 0.2f;
    settings.invert = true;
    request = full_renderer::debug::makeShadowMapPreviewRequest(settings, 3, true);
    expect(request.enabled && request.available,
        "enabled valid shadow preview request is available",
        failures);
    expect(request.cascadeIndex == 2,
        "shadow preview cascade index clamps to active cascades",
        failures);
    expect(request.previewSize == 512,
        "shadow preview request stores clamped size",
        failures);
    expect(request.whiteDepth > request.blackDepth,
        "shadow preview request stores ordered remap depths",
        failures);
    expect(request.invert,
        "shadow preview request preserves invert flag",
        failures);

    request = full_renderer::debug::makeShadowMapPreviewRequest(settings, 0, true);
    expect(!request.available,
        "zero active cascades make preview unavailable",
        failures);
    request = full_renderer::debug::makeShadowMapPreviewRequest(settings, 3, false);
    expect(!request.available,
        "invalid selected cascade target makes preview unavailable",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    disabledOptionsProduceNoLines(failures);
    chunkBoundsProduceWireBox(failures);
    lodOverlaySkipsCulledChunks(failures);
    combinedOverlayIncludesCulledAndFallbackState(failures);
    invalidHandlesDoNotProduceBounds(failures);
    shadowFrustumCornersProduceTwelveEdges(failures);
    coloredShadowFrustaAppendWithoutClearing(failures);
    invalidShadowFrustumCornersAreSkipped(failures);
    csmValidationDefaultsAreUsefulAndResettable(failures);
    shadowMapPreviewSettingsClampAndGateAvailability(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
