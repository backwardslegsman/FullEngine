#pragma once

#include "engine/jobs/JobQueue.hpp"
#include "engine/renderer_integration/TerrainAssetResolver.hpp"
#include "engine/renderer_integration/TerrainChunkRequests.hpp"
#include "engine/renderer_integration/TerrainDescriptorBuilder.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadJobCoordinator.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadService.hpp"
#include "engine/renderer_integration/TerrainManifestRuntimeStaging.hpp"
#include "engine/renderer_integration/TerrainRenderPrep.hpp"
#include "engine/renderer_integration/TerrainRendererCommands.hpp"
#include "engine/renderer_integration/TerrainSubmissionAdapter.hpp"
#include "engine/renderer_integration/WorldRenderSnapshot.hpp"
#include "engine/world/WorldResidencyRequests.hpp"

#include <cstddef>

namespace full_engine
{
/** @brief Value snapshot of a generic engine job queue. */
struct EngineJobQueueDiagnostics
{
    /** @brief Number of pending jobs retained by the queue. */
    std::size_t pendingJobCount = 0;

    /** @brief Pending queue summary counters copied from the queue. */
    EngineJobQueueSummary summary = {};
};

/** @brief Value snapshot of manifest asset-load job coordination. */
struct TerrainManifestAssetLoadJobDiagnostics
{
    /** @brief High-level coordinator status from the latest run. */
    TerrainManifestAssetLoadJobCoordinatorStatus status =
        TerrainManifestAssetLoadJobCoordinatorStatus::NoPendingLoads;

    /** @brief Current generic job queue counters after the latest run. */
    EngineJobQueueDiagnostics jobQueue = {};

    /** @brief Counters from mirroring pending load requests into jobs. */
    TerrainManifestAssetLoadJobMirrorSummary mirror = {};

    /** @brief Counters from executing relevant manifest asset load jobs. */
    EngineJobExecutionSummary execution = {};

    /** @brief Counters from consuming retained manifest asset load requests. */
    TerrainManifestAssetLoadSummary loadConsume = {};

    /** @brief True when retained load requests were fully consumed. */
    bool loadConsumed = false;

    /** @brief Compact coordinator counters such as final pending and readiness totals. */
    TerrainManifestAssetLoadJobCoordinatorSummary coordinator = {};

    /** @brief Final manifest asset handle readiness counters after successful replanning. */
    TerrainManifestAssetReadinessSummary readiness = {};
};

/** @brief Value snapshot of schedule-only manifest asset-load job mirroring. */
struct TerrainManifestAssetLoadJobScheduleDiagnostics
{
    /** @brief High-level schedule-only status. */
    TerrainManifestAssetLoadJobScheduleStatus status =
        TerrainManifestAssetLoadJobScheduleStatus::NoPendingLoads;

    /** @brief Current generic job queue counters after scheduling. */
    EngineJobQueueDiagnostics jobQueue = {};

    /** @brief Counters from mirroring pending load requests into jobs. */
    TerrainManifestAssetLoadJobMirrorSummary mirror = {};

    /** @brief Pending retained load request count before scheduling. */
    std::size_t initialPendingLoadRequestCount = 0;

    /** @brief Pending retained load request count after scheduling. */
    std::size_t finalPendingLoadRequestCount = 0;

    /** @brief Pending generic job count reported by the schedule-only result. */
    std::size_t pendingJobCount = 0;
};

/** @brief Value snapshot of external manifest asset-load job reconciliation. */
struct TerrainManifestAssetLoadJobReconcileDiagnostics
{
    /** @brief High-level reconcile status. */
    TerrainManifestAssetLoadJobReconcileStatus status =
        TerrainManifestAssetLoadJobReconcileStatus::NoPendingLoads;

    /** @brief Current generic job queue counters after reconcile. */
    EngineJobQueueDiagnostics jobQueue = {};

    /** @brief Counters from consuming retained manifest asset load requests. */
    TerrainManifestAssetLoadSummary loadConsume = {};

    /** @brief True when retained load requests were fully consumed. */
    bool loadConsumed = false;

    /** @brief Compact reconcile counters such as pending jobs and final readiness totals. */
    TerrainManifestAssetLoadJobReconcileSummary reconcile = {};

    /** @brief Final manifest asset handle readiness counters after successful replanning. */
    TerrainManifestAssetReadinessSummary readiness = {};
};

/** @brief Value snapshot of retained manifest asset-load service progress. */
struct TerrainManifestAssetLoadServiceDiagnostics
{
    /** @brief Counters from converting scheduled jobs into load-service work packets. */
    TerrainManifestAssetLoadJobWorkPacketSummary workPackets = {};

