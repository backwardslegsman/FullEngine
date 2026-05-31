#include "engine/renderer_integration/TerrainRuntimeStateDiffExport.hpp"
#include "engine/renderer_integration/TerrainRuntimeStateDiffImport.hpp"

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

void writeText(const char* path, const char* text)
{
    std::ofstream output(path, std::ios::out | std::ios::trunc);
    output << text;
}

full_engine::TerrainRuntimeStateChange makeChange(
    const full_engine::ChunkId& id,
    const full_engine::TerrainRuntimeStateChangeType type)
{
    full_engine::TerrainRuntimeStateChange change;
    change.id = id;
    change.type = type;
    change.previousReadiness = full_engine::TerrainRuntimeChunkReadiness::Renderable;
    change.currentReadiness = full_engine::TerrainRuntimeChunkReadiness::NotResident;
    change.previousResidency = full_engine::ChunkResidencyState::Resident;
    change.currentResidency = full_engine::ChunkResidencyState::Unloaded;
    change.previousHasTerrainHandle = true;
    change.currentHasTerrainHandle = false;
    return change;
}

full_engine::TerrainRuntimeStateDiff makeDiff()
{
    full_engine::TerrainRuntimeStateDiff diff;
    diff.changes.push_back(makeChange({-1, 0, 2}, full_engine::TerrainRuntimeStateChangeType::ReadinessChanged));
    diff.changes.push_back(makeChange({4, 1, -3}, full_engine::TerrainRuntimeStateChangeType::HandlePresenceChanged));
    diff.summary.readinessChangedCount = 1;
    diff.summary.handlePresenceChangedCount = 1;
    return diff;
}

void expectChangeEqual(
    const full_engine::TerrainRuntimeStateChange& actual,
    const full_engine::TerrainRuntimeStateChange& expected,
    const char* label,
    std::vector<std::string>& failures)
{
    expect(actual.id == expected.id, label, failures);
    expect(actual.type == expected.type, label, failures);
    expect(actual.previousReadiness == expected.previousReadiness, label, failures);
    expect(actual.currentReadiness == expected.currentReadiness, label, failures);
    expect(actual.previousResidency == expected.previousResidency, label, failures);
    expect(actual.currentResidency == expected.currentResidency, label, failures);
    expect(actual.previousHasTerrainHandle == expected.previousHasTerrainHandle, label, failures);
    expect(actual.currentHasTerrainHandle == expected.currentHasTerrainHandle, label, failures);
}

void testEmptyDiffExportsAndImports(std::vector<std::string>& failures)
{
    const char* path = "terrain_runtime_state_diff_empty_test.jsonl";
    std::remove(path);

    const full_engine::TerrainRuntimeStateDiff diff;
    const full_engine::TerrainRuntimeStateDiffExportResult exported =
        full_engine::exportTerrainRuntimeStateDiffJsonLines(diff, path);
    const full_engine::TerrainRuntimeStateDiffImport imported =
        full_engine::importTerrainRuntimeStateDiffJsonLines(path);

    expect(exported == full_engine::TerrainRuntimeStateDiffExportResult::Success, "empty diff export succeeds", failures);
    expect(readFile(path).empty(), "empty diff export writes empty file", failures);
    expect(imported.result == full_engine::TerrainRuntimeStateDiffImportResult::Success, "empty diff import succeeds", failures);
    expect(imported.diff.changes.empty(), "empty diff import has no changes", failures);
    std::remove(path);
}

void testSingleChangeExportsFields(std::vector<std::string>& failures)
{
    const char* path = "terrain_runtime_state_diff_single_test.jsonl";
    std::remove(path);

    const full_engine::TerrainRuntimeStateChange change =
        makeChange({-12, 3, 45}, full_engine::TerrainRuntimeStateChangeType::ReadinessChanged);

    const full_engine::TerrainRuntimeStateDiffExportResult exported =
        full_engine::exportTerrainRuntimeStateDiffJsonLines(std::vector<full_engine::TerrainRuntimeStateChange>{change}, path);
    const std::string content = readFile(path);

    expect(exported == full_engine::TerrainRuntimeStateDiffExportResult::Success, "single diff export succeeds", failures);
    expect(content.find("\"x\":-12") != std::string::npos, "single diff exports x", failures);
    expect(content.find("\"y\":3") != std::string::npos, "single diff exports y", failures);
    expect(content.find("\"z\":45") != std::string::npos, "single diff exports z", failures);
    expect(content.find("\"type\":\"ReadinessChanged\"") != std::string::npos, "single diff exports type", failures);
    expect(content.find("\"previousReadiness\":\"Renderable\"") != std::string::npos, "single diff exports previous readiness", failures);
    expect(content.find("\"currentReadiness\":\"NotResident\"") != std::string::npos, "single diff exports current readiness", failures);
    expect(content.find("\"previousResidency\":\"Resident\"") != std::string::npos, "single diff exports previous residency", failures);
    expect(content.find("\"currentResidency\":\"Unloaded\"") != std::string::npos, "single diff exports current residency", failures);
    expect(content.find("\"previousHasTerrainHandle\":true") != std::string::npos, "single diff exports previous handle", failures);
    expect(content.find("\"currentHasTerrainHandle\":false") != std::string::npos, "single diff exports current handle", failures);
    std::remove(path);
}

