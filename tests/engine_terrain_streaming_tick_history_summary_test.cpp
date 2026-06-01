#include "engine/streaming/TerrainStreamingTickHistoryExport.hpp"
#include "engine/streaming/TerrainStreamingTickHistoryImport.hpp"
#include "engine/streaming/TerrainStreamingTickHistorySummary.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* const message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

void expectNear(
    const double actual,
    const double expected,
    const char* const message,
    std::vector<std::string>& failures)
{
    if (std::fabs(actual - expected) > 0.0001)
    {
        failures.emplace_back(message);
    }
}

full_engine::TerrainStreamingTickEvent makeTick(
    const full_engine::TerrainStreamingManifestUpdateStatus streamingStatus,
    const full_engine::TerrainRuntimeUpdateStatus runtimeStatus,
    const full_engine::TerrainStreamingBudgetProfile budgetProfile,
    const std::size_t base)
{
    full_engine::TerrainStreamingTickEvent event;
    event.streamingStatus = streamingStatus;
    event.runtimeStatus = runtimeStatus;
    event.budgetProfile = budgetProfile;
    event.runtimeUpdateRan = (base % 2U) == 0U;
    event.setupRequestsBeforeRuntime = base + 1;
    event.residencyRequestsBeforeRuntime = base + 2;
    event.setupRequestsAfterRuntime = base + 3;
    event.residencyRequestsAfterRuntime = base + 4;
    event.streaming.deferredSetupAddCount = base + 5;
    event.streaming.deferredSetupRemoveCount = base + 6;
    event.streaming.deferredMakeResidentCount = base + 7;
    event.streaming.deferredMakeUnloadedCount = base + 8;
    event.runtimeLifecycle.deferredCreateCount = base + 9;
    event.runtimeLifecycle.deferredUpdateCount = base + 10;
    event.runtimeLifecycle.deferredReleaseCount = base + 11;
    return event;
}

std::size_t aggregateDeferredWork(const full_engine::TerrainStreamingTickEvent& event)
{
    return event.streaming.deferredSetupAddCount +
        event.streaming.deferredSetupRemoveCount +
        event.streaming.deferredMakeResidentCount +
        event.streaming.deferredMakeUnloadedCount +
        event.runtimeLifecycle.deferredCreateCount +
        event.runtimeLifecycle.deferredUpdateCount +
        event.runtimeLifecycle.deferredReleaseCount;
}

void testEmptyHistory(std::vector<std::string>& failures)
{
    const full_engine::TerrainStreamingTickHistory history;
    const full_engine::TerrainStreamingTickHistorySummary summary =
        full_engine::summarizeTerrainStreamingTickHistory(history);

    expect(summary.tickCount == 0, "empty summary has no ticks", failures);
    expect(summary.runtimeUpdateRanCount == 0, "empty summary has no runtime updates", failures);
    expect(summary.deferredWork.totalDeferredWorkCount == 0, "empty summary has no deferred work", failures);
    expect(summary.deferredWork.peakDeferredWorkCount == 0, "empty summary has no deferred peak", failures);
    expect(summary.maxSetupRequestsBeforeRuntime == 0, "empty summary has zero request max", failures);
    expectNear(summary.averageDeferredWork, 0.0, "empty summary has zero average", failures);
}

void testSingleTickCopiesCounters(std::vector<std::string>& failures)
{
    std::vector<full_engine::TerrainStreamingTickEvent> events;
    events.push_back(makeTick(
        full_engine::TerrainStreamingManifestUpdateStatus::AssetLoadsPending,
        full_engine::TerrainRuntimeUpdateStatus::ResidencyFailed,
        full_engine::TerrainStreamingBudgetProfile::CatchUp,
        10));

    const full_engine::TerrainStreamingTickHistorySummary summary =
        full_engine::summarizeTerrainStreamingTickHistory(events);
    const std::size_t aggregate = aggregateDeferredWork(events.front());

    expect(summary.tickCount == 1, "single tick count is one", failures);
    expect(summary.runtimeUpdateRanCount == 1, "single tick copies runtime update ran", failures);
    expect(summary.streamingStatuses.assetLoadsPendingCount == 1, "single tick counts streaming status", failures);
    expect(summary.runtimeStatuses.residencyFailedCount == 1, "single tick counts runtime status", failures);
    expect(summary.budgetProfiles.catchUpCount == 1, "single tick counts budget profile", failures);
    expect(summary.deferredWork.totalSetupAddDeferredCount == 15, "single tick copies setup add deferred total", failures);
    expect(summary.deferredWork.peakSetupAddDeferredCount == 15, "single tick copies setup add deferred peak", failures);
    expect(summary.deferredWork.totalDeferredWorkCount == aggregate, "single tick copies aggregate deferred total", failures);
    expect(summary.deferredWork.peakDeferredWorkCount == aggregate, "single tick copies aggregate deferred peak", failures);
    expect(summary.maxSetupRequestsBeforeRuntime == 11, "single tick copies setup before max", failures);
    expect(summary.maxResidencyRequestsBeforeRuntime == 12, "single tick copies residency before max", failures);
    expect(summary.maxSetupRequestsAfterRuntime == 13, "single tick copies setup after max", failures);
    expect(summary.maxResidencyRequestsAfterRuntime == 14, "single tick copies residency after max", failures);
    expectNear(summary.averageDeferredWork, static_cast<double>(aggregate), "single tick average is aggregate", failures);
}

