#pragma once

#include "engine/renderer_integration/RenderSpace.hpp"
#include "engine/world/WorldChunkRegistry.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Engine-owned chunk render input for one frame snapshot build. */
struct WorldChunkRenderDesc
{
    ChunkId id = {};
    WorldBounds bounds = {};
};

/** @brief Per-chunk result of preparing engine world data for renderer integration. */
enum class RenderChunkStatus
{
    Ready,
    NotResident,
    MissingChunk,
    InvalidBounds,
    OutOfRange,
};

/** @brief Snapshot record for one input chunk descriptor. */
struct RenderChunkSnapshot
{
    ChunkId id = {};
    ChunkResidencyState residency = ChunkResidencyState::Unloaded;
    RenderBounds bounds = {};
    RenderChunkStatus status = RenderChunkStatus::MissingChunk;
};

/** @brief Options used while preparing world chunks for render-space consumption. */
struct WorldRenderSnapshotOptions
{
    WorldOrigin origin = {};
    RenderSpaceLimits limits = {};
};

/** @brief CPU-side snapshot of engine world chunks prepared for renderer integration. */
struct WorldRenderSnapshot
{
    std::vector<RenderChunkSnapshot> chunks;
    std::size_t readyCount = 0;
    std::size_t notResidentCount = 0;
    std::size_t missingChunkCount = 0;
    std::size_t invalidBoundsCount = 0;
    std::size_t outOfRangeCount = 0;
    std::size_t invalidInputCount = 0;
};

/**
 * @brief Builds a frame-oriented render snapshot from engine chunk state.
 *
 * The snapshot owns no renderer resources and stores no renderer handles. It
 * preserves one output record per input descriptor except when `chunks` is null
 * with a non-zero count, which is reported through `invalidInputCount`.
 */
WorldRenderSnapshot buildWorldRenderSnapshot(
    const WorldChunkRegistry& registry,
    const WorldChunkRenderDesc* chunks,
    std::size_t chunkCount,
    const WorldRenderSnapshotOptions& options);
} // namespace full_engine
