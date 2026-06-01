#include "engine/streaming/TerrainStreamingTickHistoryExport.hpp"
#include "engine/streaming/TerrainStreamingTickHistoryImport.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
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

void writeText(const char* const path, const char* const text)
{
    std::ofstream output(path, std::ios::out | std::ios::trunc);
    output << text;
}

full_engine::TerrainStreamingTickEvent makeTick(
    const std::uint64_t sequence,
    const std::size_t base)
{
    full_engine::TerrainStreamingTickEvent event;
    event.sequence = sequence;
    event.streamingStatus = full_engine::TerrainStreamingManifestUpdateStatus::AssetLoadsPending;
    event.runtimeStatus = full_engine::TerrainRuntimeUpdateStatus::ResidencyFailed;
    event.budgetProfile = full_engine::TerrainStreamingBudgetProfile::CatchUp;
    event.scheduler.hasSchedulerDecision = true;
    event.scheduler.status = full_engine::TerrainStreamingSchedulerTickStatus::Success;
    event.scheduler.decisionStatus = full_engine::TerrainStreamingSchedulerStatus::RunStreamingAndAssetLoadJobs;
    event.scheduler.decisionReason = full_engine::TerrainStreamingSchedulerReason::CatchUp;
    event.scheduler.budgetProfile = full_engine::TerrainStreamingBudgetProfile::CatchUp;
    event.scheduler.pendingLoadRequestCount = base + 42;
    event.scheduler.pendingJobCount = base + 43;
    event.scheduler.deferredWorkCount = base + 44;
    event.scheduler.peakDeferredWorkCount = base + 45;
    event.scheduler.runtimeBacklogCount = base + 46;
    event.scheduler.pressureCount = base + 47;
    event.scheduler.maxAssetLoadJobs = base + 48;
    event.scheduler.loadJobsRan = true;
    event.scheduler.loadJobsScheduled = true;
    event.scheduler.streamingRan = true;
    event.runtimeUpdateRan = true;
    event.setupRequestsBeforeRuntime = base + 1;
    event.residencyRequestsBeforeRuntime = base + 2;
    event.setupRequestsAfterRuntime = base + 3;
    event.residencyRequestsAfterRuntime = base + 4;
    event.streaming.manifestTerrainChunkCount = base + 5;
    event.streaming.readinessMissingHandleCount = base + 6;
    event.streaming.loadRequestCount = base + 7;
    event.streaming.queuedLoadRequestCount = base + 8;
    event.streaming.desiredSetupCount = base + 9;
    event.streaming.streamingPlanOperationCount = base + 10;
    event.streaming.queuedSetupAddCount = base + 11;
    event.streaming.queuedSetupRemoveCount = base + 12;
    event.streaming.queuedMakeResidentCount = base + 13;
    event.streaming.queuedMakeUnloadedCount = base + 14;
    event.streaming.deferredSetupAddCount = base + 15;
    event.streaming.deferredSetupRemoveCount = base + 16;
    event.streaming.deferredMakeResidentCount = base + 17;
    event.streaming.deferredMakeUnloadedCount = base + 18;
    event.streamingQueue.queuedSetupAddCount = base + 19;
    event.streamingQueue.queuedSetupRemoveCount = base + 20;
    event.streamingQueue.queuedMakeResidentCount = base + 21;
    event.streamingQueue.queuedMakeUnloadedCount = base + 22;
    event.streamingQueue.deferredSetupAddCount = base + 23;
    event.streamingQueue.deferredSetupRemoveCount = base + 24;
    event.streamingQueue.deferredMakeResidentCount = base + 25;
    event.streamingQueue.deferredMakeUnloadedCount = base + 26;
    event.streamingQueue.missingSetupDescCount = base + 27;
    event.runtimeLifecycle.createCount = base + 28;
    event.runtimeLifecycle.keepCount = base + 29;
    event.runtimeLifecycle.updateCount = base + 30;
    event.runtimeLifecycle.releaseCount = base + 31;
    event.runtimeLifecycle.deferredCreateCount = base + 32;
    event.runtimeLifecycle.deferredUpdateCount = base + 33;
    event.runtimeLifecycle.deferredReleaseCount = base + 34;
    event.runtimeSubmission.createdCount = base + 35;
    event.runtimeSubmission.updatedCount = base + 36;
    event.runtimeSubmission.destroyedCount = base + 37;
    event.runtimeSubmission.keptCount = base + 38;
    event.runtimeSubmission.skippedCount = base + 39;
    event.runtimeSubmission.rendererFailedCount = base + 40;
    event.runtimeSubmission.handleMapFailedCount = base + 41;
    return event;
}

