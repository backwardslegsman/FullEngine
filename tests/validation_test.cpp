#include "renderer/debug/Validation.hpp"

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

void cameraBookmarksAreValidAndDeterministic(int& failures)
{
    const std::uint32_t count = full_renderer::debug::validationCameraBookmarkCount();
    expect(count == static_cast<std::uint32_t>(full_renderer::debug::ValidationCameraBookmarkId::Count),
        "camera bookmark count matches enum count",
        failures);

    for (std::uint32_t index = 0; index < count; ++index)
    {
        const full_renderer::debug::ValidationCameraBookmark* bookmark =
            full_renderer::debug::validationCameraBookmarkByIndex(index);
        expect(bookmark != nullptr, "camera bookmark lookup by index succeeds", failures);
        if (bookmark == nullptr)
        {
            continue;
        }
        expect(full_renderer::debug::isValidValidationCameraBookmark(*bookmark),
            "camera bookmark descriptor validates",
            failures);
        expect(full_renderer::debug::validationCameraBookmark(bookmark->id) == bookmark,
            "camera bookmark ID lookup is deterministic",
            failures);
        expect(full_renderer::debug::validationCameraBookmarkName(bookmark->id) == bookmark->name,
            "camera bookmark name lookup is deterministic",
            failures);
    }

    expect(full_renderer::debug::validationCameraBookmarkByIndex(count) == nullptr,
        "out-of-range camera bookmark lookup returns null",
        failures);
}

