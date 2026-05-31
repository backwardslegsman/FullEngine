#include "engine/renderer_integration/TerrainRuntimeEventExport.hpp"

#include <fstream>

namespace full_engine
{
namespace
{
void writeJsonField(std::ostream& stream, const char* name, const std::size_t value)
{
    stream << ",\"" << name << "\":" << value;
}

void writeEvent(std::ostream& stream, const TerrainRuntimeEvent& event)
{
    const TerrainSetupRequestDiagnostics& setup = event.diagnostics.setupRequests;
    const TerrainResidencyRequestDiagnostics& residency = event.diagnostics.residencyRequests;
    const TerrainPipelineDiagnostics& pipeline = event.diagnostics.pipeline;

    stream << "{\"sequence\":" << event.sequence;
    stream << ",\"status\":\"" << terrainRuntimeUpdateStatusName(event.status) << "\"";
    stream << ",\"setupRequestCount\":" << setup.requestCount;
    writeJsonField(stream, "setupAddCount", setup.addCount);
    writeJsonField(stream, "setupRemoveCount", setup.removeCount);
    writeJsonField(stream, "setupAppliedCount", setup.summary.appliedCount);
    writeJsonField(stream, "setupAlreadySatisfiedCount", setup.summary.alreadySatisfiedCount);
    writeJsonField(stream, "setupNotFoundCount", setup.summary.notFoundCount);
    writeJsonField(stream, "setupInvalidArgumentCount", setup.summary.invalidArgumentCount);
    writeJsonField(stream, "setupPartialFailureCount", setup.summary.partialFailureCount);
    writeJsonField(stream, "residencyRequestCount", residency.requestCount);
    writeJsonField(stream, "residencyMakeResidentCount", residency.makeResidentCount);
    writeJsonField(stream, "residencyMakeUnloadedCount", residency.makeUnloadedCount);
    writeJsonField(stream, "residencyAppliedCount", residency.summary.appliedCount);
    writeJsonField(stream, "residencyAlreadySatisfiedCount", residency.summary.alreadySatisfiedCount);
    writeJsonField(stream, "residencyNotFoundCount", residency.summary.notFoundCount);
    writeJsonField(stream, "residencyInvalidTransitionCount", residency.summary.invalidTransitionCount);
    writeJsonField(stream, "handleCount", pipeline.handleCount);
    writeJsonField(stream, "snapshotReadyCount", pipeline.snapshotReadyCount);
    writeJsonField(stream, "snapshotNotResidentCount", pipeline.snapshotNotResidentCount);
    writeJsonField(stream, "snapshotMissingChunkCount", pipeline.snapshotMissingChunkCount);
    writeJsonField(stream, "snapshotInvalidBoundsCount", pipeline.snapshotInvalidBoundsCount);
    writeJsonField(stream, "snapshotOutOfRangeCount", pipeline.snapshotOutOfRangeCount);
    writeJsonField(stream, "snapshotInvalidInputCount", pipeline.snapshotInvalidInputCount);
    writeJsonField(stream, "prepReadyCount", pipeline.prep.readyCount);
    writeJsonField(stream, "prepSkippedNotResidentCount", pipeline.prep.skippedNotResidentCount);
    writeJsonField(stream, "prepSkippedMissingChunkCount", pipeline.prep.skippedMissingChunkCount);
    writeJsonField(stream, "prepSkippedInvalidBoundsCount", pipeline.prep.skippedInvalidBoundsCount);
    writeJsonField(stream, "prepSkippedOutOfRangeCount", pipeline.prep.skippedOutOfRangeCount);
    writeJsonField(stream, "prepInvalidInputCount", pipeline.prep.invalidInputCount);
    writeJsonField(stream, "lifecycleCreateCount", pipeline.lifecycle.createCount);
    writeJsonField(stream, "lifecycleKeepCount", pipeline.lifecycle.keepCount);
    writeJsonField(stream, "lifecycleUpdateCount", pipeline.lifecycle.updateCount);
    writeJsonField(stream, "lifecycleReleaseCount", pipeline.lifecycle.releaseCount);
    writeJsonField(stream, "commandCreateCount", pipeline.commands.createCount);
    writeJsonField(stream, "commandKeepCount", pipeline.commands.keepCount);
    writeJsonField(stream, "commandUpdateCount", pipeline.commands.updateCount);
    writeJsonField(stream, "commandDestroyCount", pipeline.commands.destroyCount);
    writeJsonField(stream, "descriptorReadyCount", pipeline.descriptors.readyCount);
    writeJsonField(stream, "descriptorIgnoredCount", pipeline.descriptors.ignoredCount);
    writeJsonField(stream, "descriptorMissingResourcesCount", pipeline.descriptors.missingResourcesCount);
    writeJsonField(stream, "descriptorInvalidResourcesCount", pipeline.descriptors.invalidResourcesCount);
    writeJsonField(stream, "submissionCreatedCount", pipeline.submission.createdCount);
    writeJsonField(stream, "submissionUpdatedCount", pipeline.submission.updatedCount);
    writeJsonField(stream, "submissionDestroyedCount", pipeline.submission.destroyedCount);
    writeJsonField(stream, "submissionKeptCount", pipeline.submission.keptCount);
    writeJsonField(stream, "submissionSkippedCount", pipeline.submission.skippedCount);
    writeJsonField(stream, "submissionRendererFailedCount", pipeline.submission.rendererFailedCount);
    writeJsonField(stream, "submissionHandleMapFailedCount", pipeline.submission.handleMapFailedCount);
    stream << "}\n";
}
} // namespace

const char* terrainRuntimeEventExportResultName(const TerrainRuntimeEventExportResult result) noexcept
{
    switch (result)
    {
    case TerrainRuntimeEventExportResult::Success:
        return "Success";
    case TerrainRuntimeEventExportResult::InvalidArgument:
        return "InvalidArgument";
    case TerrainRuntimeEventExportResult::IoError:
        return "IoError";
    }

    return "Unknown";
}

TerrainRuntimeEventExportResult exportTerrainRuntimeEventsJsonLines(
    const TerrainRuntimeEventLog& log,
    const char* path)
{
    return exportTerrainRuntimeEventsJsonLines(log.events(), path);
}

TerrainRuntimeEventExportResult exportTerrainRuntimeEventsJsonLines(
    const std::vector<TerrainRuntimeEvent>& events,
    const char* path)
{
    if (path == nullptr || path[0] == '\0')
    {
        return TerrainRuntimeEventExportResult::InvalidArgument;
    }

    std::ofstream output(path, std::ios::out | std::ios::trunc);
    if (!output.is_open())
    {
        return TerrainRuntimeEventExportResult::IoError;
    }

    for (const TerrainRuntimeEvent& event : events)
    {
        writeEvent(output, event);
    }

    if (!output.good())
    {
        return TerrainRuntimeEventExportResult::IoError;
    }

    return TerrainRuntimeEventExportResult::Success;
}
} // namespace full_engine
