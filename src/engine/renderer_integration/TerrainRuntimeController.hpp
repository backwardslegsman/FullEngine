#pragma once

#include "engine/renderer_integration/TerrainChunkRequests.hpp"
#include "engine/renderer_integration/TerrainIntegrationDiagnostics.hpp"
#include "engine/renderer_integration/TerrainPipeline.hpp"
#include "engine/renderer_integration/TerrainRuntimeStateDiff.hpp"
#include "engine/world/WorldResidencyRequests.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace full_engine
{
/** @brief Options for one stateless terrain runtime update. */
struct TerrainRuntimeUpdateOptions
{
    TerrainPipelineRunOptions pipelineOptions = {};
};

/** @brief High-level result status for one terrain runtime update. */
enum class TerrainRuntimeUpdateStatus
{
    Success,
    SetupFailed,
    ResidencyFailed,
    PipelineFailed,
};

/** @brief Returns a stable diagnostic name for a terrain runtime update status. */
const char* terrainRuntimeUpdateStatusName(TerrainRuntimeUpdateStatus status) noexcept;

/**
 * @brief Result of applying terrain setup, residency, and renderer pipeline work.
 *
 * The result owns value snapshots of request application, pipeline output, and
 * diagnostics. It does not own registries, catalogs, queues, renderer handles,
 * mesh/material/texture resources, UI state, or sample state.
 */
struct TerrainRuntimeUpdateResult
{
    TerrainRuntimeUpdateStatus status = TerrainRuntimeUpdateStatus::Success;
    TerrainChunkRequestApplyResult setup = {};
    WorldChunkResidencyApplyResult residency = {};
    TerrainPipelineRunResult pipeline = {};
    TerrainIntegrationDiagnostics diagnostics = {};
};

/** @brief Number of recent terrain runtime update events kept in memory. */
constexpr std::size_t kTerrainRuntimeEventLogCapacity = 32;

/**
 * @brief Value snapshot of one terrain runtime update for diagnostics history.
 *
 * Events copy compact diagnostic counters only. They do not own request
 * records, descriptor vectors, renderer handles, resources, or frame data.
 */
struct TerrainRuntimeEvent
{
    std::uint64_t sequence = 0;
    TerrainRuntimeUpdateStatus status = TerrainRuntimeUpdateStatus::Success;
    TerrainIntegrationDiagnostics diagnostics = {};
};

/**
 * @brief Fixed-capacity chronological history of recent terrain runtime updates.
 *
 * The log is CPU-only and not thread-safe. Appending an update copies summary
 * diagnostics from the update result and assigns a monotonically increasing
 * sequence number local to the log.
 */
class TerrainRuntimeEventLog
{
public:
    /** @brief Appends a compact event copied from an update result. */
    void append(const TerrainRuntimeUpdateResult& update);

    /** @brief Returns recent events in oldest-to-newest order. */
    std::vector<TerrainRuntimeEvent> events() const;

    /** @brief Returns the number of events currently retained. */
    std::size_t eventCount() const noexcept;

    /** @brief Returns the newest retained event, or null when the log is empty. */
    const TerrainRuntimeEvent* latestEvent() const noexcept;

    /** @brief Clears retained events and restarts local event sequencing. */
    void clear() noexcept;

private:
    std::array<TerrainRuntimeEvent, kTerrainRuntimeEventLogCapacity> events_ = {};
    std::size_t nextIndex_ = 0;
    std::size_t count_ = 0;
    std::uint64_t nextSequence_ = 1;
};

/**
 * @brief Applies queued terrain runtime intent and runs the terrain pipeline.
 *
 * Setup requests are applied before residency requests. Residency requests are
 * applied only for chunks that remain registered in world and terrain resource
 * state. Both queues are cleared after their requests have been captured. The
 * renderer is touched only through `runTerrainPipeline`.
 */
TerrainRuntimeUpdateResult updateTerrainRuntime(
    full_renderer::IRenderer& renderer,
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resources,
    ChunkTerrainHandleMap& handles,
    TerrainChunkRequestQueue& setupRequests,
    WorldChunkResidencyRequestQueue& residencyRequests,
    const TerrainRuntimeUpdateOptions& options = {});

/**
 * @brief Caller-facing terrain runtime state for queued intent and latest results.
 *
 * The state owns only terrain setup/residency intent queues, the latest update
 * result, recent events, and optional snapshot tracking. It does not own world
 * registries, terrain resources, renderer handles, renderer resources, UI
 * state, or sample chunk mirrors.
 */
class TerrainRuntimeState
{
public:
    /** @brief Queues terrain setup creation intent for a chunk. */
    void queueSetupAdd(const WorldChunkDesc& worldDesc, const TerrainChunkResourceDesc& resourceDesc);

    /** @brief Queues terrain setup removal intent for a chunk. */
    void queueSetupRemove(const ChunkId& id);

    /** @brief Queues a request to drive a registered chunk to resident. */
    void queueMakeResident(const ChunkId& id);

    /** @brief Queues a request to drive a registered chunk to unloaded. */
    void queueMakeUnloaded(const ChunkId& id);

    /** @brief Returns the number of queued setup requests. */
    std::size_t setupRequestCount() const noexcept;

    /** @brief Returns the number of queued residency requests. */
    std::size_t residencyRequestCount() const noexcept;

    /** @brief Returns whether any setup or residency requests are pending. */
    bool hasPendingRequests() const noexcept;

    /** @brief Returns the latest stored runtime update result. */
    const TerrainRuntimeUpdateResult& latestUpdate() const noexcept;

    /** @brief Returns diagnostics from the latest stored runtime update result. */
    const TerrainIntegrationDiagnostics& latestDiagnostics() const noexcept;

    /** @brief Returns the retained runtime update event log. */
    const TerrainRuntimeEventLog& eventLog() const noexcept;

    /** @brief Returns the number of retained runtime update events. */
    std::size_t eventCount() const noexcept;

    /** @brief Returns retained runtime update events in oldest-to-newest order. */
    std::vector<TerrainRuntimeEvent> events() const;

    /** @brief Returns the newest retained runtime event, or null when none exist. */
    const TerrainRuntimeEvent* latestEvent() const noexcept;

    /** @brief Returns whether snapshot tracking has captured at least one snapshot. */
    bool hasLatestSnapshot() const noexcept;

    /** @brief Returns the latest tracked terrain runtime state snapshot. */
    const TerrainRuntimeStateSnapshot& latestSnapshot() const noexcept;

    /** @brief Returns the latest diff between tracked terrain runtime snapshots. */
    const TerrainRuntimeStateDiff& latestSnapshotDiff() const noexcept;

    /** @brief Clears retained runtime events without erasing the latest update. */
    void clearEvents() noexcept;

    /** @brief Clears tracked snapshots without erasing queues, events, or the latest update. */
    void clearSnapshotTracking() noexcept;

    /** @brief Clears pending setup and residency requests without erasing the latest result. */
    void clearRequests() noexcept;

    /** @brief Applies owned queues and stores the latest terrain runtime update result. */
    const TerrainRuntimeUpdateResult& update(
        full_renderer::IRenderer& renderer,
        WorldChunkRegistry& registry,
        WorldChunkCatalog& worldCatalog,
        TerrainResourceCatalog& resources,
        ChunkTerrainHandleMap& handles,
        const TerrainRuntimeUpdateOptions& options = {});

    /**
     * @brief Applies owned queues, then captures and diffs tracked chunk state.
     *
     * This opt-in update path delegates to `update` first, so queue handling,
     * renderer submission, latest-update storage, and event logging are
     * identical to the normal runtime update. Afterward it snapshots the
     * supplied caller-owned chunk IDs and stores a diff against the previous
     * tracked snapshot, or against an empty snapshot on first capture. The
     * tracked ID array is read only for the duration of the call; snapshot and
     * diff results are copied into the state object before returning.
     */
    const TerrainRuntimeUpdateResult& updateWithSnapshot(
        full_renderer::IRenderer& renderer,
        WorldChunkRegistry& registry,
        WorldChunkCatalog& worldCatalog,
        TerrainResourceCatalog& resources,
        ChunkTerrainHandleMap& handles,
        const ChunkId* trackedIds,
        std::size_t trackedIdCount,
        const TerrainRuntimeUpdateOptions& options = {});

private:
    TerrainChunkRequestQueue setupRequests_;
    WorldChunkResidencyRequestQueue residencyRequests_;
    TerrainRuntimeUpdateResult latestUpdate_ = {};
    TerrainRuntimeEventLog eventLog_;
    TerrainRuntimeStateSnapshot latestSnapshot_ = {};
    TerrainRuntimeStateDiff latestSnapshotDiff_ = {};
    bool hasLatestSnapshot_ = false;
};
} // namespace full_engine