void presetsAreValidAndReferenceBookmarks(int& failures)
{
    const std::uint32_t count = full_renderer::debug::validationPresetCount();
    expect(count == static_cast<std::uint32_t>(full_renderer::debug::ValidationPresetId::Count),
        "validation preset count matches enum count",
        failures);

    bool foundOutlineStatic = false;
    bool foundOutlineSkinned = false;
    bool foundDecalTerrain = false;
    bool foundCombined = false;
    bool foundTerrainLargeGrid = false;
    bool foundTerrainFallbacks = false;
    bool foundTerrainOriginShift = false;
    bool foundLargeSceneAll = false;
    bool foundLargeSceneOptionalDisabled = false;
    bool foundOpenWorldChurn = false;
    bool foundOpenWorldHeavy = false;
    bool foundOpenWorldMaterialFallback = false;
    bool foundOpenWorldPostResize = false;
    bool foundEngineStreamingBasic = false;
    bool foundEngineStreamingLongSession = false;
    bool foundLargeWorldOriginTerrain = false;
    bool foundLargeWorldAllRenderables = false;
    bool foundLargeWorldHeavy = false;
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const full_renderer::debug::ValidationPreset* preset =
            full_renderer::debug::validationPresetByIndex(index);
        expect(preset != nullptr, "validation preset lookup by index succeeds", failures);
        if (preset == nullptr)
        {
            continue;
        }
        expect(full_renderer::debug::isValidValidationPreset(*preset),
            "validation preset descriptor validates",
            failures);
        expect(full_renderer::debug::validationPreset(preset->id) == preset,
            "validation preset ID lookup is deterministic",
            failures);
        expect(full_renderer::debug::validationCameraBookmark(preset->cameraBookmark) != nullptr,
            "validation preset references an existing camera bookmark",
            failures);

        if (preset->id == full_renderer::debug::ValidationPresetId::OutlineStatic)
        {
            foundOutlineStatic = true;
            expect(preset->features.selectionOutlineEnabled, "static outline preset enables outlines", failures);
            expect(preset->features.selectStaticMesh, "static outline preset selects static mesh", failures);
            expect(!preset->features.selectSkinnedMesh, "static outline preset isolates skinned selection", failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::OutlineSkinned)
        {
            foundOutlineSkinned = true;
            expect(preset->features.selectionOutlineEnabled, "skinned outline preset enables outlines", failures);
            expect(preset->features.selectSkinnedMesh, "skinned outline preset selects skinned mesh", failures);
            expect(preset->features.animationDebugBounds, "skinned outline preset enables skinned bounds", failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::DecalTerrain)
        {
            foundDecalTerrain = true;
            expect(preset->features.projectedDecalsEnabled, "decal terrain preset enables decals", failures);
            expect(preset->features.decalDebugVolumes, "decal terrain preset enables decal volumes", failures);
            expect(!preset->features.ssaoEnabled, "decal terrain preset isolates decal behavior from SSAO", failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::CombinedPostPasses)
        {
            foundCombined = true;
            expect(preset->features.ssaoEnabled, "combined preset enables SSAO", failures);
            expect(preset->features.projectedDecalsEnabled, "combined preset enables decals", failures);
            expect(preset->features.particlesEnabled, "combined preset enables particles", failures);
            expect(preset->features.colorGradingEnabled, "combined preset enables color grading", failures);
            expect(preset->features.selectionOutlineEnabled, "combined preset enables selection outlines", failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::TerrainLargeGridCulling)
        {
            foundTerrainLargeGrid = true;
            expect(preset->cameraBookmark == full_renderer::debug::ValidationCameraBookmarkId::TerrainLargeGrid,
                "terrain large-grid preset uses the large-grid camera bookmark",
                failures);
            expect(preset->features.debugChunkBounds, "terrain large-grid preset enables chunk bounds", failures);
            expect(preset->features.terrainLodOverlay, "terrain large-grid preset enables LOD overlay", failures);
            expect(!preset->features.projectedDecalsEnabled,
                "terrain large-grid preset does not require projected decals",
                failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::TerrainMaterialFallbacks)
        {
            foundTerrainFallbacks = true;
            expect(preset->features.terrainMaterialOverlay,
                "terrain fallback preset enables material overlay",
                failures);
            expect(preset->features.terrainSplatFallbackOverlay,
                "terrain fallback preset enables splat fallback overlay",
                failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::TerrainOriginShiftCheck)
        {
            foundTerrainOriginShift = true;
            expect(preset->cameraBookmark == full_renderer::debug::ValidationCameraBookmarkId::TerrainOriginShift,
                "terrain origin-shift preset uses the origin-shift camera bookmark",
                failures);
            expect(preset->features.debugCombinedOverlay,
                "terrain origin-shift preset enables combined terrain diagnostics",
                failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::LargeSceneAllRenderableTypes)
        {
            foundLargeSceneAll = true;
            expect(preset->features.projectedDecalsEnabled,
                "large-scene all-renderables preset enables decals",
                failures);
            expect(preset->features.particlesEnabled,
                "large-scene all-renderables preset enables particles",
                failures);
            expect(preset->features.selectionOutlineEnabled,
                "large-scene all-renderables preset enables outline selection",
                failures);
            expect(preset->features.selectSkinnedMesh,
                "large-scene all-renderables preset selects skinned validation target",
                failures);
            expect(preset->features.sampleStaticDrawCount > 4 &&
                    preset->features.sampleInstancedInstanceCount > 8 &&
                    preset->features.sampleSkinnedDrawCount > 1 &&
                    preset->features.sampleDecalCount > 4 &&
                    preset->features.sampleParticleCount > 96,
                "large-scene all-renderables preset increases sample-owned stress counts",
                failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::LargeSceneOptionalPassesDisabled)
        {
            foundLargeSceneOptionalDisabled = true;
            expect(!preset->features.ssaoEnabled,
                "large-scene optional-disabled preset disables SSAO",
                failures);
            expect(!preset->features.projectedDecalsEnabled,
                "large-scene optional-disabled preset disables decals",
                failures);
            expect(!preset->features.particlesEnabled,
                "large-scene optional-disabled preset disables particles",
                failures);
            expect(preset->features.debugCombinedOverlay,
                "large-scene optional-disabled preset keeps terrain diagnostics available",
                failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::OpenWorldLongSessionChurn)
        {
            foundOpenWorldChurn = true;
            expect(preset->cameraBookmark == full_renderer::debug::ValidationCameraBookmarkId::OpenWorldChurn,
                "open-world churn preset uses the churn camera bookmark",
                failures);
            expect(preset->features.openWorldChurnEnabled,
                "open-world churn preset enables the sample-owned churn harness",
                failures);
            expect(!preset->features.openWorldChurnHeavy,
                "open-world churn preset is the fast validation preset",
                failures);
            expect(preset->features.openWorldChurnFrameCount >= 100,
                "open-world churn preset uses a multi-frame validation run",
                failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::OpenWorldLongSessionChurnHeavy)
        {
            foundOpenWorldHeavy = true;
            expect(preset->features.openWorldChurnEnabled && preset->features.openWorldChurnHeavy,
                "open-world heavy churn preset is opt-in and marked heavy",
                failures);
            expect(preset->features.openWorldChurnFrameCount >= 10000,
                "open-world heavy churn preset uses a long manual run",
                failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::OpenWorldMaterialFallbackChurn)
        {
            foundOpenWorldMaterialFallback = true;
            expect(preset->features.openWorldMaterialFallbackChurn,
                "open-world material fallback churn preset enables material fallback churn",
                failures);
            expect(!preset->features.openWorldDecalParticleChurn,
                "open-world material fallback churn preset isolates decal/particle churn",
                failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::OpenWorldPostResizeChurn)
        {
            foundOpenWorldPostResize = true;
            expect(preset->features.openWorldResizeChurn,
                "open-world post-resize churn preset enables resize churn",
                failures);
            expect(preset->features.projectedDecalsEnabled && preset->features.particlesEnabled,
                "open-world post-resize churn preset keeps post-style systems active",
                failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::EngineStreamingSeamBasic)
        {
            foundEngineStreamingBasic = true;
            expect(preset->features.engineStreamingSeamEnabled,
                "engine streaming seam basic preset enables the seam harness",
                failures);
            expect(!preset->features.openWorldChurnHeavy,
                "engine streaming seam basic preset is not heavy",
                failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::EngineStreamingSeamLongSession)
        {
            foundEngineStreamingLongSession = true;
            expect(preset->features.engineStreamingSeamEnabled &&
                    preset->features.openWorldChurnFrameCount >= 1000,
                "engine streaming seam long-session preset uses a representative frame count",
                failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::LargeWorldOriginShiftTerrain)
        {
            foundLargeWorldOriginTerrain = true;
            expect(preset->features.largeWorldOriginShiftEnabled,
                "large-world terrain preset enables origin-shift diagnostics",
                failures);
            expect(preset->features.largeWorldOriginMeters[0] >= 1000000.0f,
                "large-world terrain preset uses representative coordinates",
                failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::LargeWorldOriginShiftAllRenderables)
        {
            foundLargeWorldAllRenderables = true;
            expect(preset->features.largeWorldOriginShiftEnabled &&
                    preset->features.projectedDecalsEnabled &&
                    preset->features.particlesEnabled &&
                    preset->features.selectionOutlineEnabled,
                "large-world all-renderables preset combines origin diagnostics with renderable families",
                failures);
        }
        else if (preset->id == full_renderer::debug::ValidationPresetId::LargeWorldChurnHeavyManual)
        {
            foundLargeWorldHeavy = true;
            expect(preset->features.openWorldChurnHeavy &&
                    preset->features.largeWorldOriginShiftEnabled,
                "large-world heavy manual preset is marked heavy and origin-shifted",
                failures);
        }
    }

    expect(foundOutlineStatic, "static outline preset exists", failures);
    expect(foundOutlineSkinned, "skinned outline preset exists", failures);
    expect(foundDecalTerrain, "decal terrain preset exists", failures);
    expect(foundCombined, "combined post-pass preset exists", failures);
    expect(foundTerrainLargeGrid, "terrain large-grid preset exists", failures);
    expect(foundTerrainFallbacks, "terrain material fallback preset exists", failures);
    expect(foundTerrainOriginShift, "terrain origin-shift preset exists", failures);
    expect(foundLargeSceneAll, "large-scene all-renderables preset exists", failures);
    expect(foundLargeSceneOptionalDisabled, "large-scene optional-pass-disabled preset exists", failures);
    expect(foundOpenWorldChurn, "open-world long-session churn preset exists", failures);
    expect(foundOpenWorldHeavy, "open-world heavy long-session churn preset exists", failures);
    expect(foundOpenWorldMaterialFallback, "open-world material fallback churn preset exists", failures);
    expect(foundOpenWorldPostResize, "open-world post-resize churn preset exists", failures);
    expect(foundEngineStreamingBasic, "engine streaming seam basic preset exists", failures);
    expect(foundEngineStreamingLongSession, "engine streaming seam long-session preset exists", failures);
    expect(foundLargeWorldOriginTerrain, "large-world origin terrain preset exists", failures);
    expect(foundLargeWorldAllRenderables, "large-world all-renderables preset exists", failures);
    expect(foundLargeWorldHeavy, "large-world heavy manual preset exists", failures);
    expect(full_renderer::debug::validationPresetByIndex(count) == nullptr,
        "out-of-range validation preset lookup returns null",
        failures);
}

void invalidDescriptorsAreRejected(int& failures)
{
    full_renderer::debug::ValidationCameraBookmark bookmark;
    bookmark.name = "Bad";
    bookmark.positionWorld[0] = 0.0f;
    bookmark.positionWorld[1] = 0.0f;
    bookmark.positionWorld[2] = 0.0f;
    bookmark.targetWorld[0] = 0.0f;
    bookmark.targetWorld[1] = 0.0f;
    bookmark.targetWorld[2] = 0.0f;
    bookmark.fovYRadians = 1.0f;
    bookmark.nearMeters = 0.1f;
    bookmark.farMeters = 100.0f;
    expect(!full_renderer::debug::isValidValidationCameraBookmark(bookmark),
        "camera bookmark with identical position and target is invalid",
        failures);

    full_renderer::debug::ValidationPreset preset;
    preset.name = "";
    expect(!full_renderer::debug::isValidValidationPreset(preset),
        "unnamed validation preset is invalid",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    cameraBookmarksAreValidAndDeterministic(failures);
    presetsAreValidAndReferenceBookmarks(failures);
    invalidDescriptorsAreRejected(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
