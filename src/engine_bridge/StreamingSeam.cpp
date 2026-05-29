#include "engine_bridge/StreamingSeam.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace full_renderer::engine_bridge
{
namespace
{
bool isFinite3(const double values[3]) noexcept
{
    return std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]);
}

bool isFinite3f(const float values[3]) noexcept
{
    return std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]);
}

void mixHash(std::uint64_t& hash, const std::uint64_t value) noexcept
{
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6U) + (hash >> 2U);
}

double maxAbs3(const double values[3]) noexcept
{
    return std::max(std::max(std::abs(values[0]), std::abs(values[1])), std::abs(values[2]));
}

std::uint32_t nextRandom(std::uint32_t& state) noexcept
{
    state = state * 1664525U + 1013904223U;
    return state;
}

TerrainChunkHandle simulatedTerrainHandle(
    const std::uint32_t slot,
    const std::uint32_t generation) noexcept
{
    return TerrainChunkHandle{slot + 1U, generation + 1U};
}
} // namespace

bool isValidFrameOriginDesc(const FrameOriginDesc& origin) noexcept
{
    return isFinite3(origin.originWorld) &&
        isFinite3(origin.cameraWorld) &&
        std::isfinite(origin.warningDistanceMeters) &&
        origin.warningDistanceMeters > 0.0;
}

bool rebasePosition(
    const FrameOriginDesc& origin,
    const double worldPosition[3],
    float outRendererPosition[3]) noexcept
{
    if (!isValidFrameOriginDesc(origin) || worldPosition == nullptr || outRendererPosition == nullptr ||
        !isFinite3(worldPosition))
    {
        return false;
    }

    for (int axis = 0; axis < 3; ++axis)
    {
        const double value = origin.mode == OriginMode::CameraRelative ?
            worldPosition[axis] - origin.originWorld[axis] :
            worldPosition[axis];
        if (!std::isfinite(value) ||
            std::abs(value) > static_cast<double>(std::numeric_limits<float>::max()))
        {
            return false;
        }
        outRendererPosition[axis] = static_cast<float>(value);
    }

    return isFinite3f(outRendererPosition);
}

bool rebaseAabb(
    const FrameOriginDesc& origin,
    const double worldMin[3],
    const double worldMax[3],
    Aabb& outRendererBounds) noexcept
{
    if (worldMin == nullptr || worldMax == nullptr || !isFinite3(worldMin) || !isFinite3(worldMax))
    {
        return false;
    }

    for (int axis = 0; axis < 3; ++axis)
    {
        if (worldMin[axis] > worldMax[axis])
        {
            return false;
        }
    }

    float minRenderer[3] = {};
    float maxRenderer[3] = {};
    if (!rebasePosition(origin, worldMin, minRenderer) ||
        !rebasePosition(origin, worldMax, maxRenderer))
    {
        return false;
    }

    for (int axis = 0; axis < 3; ++axis)
    {
        outRendererBounds.min[axis] = std::min(minRenderer[axis], maxRenderer[axis]);
        outRendererBounds.max[axis] = std::max(minRenderer[axis], maxRenderer[axis]);
    }
    return true;
}

bool EngineStreamingSeam::beginFrame(const FrameOriginDesc& origin) noexcept
{
    resetFrameCounters();
    stats_.originMode = origin.mode;
    for (int axis = 0; axis < 3; ++axis)
    {
        stats_.originWorld[axis] = origin.originWorld[axis];
        stats_.cameraWorld[axis] = origin.cameraWorld[axis];
    }

    if (!isValidFrameOriginDesc(origin))
    {
        ++stats_.nanOrInfRejectionCount;
        return false;
    }

    if (!rebasePosition(origin, origin.cameraWorld, stats_.cameraRendererRelative))
    {
        ++stats_.nanOrInfRejectionCount;
        return false;
    }

    const double rendererRelativeMagnitude = std::max(std::max(
            std::abs(static_cast<double>(stats_.cameraRendererRelative[0])),
            std::abs(static_cast<double>(stats_.cameraRendererRelative[1]))),
            std::abs(static_cast<double>(stats_.cameraRendererRelative[2])));
    if ((origin.mode == OriginMode::AbsoluteRenderSpace && maxAbs3(origin.cameraWorld) > origin.warningDistanceMeters) ||
        rendererRelativeMagnitude > origin.warningDistanceMeters)
    {
        ++stats_.largeCoordinateWarningCount;
    }

    refreshCounts();
    return true;
}

