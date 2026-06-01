#include "engine/renderer_integration/TerrainManifestLoadState.hpp"

#include <utility>

namespace full_engine
{
const char* terrainManifestLoadStageStatusName(const TerrainManifestLoadStageStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestLoadStageStatus::Success:
        return "Success";
    case TerrainManifestLoadStageStatus::NoManifest:
        return "NoManifest";
    }

    return "Unknown";
}

void TerrainManifestLoadState::setManifest(CookedAssetManifest manifest)
{
    manifest_ = std::move(manifest);
    hasManifest_ = true;
    latestStage_ = {};
    latestDiagnostics_ = {};
    latestReadiness_ = {};
    latestLoadRequests_ = {};
    loadRequestQueue_.clear();
    latestLoadRequestQueueResult_ = {};
    latestLoadConsumeResult_ = {};
    latestLoadExecutorResult_ = {};
    assetSources_.clear();
    hasAssetSources_ = false;
    latestSourceRequests_ = {};
    latestSourceUploadIntents_ = {};
}

void TerrainManifestLoadState::clearManifest()
{
    manifest_ = {};
    hasManifest_ = false;
    latestStage_ = {};
    latestDiagnostics_ = {};
    latestReadiness_ = {};
    latestLoadRequests_ = {};
    loadRequestQueue_.clear();
    latestLoadRequestQueueResult_ = {};
    latestLoadConsumeResult_ = {};
    latestLoadExecutorResult_ = {};
    assetSources_.clear();
    hasAssetSources_ = false;
    latestSourceRequests_ = {};
    latestSourceUploadIntents_ = {};
}

bool TerrainManifestLoadState::hasManifest() const noexcept
{
    return hasManifest_;
}

const CookedAssetManifest& TerrainManifestLoadState::manifest() const noexcept
{
    return manifest_;
}

const TerrainManifestRuntimeStageResult& TerrainManifestLoadState::latestStage() const noexcept
{
    return latestStage_;
}

const TerrainManifestRuntimeStageDiagnostics& TerrainManifestLoadState::latestDiagnostics() const noexcept
{
    return latestDiagnostics_;
}

const TerrainManifestAssetReadinessPlan& TerrainManifestLoadState::latestReadiness() const noexcept
{
    return latestReadiness_;
}

const TerrainManifestAssetLoadRequestPlan& TerrainManifestLoadState::latestLoadRequests() const noexcept
{
    return latestLoadRequests_;
}

const TerrainManifestAssetLoadRequestQueue& TerrainManifestLoadState::loadRequestQueue() const noexcept
{
    return loadRequestQueue_;
}

const TerrainManifestAssetLoadQueuePushResult& TerrainManifestLoadState::latestLoadRequestQueueResult() const noexcept
{
    return latestLoadRequestQueueResult_;
}

const TerrainManifestAssetLoadResult& TerrainManifestLoadState::latestLoadConsumeResult() const noexcept
{
    return latestLoadConsumeResult_;
}

const TerrainManifestAssetLoadExecutorResult& TerrainManifestLoadState::latestLoadExecutorResult() const noexcept
{
    return latestLoadExecutorResult_;
}

bool TerrainManifestLoadState::hasAssetSources() const noexcept
{
    return hasAssetSources_;
}

const AssetSourceCatalog& TerrainManifestLoadState::assetSources() const noexcept
{
    return assetSources_;
}

const TerrainManifestAssetSourceRequestPlan& TerrainManifestLoadState::latestSourceRequests() const noexcept
{
    return latestSourceRequests_;
}

const AssetSourceUploadIntentPlan& TerrainManifestLoadState::latestSourceUploadIntents() const noexcept
{
    return latestSourceUploadIntents_;
}

std::size_t TerrainManifestLoadState::pendingLoadRequestCount() const noexcept
{
    return loadRequestQueue_.requestCount();
}

void TerrainManifestLoadState::setAssetSources(AssetSourceCatalog sources)
{
    assetSources_ = std::move(sources);
    hasAssetSources_ = true;
    latestSourceRequests_ = {};
    latestSourceUploadIntents_ = {};
}

void TerrainManifestLoadState::clearAssetSources()
{
    assetSources_.clear();
    hasAssetSources_ = false;
    latestSourceRequests_ = {};
    latestSourceUploadIntents_ = {};
}

const TerrainManifestAssetReadinessPlan& TerrainManifestLoadState::planAssetReadiness(
    const RendererAssetHandleCatalog& handles)
{
    latestReadiness_ = hasManifest_ ? planTerrainManifestAssetReadiness(manifest_, handles)
                                    : TerrainManifestAssetReadinessPlan{};
    latestLoadRequests_ = {};
    latestSourceRequests_ = {};
    latestSourceUploadIntents_ = {};
    return latestReadiness_;
}

