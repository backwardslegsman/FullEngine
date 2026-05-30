#pragma once

#include "engine/world/WorldChunkRegistry.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Engine-owned request type for changing chunk residency intent. */
enum class WorldChunkResidencyRequestType
{
    MakeResident,
    MakeUnloaded,
};

/** @brief One ordered request to move a registered chunk toward a residency state. */
struct WorldChunkResidencyRequest
{
    ChunkId id = {};
    WorldChunkResidencyRequestType type = WorldChunkResidencyRequestType::MakeResident;
};

/** @brief Per-request result after applying a residency request queue. */
enum class WorldChunkResidencyRequestStatus
{
    Applied,
    AlreadySatisfied,
    NotFound,
    InvalidTransition,
};

/** @brief Ordered diagnostic record for one applied residency request. */
struct WorldChunkResidencyRequestRecord
{
    WorldChunkResidencyRequest request = {};
    WorldChunkResidencyRequestStatus status = WorldChunkResidencyRequestStatus::NotFound;
    ChunkResidencyState finalState = ChunkResidencyState::Unloaded;
};

/** @brief Summary counters for a residency request queue application. */
struct WorldChunkResidencyApplySummary
{
    std::size_t appliedCount = 0;
    std::size_t alreadySatisfiedCount = 0;
    std::size_t notFoundCount = 0;
    std::size_t invalidTransitionCount = 0;
};

/** @brief Ordered result of applying queued residency requests to a registry. */
struct WorldChunkResidencyApplyResult
{
    std::vector<WorldChunkResidencyRequestRecord> records;
    WorldChunkResidencyApplySummary summary;
};

/**
 * @brief CPU-only FIFO queue of engine chunk residency intent.
 *
 * The queue owns only requested residency changes. It does not own chunk
 * descriptors, renderer handles, async work, terrain resources, or IO. Applying
 * the queue mutates only the supplied `WorldChunkRegistry` and preserves one
 * diagnostic record per queued request.
 */
class WorldChunkResidencyRequestQueue
{
public:
    /** @brief Appends a residency request to the queue. */
    void push(const WorldChunkResidencyRequest& request);

    /** @brief Appends a residency request for the supplied chunk and type. */
    void push(const ChunkId& id, WorldChunkResidencyRequestType type);

    /** @brief Returns the number of queued requests. */
    std::size_t requestCount() const noexcept;

    /** @brief Returns queued requests in FIFO order. */
    const std::vector<WorldChunkResidencyRequest>& requests() const noexcept;

    /** @brief Applies queued requests to the registry without clearing the queue. */
    WorldChunkResidencyApplyResult applyTo(WorldChunkRegistry& registry) const;

    /** @brief Removes all queued requests. */
    void clear() noexcept;

private:
    std::vector<WorldChunkResidencyRequest> requests_;
};
} // namespace full_engine
