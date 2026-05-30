#pragma once

#include "engine/world/WorldChunkCatalog.hpp"
#include "engine/world/WorldChunkRegistry.hpp"

namespace full_engine
{
/**
 * @brief Aggregate result for coordinated world chunk set operations.
 *
 * `PartialFailure` means the registry and catalog were found out of sync or an
 * unexpected owner-side operation failed. Helpers repair what they can before
 * reporting the result.
 */
enum class WorldChunkSetResult
{
    Success,
    AlreadyExists,
    NotFound,
    InvalidArgument,
    PartialFailure,
};

/**
 * @brief Result of adding a chunk to coordinated world owners.
 *
 * The aggregate result describes the helper contract, while the owner-specific
 * results expose the exact registry/catalog outcome for diagnostics.
 */
struct WorldChunkSetAddResult
{
    WorldChunkSetResult result = WorldChunkSetResult::PartialFailure;
    WorldResult registryResult = WorldResult::NotFound;
    WorldResult catalogResult = WorldResult::NotFound;
};

/**
 * @brief Result of removing a chunk from coordinated world owners.
 *
 * The aggregate result reports whether both owners agreed. The registry and
 * catalog results are copied through so callers can log repaired drift.
 */
struct WorldChunkSetRemoveResult
{
    WorldChunkSetResult result = WorldChunkSetResult::PartialFailure;
    WorldResult registryResult = WorldResult::NotFound;
    WorldResult catalogResult = WorldResult::NotFound;
};

/**
 * @brief Adds matching chunk identity and metadata to world owners.
 *
 * The descriptor is validated through the world catalog rules before registry
 * state is allowed to persist. If registry creation succeeds but catalog
 * insertion fails, the helper removes the registry entry before returning.
 *
 * This helper coordinates only engine world state. It does not create terrain
 * resources, renderer handles, async work, persistence records, or IO.
 *
 * @return `Success` only when both owners are updated, `AlreadyExists` for an
 * already coordinated chunk, `InvalidArgument` for invalid metadata, or
 * `PartialFailure` for repaired drift or unexpected owner disagreement.
 */
WorldChunkSetAddResult addWorldChunk(
    WorldChunkRegistry& registry,
    WorldChunkCatalog& catalog,
    const WorldChunkDesc& desc);

/**
 * @brief Removes matching chunk identity and metadata from world owners.
 *
 * If only one owner contains the chunk, the existing side is removed and the
 * aggregate result reports `PartialFailure` so callers can diagnose repaired
 * drift.
 *
 * @return `Success` when both owners removed the chunk, `NotFound` when neither
 * owner had it, or `PartialFailure` when only one owner had state to remove.
 */
WorldChunkSetRemoveResult removeWorldChunk(
    WorldChunkRegistry& registry,
    WorldChunkCatalog& catalog,
    const ChunkId& id);
} // namespace full_engine