const TerrainManifestAssetLoadRequestPlan& TerrainManifestLoadState::planAssetLoadRequests()
{
    latestLoadRequests_ = buildTerrainManifestAssetLoadRequestPlan(latestReadiness_);
    latestSourceRequests_ = {};
    latestSourceUploadIntents_ = {};
    return latestLoadRequests_;
}

const TerrainManifestAssetLoadQueuePushResult& TerrainManifestLoadState::queueLatestAssetLoadRequests()
{
    latestLoadRequestQueueResult_ = loadRequestQueue_.pushPlan(latestLoadRequests_);
    return latestLoadRequestQueueResult_;
}

const TerrainManifestAssetSourceRequestPlan& TerrainManifestLoadState::planAssetSources()
{
    latestSourceRequests_ = hasAssetSources_ ?
        buildTerrainManifestAssetSourceRequestPlan(latestLoadRequests_, assetSources_) :
        TerrainManifestAssetSourceRequestPlan{};
    latestSourceUploadIntents_ = buildAssetSourceUploadIntentPlan(latestSourceRequests_);
    return latestSourceRequests_;
}

const AssetSourceUploadIntentPlan& TerrainManifestLoadState::planAssetSourceUploadIntents()
{
    latestSourceUploadIntents_ = buildAssetSourceUploadIntentPlan(latestSourceRequests_);
    return latestSourceUploadIntents_;
}

const TerrainManifestAssetLoadResult& TerrainManifestLoadState::consumePendingAssetLoadRequests(
    const RendererAssetHandleCatalog& sourceHandles,
    RendererAssetHandleCatalog& destinationHandles)
{
    latestLoadConsumeResult_ =
        consumeTerrainManifestAssetLoadRequests(loadRequestQueue_, sourceHandles, destinationHandles);
    latestSourceRequests_ = {};
    latestSourceUploadIntents_ = {};
    return latestLoadConsumeResult_;
}

const TerrainManifestAssetLoadExecutorResult& TerrainManifestLoadState::executePendingAssetLoadRequests(
    RendererAssetHandleCatalog& destinationHandles,
    TerrainManifestAssetLoadCallback callback,
    void* const userData)
{
    latestLoadExecutorResult_ =
        executeTerrainManifestAssetLoadRequests(loadRequestQueue_, destinationHandles, callback, userData);
    latestLoadConsumeResult_ = latestLoadExecutorResult_.consume;
    latestSourceRequests_ = {};
    latestSourceUploadIntents_ = {};
    return latestLoadExecutorResult_;
}

TerrainManifestLoadStageResult TerrainManifestLoadState::stage(
    const RendererAssetHandleCatalog& handles,
    const WorldChunkRegistry& registry,
    const WorldChunkCatalog& worldCatalog,
    const TerrainResourceCatalog& resources,
    const WorldChunkDesc* const worldDescs,
    const std::size_t worldDescCount)
{
    if (!hasManifest_)
    {
        return noManifestResult();
    }

    TerrainManifestLoadStageResult result;
    result.stage = stageTerrainManifestRuntime(
        manifest_,
        handles,
        registry,
        worldCatalog,
        resources,
        worldDescs,
        worldDescCount);
    storeStageResult(result.stage);
    return result;
}

TerrainManifestLoadStageResult TerrainManifestLoadState::queueStage(
    TerrainRuntimeState& runtime,
    const RendererAssetHandleCatalog& handles,
    const WorldChunkRegistry& registry,
    const WorldChunkCatalog& worldCatalog,
    const TerrainResourceCatalog& resources,
    const WorldChunkDesc* const worldDescs,
    const std::size_t worldDescCount,
    const bool makeAddedChunksResident)
{
    if (!hasManifest_)
    {
        return noManifestResult();
    }

    TerrainManifestRuntimeStageOptions options;
    options.queueWhenSafe = true;
    options.makeAddedChunksResident = makeAddedChunksResident;

    TerrainManifestLoadStageResult result;
    result.stage = stageTerrainManifestRuntime(
        manifest_,
        handles,
        registry,
        worldCatalog,
        resources,
        worldDescs,
        worldDescCount,
        &runtime,
        options);
    storeStageResult(result.stage);
    return result;
}

void TerrainManifestLoadState::storeStageResult(const TerrainManifestRuntimeStageResult& stage)
{
    latestStage_ = stage;
    latestDiagnostics_ = makeTerrainManifestRuntimeStageDiagnostics(latestStage_);
}

TerrainManifestLoadStageResult TerrainManifestLoadState::noManifestResult()
{
    latestStage_ = {};
    latestDiagnostics_ = {};

    TerrainManifestLoadStageResult result;
    result.status = TerrainManifestLoadStageStatus::NoManifest;
    return result;
}
} // namespace full_engine
