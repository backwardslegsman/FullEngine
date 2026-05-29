#include "renderer/debug/LongSessionChurn.hpp"

#include <algorithm>
#include <vector>

namespace full_renderer::debug
{
namespace
{
struct SimulatedSlot
{
    bool active = false;
    bool everUsed = false;
};

std::uint32_t nextRandom(std::uint32_t& state) noexcept
{
    state = state * 1664525U + 1013904223U;
    return state;
}

void mixHash(std::uint64_t& hash, const std::uint64_t value) noexcept
{
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6U) + (hash >> 2U);
}

std::uint32_t findNthActiveSlot(
    const std::vector<SimulatedSlot>& slots,
    std::uint32_t ordinal) noexcept
{
    for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(slots.size()); ++index)
    {
        if (!slots[index].active)
        {
            continue;
        }

        if (ordinal == 0)
        {
            return index;
        }
        --ordinal;
    }
    return static_cast<std::uint32_t>(slots.size());
}

std::uint32_t findNthInactiveSlot(
    const std::vector<SimulatedSlot>& slots,
    std::uint32_t ordinal) noexcept
{
    for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(slots.size()); ++index)
    {
        if (slots[index].active)
        {
            continue;
        }

        if (ordinal == 0)
        {
            return index;
        }
        --ordinal;
    }
    return static_cast<std::uint32_t>(slots.size());
}

std::uint32_t inactiveSlotCount(
    const std::uint32_t slotCount,
    const std::uint32_t residentCount) noexcept
{
    return residentCount < slotCount ? slotCount - residentCount : 0U;
}

std::uint64_t estimateFrameStagedBytes(const LongSessionChurnFrame& frame) noexcept
{
    return static_cast<std::uint64_t>(frame.residentTerrainChunks) * 64ULL +
        static_cast<std::uint64_t>(frame.decalsSubmitted) * 96ULL +
        static_cast<std::uint64_t>(frame.particleBatchesSubmitted) * 192ULL +
        static_cast<std::uint64_t>(frame.skinnedPaletteSubmissions) * 4096ULL +
        static_cast<std::uint64_t>(frame.terrainChunksCreated) * 256ULL +
        static_cast<std::uint64_t>(frame.terrainChunksDestroyed) * 64ULL +
        static_cast<std::uint64_t>(frame.optionalPassToggleCount) * 32ULL;
}

std::uint64_t estimateLiveBytes(
    const std::uint32_t residentTerrainChunks,
    const std::uint32_t liveMaterials,
    const std::uint32_t liveTextures,
    const std::uint32_t liveLods) noexcept
{
    return static_cast<std::uint64_t>(residentTerrainChunks) * 12ULL * 1024ULL +
        static_cast<std::uint64_t>(liveMaterials) * 256ULL +
        static_cast<std::uint64_t>(liveTextures) * 64ULL * 1024ULL +
        static_cast<std::uint64_t>(liveLods) * 4ULL * 1024ULL;
}

