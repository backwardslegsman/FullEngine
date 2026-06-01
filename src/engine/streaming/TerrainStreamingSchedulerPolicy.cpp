#include "engine/streaming/TerrainStreamingSchedulerPolicy.hpp"

#include <algorithm>

namespace full_engine
{
namespace
{
std::size_t runtimeBacklogCount(const TerrainStreamingTickHistorySummary& summary) noexcept
{
    return (std::max)(
        (std::max)(
            summary.maxSetupRequestsBeforeRuntime,
            summary.maxResidencyRequestsBeforeRuntime),
        (std::max)(
            summary.maxSetupRequestsAfterRuntime,
            summary.maxResidencyRequestsAfterRuntime));
}

TerrainStreamingSchedulerStatus makeStatus(
    const bool runStreaming,
    const bool runAssetLoadJobs) noexcept
{
    if (runStreaming && runAssetLoadJobs)
    {
        return TerrainStreamingSchedulerStatus::RunStreamingAndAssetLoadJobs;
    }
    if (runStreaming)
    {
        return TerrainStreamingSchedulerStatus::RunStreaming;
    }
    if (runAssetLoadJobs)
    {
        return TerrainStreamingSchedulerStatus::RunAssetLoadJobs;
    }
    return TerrainStreamingSchedulerStatus::Idle;
}

TerrainStreamingBudgetProfile selectProfile(const std::size_t pressureCount) noexcept
{
    if (pressureCount == 0)
    {
        return TerrainStreamingBudgetProfile::Conservative;
    }
    return TerrainStreamingBudgetProfile::Balanced;
}
} // namespace

const char* terrainStreamingSchedulerStatusName(
    const TerrainStreamingSchedulerStatus status) noexcept
{
    switch (status)
    {
    case TerrainStreamingSchedulerStatus::Idle:
        return "Idle";
    case TerrainStreamingSchedulerStatus::RunStreaming:
        return "RunStreaming";
    case TerrainStreamingSchedulerStatus::RunAssetLoadJobs:
        return "RunAssetLoadJobs";
    case TerrainStreamingSchedulerStatus::RunStreamingAndAssetLoadJobs:
        return "RunStreamingAndAssetLoadJobs";
    }

    return "Unknown";
}

const char* terrainStreamingSchedulerReasonName(
    const TerrainStreamingSchedulerReason reason) noexcept
{
    switch (reason)
    {
    case TerrainStreamingSchedulerReason::NoWork:
        return "NoWork";
    case TerrainStreamingSchedulerReason::PendingAssetLoads:
        return "PendingAssetLoads";
    case TerrainStreamingSchedulerReason::PendingJobs:
        return "PendingJobs";
    case TerrainStreamingSchedulerReason::DeferredWorkPressure:
        return "DeferredWorkPressure";
    case TerrainStreamingSchedulerReason::StreamingBacklog:
        return "StreamingBacklog";
    case TerrainStreamingSchedulerReason::CatchUp:
        return "CatchUp";
    }

    return "Unknown";
}

TerrainStreamingSchedulerDecision chooseTerrainStreamingSchedulerPolicy(
    const TerrainStreamingTickHistorySummary& summary,
    const TerrainStreamingLoopDiagnostics& loopDiagnostics,
    const TerrainStreamingSchedulerOptions& options) noexcept
{
    TerrainStreamingSchedulerDecision decision;
    decision.pendingLoadRequestCount = loopDiagnostics.pendingLoadRequestCount;
    decision.pendingJobCount = loopDiagnostics.pendingJobCount;
    decision.deferredWorkCount = summary.deferredWork.totalDeferredWorkCount;
    decision.peakDeferredWorkCount = summary.deferredWork.peakDeferredWorkCount;
    decision.runtimeBacklogCount = runtimeBacklogCount(summary);
    decision.pressureCount = decision.deferredWorkCount + decision.runtimeBacklogCount;

    const bool runAssetLoadJobs =
        (options.pendingLoadRequestThreshold > 0 &&
         decision.pendingLoadRequestCount >= options.pendingLoadRequestThreshold) ||
        (options.pendingJobThreshold > 0 &&
         decision.pendingJobCount >= options.pendingJobThreshold);
    const bool runStreaming = decision.pressureCount > 0;

    decision.status = makeStatus(runStreaming, runAssetLoadJobs);
    if (decision.status == TerrainStreamingSchedulerStatus::Idle)
    {
        decision.reason = TerrainStreamingSchedulerReason::NoWork;
        decision.budgetProfile = TerrainStreamingBudgetProfile::Conservative;
    }
    else if (options.catchUpPressureThreshold > 0 &&
             decision.pressureCount >= options.catchUpPressureThreshold)
    {
        decision.reason = TerrainStreamingSchedulerReason::CatchUp;
        decision.budgetProfile = TerrainStreamingBudgetProfile::CatchUp;
    }
    else if (decision.pendingLoadRequestCount >= options.pendingLoadRequestThreshold &&
             options.pendingLoadRequestThreshold > 0)
    {
        decision.reason = TerrainStreamingSchedulerReason::PendingAssetLoads;
        decision.budgetProfile = selectProfile(decision.pressureCount);
    }
    else if (decision.pendingJobCount >= options.pendingJobThreshold &&
             options.pendingJobThreshold > 0)
    {
        decision.reason = TerrainStreamingSchedulerReason::PendingJobs;
        decision.budgetProfile = selectProfile(decision.pressureCount);
    }
    else if (decision.deferredWorkCount > 0)
    {
        decision.reason = TerrainStreamingSchedulerReason::DeferredWorkPressure;
        decision.budgetProfile = selectProfile(decision.pressureCount);
    }
    else
    {
        decision.reason = TerrainStreamingSchedulerReason::StreamingBacklog;
        decision.budgetProfile = selectProfile(decision.pressureCount);
    }

    decision.budgets = selectTerrainStreamingLoopBudgets(decision.budgetProfile);
    if (runAssetLoadJobs)
    {
        decision.maxAssetLoadJobs =
            decision.budgetProfile == TerrainStreamingBudgetProfile::CatchUp ?
                options.catchUpMaxAssetLoadJobs :
                options.normalMaxAssetLoadJobs;
    }

    return decision;
}
} // namespace full_engine
