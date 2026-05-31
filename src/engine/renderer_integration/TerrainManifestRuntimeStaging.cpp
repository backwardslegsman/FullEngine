#include "engine/renderer_integration/TerrainManifestRuntimeStaging.hpp"

#include <map>
#include <vector>

namespace full_engine
{
namespace
{
bool hasAssetResolveFailure(const TerrainAssetBatchResolveSummary& summary) noexcept
{
    return summary.missingChunkAssetsCount > 0 ||
        summary.invalidChunkAssetsCount > 0 ||
        summary.missingMeshHandleCount > 0 ||
        summary.missingMaterialHandleCount > 0 ||
        summary.missingSplatMapHandleCount > 0 ||
        summary.resourceCatalogFailedCount > 0;
}
} // namespace

const char* terrainManifestRuntimeStageStatusName(const TerrainManifestRuntimeStageStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestRuntimeStageStatus::Success:
        return "Success";
    case TerrainManifestRuntimeStageStatus::InvalidManifest:
        return "InvalidManifest";
    case TerrainManifestRuntimeStageStatus::MissingWorldDesc:
        return "MissingWorldDesc";
    case TerrainManifestRuntimeStageStatus::AssetResolveFailed:
        return "AssetResolveFailed";
    case TerrainManifestRuntimeStageStatus::QueueBlocked:
        return "QueueBlocked";
    }

    return "Unknown";
}

TerrainManifestRuntimeStageResult stageTerrainManifestRuntime(
    const CookedAssetManifest& manifest,
    const RendererAssetHandleCatalog& handles,
    const WorldChunkRegistry& registry,
    const WorldChunkCatalog& worldCatalog,
    const TerrainResourceCatalog& resources,
    const WorldChunkDesc* worldDescs,
    const std::size_t worldDescCount,
    TerrainRuntimeState* runtime,
    const TerrainManifestRuntimeStageOptions& options)
{
    TerrainManifestRuntimeStageResult result;
    result.summary.manifestAssetCount = manifest.assets.size();
    result.summary.manifestTerrainChunkCount = manifest.terrainChunks.size();

    result.manifestBuild = buildCatalogsFromCookedAssetManifest(manifest);
    if (result.manifestBuild.validation.result != CookedAssetManifestValidationResult::Success)
    {
        result.status = TerrainManifestRuntimeStageStatus::InvalidManifest;
        return result;
    }

    std::vector<ChunkId> terrainChunkIds;
    terrainChunkIds.reserve(manifest.terrainChunks.size());
    for (const TerrainChunkAssetDesc& terrainAssets : manifest.terrainChunks)
    {
        terrainChunkIds.push_back(terrainAssets.id);
    }

    result.assetResolve = resolveTerrainResourceCatalog(
        result.manifestBuild.catalogs.terrainAssets,
        terrainChunkIds.data(),
        terrainChunkIds.size(),
        handles);
    result.summary.resolvedResourceCount = result.assetResolve.summary.resolvedCount;

    std::map<ChunkId, WorldChunkDesc> worldDescById;
    if (worldDescs != nullptr)
    {
        for (std::size_t index = 0; index < worldDescCount; ++index)
        {
            worldDescById.emplace(worldDescs[index].id, worldDescs[index]);
        }
    }

    std::vector<TerrainSetupStageDesc> desiredSetup;
    desiredSetup.reserve(result.assetResolve.records.size());
    for (const TerrainAssetBatchResolveRecord& record : result.assetResolve.records)
    {
        if (record.status != TerrainAssetBatchResolveStatus::Resolved)
        {
            continue;
        }

        const auto worldDesc = worldDescById.find(record.id);
        if (worldDesc == worldDescById.end())
        {
            ++result.summary.missingWorldDescCount;
            continue;
        }

        const TerrainChunkResourceDesc* const resourceDesc =
            result.assetResolve.resources.findChunkResources(record.id);
        if (resourceDesc == nullptr)
        {
            continue;
        }

        TerrainSetupStageDesc stageDesc;
        stageDesc.id = record.id;
        stageDesc.worldDesc = worldDesc->second;
        stageDesc.resourceDesc = *resourceDesc;
        desiredSetup.push_back(stageDesc);
    }

    result.summary.desiredSetupCount = desiredSetup.size();
    result.stagePlan = planTerrainSetupChanges(
        registry,
        worldCatalog,
        resources,
        desiredSetup.data(),
        desiredSetup.size());

    if (hasAssetResolveFailure(result.assetResolve.summary))
    {
        result.status = TerrainManifestRuntimeStageStatus::AssetResolveFailed;
        return result;
    }

    if (result.summary.missingWorldDescCount > 0)
    {
        result.status = TerrainManifestRuntimeStageStatus::MissingWorldDesc;
        return result;
    }

    if (options.queueWhenSafe && runtime != nullptr)
    {
        result.queue = queueTerrainSetupStagePlan(
            *runtime,
            result.stagePlan,
            options.makeAddedChunksResident);
        result.summary.queuedSetupCount = result.queue.summary.queuedSetupCount;
        result.summary.queuedMakeResidentCount = result.queue.summary.queuedMakeResidentCount;
        if (result.queue.result == TerrainSetupStageQueueResult::BlockedUnsupportedChanges)
        {
            result.status = TerrainManifestRuntimeStageStatus::QueueBlocked;
            return result;
        }
    }

    return result;
}
} // namespace full_engine