    /** @brief Counters from retaining converted packets in the service. */
    TerrainManifestAssetLoadServiceEnqueueSummary enqueue = {};

    /** @brief High-level status from the latest retained service tick. */
    TerrainManifestAssetLoadServiceTickStatus tickStatus =
        TerrainManifestAssetLoadServiceTickStatus::Idle;

    /** @brief Counters from the latest retained service tick. */
    TerrainManifestAssetLoadServiceTickSummary tick = {};

    /** @brief Number of work records retained by the service. */
    std::size_t retainedRequestCount = 0;

    /** @brief Number of retained service records still pending. */
    std::size_t retainedPendingCount = 0;

    /** @brief Number of retained service records completed. */
    std::size_t retainedCompletedCount = 0;

    /** @brief Number of retained service records failed. */
    std::size_t retainedFailedCount = 0;

    /** @brief Number of emitted completion values waiting for reconcile. */
    std::size_t retainedCompletionCount = 0;

    /** @brief High-level status from the latest service completion reconcile. */
    TerrainManifestAssetLoadJobCompletionReconcileStatus completionReconcileStatus =
        TerrainManifestAssetLoadJobCompletionReconcileStatus::NoPendingLoads;

    /** @brief Counters from publishing emitted service completion values. */
    TerrainManifestAssetLoadJobCompletionSummary completionPublish = {};

    /** @brief Counters from consuming retained load requests during completion reconcile. */
    TerrainManifestAssetLoadSummary completionLoadConsume = {};

    /** @brief True when service completion reconcile consumed retained load requests. */
    bool completionLoadConsumed = false;

    /** @brief Compact nested reconcile counters after publishing service completions. */
    TerrainManifestAssetLoadJobReconcileSummary completionReconcile = {};

