#include "engine/streaming/TerrainStreamingTickHistoryImport.hpp"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>

namespace full_engine
{
namespace
{
bool parseRuntimeStatus(const std::string& value, TerrainRuntimeUpdateStatus& status) noexcept
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

bool parseStreamingStatus(
    const std::string& value,
    TerrainStreamingManifestUpdateStatus& status) noexcept
{
    if (value == "Success")
    {
        status = TerrainStreamingManifestUpdateStatus::Success;
        return true;
    }
    if (value == "NoManifest")
    {
        status = TerrainStreamingManifestUpdateStatus::NoManifest;
        return true;
    }
    if (value == "AssetLoadsPending")
    {
        status = TerrainStreamingManifestUpdateStatus::AssetLoadsPending;
        return true;
    }
    if (value == "ManifestStageFailed")
    {
        status = TerrainStreamingManifestUpdateStatus::ManifestStageFailed;
        return true;
    }
    if (value == "UnsupportedStageChanges")
    {
        status = TerrainStreamingManifestUpdateStatus::UnsupportedStageChanges;
        return true;
    }
    if (value == "StreamingQueueBlocked")
    {
        status = TerrainStreamingManifestUpdateStatus::StreamingQueueBlocked;
        return true;
    }

    return false;
}

bool parseBudgetProfile(
    const std::string& value,
    TerrainStreamingBudgetProfile& profile) noexcept
{
    if (value == "Unlimited")
    {
        profile = TerrainStreamingBudgetProfile::Unlimited;
        return true;
    }
    if (value == "Conservative")
    {
        profile = TerrainStreamingBudgetProfile::Conservative;
        return true;
    }
    if (value == "Balanced")
    {
        profile = TerrainStreamingBudgetProfile::Balanced;
        return true;
    }
    if (value == "CatchUp")
    {
        profile = TerrainStreamingBudgetProfile::CatchUp;
        return true;
    }

    return false;
}

bool parseSchedulerStatus(
    const std::string& value,
    TerrainStreamingSchedulerStatus& status) noexcept
{
    if (value == "Idle")
    {
        status = TerrainStreamingSchedulerStatus::Idle;
        return true;
    }
    if (value == "RunStreaming")
    {
        status = TerrainStreamingSchedulerStatus::RunStreaming;
        return true;
    }
    if (value == "RunAssetLoadJobs")
    {
        status = TerrainStreamingSchedulerStatus::RunAssetLoadJobs;
        return true;
    }
    if (value == "RunStreamingAndAssetLoadJobs")
    {
        status = TerrainStreamingSchedulerStatus::RunStreamingAndAssetLoadJobs;
        return true;
    }

    return false;
}

bool parseSchedulerReason(
    const std::string& value,
    TerrainStreamingSchedulerReason& reason) noexcept
{
    if (value == "NoWork")
    {
        reason = TerrainStreamingSchedulerReason::NoWork;
        return true;
    }
    if (value == "PendingAssetLoads")
    {
        reason = TerrainStreamingSchedulerReason::PendingAssetLoads;
        return true;
    }
    if (value == "PendingJobs")
    {
        reason = TerrainStreamingSchedulerReason::PendingJobs;
        return true;
    }
    if (value == "DeferredWorkPressure")
    {
        reason = TerrainStreamingSchedulerReason::DeferredWorkPressure;
        return true;
    }
    if (value == "StreamingBacklog")
    {
        reason = TerrainStreamingSchedulerReason::StreamingBacklog;
        return true;
    }
    if (value == "CatchUp")
    {
        reason = TerrainStreamingSchedulerReason::CatchUp;
        return true;
    }

    return false;
}

bool parseSchedulerTickStatus(
    const std::string& value,
    TerrainStreamingSchedulerTickStatus& status) noexcept
{
    if (value == "Idle")
    {
        status = TerrainStreamingSchedulerTickStatus::Idle;
        return true;
    }
    if (value == "Success")
    {
        status = TerrainStreamingSchedulerTickStatus::Success;
        return true;
    }
    if (value == "LoadJobsBlocked")
    {
        status = TerrainStreamingSchedulerTickStatus::LoadJobsBlocked;
        return true;
    }
    if (value == "StreamingBlocked")
    {
        status = TerrainStreamingSchedulerTickStatus::StreamingBlocked;
        return true;
    }
    if (value == "RuntimeSetupFailed")
    {
        status = TerrainStreamingSchedulerTickStatus::RuntimeSetupFailed;
        return true;
    }
    if (value == "RuntimeResidencyFailed")
    {
        status = TerrainStreamingSchedulerTickStatus::RuntimeResidencyFailed;
        return true;
    }
    if (value == "RuntimePipelineFailed")
    {
        status = TerrainStreamingSchedulerTickStatus::RuntimePipelineFailed;
        return true;
    }

    return false;
}

bool parseStringField(const std::string& line, const char* const name, std::string& value)
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

bool parseBoolField(const std::string& line, const char* const name, bool& value)
{
    const std::string prefix = std::string("\"") + name + "\":";
    const std::size_t start = line.find(prefix);
    if (start == std::string::npos)
    {
        return false;
    }

    const std::size_t valueStart = start + prefix.size();
    if (line.compare(valueStart, 4, "true") == 0)
    {
        const std::size_t valueEnd = valueStart + 4;
        if (valueEnd < line.size() && (line[valueEnd] == ',' || line[valueEnd] == '}'))
        {
            value = true;
            return true;
        }
    }
    if (line.compare(valueStart, 5, "false") == 0)
    {
        const std::size_t valueEnd = valueStart + 5;
        if (valueEnd < line.size() && (line[valueEnd] == ',' || line[valueEnd] == '}'))
        {
            value = false;
            return true;
        }
    }

    return false;
}

bool parseUnsignedField(const std::string& line, const char* const name, std::uint64_t& value)
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

bool parseSizeField(const std::string& line, const char* const name, std::size_t& value)
{
    std::uint64_t parsed = 0;
    if (!parseUnsignedField(line, name, parsed) ||
        parsed > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))
    {
        return false;
    }

