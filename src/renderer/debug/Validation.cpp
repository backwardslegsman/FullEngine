#include "renderer/debug/Validation.hpp"

#include <array>
#include <cmath>

namespace full_renderer::debug
{
namespace
{
constexpr float kDefaultFovYRadians = 1.0471975512f;
constexpr float kDefaultNearMeters = 0.1f;
constexpr float kDefaultFarMeters = 100.0f;

bool isFinite3(const float values[3]) noexcept
{
    return std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]);
}

float distanceSquared3(const float a[3], const float b[3]) noexcept
{
    const float x = a[0] - b[0];
    const float y = a[1] - b[1];
    const float z = a[2] - b[2];
    return x * x + y * y + z * z;
}

constexpr ValidationCameraBookmark makeBookmark(
    const ValidationCameraBookmarkId id,
    const char* name,
    const float px,
    const float py,
    const float pz,
    const float tx,
    const float ty,
    const float tz) noexcept
{
    return {
        id,
        name,
        {px, py, pz},
        {tx, ty, tz},
        kDefaultFovYRadians,
        kDefaultNearMeters,
        kDefaultFarMeters};
}

constexpr std::array<ValidationCameraBookmark, static_cast<std::uint32_t>(ValidationCameraBookmarkId::Count)>
    kCameraBookmarks = {{
        makeBookmark(ValidationCameraBookmarkId::Overview, "Overview", 0.0f, 5.0f, -12.0f, 0.0f, 0.0f, 18.0f),
        makeBookmark(ValidationCameraBookmarkId::OutlineStatic, "Outline_Static", -12.0f, 4.0f, 8.0f, -7.0f, 0.2f, 14.0f),
        makeBookmark(ValidationCameraBookmarkId::OutlineSkinned, "Outline_Skinned", 1.4f, 2.4f, 2.8f, 3.5f, 0.0f, 8.0f),
        makeBookmark(ValidationCameraBookmarkId::DecalTerrain, "Decal_Terrain", -10.0f, 4.0f, 4.0f, -6.0f, -0.6f, 12.0f),
        makeBookmark(ValidationCameraBookmarkId::DecalMesh, "Decal_Mesh", -1.5f, 3.0f, 13.5f, 2.0f, -0.2f, 20.0f),
        makeBookmark(ValidationCameraBookmarkId::CombinedPostPasses, "Combined_PostPasses", -13.0f, 6.5f, -3.0f, 1.0f, 0.0f, 22.0f),
        makeBookmark(ValidationCameraBookmarkId::ResizeCheck, "Resize_Check", 0.0f, 8.0f, -18.0f, 0.0f, 0.0f, 24.0f),
        makeBookmark(ValidationCameraBookmarkId::TerrainLargeGrid, "Terrain_LargeGrid", -18.0f, 14.0f, -22.0f, 0.0f, 0.0f, 20.0f),
        makeBookmark(ValidationCameraBookmarkId::TerrainOriginShift, "Terrain_OriginShift", 26.0f, 10.0f, -28.0f, 0.0f, 0.0f, 24.0f),
        makeBookmark(ValidationCameraBookmarkId::LargeSceneNear, "LargeScene_Near", -6.0f, 4.5f, -8.0f, 0.0f, 0.0f, 10.0f),
        makeBookmark(ValidationCameraBookmarkId::LargeSceneMid, "LargeScene_Mid", -18.0f, 10.0f, -20.0f, 0.0f, 0.0f, 24.0f),
        makeBookmark(ValidationCameraBookmarkId::LargeSceneFar, "LargeScene_Far", -35.0f, 18.0f, -42.0f, 0.0f, 0.0f, 32.0f),
        makeBookmark(ValidationCameraBookmarkId::LargeSceneCullingEdge, "LargeScene_CullingEdge", 11.5f, 4.5f, 5.0f, 0.0f, 0.0f, 22.0f),
        makeBookmark(ValidationCameraBookmarkId::LargeSceneShadowCasters, "LargeScene_ShadowCasters", -22.0f, 8.5f, 6.0f, 2.0f, 0.0f, 38.0f),
        makeBookmark(ValidationCameraBookmarkId::LargeSceneParticlesDecals, "LargeScene_ParticlesDecals", -10.0f, 5.0f, 7.0f, -4.0f, 0.0f, 16.0f),
        makeBookmark(ValidationCameraBookmarkId::OpenWorldChurn, "OpenWorld_Churn", -32.0f, 16.0f, -36.0f, 0.0f, 0.0f, 24.0f),
        makeBookmark(ValidationCameraBookmarkId::EngineStreamingSeam, "EngineStreamingSeam", -28.0f, 12.0f, -30.0f, 0.0f, 0.0f, 20.0f),
        makeBookmark(ValidationCameraBookmarkId::LargeWorldOriginShift, "LargeWorld_OriginShift", -40.0f, 18.0f, -44.0f, 0.0f, 0.0f, 24.0f),
    }};

