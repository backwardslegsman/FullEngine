#pragma once

#include "engine/renderer_integration/TerrainManifestAssetLoadPlan.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Result for queueing one manifest asset load request. */
enum class TerrainManifestAssetLoadQueueResult
{
    /** @brief Request was valid and appended to the pending queue. */
    Queued,

    /** @brief A request with the same asset ID and kind is already pending. */
    AlreadyQueued,

    /** @brief Asset ID or asset kind is not valid for this queue. */
    InvalidArgument,
};

/** @brief Ordered diagnostic record for one load request queue push. */
struct TerrainManifestAssetLoadQueueRecord
{
    /** @brief Original request supplied by the caller. */
    TerrainManifestAssetLoadRequest request = {};

    /** @brief Queueing outcome for the request. */
    TerrainManifestAssetLoadQueueResult result = TerrainManifestAssetLoadQueueResult::InvalidArgument;
};

/** @brief Summary counters for queueing manifest asset load requests. */
struct TerrainManifestAssetLoadQueuePushSummary
{
    /** @brief Number of requests appended to the pending queue. */
    std::size_t queuedCount = 0;

    /** @brief Number of requests skipped because they were already pending. */
    std::size_t alreadyQueuedCount = 0;

    /** @brief Number of requests rejected because their asset ID or kind was invalid. */
    std::size_t invalidArgumentCount = 0;
};

/** @brief Ordered result of queueing one or more manifest asset load requests. */
struct TerrainManifestAssetLoadQueuePushResult
{
    /** @brief One record per input request in input order. */
    std::vector<TerrainManifestAssetLoadQueueRecord> records;

    /** @brief Aggregate queueing counters for the push operation. */
    TerrainManifestAssetLoadQueuePushSummary summary = {};
};

/**
 * @brief CPU-only queue of pending manifest asset load intent.
 *
 * The queue owns renderer-free asset ID/kind requests only. It performs no file
 * IO, starts no jobs, creates no renderer resources, and does not mutate
 * manifests, renderer handle catalogs, runtime state, or renderer objects.
 * Pending requests are deduplicated by `(AssetId, AssetKind)`.
 *
 * @note Thread safety: Not thread-safe. Callers must serialize mutation and
 * access.
 */
class TerrainManifestAssetLoadRequestQueue
{
public:
    /**
     * @brief Queues one request when valid and not already pending.
     *
     * @param request Renderer-free load intent. Mesh, Material, and Texture
     * kinds with non-default asset IDs are accepted.
     * @return Ordered diagnostic record for this single push.
     */
    TerrainManifestAssetLoadQueueRecord push(const TerrainManifestAssetLoadRequest& request);

    /**
     * @brief Queues all requests from a load request plan in plan order.
     *
     * @param plan Value plan to copy requests from. The plan is not retained.
     * @return Per-request queueing diagnostics plus aggregate counters.
     */
    TerrainManifestAssetLoadQueuePushResult pushPlan(const TerrainManifestAssetLoadRequestPlan& plan);

    /**
     * @brief Returns whether the same asset ID and kind is already pending.
     *
     * Invalid IDs or unsupported kinds simply return false.
     */
    bool contains(AssetId id, AssetKind kind) const noexcept;

    /** @brief Returns the number of pending load requests. */
    std::size_t requestCount() const noexcept;

    /**
     * @brief Returns pending load requests in first-queued order.
     *
     * The returned reference is invalidated by any later non-const queue call.
     */
    const std::vector<TerrainManifestAssetLoadRequest>& requests() const noexcept;

    /** @brief Returns counters for the current pending request set. */
    TerrainManifestAssetLoadRequestSummary summary() const noexcept;

    /** @brief Removes all pending load requests without touching any handle catalog. */
    void clear() noexcept;

private:
    std::vector<TerrainManifestAssetLoadRequest> requests_;
};
} // namespace full_engine
