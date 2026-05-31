#include "engine/renderer_integration/TerrainRuntimeEventExport.hpp"
#include "engine/renderer_integration/TerrainRuntimeEventImport.hpp"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
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

full_engine::TerrainRuntimeEvent makeEvent(
    const std::uint64_t sequence,
    const full_engine::TerrainRuntimeUpdateStatus status,
    const std::size_t base)
{
    full_engine::TerrainRuntimeEvent event;
    event.sequence = sequence;
    event.status = status;
    event.diagnostics.setupRequests.requestCount = base + 1;
    event.diagnostics.setupRequests.addCount = base + 2;
    event.diagnostics.setupRequests.removeCount = base + 3;
    event.diagnostics.setupRequests.summary.appliedCount = base + 4;
    event.diagnostics.setupRequests.summary.alreadySatisfiedCount = base + 5;
    event.diagnostics.setupRequests.summary.notFoundCount = base + 6;
    event.diagnostics.setupRequests.summary.invalidArgumentCount = base + 7;
    event.diagnostics.setupRequests.summary.partialFailureCount = base + 8;
    event.diagnostics.residencyRequests.requestCount = base + 9;
    event.diagnostics.residencyRequests.makeResidentCount = base + 10;
    event.diagnostics.residencyRequests.makeUnloadedCount = base + 11;
    event.diagnostics.residencyRequests.summary.appliedCount = base + 12;
    event.diagnostics.residencyRequests.summary.alreadySatisfiedCount = base + 13;
    event.diagnostics.residencyRequests.summary.notFoundCount = base + 14;
    event.diagnostics.residencyRequests.summary.invalidTransitionCount = base + 15;
    event.diagnostics.pipeline.handleCount = base + 16;
    event.diagnostics.pipeline.snapshotReadyCount = base + 17;
    event.diagnostics.pipeline.snapshotNotResidentCount = base + 18;
    event.diagnostics.pipeline.snapshotMissingChunkCount = base + 19;
    event.diagnostics.pipeline.snapshotInvalidBoundsCount = base + 20;
    event.diagnostics.pipeline.snapshotOutOfRangeCount = base + 21;
    event.diagnostics.pipeline.snapshotInvalidInputCount = base + 22;
    event.diagnostics.pipeline.prep.readyCount = base + 23;
    event.diagnostics.pipeline.prep.skippedNotResidentCount = base + 24;
    event.diagnostics.pipeline.prep.skippedMissingChunkCount = base + 25;
    event.diagnostics.pipeline.prep.skippedInvalidBoundsCount = base + 26;
    event.diagnostics.pipeline.prep.skippedOutOfRangeCount = base + 27;
    event.diagnostics.pipeline.prep.invalidInputCount = base + 28;
    event.diagnostics.pipeline.lifecycle.createCount = base + 29;
    event.diagnostics.pipeline.lifecycle.keepCount = base + 30;
    event.diagnostics.pipeline.lifecycle.updateCount = base + 31;
    event.diagnostics.pipeline.lifecycle.releaseCount = base + 32;
    event.diagnostics.pipeline.commands.createCount = base + 33;
    event.diagnostics.pipeline.commands.keepCount = base + 34;
    event.diagnostics.pipeline.commands.updateCount = base + 35;
    event.diagnostics.pipeline.commands.destroyCount = base + 36;
    event.diagnostics.pipeline.descriptors.readyCount = base + 37;
    event.diagnostics.pipeline.descriptors.ignoredCount = base + 38;
    event.diagnostics.pipeline.descriptors.missingResourcesCount = base + 39;
    event.diagnostics.pipeline.descriptors.invalidResourcesCount = base + 40;
    event.diagnostics.pipeline.submission.createdCount = base + 41;
    event.diagnostics.pipeline.submission.updatedCount = base + 42;
    event.diagnostics.pipeline.submission.destroyedCount = base + 43;
    event.diagnostics.pipeline.submission.keptCount = base + 44;
    event.diagnostics.pipeline.submission.skippedCount = base + 45;
    event.diagnostics.pipeline.submission.rendererFailedCount = base + 46;
    event.diagnostics.pipeline.submission.handleMapFailedCount = base + 47;
    return event;
}

void writeText(const char* path, const char* text)
{
    std::ofstream output(path, std::ios::out | std::ios::trunc);
    output << text;
}

void testEmptyFileImportsEmptyEvents(std::vector<std::string>& failures)
{
    const char* path = "terrain_runtime_event_import_empty_test.jsonl";
    std::remove(path);
    writeText(path, "");

    const full_engine::TerrainRuntimeEventImport imported =
        full_engine::importTerrainRuntimeEventsJsonLines(path);

    expect(imported.result == full_engine::TerrainRuntimeEventImportResult::Success, "empty import succeeds", failures);
    expect(imported.events.empty(), "empty import has no events", failures);
    std::remove(path);
}

