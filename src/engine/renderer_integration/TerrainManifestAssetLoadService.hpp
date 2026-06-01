#pragma once

#include "engine/renderer_integration/TerrainManifestAssetLoadJobCompletions.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadJobWorkPackets.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Retained progress state for one manifest asset-load work item. */
enum class TerrainManifestAssetLoadServiceRequestStatus
{
    /** @brief The request is waiting for a caller-owned load callback to satisfy it. */
    Pending,

    /** @brief The request produced a completion value. */
    Completed,

    /** @brief The request failed and will not be retried until explicitly reset. */
    Failed,
};

/** @brief Outcome for adding one work packet to the retained load service. */
enum class TerrainManifestAssetLoadServiceEnqueueStatus
{
    /** @brief The work packet was valid and retained by the service. */
    Queued,

    /** @brief The same asset ID and kind are already retained by the service. */
    AlreadyQueued,

    /** @brief The work packet has invalid identity, kind, or job payload convention. */
    InvalidPacket,
};

/** @brief High-level status for one synchronous service tick. */
enum class TerrainManifestAssetLoadServiceTickStatus
{
    /** @brief No pending work was attempted. */
    Idle,

    /** @brief At least one pending request was attempted. */
    Progressed,

    /** @brief Pending work exists, but the tick could not invoke a callback. */
    Blocked,
};

/** @brief Returns a stable diagnostic name for a retained service request status. */
const char* terrainManifestAssetLoadServiceRequestStatusName(
    TerrainManifestAssetLoadServiceRequestStatus status) noexcept;

/** @brief Returns a stable diagnostic name for a service enqueue status. */
const char* terrainManifestAssetLoadServiceEnqueueStatusName(
    TerrainManifestAssetLoadServiceEnqueueStatus status) noexcept;

/** @brief Returns a stable diagnostic name for a service tick status. */
const char* terrainManifestAssetLoadServiceTickStatusName(
    TerrainManifestAssetLoadServiceTickStatus status) noexcept;

/** @brief Retained service record for one copied manifest asset-load work packet. */
struct TerrainManifestAssetLoadServiceRecord
{
    /** @brief Copied work packet that produced this retained request. */
    TerrainManifestAssetLoadJobWorkPacket packet = {};

    /** @brief Current retained request progress. */
    TerrainManifestAssetLoadServiceRequestStatus status =
        TerrainManifestAssetLoadServiceRequestStatus::Pending;

    /** @brief Number of callback attempts made for this request. */
    std::size_t attemptCount = 0;

    /** @brief Most recent callback result, or default when never attempted. */
    TerrainManifestAssetLoadCallbackResult lastCallback = {};
};

/** @brief Ordered diagnostic record for retaining one work packet. */
struct TerrainManifestAssetLoadServiceEnqueueRecord
{
    /** @brief Source packet supplied by the caller. */
    TerrainManifestAssetLoadJobWorkPacket packet = {};

    /** @brief Retention outcome for this packet. */
    TerrainManifestAssetLoadServiceEnqueueStatus status =
        TerrainManifestAssetLoadServiceEnqueueStatus::InvalidPacket;
};

/** @brief Aggregate counters for retaining work packets. */
struct TerrainManifestAssetLoadServiceEnqueueSummary
{
    /** @brief Number of packets retained as new work. */
    std::size_t queuedCount = 0;

    /** @brief Number of packets skipped because equivalent work was already retained. */
    std::size_t alreadyQueuedCount = 0;

    /** @brief Number of packets rejected for invalid payloads. */
    std::size_t invalidPacketCount = 0;
};

/** @brief Ordered result for adding work packets to the retained load service. */
struct TerrainManifestAssetLoadServiceEnqueueResult
{
    /** @brief One record per supplied packet when an array is provided. */
    std::vector<TerrainManifestAssetLoadServiceEnqueueRecord> records;

    /** @brief Aggregate enqueue counters. */
    TerrainManifestAssetLoadServiceEnqueueSummary summary = {};
};

/** @brief Ordered diagnostic record for one attempted service load callback. */
struct TerrainManifestAssetLoadServiceTickRecord
{
    /** @brief Request attempted during this tick. */
    TerrainManifestAssetLoadRequest request = {};

    /** @brief Callback output returned by the caller. */
    TerrainManifestAssetLoadCallbackResult callback = {};

    /** @brief Retained status after processing the callback output. */
    TerrainManifestAssetLoadServiceRequestStatus status =
        TerrainManifestAssetLoadServiceRequestStatus::Pending;
};

/** @brief Aggregate counters for one service tick. */
struct TerrainManifestAssetLoadServiceTickSummary
{
    /** @brief Number of pending requests attempted this tick. */
    std::size_t attemptedCount = 0;

    /** @brief Number of attempts that produced valid loaded outputs. */
    std::size_t loadedCount = 0;

