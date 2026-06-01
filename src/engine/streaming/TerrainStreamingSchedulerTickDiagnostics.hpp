#pragma once

#include "engine/renderer_integration/TerrainManifestAssetReadiness.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadJobCoordinator.hpp"
#include "engine/streaming/TerrainStreamingSchedulerTick.hpp"

namespace full_engine
{
/**
 * @brief Compact value snapshot of one policy-driven streaming scheduler tick.
 *
 * The snapshot copies statuses and summary counters only. It does not retain
 * job records, load records, readiness records, manifests, world descriptors,
 * renderer handles, renderer resources, snapshots, callbacks, or runtime
 * owners. It is intended for sample/editor UI and tooling surfaces that do not
 * need the full scheduler tick result.
 */
struct TerrainStreamingSchedulerTickDiagnostics
{
    /** @brief Compact scheduler fields suitable for retained tick history. */
    TerrainStreamingTickSchedulerDiagnostics history = {};

    /** @brief High-level scheduler tick outcome. */
    TerrainStreamingSchedulerTickStatus status = TerrainStreamingSchedulerTickStatus::Idle;

    /** @brief Scheduler phase decision selected from current pressure. */
    TerrainStreamingSchedulerStatus decisionStatus = TerrainStreamingSchedulerStatus::Idle;

    /** @brief Primary reason for the scheduler decision. */
    TerrainStreamingSchedulerReason decisionReason = TerrainStreamingSchedulerReason::NoWork;

    /** @brief Budget profile selected for the streaming phase. */
    TerrainStreamingBudgetProfile budgetProfile = TerrainStreamingBudgetProfile::Conservative;

    /** @brief Pending load request count observed by the scheduler policy. */
    std::size_t pendingLoadRequestCount = 0;

    /** @brief Pending job count observed by the scheduler policy. */
    std::size_t pendingJobCount = 0;

    /** @brief Deferred work pressure observed by the scheduler policy. */
    std::size_t deferredWorkCount = 0;

    /** @brief Peak deferred work pressure observed by the scheduler policy. */
    std::size_t peakDeferredWorkCount = 0;

    /** @brief Runtime setup/residency backlog observed by the scheduler policy. */
    std::size_t runtimeBacklogCount = 0;

    /** @brief Aggregate scheduler pressure count. */
    std::size_t pressureCount = 0;

    /** @brief Maximum asset-load jobs selected for this tick. */
    std::size_t maxAssetLoadJobs = 0;

    /** @brief True when the load-job phase ran. */
    bool loadJobsRan = false;

    /** @brief True when the streaming phase ran. */
    bool streamingRan = false;

    /** @brief High-level load-job coordinator status from the tick. */
    TerrainManifestAssetLoadJobCoordinatorStatus loadJobStatus =
        TerrainManifestAssetLoadJobCoordinatorStatus::NoPendingLoads;

    /** @brief Counters from mirroring pending load requests into jobs. */
    TerrainManifestAssetLoadJobMirrorSummary loadJobMirror = {};

    /** @brief Counters from executing relevant manifest asset load jobs. */
    EngineJobExecutionSummary loadJobExecution = {};

    /** @brief Counters from consuming retained manifest asset load requests. */
    TerrainManifestAssetLoadSummary loadConsume = {};

    /** @brief True when retained load requests were fully consumed. */
    bool loadConsumed = false;

    /** @brief Compact load-job coordinator counters. */
    TerrainManifestAssetLoadJobCoordinatorSummary loadJobCoordinator = {};

    /** @brief Final asset-handle readiness counters from the load-job phase. */
    TerrainManifestAssetReadinessSummary loadReadiness = {};

    /** @brief High-level streaming-loop update status from the tick. */
    TerrainStreamingLoopUpdateStatus streamingStatus =
        TerrainStreamingLoopUpdateStatus::Success;

    /** @brief Manifest-aware streaming status from the streaming phase. */
    TerrainStreamingManifestUpdateStatus manifestStreamingStatus =
        TerrainStreamingManifestUpdateStatus::Success;

    /** @brief Terrain runtime update status from the streaming phase. */
    TerrainRuntimeUpdateStatus runtimeStatus = TerrainRuntimeUpdateStatus::Success;

    /** @brief True when the streaming phase applied runtime queues. */
    bool runtimeUpdateRan = false;

    /** @brief Setup request count after streaming coordination and before runtime application. */
    std::size_t setupRequestsBeforeRuntime = 0;

    /** @brief Residency request count after streaming coordination and before runtime application. */
    std::size_t residencyRequestsBeforeRuntime = 0;

    /** @brief Setup request count after optional runtime application. */
    std::size_t setupRequestsAfterRuntime = 0;

    /** @brief Residency request count after optional runtime application. */
    std::size_t residencyRequestsAfterRuntime = 0;

    /** @brief Compact manifest-aware streaming counters from the streaming phase. */
    TerrainStreamingManifestUpdateSummary streamingSummary = {};

    /** @brief Compact streaming queue counters from the streaming phase. */
    TerrainStreamingQueueSummary streamingQueue = {};
};

/**
 * @brief Builds compact diagnostics from a scheduler tick result.
 *
 * The returned value copies status and summary counters only. It does not
 * mutate the result, retained loop state, job queues, manifests, renderer
 * handle catalogs, terrain runtime state, or renderer resources.
 */
TerrainStreamingSchedulerTickDiagnostics makeTerrainStreamingSchedulerTickDiagnostics(
    const TerrainStreamingSchedulerTickResult& result);
} // namespace full_engine
