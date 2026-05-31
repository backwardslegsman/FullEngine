#include "engine/streaming/TerrainStreamingRuntime.hpp"

#include <map>

namespace full_engine
{
namespace
{
const TerrainSetupStageDesc* findSetupDesc(
    const std::map<ChunkId, const TerrainSetupStageDesc*>& setupDescs,
    const ChunkId& id) noexcept
{
    const auto found = setupDescs.find(id);
    return found == setupDescs.end() ? nullptr : found->second;
}

std::map<ChunkId, const TerrainSetupStageDesc*> buildSetupDescMap(
    const TerrainSetupStageDesc* desiredSetup,
    const std::size_t desiredSetupCount)
{
    std::map<ChunkId, const TerrainSetupStageDesc*> result;
    if (desiredSetup == nullptr)
    {
        return result;
    }

    for (std::size_t index = 0; index < desiredSetupCount; ++index)
    {
        const TerrainSetupStageDesc& desc = desiredSetup[index];
        if (result.find(desc.id) == result.end())
        {
            result[desc.id] = &desc;
        }
    }
    return result;
}

void copyDiagnostics(
    TerrainStreamingRuntimeDiagnostics& diagnostics,
    const bool hasLatestPlan,
    const TerrainStreamingPlan& latestPlan,
    const TerrainStreamingQueueResult& latestQueueResult) noexcept
{
    diagnostics.hasLatestPlan = hasLatestPlan;
    diagnostics.latestPlanOperationCount = latestPlan.operations.size();
    diagnostics.latestPlanSummary = latestPlan.summary;
    diagnostics.latestQueueStatus = latestQueueResult.status;
    diagnostics.latestQueueSummary = latestQueueResult.summary;
}
} // namespace

const char* terrainStreamingQueueStatusName(const TerrainStreamingQueueStatus status) noexcept
{
    switch (status)
    {
    case TerrainStreamingQueueStatus::Success:
        return "Success";
    case TerrainStreamingQueueStatus::NoPlan:
        return "NoPlan";
    case TerrainStreamingQueueStatus::BlockedInvalidPlan:
        return "BlockedInvalidPlan";
    case TerrainStreamingQueueStatus::MissingSetupDesc:
        return "MissingSetupDesc";
    }

    return "Unknown";
}

const TerrainStreamingPlan& TerrainStreamingRuntimeState::plan(
    const TerrainStreamingPlannerConfig& config,
    const WorldPosition& cameraWorld,
    const ChunkId* knownIds,
    const std::size_t knownIdCount,
    const TerrainRuntimeStateSnapshot& current)
{
    latestPlan_ = planTerrainStreaming(config, cameraWorld, knownIds, knownIdCount, current);
    latestQueueResult_ = {};
    hasLatestPlan_ = true;
    copyDiagnostics(latestDiagnostics_, hasLatestPlan_, latestPlan_, latestQueueResult_);
    return latestPlan_;
}

const TerrainStreamingQueueResult& TerrainStreamingRuntimeState::queueLatestPlan(
    TerrainRuntimeState& runtime,
    const TerrainSetupStageDesc* desiredSetup,
    const std::size_t desiredSetupCount)
{
    latestQueueResult_ = {};
    if (!hasLatestPlan_)
    {
        latestQueueResult_.status = TerrainStreamingQueueStatus::NoPlan;
        copyDiagnostics(latestDiagnostics_, hasLatestPlan_, latestPlan_, latestQueueResult_);
        return latestQueueResult_;
    }

    if (latestPlan_.summary.invalidInputCount > 0)
    {
        latestQueueResult_.status = TerrainStreamingQueueStatus::BlockedInvalidPlan;
        latestQueueResult_.summary.skippedPlanSkippedCount = latestPlan_.summary.skippedCount;
        copyDiagnostics(latestDiagnostics_, hasLatestPlan_, latestPlan_, latestQueueResult_);
        return latestQueueResult_;
    }

    const std::map<ChunkId, const TerrainSetupStageDesc*> setupDescs =
        buildSetupDescMap(desiredSetup, desiredSetupCount);
    for (const TerrainStreamingPlanOp& op : latestPlan_.operations)
    {
        if (op.action == TerrainStreamingChunkAction::AddSetup &&
            findSetupDesc(setupDescs, op.id) == nullptr)
        {
            latestQueueResult_.status = TerrainStreamingQueueStatus::MissingSetupDesc;
            ++latestQueueResult_.summary.missingSetupDescCount;
        }
    }

    if (latestQueueResult_.status == TerrainStreamingQueueStatus::MissingSetupDesc)
    {
        copyDiagnostics(latestDiagnostics_, hasLatestPlan_, latestPlan_, latestQueueResult_);
        return latestQueueResult_;
    }

    for (const TerrainStreamingPlanOp& op : latestPlan_.operations)
    {
        switch (op.action)
        {
        case TerrainStreamingChunkAction::AddSetup:
            if (const TerrainSetupStageDesc* const setup = findSetupDesc(setupDescs, op.id))
            {
                runtime.queueSetupAdd(setup->worldDesc, setup->resourceDesc);
                ++latestQueueResult_.summary.queuedSetupAddCount;
            }
            break;
        case TerrainStreamingChunkAction::RemoveSetup:
            runtime.queueSetupRemove(op.id);
            ++latestQueueResult_.summary.queuedSetupRemoveCount;
            break;
        case TerrainStreamingChunkAction::MakeResident:
            runtime.queueMakeResident(op.id);
            ++latestQueueResult_.summary.queuedMakeResidentCount;
            break;
        case TerrainStreamingChunkAction::MakeUnloaded:
            runtime.queueMakeUnloaded(op.id);
            ++latestQueueResult_.summary.queuedMakeUnloadedCount;
            break;
        case TerrainStreamingChunkAction::KeepSetup:
            ++latestQueueResult_.summary.skippedKeepSetupCount;
            break;
        case TerrainStreamingChunkAction::KeepResident:
            ++latestQueueResult_.summary.skippedKeepResidentCount;
            break;
        case TerrainStreamingChunkAction::KeepUnloaded:
            ++latestQueueResult_.summary.skippedKeepUnloadedCount;
            break;
        case TerrainStreamingChunkAction::Skipped:
            ++latestQueueResult_.summary.skippedPlanSkippedCount;
            break;
        }
    }

    latestQueueResult_.status = TerrainStreamingQueueStatus::Success;
    copyDiagnostics(latestDiagnostics_, hasLatestPlan_, latestPlan_, latestQueueResult_);
    return latestQueueResult_;
}

bool TerrainStreamingRuntimeState::hasLatestPlan() const noexcept
{
    return hasLatestPlan_;
}

const TerrainStreamingPlan& TerrainStreamingRuntimeState::latestPlan() const noexcept
{
    return latestPlan_;
}

const TerrainStreamingQueueResult& TerrainStreamingRuntimeState::latestQueueResult() const noexcept
{
    return latestQueueResult_;
}

const TerrainStreamingRuntimeDiagnostics& TerrainStreamingRuntimeState::latestDiagnostics() const noexcept
{
    return latestDiagnostics_;
}

void TerrainStreamingRuntimeState::clear() noexcept
{
    latestPlan_ = {};
    latestQueueResult_ = {};
    latestDiagnostics_ = {};
    hasLatestPlan_ = false;
}
} // namespace full_engine