void testEmptyFileImportsEmptyEvents(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_tick_import_empty_test.jsonl";
    std::remove(path);
    writeText(path, "");

    const full_engine::TerrainStreamingTickHistoryImport imported =
        full_engine::importTerrainStreamingTickHistoryJsonLines(path);

    expect(imported.result == full_engine::TerrainStreamingTickHistoryImportResult::Success, "empty import succeeds", failures);
    expect(imported.events.empty(), "empty import has no events", failures);
    std::remove(path);
}

void testExportImportRoundTrip(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_tick_import_round_trip_test.jsonl";
    std::remove(path);

    std::vector<full_engine::TerrainStreamingTickEvent> events;
    events.push_back(makeTick(12, 0));
    events.push_back(makeTick(13, 100));

    const full_engine::TerrainStreamingTickHistoryExportResult exported =
        full_engine::exportTerrainStreamingTickHistoryJsonLines(events, path);
    const full_engine::TerrainStreamingTickHistoryImport imported =
        full_engine::importTerrainStreamingTickHistoryJsonLines(path);

    expect(exported == full_engine::TerrainStreamingTickHistoryExportResult::Success, "round trip export succeeds", failures);
    expect(imported.result == full_engine::TerrainStreamingTickHistoryImportResult::Success, "round trip import succeeds", failures);
    expect(imported.events.size() == 2, "round trip imports two events", failures);
    if (imported.events.size() == 2)
    {
        expect(imported.events[0].sequence == 12, "round trip preserves first sequence", failures);
        expect(imported.events[1].sequence == 13, "round trip preserves second sequence", failures);
        expect(imported.events[0].streamingStatus == full_engine::TerrainStreamingManifestUpdateStatus::AssetLoadsPending, "round trip preserves streaming status", failures);
        expect(imported.events[0].runtimeStatus == full_engine::TerrainRuntimeUpdateStatus::ResidencyFailed, "round trip preserves runtime status", failures);
        expect(imported.events[0].budgetProfile == full_engine::TerrainStreamingBudgetProfile::CatchUp, "round trip preserves budget profile", failures);
        expect(imported.events[0].scheduler.hasSchedulerDecision, "round trip preserves scheduler presence", failures);
        expect(imported.events[0].scheduler.status == full_engine::TerrainStreamingSchedulerTickStatus::Success, "round trip preserves scheduler status", failures);
        expect(imported.events[0].scheduler.decisionStatus == full_engine::TerrainStreamingSchedulerStatus::RunStreamingAndAssetLoadJobs, "round trip preserves scheduler decision", failures);
        expect(imported.events[0].scheduler.decisionReason == full_engine::TerrainStreamingSchedulerReason::CatchUp, "round trip preserves scheduler reason", failures);
        expect(imported.events[1].scheduler.pendingLoadRequestCount == 142, "round trip preserves scheduler pressure", failures);
        expect(imported.events[0].scheduler.maxAssetLoadJobs == 48, "round trip preserves scheduler max jobs", failures);
        expect(imported.events[0].scheduler.loadJobsRan, "round trip preserves scheduler load phase", failures);
        expect(imported.events[0].scheduler.loadJobsScheduled, "round trip preserves scheduler schedule phase", failures);
        expect(imported.events[0].scheduler.streamingRan, "round trip preserves scheduler streaming phase", failures);
        expect(imported.events[0].runtimeUpdateRan, "round trip preserves runtime bool", failures);
        expect(imported.events[1].streaming.deferredMakeUnloadedCount == 118, "round trip preserves deferred streaming counter", failures);
        expect(imported.events[0].streamingQueue.missingSetupDescCount == 27, "round trip preserves queue counter", failures);
        expect(imported.events[1].runtimeLifecycle.deferredReleaseCount == 134, "round trip preserves lifecycle counter", failures);
        expect(imported.events[0].runtimeSubmission.handleMapFailedCount == 41, "round trip preserves submission counter", failures);
    }
    std::remove(path);
}