    /** @brief Final readiness counters after successful service completion reconcile. */
    TerrainManifestAssetReadinessSummary completionReadiness = {};
};

/** @brief Value snapshot of the most recent terrain asset batch resolution. */
struct TerrainAssetBatchResolveDiagnostics
{
    std::size_t requestCount = 0;
    TerrainAssetBatchResolveSummary summary = {};
};

/** @brief Value snapshot of the most recent manifest-to-runtime staging result. */
struct TerrainManifestRuntimeStageDiagnostics
{
    TerrainManifestRuntimeStageStatus status = TerrainManifestRuntimeStageStatus::Success;
    std::size_t manifestAssetCount = 0;
    std::size_t manifestTerrainChunkCount = 0;
    std::size_t resolvedResourceCount = 0;
    std::size_t missingWorldDescCount = 0;
    std::size_t desiredSetupCount = 0;
    TerrainAssetBatchResolveSummary assetResolve = {};
    TerrainSetupStageSummary stage = {};
    TerrainSetupStageQueueSummary queue = {};
};

/** @brief Value snapshot of the most recently applied terrain setup request batch. */
struct TerrainSetupRequestDiagnostics
{
    std::size_t requestCount = 0;
    std::size_t addCount = 0;
    std::size_t removeCount = 0;
    TerrainChunkRequestApplySummary summary = {};
};

/** @brief Value snapshot of the most recently applied terrain residency request batch. */
struct TerrainResidencyRequestDiagnostics
{
    std::size_t requestCount = 0;
    std::size_t makeResidentCount = 0;
    std::size_t makeUnloadedCount = 0;
    WorldChunkResidencyApplySummary summary = {};
};

/** @brief Value snapshot of terrain integration counters produced by one pipeline run. */
struct TerrainPipelineDiagnostics
{
    std::size_t handleCount = 0;
    std::size_t snapshotReadyCount = 0;
    std::size_t snapshotNotResidentCount = 0;
    std::size_t snapshotMissingChunkCount = 0;
    std::size_t snapshotInvalidBoundsCount = 0;
    std::size_t snapshotOutOfRangeCount = 0;
    std::size_t snapshotInvalidInputCount = 0;
    TerrainRenderPrepSummary prep = {};
    TerrainLifecyclePlanSummary lifecycle = {};
    TerrainRendererCommandSummary commands = {};
    TerrainDescriptorBuildSummary descriptors = {};
    TerrainSubmissionSummary submission = {};
};

/** @brief Aggregated terrain integration diagnostics suitable for debug UI display. */
struct TerrainIntegrationDiagnostics
{
    TerrainSetupRequestDiagnostics setupRequests = {};
    TerrainResidencyRequestDiagnostics residencyRequests = {};
    TerrainPipelineDiagnostics pipeline = {};
};

/**
 * @brief Builds reusable diagnostics from a generic engine job queue.
 *
 * The returned value copies counters only. It does not retain references to job
 * records and does not mutate the queue.
 */
EngineJobQueueDiagnostics makeEngineJobQueueDiagnostics(const EngineJobQueue& queue);

/**
 * @brief Builds reusable diagnostics from manifest asset-load job coordination.
 *
 * The returned value copies counters only. It does not retain references to job
 * records, load records, readiness records, renderer handles, manifests, or
 * resource descriptors. The job queue is inspected for current pending counts
 * and is not mutated.
 */
TerrainManifestAssetLoadJobDiagnostics makeTerrainManifestAssetLoadJobDiagnostics(
    const TerrainManifestAssetLoadJobCoordinatorResult& result,
    const EngineJobQueue& jobs);

/**
 * @brief Builds reusable diagnostics from schedule-only manifest asset-load job mirroring.
 *
 * The returned value copies counters only. It does not retain references to
 * job records, manifests, load requests, renderer handles, or resource
 * descriptors. The job queue is inspected for current pending counts and is
 * not mutated.
 */
TerrainManifestAssetLoadJobScheduleDiagnostics makeTerrainManifestAssetLoadJobScheduleDiagnostics(
    const TerrainManifestAssetLoadJobScheduleResult& result,
    const EngineJobQueue& jobs);

/**
 * @brief Builds reusable diagnostics from external manifest asset-load job reconciliation.
 *
 * The returned value copies counters only. It does not retain references to
 * job records, manifests, load requests, renderer handles, or readiness
 * records. The job queue is inspected for current pending counts and is not
 * mutated.
 */
TerrainManifestAssetLoadJobReconcileDiagnostics makeTerrainManifestAssetLoadJobReconcileDiagnostics(
    const TerrainManifestAssetLoadJobReconcileResult& result,
    const EngineJobQueue& jobs);

/**
 * @brief Builds reusable diagnostics from retained manifest asset-load service state.
 *
 * The returned value copies counters and statuses only. It does not retain
 * work records, completion records, manifests, job records, renderer handles,
 * descriptors, catalogs, resource data, or renderer resources. The service and
 * result values are inspected read-only and are not mutated.
 */
TerrainManifestAssetLoadServiceDiagnostics makeTerrainManifestAssetLoadServiceDiagnostics(
    const TerrainManifestAssetLoadJobWorkPacketResult& workPackets,
    const TerrainManifestAssetLoadServiceEnqueueResult& enqueue,
    const TerrainManifestAssetLoadServiceTickResult& tick,
    const TerrainManifestAssetLoadJobCompletionReconcileResult& completionReconcile,
    const TerrainManifestAssetLoadService& service);

/**
 * @brief Builds reusable diagnostics from a terrain asset batch resolve result.
 *
 * The returned value copies counters only. It does not retain references to
 * per-chunk records, resolved resource descriptors, renderer handles, or
 * renderer resource ownership.
 */
TerrainAssetBatchResolveDiagnostics makeTerrainAssetBatchResolveDiagnostics(
    const TerrainAssetBatchResolveResult& result);

/**
 * @brief Builds reusable diagnostics from manifest-to-runtime staging output.
 *
 * The returned value copies status and counters only. It does not retain
 * references to manifest catalogs, resource descriptors, staging operations,
 * queued request details, renderer handles, or renderer resources.
 */
TerrainManifestRuntimeStageDiagnostics makeTerrainManifestRuntimeStageDiagnostics(
    const TerrainManifestRuntimeStageResult& result);

/**
 * @brief Builds reusable diagnostics from an applied terrain setup request batch.
 *
 * The returned value copies counters only. It does not retain references to the
 * request records and does not mutate setup queues, catalogs, or renderer state.
 */
TerrainSetupRequestDiagnostics makeTerrainSetupRequestDiagnostics(
    const TerrainChunkRequestApplyResult& result);

/**
 * @brief Builds reusable diagnostics from an applied residency request batch.
 *
 * The returned value copies counters only. It does not retain references to the
 * request records and does not mutate residency queues or world registries.
 */
TerrainResidencyRequestDiagnostics makeTerrainResidencyRequestDiagnostics(
    const WorldChunkResidencyApplyResult& result);

/**
 * @brief Builds reusable diagnostics from one terrain integration pipeline run.
 *
 * The returned value copies summary counters and the supplied handle-map count.
 * It does not retain references to chunk records, descriptor intents, command
 * records, renderer handles, or submission results.
 */
TerrainPipelineDiagnostics makeTerrainPipelineDiagnostics(
    const WorldRenderSnapshot& snapshot,
    const TerrainRenderPrep& prep,
    const TerrainLifecyclePlan& lifecycle,
    const TerrainRendererCommandList& commands,
    const TerrainDescriptorBuildResult& descriptors,
    const TerrainSubmissionResult& submission,
    std::size_t handleCount);
} // namespace full_engine