void testMultipleTicksAccumulateStatusesProfilesAndPeaks(std::vector<std::string>& failures)
{
    std::vector<full_engine::TerrainStreamingTickEvent> events;
    events.push_back(makeTick(
        full_engine::TerrainStreamingManifestUpdateStatus::Success,
        full_engine::TerrainRuntimeUpdateStatus::Success,
        full_engine::TerrainStreamingBudgetProfile::Unlimited,
        0));
    events.push_back(makeTick(
        full_engine::TerrainStreamingManifestUpdateStatus::NoManifest,
        full_engine::TerrainRuntimeUpdateStatus::SetupFailed,
        full_engine::TerrainStreamingBudgetProfile::Conservative,
        1));
    events.push_back(makeTick(
        full_engine::TerrainStreamingManifestUpdateStatus::ManifestStageFailed,
        full_engine::TerrainRuntimeUpdateStatus::PipelineFailed,
        full_engine::TerrainStreamingBudgetProfile::Balanced,
        20));
    events.push_back(makeTick(
        full_engine::TerrainStreamingManifestUpdateStatus::UnsupportedStageChanges,
        full_engine::TerrainRuntimeUpdateStatus::Success,
        full_engine::TerrainStreamingBudgetProfile::CatchUp,
        2));
    events.push_back(makeTick(
        full_engine::TerrainStreamingManifestUpdateStatus::StreamingQueueBlocked,
        full_engine::TerrainRuntimeUpdateStatus::Success,
        full_engine::TerrainStreamingBudgetProfile::Balanced,
        3));

    const full_engine::TerrainStreamingTickHistorySummary summary =
        full_engine::summarizeTerrainStreamingTickHistory(events);
    std::size_t totalAggregate = 0;
    for (const full_engine::TerrainStreamingTickEvent& event : events)
    {
        totalAggregate += aggregateDeferredWork(event);
    }

    expect(summary.tickCount == 5, "multiple summary counts ticks", failures);
    expect(summary.runtimeUpdateRanCount == 3, "multiple summary counts runtime update ran ticks", failures);
    expect(summary.streamingStatuses.successCount == 1, "multiple summary counts success status", failures);
    expect(summary.streamingStatuses.noManifestCount == 1, "multiple summary counts no manifest status", failures);
    expect(summary.streamingStatuses.manifestStageFailedCount == 1, "multiple summary counts manifest failure", failures);
    expect(summary.streamingStatuses.unsupportedStageChangesCount == 1, "multiple summary counts unsupported changes", failures);
    expect(summary.streamingStatuses.streamingQueueBlockedCount == 1, "multiple summary counts queue blocked", failures);
    expect(summary.runtimeStatuses.successCount == 3, "multiple summary counts runtime success", failures);
    expect(summary.runtimeStatuses.setupFailedCount == 1, "multiple summary counts setup failure", failures);
    expect(summary.runtimeStatuses.pipelineFailedCount == 1, "multiple summary counts pipeline failure", failures);
    expect(summary.budgetProfiles.unlimitedCount == 1, "multiple summary counts unlimited profile", failures);
    expect(summary.budgetProfiles.conservativeCount == 1, "multiple summary counts conservative profile", failures);
    expect(summary.budgetProfiles.balancedCount == 2, "multiple summary counts balanced profile", failures);
    expect(summary.budgetProfiles.catchUpCount == 1, "multiple summary counts catch up profile", failures);
    expect(summary.deferredWork.totalLifecycleReleaseDeferredCount == 11 + 12 + 31 + 13 + 14, "multiple summary totals lifecycle release deferred", failures);
    expect(summary.deferredWork.peakLifecycleReleaseDeferredCount == 31, "multiple summary peaks lifecycle release deferred", failures);
    expect(summary.deferredWork.totalDeferredWorkCount == totalAggregate, "multiple summary totals aggregate deferred", failures);
    expect(summary.deferredWork.peakDeferredWorkCount == aggregateDeferredWork(events[2]), "multiple summary peaks aggregate deferred", failures);
    expect(summary.maxSetupRequestsBeforeRuntime == 21, "multiple summary tracks max setup before", failures);
    expect(summary.maxResidencyRequestsAfterRuntime == 24, "multiple summary tracks max residency after", failures);
    expectNear(summary.averageDeferredWork, static_cast<double>(totalAggregate) / 5.0, "multiple summary average is deterministic", failures);
}