void accumulateFrame(LongSessionChurnSummary& summary, const LongSessionChurnFrame& frame) noexcept
{
    summary.lastFrame = frame;
    summary.peakResidentTerrainChunks = std::max(summary.peakResidentTerrainChunks, frame.residentTerrainChunks);
    summary.totalTerrainChunksCreated += frame.terrainChunksCreated;
    summary.totalTerrainChunksDestroyed += frame.terrainChunksDestroyed;
    summary.totalTerrainChunkSlotsReused += frame.terrainChunkSlotsReused;
    summary.totalStaleChunkHandleAttempts += frame.staleChunkHandleAttempts;
    summary.totalInvalidHandleAttempts += frame.invalidHandleAttempts;
    summary.totalMaterialCreates += frame.materialCreates;
    summary.totalMaterialDestroys += frame.materialDestroys;
    summary.totalMaterialFallbacks += frame.materialFallbacks;
    summary.totalTextureCreates += frame.textureCreates;
    summary.totalTextureDestroys += frame.textureDestroys;
    summary.totalTextureFallbacks += frame.textureFallbacks;
    summary.totalLodCreates += frame.lodCreates;
    summary.totalLodDestroys += frame.lodDestroys;
    summary.totalLodFallbacks += frame.lodFallbacks;
    summary.totalDecalsSubmitted += frame.decalsSubmitted;
    summary.totalDecalsRendered += frame.decalsRendered;
    summary.totalDecalsCulled += frame.decalsCulled;
    summary.totalDecalsDestroyed += frame.decalsDestroyed;
    summary.totalParticleBatchesSubmitted += frame.particleBatchesSubmitted;
    summary.totalParticleBatchesRendered += frame.particleBatchesRendered;
    summary.totalParticleBatchesCulled += frame.particleBatchesCulled;
    summary.totalSkinnedPaletteSubmissions += frame.skinnedPaletteSubmissions;
    summary.totalSkinnedPaletteRejections += frame.skinnedPaletteRejections;
    summary.totalOptionalPassToggles += frame.optionalPassToggleCount;
    summary.totalResizeEvents += frame.resizeEventCount;
    summary.totalSceneTargetRecreates += frame.sceneTargetRecreates;
    summary.totalShadowTargetRecreates += frame.shadowTargetRecreates;
    summary.totalPostTargetRecreates += frame.postTargetRecreates;
    summary.peakTrackedLiveResources = std::max(summary.peakTrackedLiveResources, frame.trackedLiveResources);
    summary.peakEstimatedLiveBytes = std::max(summary.peakEstimatedLiveBytes, frame.estimatedLiveBytes);
    summary.totalStagedBytes += frame.stagedBytes;
    summary.finalPresentInvalidFrames += frame.finalPresentValid ? 0U : 1U;
    summary.frameDataLifetimeChecks += frame.frameDataLifetimeChecks;
    summary.frameDataLifetimeFailures += frame.frameDataLifetimeFailures;

    mixHash(summary.deterministicHash, frame.frameIndex);
    mixHash(summary.deterministicHash, frame.residentTerrainChunks);
    mixHash(summary.deterministicHash, frame.terrainChunksCreated);
    mixHash(summary.deterministicHash, frame.terrainChunksDestroyed);
    mixHash(summary.deterministicHash, frame.terrainChunkSlotsReused);
    mixHash(summary.deterministicHash, frame.staleChunkHandleAttempts);
    mixHash(summary.deterministicHash, frame.invalidHandleAttempts);
    mixHash(summary.deterministicHash, frame.materialFallbacks);
    mixHash(summary.deterministicHash, frame.textureFallbacks);
    mixHash(summary.deterministicHash, frame.lodFallbacks);
    mixHash(summary.deterministicHash, frame.decalsRendered);
    mixHash(summary.deterministicHash, frame.particleBatchesRendered);
    mixHash(summary.deterministicHash, frame.skinnedPaletteRejections);
    mixHash(summary.deterministicHash, frame.optionalPassToggleCount);
    mixHash(summary.deterministicHash, frame.resizeEventCount);
    mixHash(summary.deterministicHash, frame.stagedBytes);
}
} // namespace

LongSessionChurnOptions makeDefaultLongSessionChurnOptions() noexcept
{
    return {};
}

LongSessionChurnOptions makeHeavyLongSessionChurnOptions() noexcept
{
    LongSessionChurnOptions options;
    options.seed = 0x7057cafeU;
    options.frameCount = 10000;
    options.terrainSlotCount = 512;
    options.maxResidentTerrainChunks = 192;
    options.chunkCreatesPerFrame = 8;
    options.chunkDestroysPerFrame = 7;
    options.resizePeriod = 127;
    return options;
}

bool isValidLongSessionChurnOptions(const LongSessionChurnOptions& options) noexcept
{
    return options.frameCount > 0U &&
        options.terrainSlotCount > 0U &&
        options.maxResidentTerrainChunks > 0U &&
        options.maxResidentTerrainChunks <= options.terrainSlotCount &&
        options.chunkCreatesPerFrame <= options.terrainSlotCount &&
        options.chunkDestroysPerFrame <= options.terrainSlotCount &&
        options.materialChurnPeriod > 0U &&
        options.textureChurnPeriod > 0U &&
        options.lodChurnPeriod > 0U &&
        options.decalChurnPeriod > 0U &&
        options.particleChurnPeriod > 0U &&
        options.skinnedPaletteChurnPeriod > 0U &&
        options.optionalPassTogglePeriod > 0U &&
        options.resizePeriod > 0U;
}