    value = static_cast<std::size_t>(parsed);
    return true;
}

bool hasField(const std::string& line, const char* const name)
{
    const std::string prefix = std::string("\"") + name + "\":";
    return line.find(prefix) != std::string::npos;
}

bool parseOptionalBoolField(const std::string& line, const char* const name, bool& value)
{
    return !hasField(line, name) || parseBoolField(line, name, value);
}

bool parseOptionalSizeField(const std::string& line, const char* const name, std::size_t& value)
{
    return !hasField(line, name) || parseSizeField(line, name, value);
}

bool parseTick(const std::string& line, TerrainStreamingTickEvent& event)
{
    if (line.size() < 2 || line.front() != '{' || line.back() != '}')
    {
        return false;
    }

    std::string streamingStatus;
    std::string runtimeStatus;
    if (!parseUnsignedField(line, "sequence", event.sequence) ||
        !parseStringField(line, "streamingStatus", streamingStatus) ||
        !parseStreamingStatus(streamingStatus, event.streamingStatus) ||
        !parseStringField(line, "runtimeStatus", runtimeStatus) ||
        !parseRuntimeStatus(runtimeStatus, event.runtimeStatus) ||
        !parseBoolField(line, "runtimeUpdateRan", event.runtimeUpdateRan))
    {
        return false;
    }

    std::string budgetProfile;
    if (parseStringField(line, "budgetProfile", budgetProfile) &&
        !parseBudgetProfile(budgetProfile, event.budgetProfile))
    {
        return false;
    }

    if (!parseOptionalBoolField(line, "schedulerHasDecision", event.scheduler.hasSchedulerDecision))
    {
        return false;
    }

    std::string schedulerStatus;
    if (parseStringField(line, "schedulerStatus", schedulerStatus) &&
        !parseSchedulerTickStatus(schedulerStatus, event.scheduler.status))
    {
        return false;
    }

    std::string schedulerDecisionStatus;
    if (parseStringField(line, "schedulerDecisionStatus", schedulerDecisionStatus) &&
        !parseSchedulerStatus(schedulerDecisionStatus, event.scheduler.decisionStatus))
    {
        return false;
    }

    std::string schedulerDecisionReason;
    if (parseStringField(line, "schedulerDecisionReason", schedulerDecisionReason) &&
        !parseSchedulerReason(schedulerDecisionReason, event.scheduler.decisionReason))
    {
        return false;
    }

    std::string schedulerBudgetProfile;
    if (parseStringField(line, "schedulerBudgetProfile", schedulerBudgetProfile) &&
        !parseBudgetProfile(schedulerBudgetProfile, event.scheduler.budgetProfile))
    {
        return false;
    }

    if (!parseOptionalSizeField(line, "schedulerPendingLoadRequestCount", event.scheduler.pendingLoadRequestCount) ||
        !parseOptionalSizeField(line, "schedulerPendingJobCount", event.scheduler.pendingJobCount) ||
        !parseOptionalSizeField(line, "schedulerDeferredWorkCount", event.scheduler.deferredWorkCount) ||
        !parseOptionalSizeField(line, "schedulerPeakDeferredWorkCount", event.scheduler.peakDeferredWorkCount) ||
        !parseOptionalSizeField(line, "schedulerRuntimeBacklogCount", event.scheduler.runtimeBacklogCount) ||
        !parseOptionalSizeField(line, "schedulerPressureCount", event.scheduler.pressureCount) ||
        !parseOptionalSizeField(line, "schedulerMaxAssetLoadJobs", event.scheduler.maxAssetLoadJobs) ||
        !parseOptionalBoolField(line, "schedulerLoadJobsRan", event.scheduler.loadJobsRan) ||
        !parseOptionalBoolField(line, "schedulerLoadJobsScheduled", event.scheduler.loadJobsScheduled) ||
        !parseOptionalBoolField(line, "schedulerStreamingRan", event.scheduler.streamingRan))
    {
        return false;
    }

    return
        parseSizeField(line, "setupRequestsBeforeRuntime", event.setupRequestsBeforeRuntime) &&
        parseSizeField(line, "residencyRequestsBeforeRuntime", event.residencyRequestsBeforeRuntime) &&
        parseSizeField(line, "setupRequestsAfterRuntime", event.setupRequestsAfterRuntime) &&
        parseSizeField(line, "residencyRequestsAfterRuntime", event.residencyRequestsAfterRuntime) &&
        parseSizeField(line, "streamingManifestTerrainChunkCount", event.streaming.manifestTerrainChunkCount) &&
        parseSizeField(line, "streamingReadinessMissingHandleCount", event.streaming.readinessMissingHandleCount) &&
        parseSizeField(line, "streamingLoadRequestCount", event.streaming.loadRequestCount) &&
        parseSizeField(line, "streamingQueuedLoadRequestCount", event.streaming.queuedLoadRequestCount) &&
        parseSizeField(line, "streamingDesiredSetupCount", event.streaming.desiredSetupCount) &&
        parseSizeField(line, "streamingPlanOperationCount", event.streaming.streamingPlanOperationCount) &&
        parseSizeField(line, "streamingQueuedSetupAddCount", event.streaming.queuedSetupAddCount) &&
        parseSizeField(line, "streamingQueuedSetupRemoveCount", event.streaming.queuedSetupRemoveCount) &&
        parseSizeField(line, "streamingQueuedMakeResidentCount", event.streaming.queuedMakeResidentCount) &&
        parseSizeField(line, "streamingQueuedMakeUnloadedCount", event.streaming.queuedMakeUnloadedCount) &&
        parseSizeField(line, "streamingDeferredSetupAddCount", event.streaming.deferredSetupAddCount) &&
        parseSizeField(line, "streamingDeferredSetupRemoveCount", event.streaming.deferredSetupRemoveCount) &&
        parseSizeField(line, "streamingDeferredMakeResidentCount", event.streaming.deferredMakeResidentCount) &&
        parseSizeField(line, "streamingDeferredMakeUnloadedCount", event.streaming.deferredMakeUnloadedCount) &&
        parseSizeField(line, "queueQueuedSetupAddCount", event.streamingQueue.queuedSetupAddCount) &&
        parseSizeField(line, "queueQueuedSetupRemoveCount", event.streamingQueue.queuedSetupRemoveCount) &&
        parseSizeField(line, "queueQueuedMakeResidentCount", event.streamingQueue.queuedMakeResidentCount) &&
        parseSizeField(line, "queueQueuedMakeUnloadedCount", event.streamingQueue.queuedMakeUnloadedCount) &&
        parseSizeField(line, "queueDeferredSetupAddCount", event.streamingQueue.deferredSetupAddCount) &&
        parseSizeField(line, "queueDeferredSetupRemoveCount", event.streamingQueue.deferredSetupRemoveCount) &&
        parseSizeField(line, "queueDeferredMakeResidentCount", event.streamingQueue.deferredMakeResidentCount) &&
        parseSizeField(line, "queueDeferredMakeUnloadedCount", event.streamingQueue.deferredMakeUnloadedCount) &&
        parseSizeField(line, "queueMissingSetupDescCount", event.streamingQueue.missingSetupDescCount) &&
        parseSizeField(line, "lifecycleCreateCount", event.runtimeLifecycle.createCount) &&
        parseSizeField(line, "lifecycleKeepCount", event.runtimeLifecycle.keepCount) &&
        parseSizeField(line, "lifecycleUpdateCount", event.runtimeLifecycle.updateCount) &&
        parseSizeField(line, "lifecycleReleaseCount", event.runtimeLifecycle.releaseCount) &&
        parseSizeField(line, "lifecycleDeferredCreateCount", event.runtimeLifecycle.deferredCreateCount) &&
        parseSizeField(line, "lifecycleDeferredUpdateCount", event.runtimeLifecycle.deferredUpdateCount) &&
        parseSizeField(line, "lifecycleDeferredReleaseCount", event.runtimeLifecycle.deferredReleaseCount) &&
        parseSizeField(line, "submissionCreatedCount", event.runtimeSubmission.createdCount) &&
        parseSizeField(line, "submissionUpdatedCount", event.runtimeSubmission.updatedCount) &&
        parseSizeField(line, "submissionDestroyedCount", event.runtimeSubmission.destroyedCount) &&
        parseSizeField(line, "submissionKeptCount", event.runtimeSubmission.keptCount) &&
        parseSizeField(line, "submissionSkippedCount", event.runtimeSubmission.skippedCount) &&
        parseSizeField(line, "submissionRendererFailedCount", event.runtimeSubmission.rendererFailedCount) &&
        parseSizeField(line, "submissionHandleMapFailedCount", event.runtimeSubmission.handleMapFailedCount);
}
} // namespace