void testRetainedHistoryMatchesVectorSummary(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingTickHistory history;
    std::vector<full_engine::TerrainStreamingTickEvent> events;
    events.push_back(makeTick(
        full_engine::TerrainStreamingManifestUpdateStatus::Success,
        full_engine::TerrainRuntimeUpdateStatus::Success,
        full_engine::TerrainStreamingBudgetProfile::Conservative,
        4));
    events.push_back(makeTick(
        full_engine::TerrainStreamingManifestUpdateStatus::AssetLoadsPending,
        full_engine::TerrainRuntimeUpdateStatus::SetupFailed,
        full_engine::TerrainStreamingBudgetProfile::CatchUp,
        7));
    for (const full_engine::TerrainStreamingTickEvent& event : events)
    {
        history.append(event);
    }

    const full_engine::TerrainStreamingTickHistorySummary fromHistory =
        full_engine::summarizeTerrainStreamingTickHistory(history);
    const full_engine::TerrainStreamingTickHistorySummary fromVector =
        full_engine::summarizeTerrainStreamingTickHistory(history.events());

    expect(fromHistory.tickCount == fromVector.tickCount, "history summary tick count matches vector", failures);
    expect(
        fromHistory.deferredWork.totalDeferredWorkCount == fromVector.deferredWork.totalDeferredWorkCount,
        "history summary deferred total matches vector",
        failures);
    expect(fromHistory.budgetProfiles.catchUpCount == fromVector.budgetProfiles.catchUpCount, "history summary profile count matches vector", failures);
}

void testImportedTraceCanBeSummarized(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_tick_summary_round_trip_test.jsonl";
    std::remove(path);

    std::vector<full_engine::TerrainStreamingTickEvent> events;
    events.push_back(makeTick(
        full_engine::TerrainStreamingManifestUpdateStatus::AssetLoadsPending,
        full_engine::TerrainRuntimeUpdateStatus::ResidencyFailed,
        full_engine::TerrainStreamingBudgetProfile::CatchUp,
        5));
    events.push_back(makeTick(
        full_engine::TerrainStreamingManifestUpdateStatus::Success,
        full_engine::TerrainRuntimeUpdateStatus::Success,
        full_engine::TerrainStreamingBudgetProfile::Balanced,
        6));

    const full_engine::TerrainStreamingTickHistoryExportResult exported =
        full_engine::exportTerrainStreamingTickHistoryJsonLines(events, path);
    const full_engine::TerrainStreamingTickHistoryImport imported =
        full_engine::importTerrainStreamingTickHistoryJsonLines(path);
    const full_engine::TerrainStreamingTickHistorySummary summary =
        full_engine::summarizeTerrainStreamingTickHistory(imported.events);

    expect(exported == full_engine::TerrainStreamingTickHistoryExportResult::Success, "summary round trip export succeeds", failures);
    expect(imported.result == full_engine::TerrainStreamingTickHistoryImportResult::Success, "summary round trip import succeeds", failures);
    expect(summary.tickCount == 2, "imported trace summary counts ticks", failures);
    expect(summary.streamingStatuses.assetLoadsPendingCount == 1, "imported trace summary counts pending status", failures);
    expect(summary.budgetProfiles.catchUpCount == 1, "imported trace summary counts profile", failures);
    std::remove(path);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testEmptyHistory(failures);
    testSingleTickCopiesCounters(failures);
    testMultipleTicksAccumulateStatusesProfilesAndPeaks(failures);
    testRetainedHistoryMatchesVectorSummary(failures);
    testImportedTraceCanBeSummarized(failures);

    if (!failures.empty())
    {
        for (const std::string& failure : failures)
        {
            std::cerr << failure << '\n';
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
