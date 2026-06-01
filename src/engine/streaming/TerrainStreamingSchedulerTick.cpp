#include "engine/streaming/TerrainStreamingSchedulerTick.hpp"

#include "engine/streaming/TerrainStreamingSchedulerTickDiagnostics.hpp"

namespace full_engine
{
namespace
{
bool shouldRunLoadJobs(const TerrainStreamingSchedulerStatus status) noexcept
{
    return status == TerrainStreamingSchedulerStatus::RunAssetLoadJobs ||
        status == TerrainStreamingSchedulerStatus::RunStreamingAndAssetLoadJobs;
}

bool shouldRunStreaming(const TerrainStreamingSchedulerStatus status) noexcept
{
    return status == TerrainStreamingSchedulerStatus::RunStreaming ||
        status == TerrainStreamingSchedulerStatus::RunStreamingAndAssetLoadJobs;
}

bool loadJobsBlocked(const TerrainManifestAssetLoadJobCoordinatorStatus status) noexcept
{
    return status == TerrainManifestAssetLoadJobCoordinatorStatus::JobsBlocked ||
        status == TerrainManifestAssetLoadJobCoordinatorStatus::LoadConsumeBlocked;
}

bool schedulingBlocked(const TerrainManifestAssetLoadJobScheduleStatus status) noexcept
{
    return status == TerrainManifestAssetLoadJobScheduleStatus::Blocked;
}

bool serviceEnqueueBlocked(const TerrainManifestAssetLoadServiceEnqueueSummary& summary) noexcept
{
    return summary.invalidPacketCount > 0;
}

bool serviceTickBlocked(const TerrainManifestAssetLoadServiceTickStatus status) noexcept
{
    return status == TerrainManifestAssetLoadServiceTickStatus::Blocked;
}

bool serviceReconcileBlocked(const TerrainManifestAssetLoadJobCompletionReconcileStatus status) noexcept
{
    return status == TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPublishFailed ||
        status == TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPending ||
        status == TerrainManifestAssetLoadJobCompletionReconcileStatus::LoadConsumeBlocked;
}

bool hasExternalCompletions(const TerrainStreamingSchedulerTickOptions& options) noexcept
{
    return options.externalCompletionCount > 0;
}

TerrainStreamingSchedulerTickStatus mapStreamingStatus(
    const TerrainStreamingLoopUpdateStatus status) noexcept
{
    switch (status)
    {
    case TerrainStreamingLoopUpdateStatus::Success:
        return TerrainStreamingSchedulerTickStatus::Success;
    case TerrainStreamingLoopUpdateStatus::StreamingBlocked:
        return TerrainStreamingSchedulerTickStatus::StreamingBlocked;
    case TerrainStreamingLoopUpdateStatus::RuntimeSetupFailed:
        return TerrainStreamingSchedulerTickStatus::RuntimeSetupFailed;
    case TerrainStreamingLoopUpdateStatus::RuntimeResidencyFailed:
        return TerrainStreamingSchedulerTickStatus::RuntimeResidencyFailed;
    case TerrainStreamingLoopUpdateStatus::RuntimePipelineFailed:
        return TerrainStreamingSchedulerTickStatus::RuntimePipelineFailed;
    }

    return TerrainStreamingSchedulerTickStatus::StreamingBlocked;
}
} // namespace

const char* terrainStreamingSchedulerTickStatusName(
    const TerrainStreamingSchedulerTickStatus status) noexcept
{
    switch (status)
    {
    case TerrainStreamingSchedulerTickStatus::Idle:
        return "Idle";
    case TerrainStreamingSchedulerTickStatus::Success:
        return "Success";
    case TerrainStreamingSchedulerTickStatus::LoadJobsBlocked:
        return "LoadJobsBlocked";
    case TerrainStreamingSchedulerTickStatus::StreamingBlocked:
        return "StreamingBlocked";
    case TerrainStreamingSchedulerTickStatus::RuntimeSetupFailed:
        return "RuntimeSetupFailed";
    case TerrainStreamingSchedulerTickStatus::RuntimeResidencyFailed:
        return "RuntimeResidencyFailed";
    case TerrainStreamingSchedulerTickStatus::RuntimePipelineFailed:
        return "RuntimePipelineFailed";
    }

    return "Unknown";
}

TerrainStreamingSchedulerTickResult runTerrainStreamingSchedulerTick(
    TerrainStreamingLoopState& loop,
    TerrainRuntimeState& terrainRuntime,
    full_renderer::IRenderer& renderer,
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resources,
    ChunkTerrainHandleMap& terrainHandles,
    RendererAssetHandleCatalog& assetHandles,
    const WorldChunkDesc* const worldDescs,
    const std::size_t worldDescCount,
    const ChunkId* const trackedIds,
    const std::size_t trackedIdCount,
    const TerrainStreamingPlannerConfig& streamingConfig,
    const WorldPosition& cameraWorld,
    const TerrainRuntimeStateSnapshot& currentSnapshot,
    const TerrainManifestAssetLoadCallback assetLoadCallback,
    void* const assetLoadUserData,
    const TerrainStreamingSchedulerTickOptions& options)
{
    TerrainStreamingSchedulerTickResult result;
    result.historySummary = summarizeTerrainStreamingTickHistory(loop.tickHistory());
    result.decision = chooseTerrainStreamingSchedulerPolicy(
        result.historySummary,
        loop.latestDiagnostics(),
        options.scheduler);

    if (result.decision.status == TerrainStreamingSchedulerStatus::Idle)
    {
        result.status = TerrainStreamingSchedulerTickStatus::Idle;
        return result;
    }

    if (shouldRunLoadJobs(result.decision.status))
    {
        if (options.loadJobMode == TerrainStreamingSchedulerLoadJobMode::ScheduleOnly)
        {
            result.scheduledLoadJobs = loop.scheduleAssetLoadJobs(options.assetLoadJobPriority);
            result.loadJobsScheduled = true;
            if (schedulingBlocked(result.scheduledLoadJobs.status))
            {
                result.status = TerrainStreamingSchedulerTickStatus::LoadJobsBlocked;
                return result;
            }
            result.status = TerrainStreamingSchedulerTickStatus::Success;
            return result;
        }

        if (options.loadJobMode == TerrainStreamingSchedulerLoadJobMode::ExternalCompletions)
        {
            result.scheduledLoadJobs = loop.scheduleAssetLoadJobs(options.assetLoadJobPriority);
            result.loadJobsScheduled = true;
            if (schedulingBlocked(result.scheduledLoadJobs.status))
            {
                result.status = TerrainStreamingSchedulerTickStatus::LoadJobsBlocked;
                return result;
            }

            if (!hasExternalCompletions(options))
            {
                result.status = TerrainStreamingSchedulerTickStatus::Success;
                return result;
            }

            result.externalCompletionReconcile = loop.reconcileScheduledAssetLoadCompletions(
                options.externalCompletions,
                options.externalCompletionCount,
                assetHandles);
            result.externalCompletionsReconciled = true;
            if (serviceReconcileBlocked(result.externalCompletionReconcile.status))
            {
                result.status = TerrainStreamingSchedulerTickStatus::LoadJobsBlocked;
                return result;
            }
        }

        else if (options.loadJobMode == TerrainStreamingSchedulerLoadJobMode::RetainedService)
        {
            const std::size_t maxJobs =
                options.overrideMaxAssetLoadJobs ?
                    options.maxAssetLoadJobs :
                    result.decision.maxAssetLoadJobs;
            result.scheduledLoadJobs = loop.scheduleAssetLoadJobs(options.assetLoadJobPriority);
            result.loadJobsScheduled = true;
            if (schedulingBlocked(result.scheduledLoadJobs.status))
            {
                result.status = TerrainStreamingSchedulerTickStatus::LoadJobsBlocked;
                return result;
            }

            result.loadServiceEnqueue = loop.enqueueScheduledAssetLoadWork();
            result.loadServiceWorkPackets = loop.latestLoadServiceWorkPackets();
            result.loadService = loop.latestDiagnostics().loadService;
            if (serviceEnqueueBlocked(result.loadServiceEnqueue.summary))
            {
                result.status = TerrainStreamingSchedulerTickStatus::LoadJobsBlocked;
                return result;
            }

            result.loadServiceTick = loop.tickAssetLoadService(
                maxJobs,
                assetLoadCallback,
                assetLoadUserData);
            result.loadServiceRan = true;
            result.loadService = loop.latestDiagnostics().loadService;
            if (serviceTickBlocked(result.loadServiceTick.status))
            {
                result.status = TerrainStreamingSchedulerTickStatus::LoadJobsBlocked;
                return result;
            }

            result.loadServiceReconcile = loop.reconcileAssetLoadServiceCompletions(assetHandles);
            result.loadService = loop.latestDiagnostics().loadService;
            if (serviceReconcileBlocked(result.loadServiceReconcile.status))
            {
                result.status = TerrainStreamingSchedulerTickStatus::LoadJobsBlocked;
                return result;
            }
        }
        else
        {
            const std::size_t maxJobs =
                options.overrideMaxAssetLoadJobs ?
                    options.maxAssetLoadJobs :
                    result.decision.maxAssetLoadJobs;
            result.loadJobs = loop.runAssetLoadJobs(
                assetHandles,
                assetLoadCallback,
                assetLoadUserData,
                maxJobs,
                options.assetLoadJobPriority);
            result.loadJobsRan = true;
            if (loadJobsBlocked(result.loadJobs.status))
            {
                result.status = TerrainStreamingSchedulerTickStatus::LoadJobsBlocked;
                return result;
            }
        }
    }

    if (shouldRunStreaming(result.decision.status))
    {
        TerrainStreamingLoopUpdateOptions loopOptions = options.loopUpdate;
        loopOptions.budgets = result.decision.budgets;
        result.streaming = updateTerrainStreamingLoop(
            loop,
            terrainRuntime,
            renderer,
            registry,
            worldCatalog,
            resources,
            terrainHandles,
            assetHandles,
            worldDescs,
            worldDescCount,
            trackedIds,
            trackedIdCount,
            streamingConfig,
            cameraWorld,
            currentSnapshot,
            loopOptions);
        result.streamingRan = true;
        result.status = mapStreamingStatus(result.streaming.status);
        loop.annotateLatestTickSchedulerDiagnostics(
            makeTerrainStreamingSchedulerTickDiagnostics(result).history);
        return result;
    }

    result.status = TerrainStreamingSchedulerTickStatus::Success;
    return result;
}
} // namespace full_engine
