#include "engine/renderer_integration/TerrainRuntimeEventExport.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

std::string readFile(const char* path)
{
    std::ifstream input(path);
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

full_engine::TerrainRuntimeEvent makeEvent(
    const std::uint64_t sequence,
    const full_engine::TerrainRuntimeUpdateStatus status)
{
    full_engine::TerrainRuntimeEvent event;
    event.sequence = sequence;
    event.status = status;
    event.diagnostics.setupRequests.requestCount = 2;
    event.diagnostics.setupRequests.addCount = 1;
    event.diagnostics.setupRequests.removeCount = 1;
    event.diagnostics.setupRequests.summary.appliedCount = 1;
    event.diagnostics.setupRequests.summary.invalidArgumentCount = 1;
    event.diagnostics.residencyRequests.requestCount = 3;
    event.diagnostics.residencyRequests.makeResidentCount = 2;
    event.diagnostics.residencyRequests.makeUnloadedCount = 1;
    event.diagnostics.residencyRequests.summary.notFoundCount = 1;
    event.diagnostics.pipeline.handleCount = 4;
    event.diagnostics.pipeline.lifecycle.createCount = 5;
    event.diagnostics.pipeline.commands.destroyCount = 6;
    event.diagnostics.pipeline.descriptors.readyCount = 7;
    event.diagnostics.pipeline.submission.createdCount = 8;
    event.diagnostics.pipeline.submission.rendererFailedCount = 9;
    return event;
}

void testEmptyLogExportsEmptyFile(std::vector<std::string>& failures)
{
    const char* path = "terrain_runtime_event_export_empty_test.jsonl";
    std::remove(path);

    full_engine::TerrainRuntimeEventLog log;
    const full_engine::TerrainRuntimeEventExportResult result =
        full_engine::exportTerrainRuntimeEventsJsonLines(log, path);

    expect(result == full_engine::TerrainRuntimeEventExportResult::Success, "empty log export succeeds", failures);
    expect(readFile(path).empty(), "empty log export writes empty file", failures);
    std::remove(path);
}

void testSingleEventExportsCounters(std::vector<std::string>& failures)
{
    const char* path = "terrain_runtime_event_export_single_test.jsonl";
    std::remove(path);

    std::vector<full_engine::TerrainRuntimeEvent> events;
    events.push_back(makeEvent(12, full_engine::TerrainRuntimeUpdateStatus::PipelineFailed));

    const full_engine::TerrainRuntimeEventExportResult result =
        full_engine::exportTerrainRuntimeEventsJsonLines(events, path);
    const std::string content = readFile(path);

    expect(result == full_engine::TerrainRuntimeEventExportResult::Success, "single event export succeeds", failures);
    expect(content.find("\"sequence\":12") != std::string::npos, "single event exports sequence", failures);
    expect(content.find("\"status\":\"PipelineFailed\"") != std::string::npos, "single event exports status", failures);
    expect(content.find("\"setupAppliedCount\":1") != std::string::npos, "single event exports setup counter", failures);
    expect(content.find("\"setupInvalidArgumentCount\":1") != std::string::npos, "single event exports invalid setup counter", failures);
    expect(content.find("\"residencyNotFoundCount\":1") != std::string::npos, "single event exports residency counter", failures);
    expect(content.find("\"handleCount\":4") != std::string::npos, "single event exports handle count", failures);
    expect(content.find("\"lifecycleCreateCount\":5") != std::string::npos, "single event exports lifecycle counter", failures);
    expect(content.find("\"commandDestroyCount\":6") != std::string::npos, "single event exports command counter", failures);
    expect(content.find("\"descriptorReadyCount\":7") != std::string::npos, "single event exports descriptor counter", failures);
    expect(content.find("\"submissionCreatedCount\":8") != std::string::npos, "single event exports submission counter", failures);
    expect(content.find("\"submissionRendererFailedCount\":9") != std::string::npos, "single event exports failure counter", failures);
    std::remove(path);
}

void testMultipleEventsPreserveOrder(std::vector<std::string>& failures)
{
    const char* path = "terrain_runtime_event_export_order_test.jsonl";
    std::remove(path);

    std::vector<full_engine::TerrainRuntimeEvent> events;
    events.push_back(makeEvent(2, full_engine::TerrainRuntimeUpdateStatus::Success));
    events.push_back(makeEvent(3, full_engine::TerrainRuntimeUpdateStatus::SetupFailed));

    const full_engine::TerrainRuntimeEventExportResult result =
        full_engine::exportTerrainRuntimeEventsJsonLines(events, path);
    const std::string content = readFile(path);

    const std::size_t first = content.find("\"sequence\":2");
    const std::size_t second = content.find("\"sequence\":3");
    expect(result == full_engine::TerrainRuntimeEventExportResult::Success, "multiple event export succeeds", failures);
    expect(first != std::string::npos, "multiple event export writes first event", failures);
    expect(second != std::string::npos, "multiple event export writes second event", failures);
    expect(first < second, "multiple event export preserves order", failures);
    expect(content.find("\"status\":\"Success\"") != std::string::npos, "success status serializes deterministically", failures);
    expect(content.find("\"status\":\"SetupFailed\"") != std::string::npos, "setup failure status serializes deterministically", failures);
    std::remove(path);
}

void testInvalidAndIoFailures(std::vector<std::string>& failures)
{
    std::vector<full_engine::TerrainRuntimeEvent> events;

    expect(
        full_engine::exportTerrainRuntimeEventsJsonLines(events, nullptr) ==
            full_engine::TerrainRuntimeEventExportResult::InvalidArgument,
        "null path is invalid",
        failures);
    expect(
        full_engine::exportTerrainRuntimeEventsJsonLines(events, "") ==
            full_engine::TerrainRuntimeEventExportResult::InvalidArgument,
        "empty path is invalid",
        failures);
    expect(
        full_engine::exportTerrainRuntimeEventsJsonLines(events, ".") ==
            full_engine::TerrainRuntimeEventExportResult::IoError,
        "directory path returns IO error",
        failures);
    expect(
        std::string(full_engine::terrainRuntimeEventExportResultName(
            full_engine::TerrainRuntimeEventExportResult::Success)) == "Success",
        "export result name is deterministic",
        failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testEmptyLogExportsEmptyFile(failures);
    testSingleEventExportsCounters(failures);
    testMultipleEventsPreserveOrder(failures);
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