bool EngineStreamingSeam::createOrUpdateTerrainChunk(
    const EngineChunkId id,
    const TerrainChunkHandle rendererHandle,
    const bool materialFallbackActive) noexcept
{
    if (id == 0 || !isValid(rendererHandle))
    {
        ++stats_.staleMappingAttempts;
        return false;
    }

    if (Mapping* mapping = findMutable(id))
    {
        mapping->rendererHandle = rendererHandle;
        mapping->materialFallbackActive = materialFallbackActive;
        refreshCounts();
        return true;
    }

    if (mappingCount_ >= kMaxMappings)
    {
        ++stats_.staleMappingAttempts;
        return false;
    }

    if (wasDestroyed(id))
    {
        ++stats_.mappingsReusedThisFrame;
        ++stats_.totalMappingsReused;
    }

    mappings_[mappingCount_++] = Mapping{id, rendererHandle, materialFallbackActive};
    ++stats_.mappingsCreatedThisFrame;
    ++stats_.totalMappingsCreated;
    refreshCounts();
    return true;
}

bool EngineStreamingSeam::destroyTerrainChunk(const EngineChunkId id) noexcept
{
    for (std::uint32_t index = 0; index < mappingCount_; ++index)
    {
        if (mappings_[index].id != id)
        {
            continue;
        }

        rememberDestroyed(id);
        mappings_[index] = mappings_[mappingCount_ - 1U];
        --mappingCount_;
        ++stats_.mappingsDestroyedThisFrame;
        ++stats_.totalMappingsDestroyed;
        refreshCounts();
        return true;
    }

    ++stats_.staleMappingAttempts;
    return false;
}

TerrainChunkHandle EngineStreamingSeam::findTerrainChunk(const EngineChunkId id) const noexcept
{
    const Mapping* mapping = find(id);
    return mapping != nullptr ? mapping->rendererHandle : TerrainChunkHandle{};
}

bool EngineStreamingSeam::hasTerrainChunk(const EngineChunkId id) const noexcept
{
    return find(id) != nullptr;
}

void EngineStreamingSeam::resetFrameCounters() noexcept
{
    stats_.mappingsCreatedThisFrame = 0;
    stats_.mappingsDestroyedThisFrame = 0;
    stats_.mappingsReusedThisFrame = 0;
    stats_.materialFallbackMappingCount = 0;
    refreshCounts();
}

void EngineStreamingSeam::reset() noexcept
{
    mappingCount_ = 0;
    destroyedHistoryCount_ = 0;
    stats_ = {};
    refreshCounts();
}

EngineStreamingSeam::Mapping* EngineStreamingSeam::findMutable(const EngineChunkId id) noexcept
{
    for (std::uint32_t index = 0; index < mappingCount_; ++index)
    {
        if (mappings_[index].id == id)
        {
            return &mappings_[index];
        }
    }
    return nullptr;
}

const EngineStreamingSeam::Mapping* EngineStreamingSeam::find(const EngineChunkId id) const noexcept
{
    for (std::uint32_t index = 0; index < mappingCount_; ++index)
    {
        if (mappings_[index].id == id)
        {
            return &mappings_[index];
        }
    }
    return nullptr;
}

bool EngineStreamingSeam::wasDestroyed(const EngineChunkId id) const noexcept
{
    for (std::uint32_t index = 0; index < destroyedHistoryCount_; ++index)
    {
        if (destroyedHistory_[index] == id)
        {
            return true;
        }
    }
    return false;
}

void EngineStreamingSeam::rememberDestroyed(const EngineChunkId id)
{
    if (wasDestroyed(id))
    {
        return;
    }

    if (destroyedHistoryCount_ < kMaxDestroyedHistory)
    {
        destroyedHistory_[destroyedHistoryCount_++] = id;
        return;
    }

    for (std::uint32_t index = 1; index < kMaxDestroyedHistory; ++index)
    {
        destroyedHistory_[index - 1U] = destroyedHistory_[index];
    }
    destroyedHistory_[kMaxDestroyedHistory - 1U] = id;
}

void EngineStreamingSeam::refreshCounts() noexcept
{
    stats_.mappingCount = mappingCount_;
    stats_.engineResidentChunkCount = mappingCount_;
    stats_.rendererLiveChunkMappingCount = mappingCount_;
    stats_.materialFallbackMappingCount = 0;
    for (std::uint32_t index = 0; index < mappingCount_; ++index)
    {
        if (mappings_[index].materialFallbackActive)
        {
            ++stats_.materialFallbackMappingCount;
        }
    }
}

