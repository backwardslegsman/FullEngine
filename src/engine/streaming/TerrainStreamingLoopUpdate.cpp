#include "engine/streaming/TerrainStreamingLoopUpdate.hpp"

namespace full_engine
{
namespace
{
bool isStreamingBlocked(const TerrainStreamingManifestUpdateStatus status) noexcept
{
    return status != TerrainStreamingManifestUpdateStatus::Success;
}

TerrainStreamingLoopUpdateStatus mapRuntimeStatus(const TerrainRuntimeUpdateStatus status) noexcept
{
    switch (status)
    {
    case TerrainRuntimeUpdateStatus::Success:
        return TerrainStreamingLoopUpdateStatus::Success;
    case TerrainRuntimeUpdateStatus::SetupFailed:
        return TerrainStreamingLoopUpdateStatus::RuntimeSetupFailed;
    case TerrainRuntimeUpdateStatus::ResidencyFailed:
        return TerrainStreamingLoopUpdateStatus::RuntimeResidencyFailed;
    case TerrainRuntimeUpdateStatus::PipelineFailed:
        return TerrainStreamingLoopUpdateStatus::RuntimePipelineFailed;
    }

    return TerrainStreamingLoopUpdateStatus::RuntimePipelineFailed;
}

TerrainStreamingTickEvent makeTickEvent(const TerrainStreamingLoopUpdateResult& result) noexcept
{
    TerrainStreamingTickEvent event;
    event.streamingStatus = result.streaming.status;
    event.runtimeStatus = result.runtime.status;
    event.runtimeUpdateRan = result.runtimeUpdateRan;
    event.setupRequestsBeforeRuntime = result.setupRequestsBeforeRuntime;
    event.residencyRequestsBeforeRuntime = result.residencyRequestsBeforeRuntime;
    event.setupRequestsAfterRuntime = result.setupRequestsAfterRuntime;
    event.residencyRequestsAfterRuntime = result.residencyRequestsAfterRuntime;
    event.streaming = result.streaming.summary;
    event.streamingQueue = result.streaming.streamingQueue.summary;
    event.runtimeLifecycle = result.runtime.pipeline.lifecycle.summary;
    event.runtimeSubmission = result.runtime.pipeline.submission.summary;
    return event;
}

TerrainStreamingLoopUpdateResult finishLoopUpdate(
    TerrainStreamingLoopState& loop,
    TerrainStreamingLoopUpdateResult result)
{
    loop.appendTickEvent(makeTickEvent(result));
    return result;
}
} // namespace

const char* terrainStreamingLoopUpdateStatusName(
    const TerrainStreamingLoopUpdateStatus status) noexcept
{
    switch (status)
    {
    case TerrainStreamingLoopUpdateStatus::Success:
        return "Success";
    case TerrainStreamingLoopUpdateStatus::StreamingBlocked:
        return "StreamingBlocked";
    case TerrainStreamingLoopUpdateStatus::RuntimeSetupFailed:
        return "RuntimeSetupFailed";
    case TerrainStreamingLoopUpdateStatus::RuntimeResidencyFailed:
        return "RuntimeResidencyFailed";
    case TerrainStreamingLoopUpdateStatus::RuntimePipelineFailed:
        return "RuntimePipelineFailed";
    }

    return "Unknown";
}

TerrainStreamingLoopUpdateResult updateTerrainStreamingLoop(
    TerrainStreamingLoopState& loop,
    TerrainRuntimeState& terrainRuntime,
    full_renderer::IRenderer& renderer,
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resources,
    ChunkTerrainHandleMap& handles,
    const RendererAssetHandleCatalog& assetHandles,
    const WorldChunkDesc* const worldDescs,
    const std::size_t worldDescCount,
    const ChunkId* const trackedIds,
    const std::size_t trackedIdCount,
    const TerrainStreamingPlannerConfig& streamingConfig,
    const WorldPosition& cameraWorld,
    const TerrainRuntimeStateSnapshot& currentSnapshot,
    const TerrainStreamingLoopUpdateOptions& options)
{
    TerrainStreamingManifestUpdateOptions streamingOptions = options.streaming;
    streamingOptions.queueOptions = options.budgets.queue;

    TerrainRuntimeUpdateOptions runtimeOptions = options.runtime;
    runtimeOptions.pipelineOptions.lifecycleOptions.maxCreateCount = options.budgets.maxPipelineCreates;
    runtimeOptions.pipelineOptions.lifecycleOptions.maxUpdateCount = options.budgets.maxPipelineUpdates;
    runtimeOptions.pipelineOptions.lifecycleOptions.maxReleaseCount = options.budgets.maxPipelineReleases;

    TerrainStreamingLoopUpdateResult result;
    result.streaming = loop.updateStreamingFromManifest(
        assetHandles,
        registry,
        worldCatalog,
        resources,
        worldDescs,
        worldDescCount,
        terrainRuntime,
        streamingConfig,
        cameraWorld,
        currentSnapshot,
        streamingOptions);

    result.setupRequestsBeforeRuntime = terrainRuntime.setupRequestCount();
    result.residencyRequestsBeforeRuntime = terrainRuntime.residencyRequestCount();

    if (isStreamingBlocked(result.streaming.status))
    {
        result.status = TerrainStreamingLoopUpdateStatus::StreamingBlocked;
        result.setupRequestsAfterRuntime = terrainRuntime.setupRequestCount();
        result.residencyRequestsAfterRuntime = terrainRuntime.residencyRequestCount();
        return finishLoopUpdate(loop, result);
    }

    if (!terrainRuntime.hasPendingRequests())
    {
        result.status = TerrainStreamingLoopUpdateStatus::Success;
        result.setupRequestsAfterRuntime = terrainRuntime.setupRequestCount();
        result.residencyRequestsAfterRuntime = terrainRuntime.residencyRequestCount();
        return finishLoopUpdate(loop, result);
    }

    result.runtime = terrainRuntime.updateWithSnapshot(
        renderer,
        registry,
        worldCatalog,
        resources,
        handles,
        trackedIds,
        trackedIdCount,
        runtimeOptions);
    result.runtimeUpdateRan = true;
    result.setupRequestsAfterRuntime = terrainRuntime.setupRequestCount();
    result.residencyRequestsAfterRuntime = terrainRuntime.residencyRequestCount();
    result.status = mapRuntimeStatus(result.runtime.status);
    return finishLoopUpdate(loop, result);
}

TerrainStreamingLoopUpdateResult updateTerrainStreamingLoop(
    TerrainStreamingLoopState& loop,
    TerrainRuntimeState& terrainRuntime,
    full_renderer::IRenderer& renderer,
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resources,
    ChunkTerrainHandleMap& handles,
    const RendererAssetHandleCatalog& assetHandles,
    const WorldChunkDesc* const worldDescs,
    const std::size_t worldDescCount,
    const ChunkId* const trackedIds,
    const std::size_t trackedIdCount,
    const TerrainStreamingPlannerConfig& streamingConfig,
    const WorldPosition& cameraWorld,
    const TerrainRuntimeStateSnapshot& currentSnapshot,
    const TerrainStreamingManifestUpdateOptions& streamingOptions,
    const TerrainRuntimeUpdateOptions& runtimeOptions)
{
    TerrainStreamingLoopUpdateOptions options;
    options.streaming = streamingOptions;
    options.runtime = runtimeOptions;
    return updateTerrainStreamingLoop(
        loop,
        terrainRuntime,
        renderer,
        registry,
        worldCatalog,
        resources,
        handles,
        assetHandles,
        worldDescs,
        worldDescCount,
        trackedIds,
        trackedIdCount,
        streamingConfig,
        cameraWorld,
        currentSnapshot,
        options);
}
} // namespace full_engine