constexpr ValidationFeatureState baselineFeatures() noexcept
{
    return {};
}

constexpr ValidationFeatureState outlineFeatures(
    const bool selectStatic,
    const bool selectInstanced,
    const bool selectSkinned) noexcept
{
    ValidationFeatureState state = {};
    state.selectionOutlineEnabled = true;
    state.selectStaticMesh = selectStatic;
    state.selectInstancedBatch = selectInstanced;
    state.selectSkinnedMesh = selectSkinned;
    state.animationDebugBounds = selectSkinned;
    return state;
}

constexpr ValidationFeatureState decalFeatures() noexcept
{
    ValidationFeatureState state = {};
    state.projectedDecalsEnabled = true;
    state.decalDebugVolumes = true;
    state.decalFrustumCullingEnabled = true;
    return state;
}

constexpr ValidationFeatureState combinedFeatures() noexcept
{
    ValidationFeatureState state = {};
    state.ssaoEnabled = true;
    state.projectedDecalsEnabled = true;
    state.decalDebugVolumes = true;
    state.decalFrustumCullingEnabled = true;
    state.particlesEnabled = true;
    state.sampleParticleEmissionEnabled = true;
    state.softParticlesEnabled = true;
    state.colorGradingEnabled = true;
    state.selectionOutlineEnabled = true;
    state.selectStaticMesh = true;
    state.selectInstancedBatch = true;
    state.selectSkinnedMesh = true;
    state.debugChunkBounds = true;
    state.debugCombinedOverlay = true;
    state.animationDebugBounds = true;
    state.weatherEnabled = true;
    state.sampleWeatherPrecipitationEnabled = true;
    state.structureFadeEnabled = true;
    return state;
}

constexpr ValidationFeatureState terrainResidencyFeatures(
    const bool lodOverlay,
    const bool materialOverlay,
    const bool splatFallbackOverlay,
    const bool combinedOverlay) noexcept
{
    ValidationFeatureState state = {};
    state.debugChunkBounds = true;
    state.terrainLodOverlay = lodOverlay;
    state.terrainMaterialOverlay = materialOverlay;
    state.terrainSplatFallbackOverlay = splatFallbackOverlay;
    state.debugCombinedOverlay = combinedOverlay;
    return state;
}

constexpr ValidationFeatureState largeSceneFeatures(
    const bool optionalPasses,
    const bool particles,
    const bool decals,
    const bool selection,
    const bool skinned) noexcept
{
    ValidationFeatureState state = terrainResidencyFeatures(true, false, true, true);
    state.ssaoEnabled = optionalPasses;
    state.projectedDecalsEnabled = decals;
    state.decalDebugVolumes = decals;
    state.decalFrustumCullingEnabled = true;
    state.particlesEnabled = particles;
    state.sampleParticleEmissionEnabled = particles;
    state.softParticlesEnabled = optionalPasses && particles;
    state.colorGradingEnabled = optionalPasses;
    state.selectionOutlineEnabled = selection;
    state.selectStaticMesh = selection;
    state.selectInstancedBatch = selection;
    state.selectSkinnedMesh = selection && skinned;
    state.animationDebugBounds = skinned;
    state.weatherEnabled = optionalPasses && particles;
    state.sampleWeatherPrecipitationEnabled = optionalPasses && particles;
    state.structureFadeEnabled = optionalPasses;
    state.sampleStaticDrawCount = 48;
    state.sampleInstancedInstanceCount = 160;
    state.sampleSkinnedDrawCount = skinned ? 12U : 1U;
    state.sampleDecalCount = decals ? 24U : 4U;
    state.sampleParticleCount = particles ? 192U : 0U;
    return state;
}

