#include "engine/streaming/TerrainStreamingManifestCoordinator.hpp"

#include <vector>

namespace full_engine
{
namespace
{
bool hasStageFailure(const TerrainManifestRuntimeStageResult& stage) noexcept
{
    return stage.status == TerrainManifestRuntimeStageStatus::InvalidManifest ||
        stage.status == TerrainManifestRuntimeStageStatus::MissingWorldDesc ||
        stage.status == TerrainManifestRuntimeStageStatus::AssetResolveFailed ||
        stage.status == TerrainManifestRuntimeStageStatus::QueueBlocked;
}

std::vector<ChunkId> manifestChunkIds(const CookedAssetManifest& manifest)
{
    std::vector<ChunkId> ids;
    ids.reserve(manifest.terrainChunks.size());
    for (const TerrainChunkAssetDesc& terrain : manifest.terrainChunks)
    {
        ids.push_back(terrain.id);
    }
    return ids;
}

std::vector<TerrainSetupStageDesc> setupDescsFromStagePlan(const TerrainSetupStagePlan& plan)
{
    std::vector<TerrainSetupStageDesc> result;
    result.reserve(plan.operations.size());
    for (const TerrainSetupStageOp& op : plan.operations)
    {
        if (op.action != TerrainSetupStageAction::Add && op.action != TerrainSetupStageAction::Keep)
        {
            continue;
        }

        TerrainSetupStageDesc desc;
        desc.id = op.id;
        desc.worldDesc = op.worldDesc;
        desc.resourceDesc = op.resourceDesc;
        result.push_back(desc);
    }
    return result;
}

void copySummary(TerrainStreamingManifestUpdateResult& result)
{
    result.summary.readinessMissingHandleCount = result.readiness.summary.missingHandleCount;
    result.summary.loadRequestCount = result.loadRequests.summary.requestCount;
    result.summary.queuedLoadRequestCount = result.loadQueue.summary.queuedCount;
    result.summary.desiredSetupCount = result.manifestStage.stage.summary.desiredSetupCount;
    result.summary.streamingPlanOperationCount = result.streamingPlan.operations.size();
    result.summary.queuedSetupAddCount = result.streamingQueue.summary.queuedSetupAddCount;
    result.summary.queuedSetupRemoveCount = result.streamingQueue.summary.queuedSetupRemoveCount;
    result.summary.queuedMakeResidentCount = result.streamingQueue.summary.queuedMakeResidentCount;
    result.summary.queuedMakeUnloadedCount = result.streamingQueue.summary.queuedMakeUnloadedCount;
}
} // namespace

const char* terrainStreamingManifestUpdateStatusName(
    const TerrainStreamingManifestUpdateStatus status) noexcept
{
    switch (status)
    {
    case TerrainStreamingManifestUpdateStatus::Success:
        return "Success";
    case TerrainStreamingManifestUpdateStatus::NoManifest:
        return "NoManifest";
    case TerrainStreamingManifestUpdateStatus::AssetLoadsPending:
        return "AssetLoadsPending";
    case TerrainStreamingManifestUpdateStatus::ManifestStageFailed:
        return "ManifestStageFailed";
    case TerrainStreamingManifestUpdateStatus::UnsupportedStageChanges:
        return "UnsupportedStageChanges";
    case TerrainStreamingManifestUpdateStatus::StreamingQueueBlocked:
        return "StreamingQueueBlocked";
    }

    return "Unknown";
}

TerrainStreamingManifestUpdateResult updateTerrainStreamingFromManifest(
    TerrainManifestLoadState& manifestLoad,
    const RendererAssetHandleCatalog& handles,
    const WorldChunkRegistry& registry,
    const WorldChunkCatalog& worldCatalog,
    const TerrainResourceCatalog& resources,
    const WorldChunkDesc* const worldDescs,
    const std::size_t worldDescCount,
    TerrainStreamingRuntimeState& streaming,
    TerrainRuntimeState& runtime,
    const TerrainStreamingPlannerConfig& streamingConfig,
    const WorldPosition& cameraWorld,
    const TerrainRuntimeStateSnapshot& currentSnapshot,
    const TerrainStreamingManifestUpdateOptions& options)
{
    TerrainStreamingManifestUpdateResult result;
    if (!manifestLoad.hasManifest())
    {
        result.status = TerrainStreamingManifestUpdateStatus::NoManifest;
        result.manifestStage.status = TerrainManifestLoadStageStatus::NoManifest;
        return result;
    }

    const CookedAssetManifest& manifest = manifestLoad.manifest();
    result.summary.manifestTerrainChunkCount = manifest.terrainChunks.size();

    result.readiness = manifestLoad.planAssetReadiness(handles);
    result.loadRequests = manifestLoad.planAssetLoadRequests();
    if (options.queueMissingAssetLoads)
    {
        result.loadQueue = manifestLoad.queueLatestAssetLoadRequests();
    }
    if (result.readiness.summary.missingHandleCount > 0 ||
        result.loadRequests.summary.requestCount > 0)
    {
        result.status = TerrainStreamingManifestUpdateStatus::AssetLoadsPending;
        copySummary(result);
        return result;
    }

    result.manifestStage = manifestLoad.stage(
        handles,
        registry,
        worldCatalog,
        resources,
        worldDescs,
        worldDescCount);
    if (result.manifestStage.status != TerrainManifestLoadStageStatus::Success ||
        hasStageFailure(result.manifestStage.stage))
    {
        result.status = TerrainStreamingManifestUpdateStatus::ManifestStageFailed;
        copySummary(result);
        return result;
    }
    if (result.manifestStage.stage.stagePlan.summary.changedUnsupportedCount > 0)
    {
        result.status = TerrainStreamingManifestUpdateStatus::UnsupportedStageChanges;
        copySummary(result);
        return result;
    }

    const std::vector<ChunkId> knownIds = manifestChunkIds(manifest);
    const TerrainStreamingPlan& retainedPlan = streaming.plan(
        streamingConfig,
        cameraWorld,
        knownIds.empty() ? nullptr : knownIds.data(),
        knownIds.size(),
        currentSnapshot);
    result.streamingPlan = retainedPlan;

    if (options.queueRuntimeRequests)
    {
        const std::vector<TerrainSetupStageDesc> setupDescs =
            setupDescsFromStagePlan(result.manifestStage.stage.stagePlan);
        result.streamingQueue = streaming.queueLatestPlan(
            runtime,
            setupDescs.empty() ? nullptr : setupDescs.data(),
            setupDescs.size());
        if (result.streamingQueue.status != TerrainStreamingQueueStatus::Success)
        {
            result.status = TerrainStreamingManifestUpdateStatus::StreamingQueueBlocked;
            copySummary(result);
            return result;
        }
    }

    result.status = TerrainStreamingManifestUpdateStatus::Success;
    copySummary(result);
    return result;
}
} // namespace full_engine
