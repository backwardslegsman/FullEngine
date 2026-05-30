#pragma once

#include <cstdint>
#include <cstddef>
#include <map>

namespace full_engine
{
/**
 * @brief Stable engine-owned chunk coordinate identity.
 *
 * Coordinates are signed integer world-partition indices. The renderer must
 * not treat this as a renderer resource handle; it is engine world identity.
 */
struct ChunkId
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

/** @brief Compares chunk identities for exact coordinate equality. */
bool operator==(const ChunkId& lhs, const ChunkId& rhs) noexcept;

/** @brief Compares chunk identities for deterministic registry ordering. */
bool operator<(const ChunkId& lhs, const ChunkId& rhs) noexcept;

/** @brief Engine-owned residency policy state for a world chunk. */
enum class ChunkResidencyState
{
    Unloaded,
    Loading,
    Resident,
    Unloading,
};

/** @brief Result code for world chunk registry operations. */
enum class WorldResult
{
    Success,
    AlreadyExists,
    NotFound,
    InvalidArgument,
    InvalidTransition,
};

/** @brief Value snapshot of one registered world chunk. */
struct WorldChunkInfo
{
    ChunkId id = {};
    ChunkResidencyState residency = ChunkResidencyState::Unloaded;
};

/**
 * @brief CPU-only owner of engine world chunk identity and residency state.
 *
 * This registry deliberately owns no renderer handles, terrain descriptors,
 * file paths, background jobs, or origin-rebasing policy. It records only the
 * engine's chunk identity and high-level residency state.
 */
class WorldChunkRegistry
{
public:
    /** @brief Registers a chunk in the `Unloaded` residency state. */
    WorldResult createChunk(const ChunkId& id);

    /** @brief Removes a registered chunk. */
    WorldResult removeChunk(const ChunkId& id);

    /** @brief Updates the residency state if the transition is valid. */
    WorldResult setResidencyState(const ChunkId& id, ChunkResidencyState state);

    /** @brief Returns whether a chunk identity is currently registered. */
    bool contains(const ChunkId& id) const;

    /** @brief Returns a read-only chunk snapshot, or null if the chunk is missing. */
    const WorldChunkInfo* findChunk(const ChunkId& id) const;

    /** @brief Returns the number of registered chunks. */
    std::size_t chunkCount() const noexcept;

    /** @brief Removes all registered chunk state. */
    void clear() noexcept;

private:
    std::map<ChunkId, WorldChunkInfo> chunks_;
};
} // namespace full_engine