constexpr ValidationFeatureState openWorldChurnFeatures(
    const bool heavy,
    const bool materialFallbackChurn,
    const bool decalParticleChurn,
    const bool resizeChurn) noexcept
{
    ValidationFeatureState state = largeSceneFeatures(true, decalParticleChurn, decalParticleChurn, true, true);
    state.openWorldChurnEnabled = true;
    state.openWorldChurnHeavy = heavy;
    state.openWorldMaterialFallbackChurn = materialFallbackChurn;
    state.openWorldDecalParticleChurn = decalParticleChurn;
    state.openWorldResizeChurn = resizeChurn;
    state.openWorldOptionalPassChurn = true;
    state.openWorldChurnFrameCount = heavy ? 10000U : 600U;
    state.openWorldChurnChunkRadius = heavy ? 10U : 5U;
    state.openWorldChurnRate = heavy ? 8U : 3U;
    state.sampleStaticDrawCount = heavy ? 96U : 48U;
    state.sampleInstancedInstanceCount = heavy ? 256U : 160U;
    state.sampleSkinnedDrawCount = heavy ? 16U : 8U;
    state.sampleDecalCount = decalParticleChurn ? (heavy ? 32U : 16U) : 4U;
    state.sampleParticleCount = decalParticleChurn ? (heavy ? 384U : 192U) : 0U;
    state.terrainMaterialOverlay = materialFallbackChurn;
    state.terrainSplatFallbackOverlay = materialFallbackChurn;
    return state;
}

constexpr ValidationFeatureState engineStreamingSeamFeatures(
    const bool materialFallbackChurn,
    const bool longSession,
    const bool resizeChurn,
    const bool heavy) noexcept
{
    ValidationFeatureState state = openWorldChurnFeatures(heavy, materialFallbackChurn, longSession, resizeChurn);
    state.engineStreamingSeamEnabled = true;
    state.openWorldChurnFrameCount = heavy ? 10000U : (longSession ? 1200U : 240U);
    state.openWorldChurnChunkRadius = heavy ? 12U : 6U;
    state.openWorldChurnRate = heavy ? 8U : 4U;
    return state;
}

constexpr ValidationFeatureState largeWorldOriginFeatures(
    const bool allRenderables,
    const bool csm,
    const bool heavyChurn) noexcept
{
    ValidationFeatureState state = engineStreamingSeamFeatures(true, allRenderables || heavyChurn, csm, heavyChurn);
    state.largeWorldOriginShiftEnabled = true;
    state.largeWorldOriginMeters[0] = 1000000.0f;
    state.largeWorldOriginMeters[1] = 0.0f;
    state.largeWorldOriginMeters[2] = -1000000.0f;
    state.projectedDecalsEnabled = allRenderables;
    state.particlesEnabled = allRenderables;
    state.sampleParticleEmissionEnabled = allRenderables;
    state.colorGradingEnabled = allRenderables;
    state.selectionOutlineEnabled = allRenderables;
    state.selectStaticMesh = allRenderables;
    state.selectInstancedBatch = allRenderables;
    state.selectSkinnedMesh = allRenderables;
    state.debugCombinedOverlay = true;
    state.sampleStaticDrawCount = allRenderables ? 64U : 16U;
    state.sampleInstancedInstanceCount = allRenderables ? 192U : 64U;
    state.sampleSkinnedDrawCount = allRenderables ? 10U : 2U;
    state.sampleDecalCount = allRenderables ? 20U : 4U;
    state.sampleParticleCount = allRenderables ? 192U : 0U;
    return state;
}

