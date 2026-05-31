#include "engine/assets/CookedAssetManifestSummary.hpp"

#include <algorithm>

namespace full_engine
{
namespace
{
void incrementKindSummary(AssetKindSummary& summary, const AssetKind kind) noexcept
{
    switch (kind)
    {
    case AssetKind::Mesh:
        ++summary.meshCount;
        break;
    case AssetKind::Material:
        ++summary.materialCount;
        break;
    case AssetKind::Texture:
        ++summary.textureCount;
        break;
    case AssetKind::TerrainChunk:
        ++summary.terrainChunkCount;
        break;
    case AssetKind::Skeleton:
        ++summary.skeletonCount;
        break;
    case AssetKind::SkinnedMesh:
        ++summary.skinnedMeshCount;
        break;
    case AssetKind::Shader:
        ++summary.shaderCount;
        break;
    case AssetKind::Unknown:
        break;
    }
}

void sortAndUnique(std::vector<AssetId>& ids)
{
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end(), [](const AssetId lhs, const AssetId rhs) {
                  return lhs == rhs;
              }),
        ids.end());
}
} // namespace

CookedAssetManifestDependencySummary summarizeCookedAssetManifestDependencies(
    const CookedAssetManifest& manifest)
{
    CookedAssetManifestDependencySummary summary;
    summary.genericAssetCount = manifest.assets.size();
    summary.terrainChunkCount = manifest.terrainChunks.size();

    for (const AssetRecord& record : manifest.assets)
    {
        incrementKindSummary(summary.assetKinds, record.kind);
        const std::uint32_t dependencyCount =
            record.dependencyCount <= kMaxAssetDependencies ? record.dependencyCount : kMaxAssetDependencies;
        summary.genericDependencyReferenceCount += dependencyCount;
        for (std::uint32_t index = 0; index < dependencyCount; ++index)
        {
            summary.uniqueGenericDependencies.push_back(record.dependencies[index].id);
        }
    }

    for (const TerrainChunkAssetDesc& desc : manifest.terrainChunks)
    {
        const std::uint32_t lodCount =
            desc.lodCount <= kMaxTerrainAssetLodLevels ? desc.lodCount : kMaxTerrainAssetLodLevels;
        summary.terrainLodReferenceCount += lodCount;
        for (std::uint32_t index = 0; index < lodCount; ++index)
        {
            summary.uniqueTerrainMeshes.push_back(desc.lods[index].mesh);
            summary.uniqueTerrainMaterials.push_back(desc.lods[index].material);
        }

        if (isValid(desc.splatMap))
        {
            summary.uniqueTerrainSplatMaps.push_back(desc.splatMap);
        }
        else
        {
            ++summary.defaultSplatMapCount;
        }
    }

    sortAndUnique(summary.uniqueGenericDependencies);
    sortAndUnique(summary.uniqueTerrainMeshes);
    sortAndUnique(summary.uniqueTerrainMaterials);
    sortAndUnique(summary.uniqueTerrainSplatMaps);
    return summary;
}
} // namespace full_engine
