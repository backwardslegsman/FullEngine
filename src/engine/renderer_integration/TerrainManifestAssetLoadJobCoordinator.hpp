#pragma once

#include "engine/jobs/JobQueue.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadExecutor.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadJobs.hpp"

namespace full_engine
{
class TerrainManifestLoadState;

/** @brief High-level result for one manifest asset load job coordination pass. */
enum class TerrainManifestAssetLoadJobCoordinatorStatus
{
    /** @brief Jobs ran, retained load requests were consumed, and readiness was replanned. */
    Success,

    /** @brief The retained manifest load state had no pending asset load requests. */
    NoPendingLoads,

    /** @brief One or more manifest asset load jobs were blocked, failed, or left pending. */
    JobsBlocked,

    /** @brief Jobs completed but retained load requests could not be consumed. */
    LoadConsumeBlocked,
};

/** @brief Returns a stable diagnostic name for a manifest asset load job coordinator status. */
const char* terrainManifestAssetLoadJobCoordinatorStatusName(
    TerrainManifestAssetLoadJobCoordinatorStatus status) noexcept;

/** @brief Compact counters copied from one manifest asset load job coordination pass. */
struct TerrainManifestAssetLoadJobCoordinatorSummary
{
    /** @brief Pending retained load request count before coordination. */
    std::size_t initialPendingLoadRequestCount = 0;

    /** @brief Pending retained load request count after coordination. */
    std::size_t finalPendingLoadRequestCount = 0;

    /** @brief Final ready handle count after a successful consume/replan pass. */
    std::size_t finalReadyHandleCount = 0;

    /** @brief Final missing handle count after a successful consume/replan pass. */
    std::size_t finalMissingHandleCount = 0;
};

/**
 * @brief Result of coordinating pending manifest asset load requests with jobs.
 *
 * The result copies diagnostics from request-to-job mirroring, filtered job
 * execution, retained load-queue consumption, and final readiness replanning.
 */
struct TerrainManifestAssetLoadJobCoordinatorResult
{
    /** @brief High-level coordination outcome. */
    TerrainManifestAssetLoadJobCoordinatorStatus status =
        TerrainManifestAssetLoadJobCoordinatorStatus::NoPendingLoads;

    /** @brief Diagnostics from mirroring retained load requests into the job queue. */
    TerrainManifestAssetLoadJobMirrorResult mirror = {};

    /** @brief Diagnostics from executing relevant manifest asset load jobs. */
    EngineJobExecutionResult jobs = {};

    /** @brief Diagnostics from consuming retained load requests through the load state. */
    TerrainManifestAssetLoadExecutorResult load = {};

    /** @brief Final readiness plan after successful load consumption. */
    TerrainManifestAssetReadinessPlan readiness = {};

    /** @brief Compact aggregate counters for UI or logs. */
    TerrainManifestAssetLoadJobCoordinatorSummary summary = {};
};

/**
 * @brief Runs one single-threaded manifest asset load job coordination pass.
 *
 * The helper mirrors retained pending load requests into `jobs`, executes only
 * matching `ManifestAssetLoad` jobs through the caller callback, consumes the
 * retained load queue through `manifestLoad`, and replans readiness when the
 * whole batch succeeds. It does not perform file IO, create threads, call
 * renderer APIs, create renderer resources, stage terrain setup, or apply
 * terrain runtime queues. The callback remains responsible for any external
 * loading or renderer-resource creation policy.
 *
 * Unrelated jobs in `jobs` are not executed or removed. Mirrored manifest load
 * jobs are removed from `jobs` only after the retained load queue is consumed.
 * The callback can be invoked during job execution and again during retained
 * load-queue consumption, so it should return stable handles for a request
 * within one coordination pass. Future async loaders can replace this
 * synchronous callback boundary without changing terrain runtime ownership.
 *
 * @param manifestLoad Retained manifest load state owning pending load intent.
 * @param jobs Caller-owned generic job queue used for scheduling intent.
 * @param destinationHandles Runtime handle catalog to receive loaded handles.
 * @param callback Caller-owned synchronous handle provider.
 * @param userData Opaque pointer forwarded to `callback`.
 * @param maxJobs Maximum relevant manifest load jobs to execute this pass.
 * @param priority Priority assigned to newly mirrored load jobs.
 * @return Value diagnostics for the coordination pass.
 *
 * @note Thread safety: Not thread-safe for shared state, queue, or catalog
 * instances. Callers must serialize access.
 */
TerrainManifestAssetLoadJobCoordinatorResult runTerrainManifestAssetLoadJobs(
    TerrainManifestLoadState& manifestLoad,
    EngineJobQueue& jobs,
    RendererAssetHandleCatalog& destinationHandles,
    TerrainManifestAssetLoadCallback callback,
    void* userData,
    std::size_t maxJobs,
    EngineJobPriority priority = EngineJobPriority::Normal);
} // namespace full_engine
