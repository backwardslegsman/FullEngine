#pragma once

#include "engine/renderer_integration/WorldRenderSnapshot.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Engine-owned terrain chunk record prepared from a ready world snapshot record. */
struct TerrainChunkRenderPrep
{
    ChunkId id = {};
    RenderBounds bounds = {};
    RenderChunkStatus sourceStatus = RenderChunkStatus::Ready;
};

/** @brief Counters describing terrain prep output and skipped snapshot records. */
struct TerrainRenderPrepSummary
{
    std::size_t readyCount = 0;
    std::size_t skippedNotResidentCount = 0;
    std::size_t skippedMissingChunkCount = 0;
    std::size_t skippedInvalidBoundsCount = 0;
    std::size_t skippedOutOfRangeCount = 0;
    std::size_t invalidInputCount = 0;
};

/** @brief CPU-side terrain preparation output owned by the engine integration layer. */
struct TerrainRenderPrep
{
    std::vector<TerrainChunkRenderPrep> chunks;
    TerrainRenderPrepSummary summary;
};

/**
 * @brief Filters ready world snapshot records into terrain-prep records.
 *
 * This does not create renderer terrain descriptors, renderer handles, asset
 * references, LOD choices, or backend resources.
 */
TerrainRenderPrep prepareTerrainRenderChunks(const WorldRenderSnapshot& snapshot);
} // namespace full_engine
