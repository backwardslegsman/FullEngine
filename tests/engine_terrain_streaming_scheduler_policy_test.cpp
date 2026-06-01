#include "engine/streaming/TerrainStreamingSchedulerPolicy.hpp"

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

full_engine::TerrainStreamingTickHistorySummary makeSummary(
    const std::size_t deferredWork,
    const std::size_t peakDeferredWork,
    const std::size_t setupBacklog,
    const std::size_t residencyBacklog)
{
    full_engine::TerrainStreamingTickHistorySummary summary;
    summary.deferredWork.totalDeferredWorkCount = deferredWork;
    summary.deferredWork.peakDeferredWorkCount = peakDeferredWork;
    summary.maxSetupRequestsBeforeRuntime = setupBacklog;
    summary.maxResidencyRequestsAfterRuntime = residencyBacklog;
    return summary;
}

full_engine::TerrainStreamingLoopDiagnostics makeDiagnostics(
    const std::size_t pendingLoads,
    const std::size_t pendingJobs)
{
    full_engine::TerrainStreamingLoopDiagnostics diagnostics;
    diagnostics.pendingLoadRequestCount = pendingLoads;
    diagnostics.pendingJobCount = pendingJobs;
    return diagnostics;
}

void testIdleDecision(std::vector<std::string>& failures)
{
    const full_engine::TerrainStreamingTickHistorySummary summary;
    const full_engine::TerrainStreamingLoopDiagnostics diagnostics;
    const full_engine::TerrainStreamingSchedulerDecision decision =
        full_engine::chooseTerrainStreamingSchedulerPolicy(summary, diagnostics);

    expect(decision.status == full_engine::TerrainStreamingSchedulerStatus::Idle, "empty diagnostics are idle", failures);
    expect(decision.reason == full_engine::TerrainStreamingSchedulerReason::NoWork, "idle decision reports no work", failures);
    expect(decision.budgetProfile == full_engine::TerrainStreamingBudgetProfile::Conservative, "idle decision uses conservative budget", failures);
    expect(decision.maxAssetLoadJobs == 0, "idle decision has no load job cap", failures);
    expect(decision.pressureCount == 0, "idle decision has no pressure", failures);
}

void testPendingLoadRequestsRunJobs(std::vector<std::string>& failures)
{
    const full_engine::TerrainStreamingSchedulerDecision decision =
        full_engine::chooseTerrainStreamingSchedulerPolicy({}, makeDiagnostics(2, 0));

    expect(decision.status == full_engine::TerrainStreamingSchedulerStatus::RunAssetLoadJobs, "pending loads run asset load jobs", failures);
    expect(decision.reason == full_engine::TerrainStreamingSchedulerReason::PendingAssetLoads, "pending loads drive reason", failures);
    expect(decision.pendingLoadRequestCount == 2, "pending load count is copied", failures);
    expect(decision.budgetProfile == full_engine::TerrainStreamingBudgetProfile::Conservative, "pending loads alone have no streaming pressure", failures);
    expect(decision.maxAssetLoadJobs == 2, "pending loads use normal max jobs", failures);
}

void testPendingJobsRunJobs(std::vector<std::string>& failures)
{
    const full_engine::TerrainStreamingSchedulerDecision decision =
        full_engine::chooseTerrainStreamingSchedulerPolicy({}, makeDiagnostics(0, 3));

    expect(decision.status == full_engine::TerrainStreamingSchedulerStatus::RunAssetLoadJobs, "pending jobs run asset load jobs", failures);
    expect(decision.reason == full_engine::TerrainStreamingSchedulerReason::PendingJobs, "pending jobs drive reason", failures);
    expect(decision.pendingJobCount == 3, "pending job count is copied", failures);
    expect(decision.maxAssetLoadJobs == 2, "pending jobs use normal max jobs", failures);
}

void testDeferredWorkRunsStreaming(std::vector<std::string>& failures)
{
    const full_engine::TerrainStreamingTickHistorySummary summary = makeSummary(3, 2, 0, 0);
    const full_engine::TerrainStreamingSchedulerDecision decision =
        full_engine::chooseTerrainStreamingSchedulerPolicy(summary, {});

    expect(decision.status == full_engine::TerrainStreamingSchedulerStatus::RunStreaming, "deferred work runs streaming", failures);
    expect(decision.reason == full_engine::TerrainStreamingSchedulerReason::DeferredWorkPressure, "deferred work drives reason", failures);
    expect(decision.deferredWorkCount == 3, "deferred work is copied", failures);
    expect(decision.peakDeferredWorkCount == 2, "peak deferred work is copied", failures);
    expect(decision.budgetProfile == full_engine::TerrainStreamingBudgetProfile::Balanced, "light deferred pressure uses balanced budget", failures);
}