constexpr ValidationPreset makePreset(
    const ValidationPresetId id,
    const char* name,
    const ValidationCameraBookmarkId bookmark,
    const ValidationFeatureState features) noexcept
{
    return {id, name, bookmark, features};
}

constexpr std::array<ValidationPreset, static_cast<std::uint32_t>(ValidationPresetId::Count)> kPresets = {{
    makePreset(
        ValidationPresetId::BaselineDirect,
        "BaselineDirect",
        ValidationCameraBookmarkId::Overview,
        baselineFeatures()),
    makePreset(
        ValidationPresetId::OutlineStatic,
        "Outline_Static",
        ValidationCameraBookmarkId::OutlineStatic,
        outlineFeatures(true, false, false)),
    makePreset(
        ValidationPresetId::OutlineSkinned,
        "Outline_Skinned",
        ValidationCameraBookmarkId::OutlineSkinned,
        outlineFeatures(false, false, true)),
    makePreset(
        ValidationPresetId::DecalTerrain,
        "Decal_Terrain",
        ValidationCameraBookmarkId::DecalTerrain,
        decalFeatures()),
    makePreset(
        ValidationPresetId::DecalMesh,
        "Decal_Mesh",
        ValidationCameraBookmarkId::DecalMesh,
        decalFeatures()),
    makePreset(
        ValidationPresetId::CombinedPostPasses,
        "Combined_PostPasses",
        ValidationCameraBookmarkId::CombinedPostPasses,
        combinedFeatures()),
    makePreset(
        ValidationPresetId::ResizeCheck,
        "Resize_Check",
        ValidationCameraBookmarkId::ResizeCheck,
        combinedFeatures()),
    makePreset(
        ValidationPresetId::AllFeaturesSmoke,
        "All_Features_Smoke",
        ValidationCameraBookmarkId::CombinedPostPasses,
        combinedFeatures()),
    makePreset(
        ValidationPresetId::ResourceResizePostTargets,
        "Resource_Resize_PostTargets",
        ValidationCameraBookmarkId::ResizeCheck,
        combinedFeatures()),
    makePreset(
        ValidationPresetId::ResourceAllOptionalPassesToggle,
        "Resource_AllOptionalPasses_Toggle",
        ValidationCameraBookmarkId::CombinedPostPasses,
        combinedFeatures()),
    makePreset(
        ValidationPresetId::TerrainLargeGridCulling,
        "Terrain_LargeGrid_Culling",
        ValidationCameraBookmarkId::TerrainLargeGrid,
        terrainResidencyFeatures(true, false, false, false)),
    makePreset(
        ValidationPresetId::TerrainMaterialFallbacks,
        "Terrain_MaterialFallbacks",
        ValidationCameraBookmarkId::TerrainLargeGrid,
        terrainResidencyFeatures(false, true, true, true)),
    makePreset(
        ValidationPresetId::TerrainLodStress,
        "Terrain_LOD_Stress",
        ValidationCameraBookmarkId::TerrainLargeGrid,
        terrainResidencyFeatures(true, false, false, true)),
    makePreset(
        ValidationPresetId::TerrainOriginShiftCheck,
        "Terrain_OriginShift_Check",
        ValidationCameraBookmarkId::TerrainOriginShift,
        terrainResidencyFeatures(true, false, false, true)),
    makePreset(
        ValidationPresetId::TerrainAllResidencySmoke,
        "Terrain_AllResidency_Smoke",
        ValidationCameraBookmarkId::TerrainLargeGrid,
        terrainResidencyFeatures(true, true, true, true)),
    makePreset(
        ValidationPresetId::LargeSceneTerrainGrid,
        "LargeScene_TerrainGrid",
        ValidationCameraBookmarkId::LargeSceneMid,
        terrainResidencyFeatures(true, false, true, true)),
    makePreset(
        ValidationPresetId::LargeSceneStaticMeshes,
        "LargeScene_StaticMeshes",
        ValidationCameraBookmarkId::LargeSceneNear,
        largeSceneFeatures(false, false, false, true, false)),
    makePreset(
        ValidationPresetId::LargeSceneInstancing,
        "LargeScene_Instancing",
        ValidationCameraBookmarkId::LargeSceneMid,
        largeSceneFeatures(false, false, false, true, false)),
    makePreset(
        ValidationPresetId::LargeSceneSkinnedActors,
        "LargeScene_SkinnedActors",
        ValidationCameraBookmarkId::OutlineSkinned,
        largeSceneFeatures(false, false, false, true, true)),
    makePreset(
        ValidationPresetId::LargeSceneDecals,
        "LargeScene_Decals",
        ValidationCameraBookmarkId::LargeSceneParticlesDecals,
        largeSceneFeatures(false, false, true, false, false)),
    makePreset(
        ValidationPresetId::LargeSceneParticles,
        "LargeScene_Particles",
        ValidationCameraBookmarkId::LargeSceneParticlesDecals,
        largeSceneFeatures(false, true, false, false, false)),
    makePreset(
        ValidationPresetId::LargeSceneShadowCasters,
        "LargeScene_ShadowCasters",
        ValidationCameraBookmarkId::LargeSceneShadowCasters,
        largeSceneFeatures(false, false, false, true, true)),
    makePreset(
        ValidationPresetId::LargeSceneAllRenderableTypes,
        "LargeScene_AllRenderableTypes",
        ValidationCameraBookmarkId::LargeSceneMid,
        largeSceneFeatures(true, true, true, true, true)),
    makePreset(
        ValidationPresetId::LargeSceneOptionalPassesDisabled,
        "LargeScene_OptionalPassesDisabled",
        ValidationCameraBookmarkId::LargeSceneMid,
        largeSceneFeatures(false, false, false, false, false)),
    makePreset(
        ValidationPresetId::LargeSceneOriginShiftOrLargeCoordinates,
        "LargeScene_OriginShiftOrLargeCoordinates",
        ValidationCameraBookmarkId::TerrainOriginShift,
        terrainResidencyFeatures(true, false, false, true)),
    makePreset(
        ValidationPresetId::LargeSceneResizeAndReconfigure,
        "LargeScene_ResizeAndReconfigure",
        ValidationCameraBookmarkId::ResizeCheck,
        largeSceneFeatures(true, true, true, true, true)),
    makePreset(
        ValidationPresetId::OpenWorldLongSessionChurn,
        "OpenWorld_LongSession_Churn",
        ValidationCameraBookmarkId::OpenWorldChurn,
        openWorldChurnFeatures(false, true, true, false)),
    makePreset(
        ValidationPresetId::OpenWorldLongSessionChurnHeavy,
        "OpenWorld_LongSession_Churn_Heavy",
        ValidationCameraBookmarkId::OpenWorldChurn,
        openWorldChurnFeatures(true, true, true, true)),
    makePreset(
        ValidationPresetId::OpenWorldResidencyRing,
        "OpenWorld_Residency_Ring",
        ValidationCameraBookmarkId::OpenWorldChurn,
        openWorldChurnFeatures(false, false, false, false)),
    makePreset(
        ValidationPresetId::OpenWorldMaterialFallbackChurn,
        "OpenWorld_MaterialFallback_Churn",
        ValidationCameraBookmarkId::TerrainLargeGrid,
        openWorldChurnFeatures(false, true, false, false)),
    makePreset(
        ValidationPresetId::OpenWorldPostResizeChurn,
        "OpenWorld_PostResize_Churn",
        ValidationCameraBookmarkId::ResizeCheck,
        openWorldChurnFeatures(false, true, true, true)),
    makePreset(
        ValidationPresetId::EngineStreamingSeamBasic,
        "EngineStreamingSeam_Basic",
        ValidationCameraBookmarkId::EngineStreamingSeam,
        engineStreamingSeamFeatures(false, false, false, false)),
    makePreset(
        ValidationPresetId::EngineStreamingSeamMaterialFallbacks,
        "EngineStreamingSeam_MaterialFallbacks",
        ValidationCameraBookmarkId::EngineStreamingSeam,
        engineStreamingSeamFeatures(true, false, false, false)),
    makePreset(
        ValidationPresetId::EngineStreamingSeamLongSession,
        "EngineStreamingSeam_LongSession",
        ValidationCameraBookmarkId::EngineStreamingSeam,
        engineStreamingSeamFeatures(true, true, false, false)),
    makePreset(
        ValidationPresetId::LargeWorldOriginShiftTerrain,
        "LargeWorld_OriginShift_Terrain",
        ValidationCameraBookmarkId::LargeWorldOriginShift,
        largeWorldOriginFeatures(false, false, false)),
    makePreset(
        ValidationPresetId::LargeWorldOriginShiftAllRenderables,
        "LargeWorld_OriginShift_AllRenderables",
        ValidationCameraBookmarkId::LargeWorldOriginShift,
        largeWorldOriginFeatures(true, false, false)),
    makePreset(
        ValidationPresetId::LargeWorldCsmOriginShift,
        "LargeWorld_CSM_OriginShift",
        ValidationCameraBookmarkId::LargeWorldOriginShift,
        largeWorldOriginFeatures(false, true, false)),
    makePreset(
        ValidationPresetId::LargeWorldChurnHeavyManual,
        "LargeWorld_Churn_Heavy_Manual",
        ValidationCameraBookmarkId::LargeWorldOriginShift,
        largeWorldOriginFeatures(true, true, true)),
}};
} // namespace

