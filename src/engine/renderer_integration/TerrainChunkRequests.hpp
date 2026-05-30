#pragma once

#include "engine/renderer_integration/TerrainChunkSetup.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Engine-owned request type for terrain setup intent. */
enum class TerrainChunkRequestType
{
    Add,
    Remove,
};

/** @brief One ordered request to add or remove terrain setup state. */
struct TerrainChunkRequest
{
    TerrainChunkRequestType type = TerrainChunkRequestType::Add;
    ChunkId id = {};
    WorldChunkDesc worldDesc = {};
    TerrainChunkResourceDesc resourceDesc = {};
};

/** @brief Per-request result after applying a terrain setup request queue. */
enum class TerrainChunkRequestStatus
{
    Applied,
    AlreadySatisfied,
    NotFound,
    InvalidArgument,
    PartialFailure,
};

/** @brief Ordered diagnostic record for one terrain setup request. */
struct TerrainChunkRequestRecord
{
    TerrainChunkRequest request = {};
    TerrainChunkRequestStatus status = TerrainChunkRequestStatus::NotFound;
    TerrainChunkSetupAddResult addResult = {};
    TerrainChunkSetupRemoveResult removeResult = {};
};

/** @brief Summary counters for a terrain setup request queue application. */
struct TerrainChunkRequestApplySummary
{
    std::size_t appliedCount = 0;
    std::size_t alreadySatisfiedCount = 0;
    std::size_t notFoundCount = 0;
    std::size_t invalidArgumentCount = 0;
    std::size_t partialFailureCount = 0;
};

/** @brief Ordered result of applying terrain setup requests. */
struct TerrainChunkRequestApplyResult
{
    std::vector<TerrainChunkRequestRecord> records;
    TerrainChunkRequestApplySummary summary;
};

/**
 * @brief CPU-only FIFO queue of terrain setup add/remove intent.
 *
 * The queue owns only setup requests. Applying it mutates only the supplied
 * world chunk registry/catalog and terrain resource catalog through
 * `addTerrainChunk` and `removeTerrainChunk`. It does not change residency,
 * renderer terrain handles, renderer submission, async work, or IO.
 */
class TerrainChunkRequestQueue
{
public:
    /** @brief Appends a terrain setup request to the queue. */
    void push(const TerrainChunkRequest& request);

    /** @brief Appends an add request for matching world and terrain resource descriptors. */
    void pushAdd(const WorldChunkDesc& worldDesc, const TerrainChunkResourceDesc& resourceDesc);

    /** @brief Appends a remove request for the supplied chunk ID. */
    void pushRemove(const ChunkId& id);

    /** @brief Returns the number of queued requests. */
    std::size_t requestCount() const noexcept;

    /** @brief Returns queued requests in FIFO order. */
    const std::vector<TerrainChunkRequest>& requests() const noexcept;

    /** @brief Applies queued requests to setup owners without clearing the queue. */
    TerrainChunkRequestApplyResult applyTo(
        WorldChunkRegistry& registry,
        WorldChunkCatalog& worldCatalog,
        TerrainResourceCatalog& resourceCatalog) const;

    /** @brief Removes all queued requests. */
    void clear() noexcept;

private:
    std::vector<TerrainChunkRequest> requests_;
};
} // namespace full_engine
