#include "engine/streaming/TerrainStreamingTickHistoryExport.hpp"

#include <fstream>

namespace full_engine
{
namespace
{
void writeJsonField(std::ostream& stream, const char* const name, const std::size_t value)
{
    stream << ",\"" << name << "\":" << value;
}

void writeJsonBoolField(std::ostream& stream, const char* const name, const bool value)
{
    stream << ",\"" << name << "\":" << (value ? "true" : "false");
}

void writeTick(std::ostream& stream, const TerrainStreamingTickEvent& event)
{
    stream << "{\"sequence\":" << event.sequence;
    stream << ",\"streamingStatus\":\"" << terrainStreamingManifestUpdateStatusName(event.streamingStatus) << "\"";
    stream << ",\"runtimeStatus\":\"" << terrainRuntimeUpdateStatusName(event.runtimeStatus) << "\"";
    writeJsonBoolField(stream, "runtimeUpdateRan", event.runtimeUpdateRan);
    writeJsonField(stream, "setupRequestsBeforeRuntime", event.setupRequestsBeforeRuntime);
    writeJsonField(stream, "residencyRequestsBeforeRuntime", event.residencyRequestsBeforeRuntime);
    writeJsonField(stream, "setupRequestsAfterRuntime", event.setupRequestsAfterRuntime);
    writeJsonField(stream, "residencyRequestsAfterRuntime", event.residencyRequestsAfterRuntime);

    writeJsonField(stream, "streamingManifestTerrainChunkCount", event.streaming.manifestTerrainChunkCount);
    writeJsonField(stream, "streamingReadinessMissingHandleCount", event.streaming.readinessMissingHandleCount);
    writeJsonField(stream, "streamingLoadRequestCount", event.streaming.loadRequestCount);
    writeJsonField(stream, "streamingQueuedLoadRequestCount", event.streaming.queuedLoadRequestCount);
    writeJsonField(stream, "streamingDesiredSetupCount", event.streaming.desiredSetupCount);
    writeJsonField(stream, "streamingPlanOperationCount", event.streaming.streamingPlanOperationCount);
    writeJsonField(stream, "streamingQueuedSetupAddCount", event.streaming.queuedSetupAddCount);
    writeJsonField(stream, "streamingQueuedSetupRemoveCount", event.streaming.queuedSetupRemoveCount);
    writeJsonField(stream, "streamingQueuedMakeResidentCount", event.streaming.queuedMakeResidentCount);
    writeJsonField(stream, "streamingQueuedMakeUnloadedCount", event.streaming.queuedMakeUnloadedCount);
    writeJsonField(stream, "streamingDeferredSetupAddCount", event.streaming.deferredSetupAddCount);
    writeJsonField(stream, "streamingDeferredSetupRemoveCount", event.streaming.deferredSetupRemoveCount);
    writeJsonField(stream, "streamingDeferredMakeResidentCount", event.streaming.deferredMakeResidentCount);
    writeJsonField(stream, "streamingDeferredMakeUnloadedCount", event.streaming.deferredMakeUnloadedCount);

    writeJsonField(stream, "queueQueuedSetupAddCount", event.streamingQueue.queuedSetupAddCount);
    writeJsonField(stream, "queueQueuedSetupRemoveCount", event.streamingQueue.queuedSetupRemoveCount);
    writeJsonField(stream, "queueQueuedMakeResidentCount", event.streamingQueue.queuedMakeResidentCount);
    writeJsonField(stream, "queueQueuedMakeUnloadedCount", event.streamingQueue.queuedMakeUnloadedCount);
    writeJsonField(stream, "queueDeferredSetupAddCount", event.streamingQueue.deferredSetupAddCount);
    writeJsonField(stream, "queueDeferredSetupRemoveCount", event.streamingQueue.deferredSetupRemoveCount);
    writeJsonField(stream, "queueDeferredMakeResidentCount", event.streamingQueue.deferredMakeResidentCount);
    writeJsonField(stream, "queueDeferredMakeUnloadedCount", event.streamingQueue.deferredMakeUnloadedCount);
    writeJsonField(stream, "queueMissingSetupDescCount", event.streamingQueue.missingSetupDescCount);

    writeJsonField(stream, "lifecycleCreateCount", event.runtimeLifecycle.createCount);
    writeJsonField(stream, "lifecycleKeepCount", event.runtimeLifecycle.keepCount);
    writeJsonField(stream, "lifecycleUpdateCount", event.runtimeLifecycle.updateCount);
    writeJsonField(stream, "lifecycleReleaseCount", event.runtimeLifecycle.releaseCount);
    writeJsonField(stream, "lifecycleDeferredCreateCount", event.runtimeLifecycle.deferredCreateCount);
    writeJsonField(stream, "lifecycleDeferredUpdateCount", event.runtimeLifecycle.deferredUpdateCount);
    writeJsonField(stream, "lifecycleDeferredReleaseCount", event.runtimeLifecycle.deferredReleaseCount);

    writeJsonField(stream, "submissionCreatedCount", event.runtimeSubmission.createdCount);
    writeJsonField(stream, "submissionUpdatedCount", event.runtimeSubmission.updatedCount);
    writeJsonField(stream, "submissionDestroyedCount", event.runtimeSubmission.destroyedCount);
    writeJsonField(stream, "submissionKeptCount", event.runtimeSubmission.keptCount);
    writeJsonField(stream, "submissionSkippedCount", event.runtimeSubmission.skippedCount);
    writeJsonField(stream, "submissionRendererFailedCount", event.runtimeSubmission.rendererFailedCount);
    writeJsonField(stream, "submissionHandleMapFailedCount", event.runtimeSubmission.handleMapFailedCount);
    stream << "}\n";
}
} // namespace

const char* terrainStreamingTickHistoryExportResultName(
    const TerrainStreamingTickHistoryExportResult result) noexcept
{
    switch (result)
    {
    case TerrainStreamingTickHistoryExportResult::Success:
        return "Success";
    case TerrainStreamingTickHistoryExportResult::InvalidArgument:
        return "InvalidArgument";
    case TerrainStreamingTickHistoryExportResult::IoError:
        return "IoError";
    }

    return "Unknown";
}

TerrainStreamingTickHistoryExportResult exportTerrainStreamingTickHistoryJsonLines(
    const TerrainStreamingTickHistory& history,
    const char* const path)
{
    return exportTerrainStreamingTickHistoryJsonLines(history.events(), path);
}

TerrainStreamingTickHistoryExportResult exportTerrainStreamingTickHistoryJsonLines(
    const std::vector<TerrainStreamingTickEvent>& events,
    const char* const path)
{
    if (path == nullptr || path[0] == '\0')
    {
        return TerrainStreamingTickHistoryExportResult::InvalidArgument;
    }

    std::ofstream output(path, std::ios::out | std::ios::trunc);
    if (!output.is_open())
    {
        return TerrainStreamingTickHistoryExportResult::IoError;
    }

    for (const TerrainStreamingTickEvent& event : events)
    {
        writeTick(output, event);
    }

    if (!output.good())
    {
        return TerrainStreamingTickHistoryExportResult::IoError;
    }

    return TerrainStreamingTickHistoryExportResult::Success;
}
} // namespace full_engine