    /** @brief Number of attempts that reported the asset was not available yet. */
    std::size_t missingCount = 0;

    /** @brief Number of attempts that failed or supplied an invalid loaded handle. */
    std::size_t failedCount = 0;

    /** @brief Total retained requests currently completed after this tick. */
    std::size_t completedCount = 0;

    /** @brief Total retained requests currently pending after this tick. */
    std::size_t pendingCount = 0;

    /** @brief Total retained requests currently failed after this tick. */
    std::size_t retainedFailedCount = 0;
};

/** @brief Result of advancing retained manifest asset-load work. */
struct TerrainManifestAssetLoadServiceTickResult
{
    /** @brief High-level tick outcome. */
    TerrainManifestAssetLoadServiceTickStatus status =
        TerrainManifestAssetLoadServiceTickStatus::Idle;

    /** @brief One diagnostic record per attempted pending request, in service order. */
    std::vector<TerrainManifestAssetLoadServiceTickRecord> records;

    /** @brief Aggregate tick counters. */
    TerrainManifestAssetLoadServiceTickSummary summary = {};
};

/**
 * @brief Retained synchronous service for manifest asset-load work packets.
 *
 * The service owns copied work packets, progress counters, and completion
 * values produced by caller-owned load callbacks. It is the stateful bridge
 * between scheduled `ManifestAssetLoad` jobs and
 * `TerrainManifestAssetLoadJobCompletion` records that can be published and
 * reconciled by existing helpers.
 *
 * The service performs no file IO, starts no threads, owns no futures, creates
 * no renderer resources, calls no renderer APIs, mutates no job queues,
 * consumes no retained manifest load requests, and touches no renderer handle
 * catalogs. Callback code may do external work, but ownership of that work and
 * any produced renderer resources remains with the caller.
 *
 * @note Thread safety: Not thread-safe. Callers must serialize access.
 */
class TerrainManifestAssetLoadService
{
public:
    /**
     * @brief Retains valid work packets for later ticking.
     *
     * Packets are copied by value and deduplicated by `(AssetId, AssetKind)`.
     * A null packet pointer is accepted only when `packetCount` is zero; a null
     * pointer with nonzero count reports invalid packets and mutates nothing.
     */
    TerrainManifestAssetLoadServiceEnqueueResult enqueueWorkPackets(
        const TerrainManifestAssetLoadJobWorkPacket* packets,
        std::size_t packetCount);

    /**
     * @brief Retains valid packets from a packet conversion result.
     *
     * Only the result's `packets` array is consumed. Diagnostic records from
     * packet conversion are not retained.
     */
    TerrainManifestAssetLoadServiceEnqueueResult enqueueWorkPackets(
        const TerrainManifestAssetLoadJobWorkPacketResult& packets);

    /**
     * @brief Advances pending work through a caller-owned synchronous callback.
     *
     * At most `maxLoads` pending records are attempted in retained order.
     * `Loaded` results with a valid active handle produce completion records
     * and mark work completed. `Missing` leaves work pending. `Failed`, and
     * `Loaded` results with invalid active handles, mark work failed and emit a
     * failed completion value for diagnostics/reconcile blocking.
     *
     * @param maxLoads Maximum pending requests to attempt this call.
     * @param callback Caller-owned loader callback. Required when `maxLoads`
     * is nonzero and pending work exists.
     * @param userData Opaque caller data forwarded to `callback`.
     * @return Ordered tick diagnostics and current retained counters.
     */
    TerrainManifestAssetLoadServiceTickResult tick(
        std::size_t maxLoads,
        TerrainManifestAssetLoadCallback callback,
        void* userData = nullptr);

    /** @brief Moves all failed retained records back to pending for retry. */
    std::size_t retryFailed() noexcept;

    /** @brief Clears emitted completion values without removing retained work records. */
    void clearCompletions() noexcept;

    /** @brief Removes all retained work, completion values, and diagnostics state. */
    void clear() noexcept;

    /** @brief Returns retained work records in first-queued order. */
    const std::vector<TerrainManifestAssetLoadServiceRecord>& records() const noexcept;

    /** @brief Returns emitted completion values in callback completion order. */
    const std::vector<TerrainManifestAssetLoadJobCompletion>& completions() const noexcept;

    /** @brief Returns the number of retained work records. */
    std::size_t requestCount() const noexcept;

    /** @brief Returns the number of retained pending records. */
    std::size_t pendingCount() const noexcept;

    /** @brief Returns the number of retained completed records. */
    std::size_t completedCount() const noexcept;

    /** @brief Returns the number of retained failed records. */
    std::size_t failedCount() const noexcept;

    /** @brief Returns whether equivalent work is already retained. */
    bool contains(AssetId id, AssetKind kind) const noexcept;

private:
    std::vector<TerrainManifestAssetLoadServiceRecord> records_;
    std::vector<TerrainManifestAssetLoadJobCompletion> completions_;
};
} // namespace full_engine
