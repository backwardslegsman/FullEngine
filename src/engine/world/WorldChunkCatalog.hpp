#pragma once

#include "engine/world/WorldChunkRegistry.hpp"
#include "engine/world/WorldOrigin.hpp"

#include <cstddef>
#include <map>
#include <vector>

namespace full_engine
{
/** @brief Engine-owned stable metadata for one world chunk. */
struct WorldChunkDesc
{
    ChunkId id = {};
    WorldBounds bounds = {};
};

/**
 * @brief CPU-only catalog of stable chunk world metadata.
 *
 * The catalog stores chunk descriptors by value in deterministic `ChunkId`
 * order. It owns no residency state, renderer handles, terrain resources,
 * persistence records, async work, or IO.
 */
class WorldChunkCatalog
{
public:
    /** @brief Adds a valid descriptor for an untracked chunk. */
    WorldResult addChunk(const WorldChunkDesc& desc);

    /** @brief Replaces the descriptor for an existing chunk. */
    WorldResult updateChunk(const WorldChunkDesc& desc);

    /** @brief Removes a chunk descriptor. */
    WorldResult removeChunk(const ChunkId& id);

    /** @brief Returns a read-only descriptor snapshot, or null if missing. */
    const WorldChunkDesc* findChunk(const ChunkId& id) const;

    /** @brief Returns whether a descriptor exists for the chunk. */
    bool contains(const ChunkId& id) const;

    /** @brief Returns the number of stored descriptors. */
    std::size_t chunkCount() const noexcept;

    /** @brief Returns value snapshots in deterministic `ChunkId` order. */
    std::vector<WorldChunkDesc> descs() const;

    /** @brief Removes all stored descriptors. */
    void clear() noexcept;

private:
    std::map<ChunkId, WorldChunkDesc> chunks_;
};
} // namespace full_engine