const char* terrainStreamingTickHistoryImportResultName(
    const TerrainStreamingTickHistoryImportResult result) noexcept
{
    switch (result)
    {
    case TerrainStreamingTickHistoryImportResult::Success:
        return "Success";
    case TerrainStreamingTickHistoryImportResult::InvalidArgument:
        return "InvalidArgument";
    case TerrainStreamingTickHistoryImportResult::IoError:
        return "IoError";
    case TerrainStreamingTickHistoryImportResult::ParseError:
        return "ParseError";
    }

    return "Unknown";
}

TerrainStreamingTickHistoryImport importTerrainStreamingTickHistoryJsonLines(
    const char* const path)
{
    TerrainStreamingTickHistoryImport imported;
    if (path == nullptr || path[0] == '\0')
    {
        imported.result = TerrainStreamingTickHistoryImportResult::InvalidArgument;
        return imported;
    }

    std::ifstream input(path);
    if (!input.is_open())
    {
        imported.result = TerrainStreamingTickHistoryImportResult::IoError;
        return imported;
    }

    std::string line;
    while (std::getline(input, line))
    {
        TerrainStreamingTickEvent event;
        if (!parseTick(line, event))
        {
            imported.events.clear();
            imported.result = TerrainStreamingTickHistoryImportResult::ParseError;
            return imported;
        }
        imported.events.push_back(event);
    }

    if (input.bad())
    {
        imported.events.clear();
        imported.result = TerrainStreamingTickHistoryImportResult::IoError;
        return imported;
    }

    imported.result = TerrainStreamingTickHistoryImportResult::Success;
    return imported;
}
} // namespace full_engine
