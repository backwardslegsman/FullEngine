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
}

void TerrainManifestLoadState::clearManifest()
{
    manifest_ = {};
    hasManifest_ = false;
    latestStage_ = {};
    latestDiagnostics_ = {};
    latestReadiness_ = {};
    latestLoadRequests_ = {};
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

const TerrainManifestAssetReadinessPlan& TerrainManifestLoadState::planAssetReadiness(
    const RendererAssetHandleCatalog& handles)
{
    latestReadiness_ = hasManifest_ ? planTerrainManifestAssetReadiness(manifest_, handles)
                                    : TerrainManifestAssetReadinessPlan{};
    latestLoadRequests_ = {};
    return latestReadiness_;
}

const TerrainManifestAssetLoadRequestPlan& TerrainManifestLoadState::planAssetLoadRequests()
{
    latestLoadRequests_ = buildTerrainManifestAssetLoadRequestPlan(latestReadiness_);
    return latestLoadRequests_;
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