LongSessionChurnSummary runLongSessionChurnSimulation(const LongSessionChurnOptions& options)
{
    LongSessionChurnSummary summary;
    summary.seed = options.seed;
    summary.requestedFrames = options.frameCount;
    summary.callerRebasedLargeCoordinates = options.callerRebasedLargeCoordinates;
    summary.deterministicHash = 1469598103934665603ULL ^ options.seed;

    if (!isValidLongSessionChurnOptions(options))
    {
        summary.finalPresentInvalidFrames = options.frameCount;
        summary.unboundedGrowthWarnings = 1;
        return summary;
    }

    std::vector<SimulatedSlot> slots(options.terrainSlotCount);
    std::uint32_t randomState = options.seed;
    std::uint32_t residentTerrainChunks = 0;
    std::uint32_t staleHandleBacklog = 0;
    std::uint32_t liveMaterials = 0;
    std::uint32_t liveTextures = 0;
    std::uint32_t liveLods = 0;

    for (std::uint32_t frameIndex = 0; frameIndex < options.frameCount; ++frameIndex)
    {
        LongSessionChurnFrame frame;
        frame.frameIndex = frameIndex;

        const std::uint32_t destroyBudget = std::min(options.chunkDestroysPerFrame, residentTerrainChunks);
        for (std::uint32_t destroyIndex = 0; destroyIndex < destroyBudget; ++destroyIndex)
        {
            const std::uint32_t ordinal = residentTerrainChunks > 0U ? nextRandom(randomState) % residentTerrainChunks : 0U;
            const std::uint32_t slotIndex = findNthActiveSlot(slots, ordinal);
            if (slotIndex >= slots.size())
            {
                continue;
            }
            slots[slotIndex].active = false;
            --residentTerrainChunks;
            ++frame.terrainChunksDestroyed;
            if (options.submitStaleHandles)
            {
                ++staleHandleBacklog;
            }
        }

        const std::uint32_t createBudget =
            std::min(options.chunkCreatesPerFrame, options.maxResidentTerrainChunks - residentTerrainChunks);
        for (std::uint32_t createIndex = 0; createIndex < createBudget; ++createIndex)
        {
            const std::uint32_t inactiveCount = inactiveSlotCount(options.terrainSlotCount, residentTerrainChunks);
            if (inactiveCount == 0U)
            {
                break;
            }
            const std::uint32_t ordinal = nextRandom(randomState) % inactiveCount;
            const std::uint32_t slotIndex = findNthInactiveSlot(slots, ordinal);
            if (slotIndex >= slots.size())
            {
                break;
            }
            if (slots[slotIndex].everUsed)
            {
                ++frame.terrainChunkSlotsReused;
            }
            slots[slotIndex].active = true;
            slots[slotIndex].everUsed = true;
            ++residentTerrainChunks;
            ++frame.terrainChunksCreated;
        }

        if (options.submitStaleHandles && staleHandleBacklog > 0U)
        {
            frame.staleChunkHandleAttempts = std::min(staleHandleBacklog, 1U + (nextRandom(randomState) % 3U));
            staleHandleBacklog -= frame.staleChunkHandleAttempts;
        }
        if (options.submitStaleHandles && frameIndex % 17U == 0U)
        {
            frame.invalidHandleAttempts = 1;
        }

        if (options.materialFallbackChurnEnabled)
        {
            if (frameIndex % options.materialChurnPeriod == 0U)
            {
                frame.materialCreates = 1;
                ++liveMaterials;
            }
            if (frameIndex > 0U && frameIndex % (options.materialChurnPeriod + 2U) == 0U)
            {
                frame.materialDestroys = liveMaterials > 0U ? 1U : 0U;
                liveMaterials -= frame.materialDestroys;
                frame.materialFallbacks = 1;
            }

            if (frameIndex % options.textureChurnPeriod == 0U)
            {
                frame.textureCreates = 1;
                ++liveTextures;
            }
            if (frameIndex > 0U && frameIndex % (options.textureChurnPeriod + 3U) == 0U)
            {
                frame.textureDestroys = liveTextures > 0U ? 1U : 0U;
                liveTextures -= frame.textureDestroys;
                frame.textureFallbacks = 1;
            }

            if (frameIndex % options.lodChurnPeriod == 0U)
            {
                frame.lodCreates = 1;
                ++liveLods;
            }
            if (frameIndex > 0U && frameIndex % (options.lodChurnPeriod + 5U) == 0U)
            {
                frame.lodDestroys = liveLods > 0U ? 1U : 0U;
                liveLods -= frame.lodDestroys;
                frame.lodFallbacks = 1;
            }
        }

        if (options.decalParticleChurnEnabled)
        {
            frame.decalsSubmitted = 4U + (nextRandom(randomState) % 5U);
            frame.decalsCulled = frameIndex % options.decalChurnPeriod == 0U ? 1U : 0U;
            frame.decalsRendered = frame.decalsSubmitted - std::min(frame.decalsSubmitted, frame.decalsCulled);
            frame.decalsDestroyed = frameIndex > 0U && frameIndex % (options.decalChurnPeriod + 4U) == 0U ? 1U : 0U;

            frame.particleBatchesSubmitted = 2U + (nextRandom(randomState) % 4U);
            frame.particleBatchesCulled = frameIndex % options.particleChurnPeriod == 0U ? 1U : 0U;
            frame.particleBatchesRendered =
                frame.particleBatchesSubmitted - std::min(frame.particleBatchesSubmitted, frame.particleBatchesCulled);
        }

        if (options.skinnedPaletteChurnEnabled)
        {
            frame.skinnedPaletteSubmissions = 1U + (frameIndex % 4U);
            frame.skinnedPaletteRejections =
                frameIndex > 0U && frameIndex % options.skinnedPaletteChurnPeriod == 0U ? 1U : 0U;
        }

        if (options.optionalPassToggleChurnEnabled)
        {
            const bool phase = ((frameIndex / options.optionalPassTogglePeriod) % 2U) == 0U;
            frame.ssaoEnabled = phase;
            frame.decalsEnabled = phase || (frameIndex % 3U != 0U);
            frame.particlesEnabled = !phase || (frameIndex % 5U != 0U);
            frame.colorGradingEnabled = phase;
            frame.selectionOutlineEnabled = frameIndex % 2U == 0U;
            frame.optionalPassToggleCount =
                frameIndex > 0U && frameIndex % options.optionalPassTogglePeriod == 0U ? 5U : 0U;
        }

        if (options.resizeChurnEnabled && frameIndex > 0U && frameIndex % options.resizePeriod == 0U)
        {
            frame.resizeEventCount = 1;
            frame.sceneTargetRecreates = 1;
            frame.postTargetRecreates = 1;
            frame.shadowTargetRecreates = frameIndex % (options.resizePeriod * 2U) == 0U ? 1U : 0U;
        }

        frame.residentTerrainChunks = residentTerrainChunks;
        frame.trackedLiveResources = residentTerrainChunks + liveMaterials + liveTextures + liveLods;
        frame.estimatedLiveBytes = estimateLiveBytes(residentTerrainChunks, liveMaterials, liveTextures, liveLods);
        frame.stagedBytes = estimateFrameStagedBytes(frame);
        frame.frameDataLifetimeChecks = 8;
        frame.finalPresentValid = true;

        accumulateFrame(summary, frame);
        ++summary.simulatedFrames;
    }

    summary.residentTerrainChunksBeforeReset = residentTerrainChunks;
    summary.trackedLiveResourcesBeforeReset = residentTerrainChunks + liveMaterials + liveTextures + liveLods;
    summary.finalResidentTerrainChunksAfterReset = 0;
    summary.finalTrackedLiveResourcesAfterReset = 0;

    if (summary.peakTrackedLiveResources > options.terrainSlotCount + options.frameCount)
    {
        summary.unboundedGrowthWarnings = 1;
    }

    mixHash(summary.deterministicHash, summary.simulatedFrames);
    mixHash(summary.deterministicHash, summary.peakResidentTerrainChunks);
    mixHash(summary.deterministicHash, summary.trackedLiveResourcesBeforeReset);
    mixHash(summary.deterministicHash, summary.finalTrackedLiveResourcesAfterReset);

    return summary;
}
} // namespace full_renderer::debug
