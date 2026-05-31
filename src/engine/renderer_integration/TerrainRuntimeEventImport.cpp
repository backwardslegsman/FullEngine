#include "engine/renderer_integration/TerrainRuntimeEventImport.hpp"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>

namespace full_engine
{
namespace
{
bool parseStatus(const std::string& value, TerrainRuntimeUpdateStatus& status) noexcept
{
    if (value == "Success")
    {
        status = TerrainRuntimeUpdateStatus::Success;
        return true;
    }
    if (value == "SetupFailed")
    {
        status = TerrainRuntimeUpdateStatus::SetupFailed;
        return true;
    }
    if (value == "ResidencyFailed")
    {
        status = TerrainRuntimeUpdateStatus::ResidencyFailed;
        return true;
    }
    if (value == "PipelineFailed")
    {
        status = TerrainRuntimeUpdateStatus::PipelineFailed;
        return true;
    }

    return false;
}

bool parseStringField(const std::string& line, const char* name, std::string& value)
{
    const std::string prefix = std::string("\"") + name + "\":\"";
    const std::size_t start = line.find(prefix);
    if (start == std::string::npos)
    {
        return false;
    }

    const std::size_t valueStart = start + prefix.size();
    const std::size_t valueEnd = line.find('"', valueStart);
    if (valueEnd == std::string::npos)
    {
        return false;
    }

    value = line.substr(valueStart, valueEnd - valueStart);
    return true;
}

bool parseUnsignedField(const std::string& line, const char* name, std::uint64_t& value)
{
    const std::string prefix = std::string("\"") + name + "\":";
    const std::size_t start = line.find(prefix);
    if (start == std::string::npos)
    {
        return false;
    }

    std::size_t cursor = start + prefix.size();
    if (cursor >= line.size() || !std::isdigit(static_cast<unsigned char>(line[cursor])))
    {
        return false;
    }

    std::uint64_t parsed = 0;
    while (cursor < line.size() && std::isdigit(static_cast<unsigned char>(line[cursor])))
    {
        const std::uint64_t digit = static_cast<std::uint64_t>(line[cursor] - '0');
        if (parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U)
        {
            return false;
        }
        parsed = parsed * 10U + digit;
        ++cursor;
    }

    if (cursor >= line.size() || (line[cursor] != ',' && line[cursor] != '}'))
    {
        return false;
    }

    value = parsed;
    return true;
}

bool parseSizeField(const std::string& line, const char* name, std::size_t& value)
{
    std::uint64_t parsed = 0;
    if (!parseUnsignedField(line, name, parsed) ||
        parsed > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    {
        return false;
    }

    value = static_cast<std::size_t>(parsed);
    return true;
}

bool parseEvent(const std::string& line, TerrainRuntimeEvent& event)
{
    if (line.size() < 2 || line.front() != '{' || line.back() != '}')
    {
        return false;
    }

    std::string status;
    if (!parseUnsignedField(line, "sequence", event.sequence) ||
        !parseStringField(line, "status", status) ||
        !parseStatus(status, event.status))
    {
        return false;
    }

    TerrainSetupRequestDiagnostics& setup = event.diagnostics.setupRequests;
    TerrainResidencyRequestDiagnostics& residency = event.diagnostics.residencyRequests;
    TerrainPipelineDiagnostics& pipeline = event.diagnostics.pipeline;

    return
        parseSizeField(line, "setupRequestCount", setup.requestCount) &&
        parseSizeField(line, "setupAddCount", setup.addCount) &&
        parseSizeField(line, "setupRemoveCount", setup.removeCount) &&
        parseSizeField(line, "setupAppliedCount", setup.summary.appliedCount) &&
        parseSizeField(line, "setupAlreadySatisfiedCount", setup.summary.alreadySatisfiedCount) &&
        parseSizeField(line, "setupNotFoundCount", setup.summary.notFoundCount) &&
        parseSizeField(line, "setupInvalidArgumentCount", setup.summary.invalidArgumentCount) &&
        parseSizeField(line, "setupPartialFailureCount", setup.summary.partialFailureCount) &&
        parseSizeField(line, "residencyRequestCount", residency.requestCount) &&
        parseSizeField(line, "residencyMakeResidentCount", residency.makeResidentCount) &&
        parseSizeField(line, "residencyMakeUnloadedCount", residency.makeUnloadedCount) &&
        parseSizeField(line, "residencyAppliedCount", residency.summary.appliedCount) &&
        parseSizeField(line, "residencyAlreadySatisfiedCount", residency.summary.alreadySatisfiedCount) &&
        parseSizeField(line, "residencyNotFoundCount", residency.summary.notFoundCount) &&
        parseSizeField(line, "residencyInvalidTransitionCount", residency.summary.invalidTransitionCount) &&
        parseSizeField(line, "handleCount", pipeline.handleCount) &&
        parseSizeField(line, "snapshotReadyCount", pipeline.snapshotReadyCount) &&
        parseSizeField(line, "snapshotNotResidentCount", pipeline.snapshotNotResidentCount) &&
        parseSizeField(line, "snapshotMissingChunkCount", pipeline.snapshotMissingChunkCount) &&
        parseSizeField(line, "snapshotInvalidBoundsCount", pipeline.snapshotInvalidBoundsCount) &&
        parseSizeField(line, "snapshotOutOfRangeCount", pipeline.snapshotOutOfRangeCount) &&
        parseSizeField(line, "snapshotInvalidInputCount", pipeline.snapshotInvalidInputCount) &&
        parseSizeField(line, "prepReadyCount", pipeline.prep.readyCount) &&
        parseSizeField(line, "prepSkippedNotResidentCount", pipeline.prep.skippedNotResidentCount) &&
        parseSizeField(line, "prepSkippedMissingChunkCount", pipeline.prep.skippedMissingChunkCount) &&
        parseSizeField(line, "prepSkippedInvalidBoundsCount", pipeline.prep.skippedInvalidBoundsCount) &&
        parseSizeField(line, "prepSkippedOutOfRangeCount", pipeline.prep.skippedOutOfRangeCount) &&
        parseSizeField(line, "prepInvalidInputCount", pipeline.prep.invalidInputCount) &&
        parseSizeField(line, "lifecycleCreateCount", pipeline.lifecycle.createCount) &&
        parseSizeField(line, "lifecycleKeepCount", pipeline.lifecycle.keepCount) &&
        parseSizeField(line, "lifecycleUpdateCount", pipeline.lifecycle.updateCount) &&
        parseSizeField(line, "lifecycleReleaseCount", pipeline.lifecycle.releaseCount) &&
        parseSizeField(line, "commandCreateCount", pipeline.commands.createCount) &&
        parseSizeField(line, "commandKeepCount", pipeline.commands.keepCount) &&
        parseSizeField(line, "commandUpdateCount", pipeline.commands.updateCount) &&
        parseSizeField(line, "commandDestroyCount", pipeline.commands.destroyCount) &&
        parseSizeField(line, "descriptorReadyCount", pipeline.descriptors.readyCount) &&
        parseSizeField(line, "descriptorIgnoredCount", pipeline.descriptors.ignoredCount) &&
        parseSizeField(line, "descriptorMissingResourcesCount", pipeline.descriptors.missingResourcesCount) &&
        parseSizeField(line, "descriptorInvalidResourcesCount", pipeline.descriptors.invalidResourcesCount) &&
        parseSizeField(line, "submissionCreatedCount", pipeline.submission.createdCount) &&
        parseSizeField(line, "submissionUpdatedCount", pipeline.submission.updatedCount) &&
        parseSizeField(line, "submissionDestroyedCount", pipeline.submission.destroyedCount) &&
        parseSizeField(line, "submissionKeptCount", pipeline.submission.keptCount) &&
        parseSizeField(line, "submissionSkippedCount", pipeline.submission.skippedCount) &&
        parseSizeField(line, "submissionRendererFailedCount", pipeline.submission.rendererFailedCount) &&
        parseSizeField(line, "submissionHandleMapFailedCount", pipeline.submission.handleMapFailedCount);
}
} // namespace

const char* terrainRuntimeEventImportResultName(const TerrainRuntimeEventImportResult result) noexcept
{
    switch (result)
    {
    case TerrainRuntimeEventImportResult::Success:
        return "Success";
    case TerrainRuntimeEventImportResult::InvalidArgument:
        return "InvalidArgument";
    case TerrainRuntimeEventImportResult::IoError:
        return "IoError";
    case TerrainRuntimeEventImportResult::ParseError:
        return "ParseError";
    }

    return "Unknown";
}

TerrainRuntimeEventImport importTerrainRuntimeEventsJsonLines(const char* path)
{
    TerrainRuntimeEventImport import;
    if (path == nullptr || path[0] == '\0')
    {
        import.result = TerrainRuntimeEventImportResult::InvalidArgument;
        return import;
    }

    std::ifstream input(path);
    if (!input.is_open())
    {
        import.result = TerrainRuntimeEventImportResult::IoError;
        return import;
    }

    std::string line;
    while (std::getline(input, line))
    {
        TerrainRuntimeEvent event;
        if (!parseEvent(line, event))
        {
            import.events.clear();
            import.result = TerrainRuntimeEventImportResult::ParseError;
            return import;
        }
        import.events.push_back(event);
    }

    if (input.bad())
    {
        import.events.clear();
        import.result = TerrainRuntimeEventImportResult::IoError;
        return import;
    }

    import.result = TerrainRuntimeEventImportResult::Success;
    return import;
}
} // namespace full_engine
