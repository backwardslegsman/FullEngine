#pragma once

#include "engine/renderer_integration/TerrainResourceCatalog.hpp"
#include "engine/world/WorldChunkSet.hpp"

namespace full_engine
{
/**
 * @brief Aggregate result for coordinated terrain chunk setup operations.
 *
 * This result describes coordination across world chunk owners and terrain
 * resource metadata only. Renderer handles, renderer resources, frame
 * submission, streaming jobs, and IO are outside this helper.
 */
enum class TerrainChunkSetupResult
{
    Success,
    AlreadyExists,
    NotFound,
    InvalidArgument,
    PartialFailure,
};

/**
 * @brief Result of adding coordinated terrain chunk setup state.
 *
 * The world and terrain resource results are preserved for diagnostics so
 * callers can identify whether drift was repaired or rejected.
 */
struct TerrainChunkSetupAddResult
{
    TerrainChunkSetupResult result = TerrainChunkSetupResult::PartialFailure;
    WorldChunkSetAddResult worldResult = {};
    TerrainResourceResult resourceResult = TerrainResourceResult::NotFound;
};

/**
 * @brief Result of removing coordinated terrain chunk setup state.
 *
 * A `PartialFailure` aggregate result means at least one owner had state while
 * another owner did not; the helper removes any state it can to repair drift.
 */
struct TerrainChunkSetupRemoveResult
{
    TerrainChunkSetupResult result = TerrainChunkSetupResult::PartialFailure;
    WorldChunkSetRemoveResult worldResult = {};
    TerrainResourceResult resourceResult = TerrainResourceResult::NotFound;
};

/**
 * @brief Adds world chunk state and terrain resource metadata for one terrain chunk.
 *
 * The world and resource descriptors must reference the same `ChunkId`.
 * Terrain resource descriptors are validated before world state is created.
 * If world setup succeeds but resource registration fails, world setup is
 * removed before returning.
 */
TerrainChunkSetupAddResult addTerrainChunk(
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resourceCatalog,
    const WorldChunkDesc& worldDesc,
    const TerrainChunkResourceDesc& resourceDesc);

/**
 * @brief Removes world chunk state and terrain resource metadata for one terrain chunk.
 *
 * Missing state in all owners returns `NotFound`. Missing state in only some
 * owners returns `PartialFailure` after any existing owner state is removed.
 */
TerrainChunkSetupRemoveResult removeTerrainChunk(
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resourceCatalog,
    const ChunkId& id);
} // namespace full_engine