void testExportImportRoundTripPreservesOrderAndSummary(std::vector<std::string>& failures)
{
    const char* path = "terrain_runtime_state_diff_round_trip_test.jsonl";
    std::remove(path);

    const full_engine::TerrainRuntimeStateDiff diff = makeDiff();

    const full_engine::TerrainRuntimeStateDiffExportResult exported =
        full_engine::exportTerrainRuntimeStateDiffJsonLines(diff, path);
    const full_engine::TerrainRuntimeStateDiffImport imported =
        full_engine::importTerrainRuntimeStateDiffJsonLines(path);

    expect(exported == full_engine::TerrainRuntimeStateDiffExportResult::Success, "round trip diff export succeeds", failures);
    expect(imported.result == full_engine::TerrainRuntimeStateDiffImportResult::Success, "round trip diff import succeeds", failures);
    expect(imported.diff.changes.size() == diff.changes.size(), "round trip imports all changes", failures);
    if (imported.diff.changes.size() == diff.changes.size())
    {
        expectChangeEqual(imported.diff.changes[0], diff.changes[0], "round trip preserves first change", failures);
        expectChangeEqual(imported.diff.changes[1], diff.changes[1], "round trip preserves second change", failures);
    }
    expect(imported.diff.summary.readinessChangedCount == 1, "round trip rebuilds readiness summary", failures);
    expect(imported.diff.summary.handlePresenceChangedCount == 1, "round trip rebuilds handle summary", failures);
    std::remove(path);
}

void testInvalidArgumentsAndIo(std::vector<std::string>& failures)
{
    const std::vector<full_engine::TerrainRuntimeStateChange> changes;

    expect(
        full_engine::exportTerrainRuntimeStateDiffJsonLines(changes, nullptr) ==
            full_engine::TerrainRuntimeStateDiffExportResult::InvalidArgument,
        "null export path is invalid",
        failures);
    expect(
        full_engine::exportTerrainRuntimeStateDiffJsonLines(changes, "") ==
            full_engine::TerrainRuntimeStateDiffExportResult::InvalidArgument,
        "empty export path is invalid",
        failures);
    expect(
        full_engine::exportTerrainRuntimeStateDiffJsonLines(changes, ".") ==
            full_engine::TerrainRuntimeStateDiffExportResult::IoError,
        "directory export path is IO error",
        failures);
    expect(
        full_engine::importTerrainRuntimeStateDiffJsonLines(nullptr).result ==
            full_engine::TerrainRuntimeStateDiffImportResult::InvalidArgument,
        "null import path is invalid",
        failures);
    expect(
        full_engine::importTerrainRuntimeStateDiffJsonLines("").result ==
            full_engine::TerrainRuntimeStateDiffImportResult::InvalidArgument,
        "empty import path is invalid",
        failures);
    expect(
        full_engine::importTerrainRuntimeStateDiffJsonLines("missing_terrain_runtime_state_diff.jsonl").result ==
            full_engine::TerrainRuntimeStateDiffImportResult::IoError,
        "missing import path is IO error",
        failures);
    expect(
        std::string(full_engine::terrainRuntimeStateDiffExportResultName(
            full_engine::TerrainRuntimeStateDiffExportResult::IoError)) == "IoError",
        "diff export result names are deterministic",
        failures);
    expect(
        std::string(full_engine::terrainRuntimeStateDiffImportResultName(
            full_engine::TerrainRuntimeStateDiffImportResult::ParseError)) == "ParseError",
        "diff import result names are deterministic",
        failures);
}

void testUnknownFieldsAreIgnored(std::vector<std::string>& failures)
{
    const char* path = "terrain_runtime_state_diff_unknown_field_test.jsonl";
    std::remove(path);
    writeText(
        path,
        "{\"ignored\":99,\"x\":1,\"y\":2,\"z\":3,\"type\":\"Added\",\"previousReadiness\":\"MissingRegistry\",\"currentReadiness\":\"Renderable\",\"previousResidency\":\"Unloaded\",\"currentResidency\":\"Resident\",\"previousHasTerrainHandle\":false,\"currentHasTerrainHandle\":true}\n");

    const full_engine::TerrainRuntimeStateDiffImport imported =
        full_engine::importTerrainRuntimeStateDiffJsonLines(path);

    expect(imported.result == full_engine::TerrainRuntimeStateDiffImportResult::Success, "unknown fields are ignored", failures);
    expect(imported.diff.changes.size() == 1, "unknown field import keeps change", failures);
    expect(imported.diff.summary.addedCount == 1, "unknown field import counts change", failures);
    std::remove(path);
}