void testCatchUpPressure(std::vector<std::string>& failures)
{
    const full_engine::TerrainStreamingTickHistorySummary summary = makeSummary(8, 5, 0, 0);
    const full_engine::TerrainStreamingSchedulerDecision decision =
        full_engine::chooseTerrainStreamingSchedulerPolicy(summary, makeDiagnostics(1, 0));

    expect(decision.status == full_engine::TerrainStreamingSchedulerStatus::RunStreamingAndAssetLoadJobs, "catch up with pending loads runs both", failures);
    expect(decision.reason == full_engine::TerrainStreamingSchedulerReason::CatchUp, "high pressure drives catch up reason", failures);
    expect(decision.budgetProfile == full_engine::TerrainStreamingBudgetProfile::CatchUp, "high pressure selects catch up", failures);
    expect(decision.maxAssetLoadJobs == 8, "catch up uses catch up max jobs", failures);
    expect(decision.budgets.queue.maxSetupAdds == 4, "catch up budget caps are selected", failures);
}

void testStreamingBacklogRunsStreaming(std::vector<std::string>& failures)
{
    const full_engine::TerrainStreamingTickHistorySummary summary = makeSummary(0, 0, 4, 6);
    const full_engine::TerrainStreamingSchedulerDecision decision =
        full_engine::chooseTerrainStreamingSchedulerPolicy(summary, {});

    expect(decision.status == full_engine::TerrainStreamingSchedulerStatus::RunStreaming, "runtime backlog runs streaming", failures);
    expect(decision.reason == full_engine::TerrainStreamingSchedulerReason::StreamingBacklog, "runtime backlog drives reason", failures);
    expect(decision.runtimeBacklogCount == 6, "runtime backlog max is copied", failures);
    expect(decision.budgetProfile == full_engine::TerrainStreamingBudgetProfile::Balanced, "runtime backlog uses balanced budget", failures);
}

void testOptionsOverrideThresholdsAndJobCaps(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingSchedulerOptions options;
    options.pendingLoadRequestThreshold = 3;
    options.pendingJobThreshold = 2;
    options.catchUpPressureThreshold = 5;
    options.normalMaxAssetLoadJobs = 4;
    options.catchUpMaxAssetLoadJobs = 11;

    full_engine::TerrainStreamingSchedulerDecision decision =
        full_engine::chooseTerrainStreamingSchedulerPolicy(makeSummary(0, 0, 0, 0), makeDiagnostics(2, 0), options);
    expect(decision.status == full_engine::TerrainStreamingSchedulerStatus::Idle, "load threshold suppresses low pending load count", failures);

    decision = full_engine::chooseTerrainStreamingSchedulerPolicy(makeSummary(0, 0, 0, 0), makeDiagnostics(0, 2), options);
    expect(decision.status == full_engine::TerrainStreamingSchedulerStatus::RunAssetLoadJobs, "job threshold allows matching pending job count", failures);
    expect(decision.maxAssetLoadJobs == 4, "normal max job option is used", failures);

    decision = full_engine::chooseTerrainStreamingSchedulerPolicy(makeSummary(5, 5, 0, 0), makeDiagnostics(3, 0), options);
    expect(decision.budgetProfile == full_engine::TerrainStreamingBudgetProfile::CatchUp, "catch up threshold option is used", failures);
    expect(decision.maxAssetLoadJobs == 11, "catch up max job option is used", failures);
}

void testInputsAreNotMutated(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingTickHistorySummary summary = makeSummary(2, 2, 1, 0);
    full_engine::TerrainStreamingLoopDiagnostics diagnostics = makeDiagnostics(1, 1);

    const full_engine::TerrainStreamingTickHistorySummary beforeSummary = summary;
    const full_engine::TerrainStreamingLoopDiagnostics beforeDiagnostics = diagnostics;
    (void)full_engine::chooseTerrainStreamingSchedulerPolicy(summary, diagnostics);

    expect(summary.deferredWork.totalDeferredWorkCount == beforeSummary.deferredWork.totalDeferredWorkCount, "summary deferred work is not mutated", failures);
    expect(summary.maxSetupRequestsBeforeRuntime == beforeSummary.maxSetupRequestsBeforeRuntime, "summary backlog is not mutated", failures);
    expect(diagnostics.pendingLoadRequestCount == beforeDiagnostics.pendingLoadRequestCount, "diagnostics pending loads are not mutated", failures);
    expect(diagnostics.pendingJobCount == beforeDiagnostics.pendingJobCount, "diagnostics pending jobs are not mutated", failures);
}

void testNameHelpers(std::vector<std::string>& failures)
{
    expect(
        std::string(full_engine::terrainStreamingSchedulerStatusName(
            full_engine::TerrainStreamingSchedulerStatus::RunStreamingAndAssetLoadJobs)) ==
            "RunStreamingAndAssetLoadJobs",
        "scheduler status name is deterministic",
        failures);
    expect(
        std::string(full_engine::terrainStreamingSchedulerReasonName(
            full_engine::TerrainStreamingSchedulerReason::CatchUp)) == "CatchUp",
        "scheduler reason name is deterministic",
        failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testIdleDecision(failures);
    testPendingLoadRequestsRunJobs(failures);
    testPendingJobsRunJobs(failures);
    testDeferredWorkRunsStreaming(failures);
    testCatchUpPressure(failures);
    testStreamingBacklogRunsStreaming(failures);
    testOptionsOverrideThresholdsAndJobCaps(failures);
    testInputsAreNotMutated(failures);
    testNameHelpers(failures);

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