EngineStreamingChurnSummary runEngineStreamingSeamChurnSimulation(
    const EngineStreamingChurnOptions& options)
{
    EngineStreamingChurnSummary summary;
    summary.churn = debug::runLongSessionChurnSimulation(options.churn);
    summary.deterministicHash = 1469598103934665603ULL ^ options.churn.seed;

    if (!debug::isValidLongSessionChurnOptions(options.churn) ||
        !isValidFrameOriginDesc(options.origin) ||
        options.originShiftPeriod == 0U)
    {
        summary.finalStats.nanOrInfRejectionCount = 1;
        return summary;
    }

    EngineStreamingSeam seam;
    std::uint32_t randomState = options.churn.seed;
    std::uint32_t nextEngineId = 1;
    std::uint32_t nextGeneration = 1;
    EngineChunkId activeIds[512] = {};
    std::uint32_t activeCount = 0;
    const std::uint32_t activeCapacity =
        std::min<std::uint32_t>(512U, options.churn.maxResidentTerrainChunks);

    for (std::uint32_t frameIndex = 0; frameIndex < options.churn.frameCount; ++frameIndex)
    {
        FrameOriginDesc origin = options.origin;
        if (options.shiftOriginAtInterval && frameIndex > 0U && frameIndex % options.originShiftPeriod == 0U)
        {
            const double step = options.originStepMeters * static_cast<double>(frameIndex / options.originShiftPeriod);
            origin.originWorld[0] += step;
            origin.originWorld[2] -= step;
            origin.cameraWorld[0] += step;
            origin.cameraWorld[2] -= step;
            ++summary.originShiftCount;
        }
        (void)seam.beginFrame(origin);

        const std::uint32_t destroyBudget = std::min(options.churn.chunkDestroysPerFrame, activeCount);
        for (std::uint32_t destroyIndex = 0; destroyIndex < destroyBudget; ++destroyIndex)
        {
            const std::uint32_t activeIndex = activeCount > 0U ? nextRandom(randomState) % activeCount : 0U;
            const EngineChunkId id = activeIds[activeIndex];
            (void)seam.destroyTerrainChunk(id);
            activeIds[activeIndex] = activeIds[activeCount - 1U];
            --activeCount;
        }

        if (options.churn.submitStaleHandles && frameIndex % 13U == 0U)
        {
            (void)seam.destroyTerrainChunk(9000000ULL + frameIndex);
        }

        const std::uint32_t createBudget =
            std::min(options.churn.chunkCreatesPerFrame, activeCapacity - activeCount);
        for (std::uint32_t createIndex = 0; createIndex < createBudget; ++createIndex)
        {
            EngineChunkId id = nextEngineId++;
            if (frameIndex > 0U && frameIndex % 29U == 0U && activeCount < activeCapacity)
            {
                id = 1U + (nextRandom(randomState) % std::max(1U, nextEngineId - 1U));
            }
            const bool materialFallback =
                options.churn.materialFallbackChurnEnabled && frameIndex % options.churn.materialChurnPeriod == 0U;
            const std::uint32_t slot = nextRandom(randomState) % std::max(1U, options.churn.terrainSlotCount);
            bool alreadyActive = false;
            for (std::uint32_t activeIndex = 0; activeIndex < activeCount; ++activeIndex)
            {
                alreadyActive = alreadyActive || activeIds[activeIndex] == id;
            }
            if (seam.createOrUpdateTerrainChunk(id, simulatedTerrainHandle(slot, nextGeneration++), materialFallback) &&
                activeCount < activeCapacity &&
                !alreadyActive)
            {
                activeIds[activeCount++] = id;
            }
        }

        const EngineStreamingSeamStats& stats = seam.stats();
        mixHash(summary.deterministicHash, frameIndex);
        mixHash(summary.deterministicHash, stats.mappingCount);
        mixHash(summary.deterministicHash, stats.mappingsCreatedThisFrame);
        mixHash(summary.deterministicHash, stats.mappingsDestroyedThisFrame);
        mixHash(summary.deterministicHash, stats.mappingsReusedThisFrame);
        mixHash(summary.deterministicHash, stats.staleMappingAttempts);
        mixHash(summary.deterministicHash, stats.materialFallbackMappingCount);
        mixHash(summary.deterministicHash, summary.originShiftCount);
        ++summary.simulatedFrames;
    }

    summary.finalStats = seam.stats();
    seam.reset();
    summary.resetFinalMappingCount = seam.stats().mappingCount;
    summary.resetFinalResidentCount = seam.stats().engineResidentChunkCount;
    mixHash(summary.deterministicHash, summary.churn.deterministicHash);
    mixHash(summary.deterministicHash, summary.finalStats.mappingCount);
    mixHash(summary.deterministicHash, summary.resetFinalMappingCount);
    return summary;
}
} // namespace full_renderer::engine_bridge