void testMalformedLinesReturnParseError(std::vector<std::string>& failures)
{
    const char* path = "terrain_runtime_state_diff_malformed_test.jsonl";
    std::remove(path);
    writeText(path, "{\"x\":1,\"type\":\"Added\"}\n");

    const full_engine::TerrainRuntimeStateDiffImport imported =
        full_engine::importTerrainRuntimeStateDiffJsonLines(path);

    expect(imported.result == full_engine::TerrainRuntimeStateDiffImportResult::ParseError, "malformed diff import is parse error", failures);
    expect(imported.diff.changes.empty(), "malformed diff import returns no partial changes", failures);
    std::remove(path);
}

void testInvalidValuesReturnParseError(std::vector<std::string>& failures)
{
    const char* path = "terrain_runtime_state_diff_invalid_values_test.jsonl";
    std::remove(path);

    writeText(
        path,
        "{\"x\":1,\"y\":2,\"z\":3,\"type\":\"Mystery\",\"previousReadiness\":\"MissingRegistry\",\"currentReadiness\":\"Renderable\",\"previousResidency\":\"Unloaded\",\"currentResidency\":\"Resident\",\"previousHasTerrainHandle\":false,\"currentHasTerrainHandle\":true}\n");
    expect(
        full_engine::importTerrainRuntimeStateDiffJsonLines(path).result ==
            full_engine::TerrainRuntimeStateDiffImportResult::ParseError,
        "invalid change type is parse error",
        failures);

    writeText(
        path,
        "{\"x\":1,\"y\":2,\"z\":3,\"type\":\"Added\",\"previousReadiness\":\"Nope\",\"currentReadiness\":\"Renderable\",\"previousResidency\":\"Unloaded\",\"currentResidency\":\"Resident\",\"previousHasTerrainHandle\":false,\"currentHasTerrainHandle\":true}\n");
    expect(
        full_engine::importTerrainRuntimeStateDiffJsonLines(path).result ==
            full_engine::TerrainRuntimeStateDiffImportResult::ParseError,
        "invalid readiness is parse error",
        failures);

    writeText(
        path,
        "{\"x\":1,\"y\":2,\"z\":3,\"type\":\"Added\",\"previousReadiness\":\"MissingRegistry\",\"currentReadiness\":\"Renderable\",\"previousResidency\":\"Gone\",\"currentResidency\":\"Resident\",\"previousHasTerrainHandle\":false,\"currentHasTerrainHandle\":true}\n");
    expect(
        full_engine::importTerrainRuntimeStateDiffJsonLines(path).result ==
            full_engine::TerrainRuntimeStateDiffImportResult::ParseError,
        "invalid residency is parse error",
        failures);

    writeText(
        path,
        "{\"x\":1,\"y\":2,\"z\":3,\"type\":\"Added\",\"previousReadiness\":\"MissingRegistry\",\"currentReadiness\":\"Renderable\",\"previousResidency\":\"Unloaded\",\"currentResidency\":\"Resident\",\"previousHasTerrainHandle\":maybe,\"currentHasTerrainHandle\":true}\n");
    expect(
        full_engine::importTerrainRuntimeStateDiffJsonLines(path).result ==
            full_engine::TerrainRuntimeStateDiffImportResult::ParseError,
        "invalid bool is parse error",
        failures);

    writeText(
        path,
        "{\"x\":2147483648,\"y\":2,\"z\":3,\"type\":\"Added\",\"previousReadiness\":\"MissingRegistry\",\"currentReadiness\":\"Renderable\",\"previousResidency\":\"Unloaded\",\"currentResidency\":\"Resident\",\"previousHasTerrainHandle\":false,\"currentHasTerrainHandle\":true}\n");
    expect(
        full_engine::importTerrainRuntimeStateDiffJsonLines(path).result ==
            full_engine::TerrainRuntimeStateDiffImportResult::ParseError,
        "overflow coordinate is parse error",
        failures);

    std::remove(path);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testEmptyDiffExportsAndImports(failures);
    testSingleChangeExportsFields(failures);
    testExportImportRoundTripPreservesOrderAndSummary(failures);
    testInvalidArgumentsAndIo(failures);
    testUnknownFieldsAreIgnored(failures);
    testMalformedLinesReturnParseError(failures);
    testInvalidValuesReturnParseError(failures);

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
