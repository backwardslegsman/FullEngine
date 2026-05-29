#pragma once

#include <cstdint>

namespace full_renderer::debug
{
enum class ValidationCameraBookmarkId : std::uint32_t
{
    Overview,
    OutlineStatic,
    OutlineSkinned,
    DecalTerrain,
    DecalMesh,
    CombinedPostPasses,
    ResizeCheck,
    TerrainLargeGrid,
    TerrainOriginShift,
    LargeSceneNear,
    LargeSceneMid,
    LargeSceneFar,
    LargeSceneCullingEdge,
    LargeSceneShadowCasters,
    LargeSceneParticlesDecals,
    OpenWorldChurn,
    EngineStreamingSeam,
    LargeWorldOriginShift,
    Count
};

enum class ValidationPresetId : std::uint32_t
{
    BaselineDirect,
    OutlineStatic,
    OutlineSkinned,
    DecalTerrain,
    DecalMesh,
    CombinedPostPasses,
    ResizeCheck,
    AllFeaturesSmoke,
    ResourceResizePostTargets,
    ResourceAllOptionalPassesToggle,
    TerrainLargeGridCulling,
    TerrainMaterialFallbacks,
    TerrainLodStress,
    TerrainOriginShiftCheck,
    TerrainAllResidencySmoke,
    LargeSceneTerrainGrid,
    LargeSceneStaticMeshes,
    LargeSceneInstancing,
    LargeSceneSkinnedActors,
    LargeSceneDecals,
    LargeSceneParticles,
    LargeSceneShadowCasters,
    LargeSceneAllRenderableTypes,
    LargeSceneOptionalPassesDisabled,
    LargeSceneOriginShiftOrLargeCoordinates,
    LargeSceneResizeAndReconfigure,
    OpenWorldLongSessionChurn,
    OpenWorldLongSessionChurnHeavy,
    OpenWorldResidencyRing,
    OpenWorldMaterialFallbackChurn,
    OpenWorldPostResizeChurn,
    EngineStreamingSeamBasic,
    EngineStreamingSeamMaterialFallbacks,
    EngineStreamingSeamLongSession,
    LargeWorldOriginShiftTerrain,
    LargeWorldOriginShiftAllRenderables,
    LargeWorldCsmOriginShift,
    LargeWorldChurnHeavyManual,
    Count
};

struct ValidationCameraBookmark
{
    ValidationCameraBookmarkId id = ValidationCameraBookmarkId::Overview;
    const char* name = "";
    float positionWorld[3] = {};
    float targetWorld[3] = {};
    float fovYRadians = 1.0471975512f;
    float nearMeters = 0.1f;
    float farMeters = 100.0f;
};

struct ValidationFeatureState
{
    bool ssaoEnabled = false;
    bool projectedDecalsEnabled = false;
    bool decalDebugVolumes = false;
    bool decalFrustumCullingEnabled = true;
    bool particlesEnabled = false;
    bool sampleParticleEmissionEnabled = false;
    bool softParticlesEnabled = false;
    bool colorGradingEnabled = false;
    bool selectionOutlineEnabled = false;
    bool selectStaticMesh = false;
    bool selectInstancedBatch = false;
    bool selectSkinnedMesh = false;
    bool debugChunkBounds = false;
    bool terrainLodOverlay = false;
    bool terrainMaterialOverlay = false;
    bool terrainSplatFallbackOverlay = false;
    bool debugCombinedOverlay = false;
    bool animationDebugBounds = false;
    bool animationDebugSkeletons = false;
    bool weatherEnabled = false;
    bool sampleWeatherPrecipitationEnabled = false;
    bool structureFadeEnabled = false;
    std::uint32_t sampleStaticDrawCount = 4;
    std::uint32_t sampleInstancedInstanceCount = 8;
    std::uint32_t sampleSkinnedDrawCount = 1;
    std::uint32_t sampleDecalCount = 4;
    std::uint32_t sampleParticleCount = 96;
    bool openWorldChurnEnabled = false;
    bool openWorldChurnHeavy = false;
    bool openWorldMaterialFallbackChurn = false;
    bool openWorldDecalParticleChurn = false;
    bool openWorldResizeChurn = false;
    bool openWorldOptionalPassChurn = false;
    bool engineStreamingSeamEnabled = false;
    bool largeWorldOriginShiftEnabled = false;
    float largeWorldOriginMeters[3] = {};
    std::uint32_t openWorldChurnFrameCount = 0;
    std::uint32_t openWorldChurnChunkRadius = 0;
    std::uint32_t openWorldChurnRate = 0;
};

struct ValidationPreset
{
    ValidationPresetId id = ValidationPresetId::BaselineDirect;
    const char* name = "";
    ValidationCameraBookmarkId cameraBookmark = ValidationCameraBookmarkId::Overview;
    ValidationFeatureState features;
};

std::uint32_t validationCameraBookmarkCount() noexcept;
std::uint32_t validationPresetCount() noexcept;

const ValidationCameraBookmark* validationCameraBookmarkByIndex(std::uint32_t index) noexcept;
const ValidationCameraBookmark* validationCameraBookmark(ValidationCameraBookmarkId id) noexcept;
const ValidationPreset* validationPresetByIndex(std::uint32_t index) noexcept;
const ValidationPreset* validationPreset(ValidationPresetId id) noexcept;

const char* validationCameraBookmarkName(ValidationCameraBookmarkId id) noexcept;
const char* validationPresetName(ValidationPresetId id) noexcept;

bool isValidValidationCameraBookmark(const ValidationCameraBookmark& bookmark) noexcept;
bool isValidValidationPreset(const ValidationPreset& preset) noexcept;
} // namespace full_renderer::debug
