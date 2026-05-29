#pragma once

#include <cstdint>

namespace full_renderer::debug
{
/** @brief Debug/test options for deterministic open-world residency churn simulation. */
struct LongSessionChurnOptions
{
    std::uint32_t seed = 0x5eed1234U;
    std::uint32_t frameCount = 100;
    std::uint32_t terrainSlotCount = 96;
    std::uint32_t maxResidentTerrainChunks = 36;
    std::uint32_t chunkCreatesPerFrame = 3;
    std::uint32_t chunkDestroysPerFrame = 2;
    std::uint32_t materialChurnPeriod = 7;
    std::uint32_t textureChurnPeriod = 5;
    std::uint32_t lodChurnPeriod = 11;
    std::uint32_t decalChurnPeriod = 3;
    std::uint32_t particleChurnPeriod = 2;
    std::uint32_t skinnedPaletteChurnPeriod = 4;
    std::uint32_t optionalPassTogglePeriod = 8;
    std::uint32_t resizePeriod = 31;
    bool materialFallbackChurnEnabled = true;
    bool decalParticleChurnEnabled = true;
    bool skinnedPaletteChurnEnabled = true;
    bool optionalPassToggleChurnEnabled = true;
    bool resizeChurnEnabled = true;
    bool submitStaleHandles = true;
    bool callerRebasedLargeCoordinates = true;
};

/** @brief Per-frame counters produced by the long-session churn simulator. */
struct LongSessionChurnFrame
{
    std::uint32_t frameIndex = 0;
    std::uint32_t residentTerrainChunks = 0;
    std::uint32_t terrainChunksCreated = 0;
    std::uint32_t terrainChunksDestroyed = 0;
    std::uint32_t terrainChunkSlotsReused = 0;
    std::uint32_t staleChunkHandleAttempts = 0;
    std::uint32_t invalidHandleAttempts = 0;
    std::uint32_t materialCreates = 0;
    std::uint32_t materialDestroys = 0;
    std::uint32_t materialFallbacks = 0;
    std::uint32_t textureCreates = 0;
    std::uint32_t textureDestroys = 0;
    std::uint32_t textureFallbacks = 0;
    std::uint32_t lodCreates = 0;
    std::uint32_t lodDestroys = 0;
    std::uint32_t lodFallbacks = 0;
    std::uint32_t decalsSubmitted = 0;
    std::uint32_t decalsRendered = 0;
    std::uint32_t decalsCulled = 0;
    std::uint32_t decalsDestroyed = 0;
    std::uint32_t particleBatchesSubmitted = 0;
    std::uint32_t particleBatchesRendered = 0;
    std::uint32_t particleBatchesCulled = 0;
    std::uint32_t skinnedPaletteSubmissions = 0;
    std::uint32_t skinnedPaletteRejections = 0;
    std::uint32_t optionalPassToggleCount = 0;
    std::uint32_t resizeEventCount = 0;
    std::uint32_t sceneTargetRecreates = 0;
    std::uint32_t shadowTargetRecreates = 0;
    std::uint32_t postTargetRecreates = 0;
    std::uint32_t trackedLiveResources = 0;
    std::uint64_t stagedBytes = 0;
    std::uint64_t estimatedLiveBytes = 0;
    std::uint32_t frameDataLifetimeChecks = 0;
    std::uint32_t frameDataLifetimeFailures = 0;
    bool ssaoEnabled = false;
    bool decalsEnabled = false;
    bool particlesEnabled = false;
    bool colorGradingEnabled = false;
    bool selectionOutlineEnabled = false;
    bool finalPresentValid = true;
};

/** @brief Aggregate counters for a deterministic open-world churn run. */
struct LongSessionChurnSummary
{
    std::uint32_t seed = 0;
    std::uint32_t requestedFrames = 0;
    std::uint32_t simulatedFrames = 0;
    std::uint64_t deterministicHash = 0;
    LongSessionChurnFrame lastFrame;
    std::uint32_t peakResidentTerrainChunks = 0;
    std::uint32_t residentTerrainChunksBeforeReset = 0;
    std::uint32_t finalResidentTerrainChunksAfterReset = 0;
    std::uint32_t totalTerrainChunksCreated = 0;
    std::uint32_t totalTerrainChunksDestroyed = 0;
    std::uint32_t totalTerrainChunkSlotsReused = 0;
    std::uint32_t totalStaleChunkHandleAttempts = 0;
    std::uint32_t totalInvalidHandleAttempts = 0;
    std::uint32_t totalMaterialCreates = 0;
    std::uint32_t totalMaterialDestroys = 0;
    std::uint32_t totalMaterialFallbacks = 0;
    std::uint32_t totalTextureCreates = 0;
    std::uint32_t totalTextureDestroys = 0;
    std::uint32_t totalTextureFallbacks = 0;
    std::uint32_t totalLodCreates = 0;
    std::uint32_t totalLodDestroys = 0;
    std::uint32_t totalLodFallbacks = 0;
    std::uint32_t totalDecalsSubmitted = 0;
    std::uint32_t totalDecalsRendered = 0;
    std::uint32_t totalDecalsCulled = 0;
    std::uint32_t totalDecalsDestroyed = 0;
    std::uint32_t totalParticleBatchesSubmitted = 0;
    std::uint32_t totalParticleBatchesRendered = 0;
    std::uint32_t totalParticleBatchesCulled = 0;
    std::uint32_t totalSkinnedPaletteSubmissions = 0;
    std::uint32_t totalSkinnedPaletteRejections = 0;
    std::uint32_t totalOptionalPassToggles = 0;
    std::uint32_t totalResizeEvents = 0;
    std::uint32_t totalSceneTargetRecreates = 0;
    std::uint32_t totalShadowTargetRecreates = 0;
    std::uint32_t totalPostTargetRecreates = 0;
    std::uint32_t trackedLiveResourcesBeforeReset = 0;
    std::uint32_t finalTrackedLiveResourcesAfterReset = 0;
    std::uint32_t peakTrackedLiveResources = 0;
    std::uint64_t peakEstimatedLiveBytes = 0;
    std::uint64_t totalStagedBytes = 0;
    std::uint32_t finalPresentInvalidFrames = 0;
    std::uint32_t unboundedGrowthWarnings = 0;
    std::uint32_t frameDataLifetimeChecks = 0;
    std::uint32_t frameDataLifetimeFailures = 0;
    bool callerRebasedLargeCoordinates = true;
};

/** @brief Returns default fast validation options for CPU tests and sample diagnostics. */
LongSessionChurnOptions makeDefaultLongSessionChurnOptions() noexcept;

/** @brief Returns opt-in heavy validation options intended for manual stress runs. */
LongSessionChurnOptions makeHeavyLongSessionChurnOptions() noexcept;

/** @brief Validates churn simulation options without touching backend resources. */
bool isValidLongSessionChurnOptions(const LongSessionChurnOptions& options) noexcept;

/** @brief Runs the deterministic CPU-only long-session churn simulation. */
LongSessionChurnSummary runLongSessionChurnSimulation(const LongSessionChurnOptions& options);
} // namespace full_renderer::debug