std::uint32_t validationCameraBookmarkCount() noexcept
{
    return static_cast<std::uint32_t>(kCameraBookmarks.size());
}

std::uint32_t validationPresetCount() noexcept
{
    return static_cast<std::uint32_t>(kPresets.size());
}

const ValidationCameraBookmark* validationCameraBookmarkByIndex(const std::uint32_t index) noexcept
{
    return index < kCameraBookmarks.size() ? &kCameraBookmarks[index] : nullptr;
}

const ValidationCameraBookmark* validationCameraBookmark(const ValidationCameraBookmarkId id) noexcept
{
    return validationCameraBookmarkByIndex(static_cast<std::uint32_t>(id));
}

const ValidationPreset* validationPresetByIndex(const std::uint32_t index) noexcept
{
    return index < kPresets.size() ? &kPresets[index] : nullptr;
}

const ValidationPreset* validationPreset(const ValidationPresetId id) noexcept
{
    return validationPresetByIndex(static_cast<std::uint32_t>(id));
}

const char* validationCameraBookmarkName(const ValidationCameraBookmarkId id) noexcept
{
    const ValidationCameraBookmark* bookmark = validationCameraBookmark(id);
    return bookmark != nullptr ? bookmark->name : "Unknown";
}

const char* validationPresetName(const ValidationPresetId id) noexcept
{
    const ValidationPreset* preset = validationPreset(id);
    return preset != nullptr ? preset->name : "Unknown";
}

bool isValidValidationCameraBookmark(const ValidationCameraBookmark& bookmark) noexcept
{
    return bookmark.name != nullptr &&
        bookmark.name[0] != '\0' &&
        isFinite3(bookmark.positionWorld) &&
        isFinite3(bookmark.targetWorld) &&
        distanceSquared3(bookmark.positionWorld, bookmark.targetWorld) > 0.000001f &&
        std::isfinite(bookmark.fovYRadians) &&
        bookmark.fovYRadians > 0.01f &&
        bookmark.fovYRadians < 3.13f &&
        std::isfinite(bookmark.nearMeters) &&
        std::isfinite(bookmark.farMeters) &&
        bookmark.nearMeters > 0.0f &&
        bookmark.farMeters > bookmark.nearMeters;
}

bool isValidValidationPreset(const ValidationPreset& preset) noexcept
{
    return preset.name != nullptr &&
        preset.name[0] != '\0' &&
        validationCameraBookmark(preset.cameraBookmark) != nullptr &&
        static_cast<std::uint32_t>(preset.id) < validationPresetCount();
}
} // namespace full_renderer::debug
