#include "engine/streaming/TerrainStreamingTickHistorySummary.hpp"

#include <algorithm>

namespace full_engine
{
namespace
{
void countStreamingStatus(
    const TerrainStreamingManifestUpdateStatus status,
    TerrainStreamingStatusSummary& summary) noexcept
{
    switch (status)
    {
    case TerrainStreamingManifestUpdateStatus::Success:
        ++summary.successCount;
        break;
    case TerrainStreamingManifestUpdateStatus::NoManifest:
        ++summary.noManifestCount;
        break;
    case TerrainStreamingManifestUpdateStatus::AssetLoadsPending:
        ++summary.assetLoadsPendingCount;
        break;
    case TerrainStreamingManifestUpdateStatus::ManifestStageFailed:
        ++summary.manifestStageFailedCount;
        break;
    case TerrainStreamingManifestUpdateStatus::UnsupportedStageChanges:
        ++summary.unsupportedStageChangesCount;
        break;
    case TerrainStreamingManifestUpdateStatus::StreamingQueueBlocked:
        ++summary.streamingQueueBlockedCount;
        break;
    }
}

void countRuntimeStatus(
    const TerrainRuntimeUpdateStatus status,
    TerrainStreamingRuntimeStatusSummary& summary) noexcept
{
    switch (status)
    {
    case TerrainRuntimeUpdateStatus::Success:
        ++summary.successCount;
        break;
    case TerrainRuntimeUpdateStatus::SetupFailed:
        ++summary.setupFailedCount;
        break;
    case TerrainRuntimeUpdateStatus::ResidencyFailed:
        ++summary.residencyFailedCount;
        break;
    case TerrainRuntimeUpdateStatus::PipelineFailed:
        ++summary.pipelineFailedCount;
        break;
    }
}

void countBudgetProfile(
    const TerrainStreamingBudgetProfile profile,
    TerrainStreamingBudgetProfileSummary& summary) noexcept
{
    switch (profile)
    {
    case TerrainStreamingBudgetProfile::Unlimited:
        ++summary.unlimitedCount;
        break;
    case TerrainStreamingBudgetProfile::Conservative:
        ++summary.conservativeCount;
        break;
    case TerrainStreamingBudgetProfile::Balanced:
        ++summary.balancedCount;
        break;
    case TerrainStreamingBudgetProfile::CatchUp:
        ++summary.catchUpCount;
        break;
    }
}

std::size_t deferredWorkCount(const TerrainStreamingTickEvent& event) noexcept
{
    return event.streaming.deferredSetupAddCount +
        event.streaming.deferredSetupRemoveCount +
        event.streaming.deferredMakeResidentCount +
        event.streaming.deferredMakeUnloadedCount +
        event.runtimeLifecycle.deferredCreateCount +
        event.runtimeLifecycle.deferredUpdateCount +
        event.runtimeLifecycle.deferredReleaseCount;
}

void accumulateDeferredWork(
    const TerrainStreamingTickEvent& event,
    TerrainStreamingDeferredWorkSummary& summary) noexcept
{
    summary.totalSetupAddDeferredCount += event.streaming.deferredSetupAddCount;
    summary.peakSetupAddDeferredCount =
        (std::max)(summary.peakSetupAddDeferredCount, event.streaming.deferredSetupAddCount);
    summary.totalSetupRemoveDeferredCount += event.streaming.deferredSetupRemoveCount;
    summary.peakSetupRemoveDeferredCount =
        (std::max)(summary.peakSetupRemoveDeferredCount, event.streaming.deferredSetupRemoveCount);
    summary.totalMakeResidentDeferredCount += event.streaming.deferredMakeResidentCount;
    summary.peakMakeResidentDeferredCount =
        (std::max)(summary.peakMakeResidentDeferredCount, event.streaming.deferredMakeResidentCount);
    summary.totalMakeUnloadedDeferredCount += event.streaming.deferredMakeUnloadedCount;
    summary.peakMakeUnloadedDeferredCount =
        (std::max)(summary.peakMakeUnloadedDeferredCount, event.streaming.deferredMakeUnloadedCount);
    summary.totalLifecycleCreateDeferredCount += event.runtimeLifecycle.deferredCreateCount;
    summary.peakLifecycleCreateDeferredCount =
        (std::max)(summary.peakLifecycleCreateDeferredCount, event.runtimeLifecycle.deferredCreateCount);
    summary.totalLifecycleUpdateDeferredCount += event.runtimeLifecycle.deferredUpdateCount;
    summary.peakLifecycleUpdateDeferredCount =
        (std::max)(summary.peakLifecycleUpdateDeferredCount, event.runtimeLifecycle.deferredUpdateCount);
    summary.totalLifecycleReleaseDeferredCount += event.runtimeLifecycle.deferredReleaseCount;
    summary.peakLifecycleReleaseDeferredCount =
        (std::max)(summary.peakLifecycleReleaseDeferredCount, event.runtimeLifecycle.deferredReleaseCount);

    const std::size_t aggregate = deferredWorkCount(event);
    summary.totalDeferredWorkCount += aggregate;
    summary.peakDeferredWorkCount = (std::max)(summary.peakDeferredWorkCount, aggregate);
}
} // namespace

TerrainStreamingTickHistorySummary summarizeTerrainStreamingTickHistory(
    const TerrainStreamingTickHistory& history)
{
    return summarizeTerrainStreamingTickHistory(history.events());
}

TerrainStreamingTickHistorySummary summarizeTerrainStreamingTickHistory(
    const std::vector<TerrainStreamingTickEvent>& events)
{
    TerrainStreamingTickHistorySummary summary;
    summary.tickCount = events.size();

    for (const TerrainStreamingTickEvent& event : events)
    {
        if (event.runtimeUpdateRan)
        {
            ++summary.runtimeUpdateRanCount;
        }

        countStreamingStatus(event.streamingStatus, summary.streamingStatuses);
        countRuntimeStatus(event.runtimeStatus, summary.runtimeStatuses);
        countBudgetProfile(event.budgetProfile, summary.budgetProfiles);
        accumulateDeferredWork(event, summary.deferredWork);

        summary.maxSetupRequestsBeforeRuntime =
            (std::max)(summary.maxSetupRequestsBeforeRuntime, event.setupRequestsBeforeRuntime);
        summary.maxResidencyRequestsBeforeRuntime =
            (std::max)(summary.maxResidencyRequestsBeforeRuntime, event.residencyRequestsBeforeRuntime);
        summary.maxSetupRequestsAfterRuntime =
            (std::max)(summary.maxSetupRequestsAfterRuntime, event.setupRequestsAfterRuntime);
        summary.maxResidencyRequestsAfterRuntime =
            (std::max)(summary.maxResidencyRequestsAfterRuntime, event.residencyRequestsAfterRuntime);
    }

    if (!events.empty())
    {
        summary.averageDeferredWork =
            static_cast<double>(summary.deferredWork.totalDeferredWorkCount) /
            static_cast<double>(events.size());
    }

    return summary;
}
} // namespace full_engine