void testExportImportRoundTrip(std::vector<std::string>& failures)
{
    const char* path = "terrain_runtime_event_import_round_trip_test.jsonl";
    std::remove(path);

    std::vector<full_engine::TerrainRuntimeEvent> events;
    events.push_back(makeEvent(21, full_engine::TerrainRuntimeUpdateStatus::Success, 0));
    events.push_back(makeEvent(22, full_engine::TerrainRuntimeUpdateStatus::PipelineFailed, 100));

    const full_engine::TerrainRuntimeEventExportResult exported =
        full_engine::exportTerrainRuntimeEventsJsonLines(events, path);
    const full_engine::TerrainRuntimeEventImport imported =
        full_engine::importTerrainRuntimeEventsJsonLines(path);

    expect(exported == full_engine::TerrainRuntimeEventExportResult::Success, "round trip export succeeds", failures);
    expect(imported.result == full_engine::TerrainRuntimeEventImportResult::Success, "round trip import succeeds", failures);
    expect(imported.events.size() == 2, "round trip imports two events", failures);
    if (imported.events.size() == 2)
    {
        expect(imported.events[0].sequence == 21, "round trip preserves first sequence", failures);
        expect(imported.events[1].sequence == 22, "round trip preserves second sequence", failures);
        expect(imported.events[0].status == full_engine::TerrainRuntimeUpdateStatus::Success, "round trip preserves success status", failures);
        expect(imported.events[1].status == full_engine::TerrainRuntimeUpdateStatus::PipelineFailed, "round trip preserves failure status", failures);
        expect(imported.events[0].diagnostics.setupRequests.summary.partialFailureCount == 8, "round trip preserves setup counter", failures);
        expect(imported.events[1].diagnostics.residencyRequests.summary.invalidTransitionCount == 115, "round trip preserves residency counter", failures);
        expect(imported.events[0].diagnostics.pipeline.prep.invalidInputCount == 28, "round trip preserves prep counter", failures);
        expect(imported.events[1].diagnostics.pipeline.lifecycle.releaseCount == 132, "round trip preserves lifecycle counter", failures);
        expect(imported.events[0].diagnostics.pipeline.commands.destroyCount == 36, "round trip preserves command counter", failures);
        expect(imported.events[1].diagnostics.pipeline.descriptors.invalidResourcesCount == 140, "round trip preserves descriptor counter", failures);
        expect(imported.events[0].diagnostics.pipeline.submission.handleMapFailedCount == 47, "round trip preserves submission counter", failures);
    }
    std::remove(path);
}

void testInvalidArgumentsAndIo(std::vector<std::string>& failures)
{
    expect(
        full_engine::importTerrainRuntimeEventsJsonLines(nullptr).result ==
            full_engine::TerrainRuntimeEventImportResult::InvalidArgument,
        "null import path is invalid",
        failures);
    expect(
        full_engine::importTerrainRuntimeEventsJsonLines("").result ==
            full_engine::TerrainRuntimeEventImportResult::InvalidArgument,
        "empty import path is invalid",
        failures);
    expect(
        full_engine::importTerrainRuntimeEventsJsonLines("missing_terrain_runtime_events.jsonl").result ==
            full_engine::TerrainRuntimeEventImportResult::IoError,
        "missing import path is IO error",
        failures);
    expect(
        std::string(full_engine::terrainRuntimeEventImportResultName(
            full_engine::TerrainRuntimeEventImportResult::ParseError)) == "ParseError",
        "import result names are deterministic",
        failures);
}

void testMalformedLinesReturnParseError(std::vector<std::string>& failures)
{
    const char* path = "terrain_runtime_event_import_malformed_test.jsonl";
    std::remove(path);
    writeText(path, "{\"sequence\":1,\"status\":\"Success\"}\n");

    const full_engine::TerrainRuntimeEventImport imported =
        full_engine::importTerrainRuntimeEventsJsonLines(path);

    expect(imported.result == full_engine::TerrainRuntimeEventImportResult::ParseError, "malformed import is parse error", failures);
    expect(imported.events.empty(), "malformed import returns no partial events", failures);
    std::remove(path);
}

void testUnknownStatusReturnsParseError(std::vector<std::string>& failures)
{
    const char* path = "terrain_runtime_event_import_bad_status_test.jsonl";
    std::remove(path);

    std::vector<full_engine::TerrainRuntimeEvent> events;
    events.push_back(makeEvent(5, full_engine::TerrainRuntimeUpdateStatus::Success, 0));
    (void)full_engine::exportTerrainRuntimeEventsJsonLines(events, path);

    std::string content;
    {
        std::ifstream input(path);
        std::getline(input, content);
    }
    const std::size_t statusStart = content.find("\"Success\"");
    if (statusStart != std::string::npos)
    {
        content.replace(statusStart, 9, "\"Mystery\"");
    }
    writeText(path, (content + "\n").c_str());

    const full_engine::TerrainRuntimeEventImport imported =
        full_engine::importTerrainRuntimeEventsJsonLines(path);

    expect(imported.result == full_engine::TerrainRuntimeEventImportResult::ParseError, "unknown status is parse error", failures);
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
    testUnknownStatusReturnsParseError(failures);

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
