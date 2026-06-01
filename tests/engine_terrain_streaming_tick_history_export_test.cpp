#include "engine/streaming/TerrainStreamingTickHistoryExport.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
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

std::string readFile(const char* const path)
{
    std::ifstream input(path);
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

full_engine::TerrainStreamingTickEvent makeTick(const std::uint64_t sequence)
{
    full_engine::TerrainStreamingTickEvent event;
    event.sequence = sequence;
    event.budgetProfile = full_engine::TerrainStreamingBudgetProfile::CatchUp;
    event.streamingStatus = full_engine::TerrainStreamingManifestUpdateStatus::Success;
    event.runtimeStatus = full_engine::TerrainRuntimeUpdateStatus::PipelineFailed;
    event.runtimeUpdateRan = true;
    event.setupRequestsBeforeRuntime = 1;
    event.residencyRequestsBeforeRuntime = 2;
    event.setupRequestsAfterRuntime = 3;
    event.residencyRequestsAfterRuntime = 4;
    event.streaming.manifestTerrainChunkCount = 5;
    event.streaming.readinessMissingHandleCount = 6;
    event.streaming.loadRequestCount = 7;
    event.streaming.queuedLoadRequestCount = 8;
    event.streaming.desiredSetupCount = 9;
    event.streaming.streamingPlanOperationCount = 10;
    event.streaming.queuedSetupAddCount = 11;
    event.streaming.queuedSetupRemoveCount = 12;
    event.streaming.queuedMakeResidentCount = 13;
    event.streaming.queuedMakeUnloadedCount = 14;
    event.streaming.deferredSetupAddCount = 15;
    event.streaming.deferredSetupRemoveCount = 16;
    event.streaming.deferredMakeResidentCount = 17;
    event.streaming.deferredMakeUnloadedCount = 18;
    event.streamingQueue.deferredSetupAddCount = 19;
    event.streamingQueue.missingSetupDescCount = 20;
    event.runtimeLifecycle.createCount = 21;
    event.runtimeLifecycle.updateCount = 22;
    event.runtimeLifecycle.releaseCount = 23;
    event.runtimeLifecycle.deferredCreateCount = 24;
    event.runtimeLifecycle.deferredUpdateCount = 25;
    event.runtimeLifecycle.deferredReleaseCount = 26;
    event.runtimeSubmission.createdCount = 27;
    event.runtimeSubmission.updatedCount = 28;
    event.runtimeSubmission.destroyedCount = 29;
    event.runtimeSubmission.skippedCount = 30;
    event.runtimeSubmission.rendererFailedCount = 31;
    event.runtimeSubmission.handleMapFailedCount = 32;
    return event;
}

void testEmptyHistoryExportsEmptyFile(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_tick_export_empty_test.jsonl";
    std::remove(path);

    full_engine::TerrainStreamingTickHistory history;
    const full_engine::TerrainStreamingTickHistoryExportResult result =
        full_engine::exportTerrainStreamingTickHistoryJsonLines(history, path);

    expect(result == full_engine::TerrainStreamingTickHistoryExportResult::Success, "empty tick history export succeeds", failures);
    expect(readFile(path).empty(), "empty tick history export writes empty file", failures);
    std::remove(path);
}

void testSingleTickExportsCounters(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_tick_export_single_test.jsonl";
    std::remove(path);

    std::vector<full_engine::TerrainStreamingTickEvent> ticks;
    ticks.push_back(makeTick(42));

    const full_engine::TerrainStreamingTickHistoryExportResult result =
        full_engine::exportTerrainStreamingTickHistoryJsonLines(ticks, path);
    const std::string content = readFile(path);

    expect(result == full_engine::TerrainStreamingTickHistoryExportResult::Success, "single tick export succeeds", failures);
    expect(content.find("\"sequence\":42") != std::string::npos, "single tick exports sequence", failures);
    expect(content.find("\"streamingStatus\":\"Success\"") != std::string::npos, "single tick exports streaming status", failures);
    expect(content.find("\"runtimeStatus\":\"PipelineFailed\"") != std::string::npos, "single tick exports runtime status", failures);
    expect(content.find("\"budgetProfile\":\"CatchUp\"") != std::string::npos, "single tick exports budget profile", failures);
    expect(content.find("\"runtimeUpdateRan\":true") != std::string::npos, "single tick exports runtime bool", failures);
    expect(content.find("\"setupRequestsBeforeRuntime\":1") != std::string::npos, "single tick exports setup before count", failures);
    expect(content.find("\"streamingManifestTerrainChunkCount\":5") != std::string::npos, "single tick exports streaming counter", failures);
    expect(content.find("\"streamingDeferredMakeUnloadedCount\":18") != std::string::npos, "single tick exports deferred streaming counter", failures);
    expect(content.find("\"queueDeferredSetupAddCount\":19") != std::string::npos, "single tick exports queue counter", failures);
    expect(content.find("\"lifecycleDeferredReleaseCount\":26") != std::string::npos, "single tick exports lifecycle counter", failures);
    expect(content.find("\"submissionRendererFailedCount\":31") != std::string::npos, "single tick exports submission counter", failures);
    std::remove(path);
}

void testHistoryExportPreservesChronologicalOrder(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_tick_export_order_test.jsonl";
    std::remove(path);

    full_engine::TerrainStreamingTickHistory history;
    history.append(makeTick(99));
    history.append(makeTick(99));

    const full_engine::TerrainStreamingTickHistoryExportResult result =
        full_engine::exportTerrainStreamingTickHistoryJsonLines(history, path);
    const std::string content = readFile(path);

    const std::size_t first = content.find("\"sequence\":1");
    const std::size_t second = content.find("\"sequence\":2");
    expect(result == full_engine::TerrainStreamingTickHistoryExportResult::Success, "history export succeeds", failures);
    expect(first != std::string::npos, "history export writes first tick", failures);
    expect(second != std::string::npos, "history export writes second tick", failures);
    expect(first < second, "history export preserves chronological order", failures);
    std::remove(path);
}

void testInvalidAndIoFailures(std::vector<std::string>& failures)
{
    const std::vector<full_engine::TerrainStreamingTickEvent> ticks;

    expect(
        full_engine::exportTerrainStreamingTickHistoryJsonLines(ticks, nullptr) ==
            full_engine::TerrainStreamingTickHistoryExportResult::InvalidArgument,
        "null path is invalid",
        failures);
    expect(
        full_engine::exportTerrainStreamingTickHistoryJsonLines(ticks, "") ==
            full_engine::TerrainStreamingTickHistoryExportResult::InvalidArgument,
        "empty path is invalid",
        failures);
    expect(
        full_engine::exportTerrainStreamingTickHistoryJsonLines(ticks, ".") ==
            full_engine::TerrainStreamingTickHistoryExportResult::IoError,
        "directory path returns IO error",
        failures);
    expect(
        std::string(full_engine::terrainStreamingTickHistoryExportResultName(
            full_engine::TerrainStreamingTickHistoryExportResult::Success)) == "Success",
        "export result name is deterministic",
        failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testEmptyHistoryExportsEmptyFile(failures);
    testSingleTickExportsCounters(failures);
    testHistoryExportPreservesChronologicalOrder(failures);
    testInvalidAndIoFailures(failures);

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
