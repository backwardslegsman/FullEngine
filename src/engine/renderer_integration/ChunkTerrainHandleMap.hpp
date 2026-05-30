#pragma once

#include "engine/world/WorldChunkRegistry.hpp"
#include "full_renderer/Terrain.hpp"

#include <cstddef>
#include <map>

namespace full_engine
{
/** @brief Result code for engine chunk to renderer terrain handle map operations. */
enum class ChunkTerrainHandleMapResult
{
    Success,
    AlreadyMapped,
    NotFound,
    InvalidHandle,
};

/** @brief Value snapshot of one engine chunk to renderer terrain handle association. */
struct ChunkTerrainHandleRecord
{
    ChunkId id = {};
    full_renderer::TerrainChunkHandle handle = {};
};

/**
 * @brief CPU-only association map from engine chunk identity to renderer terrain handles.
 *
 * This type stores opaque renderer terrain chunk handles that were obtained by
 * another integration layer. It does not create, update, destroy, validate
 * liveness, or submit terrain chunks. Handle ownership remains with the
 * renderer; the engine owns only the association to `ChunkId`.
 */
class ChunkTerrainHandleMap
{
public:
    /** @brief Adds a new chunk-to-handle association if the handle is valid and the chunk is unmapped. */
    ChunkTerrainHandleMapResult mapChunk(const ChunkId& id, full_renderer::TerrainChunkHandle handle);

    /** @brief Replaces an existing chunk-to-handle association with another valid handle. */
    ChunkTerrainHandleMapResult updateChunk(const ChunkId& id, full_renderer::TerrainChunkHandle handle);

    /** @brief Removes an existing chunk-to-handle association. */
    ChunkTerrainHandleMapResult removeChunk(const ChunkId& id);

    /** @brief Returns the mapped renderer terrain handle, or null if the chunk is unmapped. */
    const full_renderer::TerrainChunkHandle* findHandle(const ChunkId& id) const;

    /** @brief Returns whether a chunk currently has a mapped renderer terrain handle. */
    bool contains(const ChunkId& id) const;

    /** @brief Returns the number of chunk-to-handle associations. */
    std::size_t mappedCount() const noexcept;

    /** @brief Removes all chunk-to-handle associations without touching renderer resources. */
    void clear() noexcept;

private:
    std::map<ChunkId, full_renderer::TerrainChunkHandle> handles_;
};
} // namespace full_engine