void testInvalidArgumentsAndIo(std::vector<std::string>& failures)
{
    expect(
        full_engine::importTerrainStreamingTickHistoryJsonLines(nullptr).result ==
            full_engine::TerrainStreamingTickHistoryImportResult::InvalidArgument,
        "null import path is invalid",
        failures);
    expect(
        full_engine::importTerrainStreamingTickHistoryJsonLines("").result ==
            full_engine::TerrainStreamingTickHistoryImportResult::InvalidArgument,
        "empty import path is invalid",
        failures);
    expect(
        full_engine::importTerrainStreamingTickHistoryJsonLines("missing_terrain_streaming_ticks.jsonl").result ==
            full_engine::TerrainStreamingTickHistoryImportResult::IoError,
        "missing import path is IO error",
        failures);
    expect(
        std::string(full_engine::terrainStreamingTickHistoryImportResultName(
            full_engine::TerrainStreamingTickHistoryImportResult::ParseError)) == "ParseError",
        "import result name is deterministic",
        failures);
}

void testMalformedLinesReturnParseError(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_tick_import_malformed_test.jsonl";
    std::remove(path);
    writeText(path, "{\"sequence\":1,\"streamingStatus\":\"Success\"}\n");

    const full_engine::TerrainStreamingTickHistoryImport imported =
        full_engine::importTerrainStreamingTickHistoryJsonLines(path);

    expect(imported.result == full_engine::TerrainStreamingTickHistoryImportResult::ParseError, "malformed import is parse error", failures);
    expect(imported.events.empty(), "malformed import returns no partial events", failures);
    std::remove(path);
}

void testUnknownStatusAndInvalidBoolReturnParseError(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_tick_import_bad_field_test.jsonl";
    std::remove(path);

    std::vector<full_engine::TerrainStreamingTickEvent> events;
    events.push_back(makeTick(1, 0));
    (void)full_engine::exportTerrainStreamingTickHistoryJsonLines(events, path);

    std::string content;
    {
        std::ifstream input(path);
        std::getline(input, content);
    }

    std::string badStatus = content;
    const char* const originalStatus = "\"AssetLoadsPending\"";
    const std::size_t statusStart = badStatus.find(originalStatus);
    if (statusStart != std::string::npos)
    {
        badStatus.replace(statusStart, std::string(originalStatus).size(), "\"Mystery\"");
    }
    writeText(path, (badStatus + "\n").c_str());
    expect(
        full_engine::importTerrainStreamingTickHistoryJsonLines(path).result ==
            full_engine::TerrainStreamingTickHistoryImportResult::ParseError,
        "unknown streaming status is parse error",
        failures);

    std::string badBool = content;
    const char* const originalBool = "\"runtimeUpdateRan\":true";
    const std::size_t boolStart = badBool.find(originalBool);
    if (boolStart != std::string::npos)
    {
        badBool.replace(boolStart, std::string(originalBool).size(), "\"runtimeUpdateRan\":maybe");
    }
    writeText(path, (badBool + "\n").c_str());
    expect(
        full_engine::importTerrainStreamingTickHistoryJsonLines(path).result ==
            full_engine::TerrainStreamingTickHistoryImportResult::ParseError,
        "invalid bool is parse error",
        failures);

    std::string badSchedulerStatus = content;
    const char* const originalSchedulerStatus = "\"schedulerDecisionStatus\":\"RunStreamingAndAssetLoadJobs\"";
    const std::size_t schedulerStatusStart = badSchedulerStatus.find(originalSchedulerStatus);
    if (schedulerStatusStart != std::string::npos)
    {
        badSchedulerStatus.replace(
            schedulerStatusStart,
            std::string(originalSchedulerStatus).size(),
            "\"schedulerDecisionStatus\":\"Teleport\"");
    }
    writeText(path, (badSchedulerStatus + "\n").c_str());
    expect(
        full_engine::importTerrainStreamingTickHistoryJsonLines(path).result ==
            full_engine::TerrainStreamingTickHistoryImportResult::ParseError,
        "unknown scheduler status is parse error",
        failures);

    std::remove(path);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testEmptyFileImportsEmptyEvents(failures);
    testExportImportRoundTrip(failures);
    testInvalidArgumentsAndIo(failures);
    testMalformedLinesReturnParseError(failures);
    testUnknownStatusAndInvalidBoolReturnParseError(failures);

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
