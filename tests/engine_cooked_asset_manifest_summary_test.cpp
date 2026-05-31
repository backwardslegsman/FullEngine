#include "engine/assets/CookedAssetManifestSummary.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace
{
full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return full_engine::AssetId{value};
}

full_engine::ChunkId chunk(const int x, const int y = 0, const int z = 0) noexcept
{
    return full_engine::ChunkId{x, y, z};
}

full_engine::AssetRecord makeAsset(
    const std::uint64_t id,
    const full_engine::AssetKind kind)
{
    full_engine::AssetRecord record;
    record.id = asset(id);
    record.kind = kind;
    return record;
}

full_engine::TerrainChunkAssetDesc makeTerrain(
    const full_engine::ChunkId id,
    const std::uint64_t meshId,
    const std::uint64_t materialId,
    const std::uint64_t splatId)
{
    full_engine::TerrainChunkAssetDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = asset(meshId);
    desc.lods[0].material = asset(materialId);
    desc.lods[0].maxDistanceMeters = 100.0f;
    desc.splatMap = asset(splatId);
    return desc;
}

bool sameIds(const std::vector<full_engine::AssetId>& actual, const std::vector<full_engine::AssetId>& expected)
{
    if (actual.size() != expected.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < actual.size(); ++index)
    {
        if (!(actual[index] == expected[index]))
        {
            return false;
        }
    }

    return true;
}
} // namespace

int main()
{
    {
        const full_engine::CookedAssetManifest manifest;
        const full_engine::CookedAssetManifestDependencySummary summary =
            full_engine::summarizeCookedAssetManifestDependencies(manifest);

        assert(summary.genericAssetCount == 0);
        assert(summary.terrainChunkCount == 0);
        assert(summary.genericDependencyReferenceCount == 0);
        assert(summary.terrainLodReferenceCount == 0);
        assert(summary.defaultSplatMapCount == 0);
        assert(summary.uniqueGenericDependencies.empty());
        assert(summary.uniqueTerrainMeshes.empty());
        assert(summary.uniqueTerrainMaterials.empty());
        assert(summary.uniqueTerrainSplatMaps.empty());
    }

    {
        full_engine::CookedAssetManifest manifest;
        manifest.assets.push_back(makeAsset(1, full_engine::AssetKind::Mesh));
        manifest.assets.push_back(makeAsset(2, full_engine::AssetKind::Material));
        manifest.assets.push_back(makeAsset(3, full_engine::AssetKind::Texture));
        manifest.assets.push_back(makeAsset(4, full_engine::AssetKind::TerrainChunk));
        manifest.assets.push_back(makeAsset(5, full_engine::AssetKind::Skeleton));
        manifest.assets.push_back(makeAsset(6, full_engine::AssetKind::SkinnedMesh));
        manifest.assets.push_back(makeAsset(7, full_engine::AssetKind::Shader));
        manifest.assets.push_back(makeAsset(8, full_engine::AssetKind::Unknown));

        const full_engine::CookedAssetManifestDependencySummary summary =
            full_engine::summarizeCookedAssetManifestDependencies(manifest);

        assert(summary.genericAssetCount == 8);
        assert(summary.assetKinds.meshCount == 1);
        assert(summary.assetKinds.materialCount == 1);
        assert(summary.assetKinds.textureCount == 1);
        assert(summary.assetKinds.terrainChunkCount == 1);
        assert(summary.assetKinds.skeletonCount == 1);
        assert(summary.assetKinds.skinnedMeshCount == 1);
        assert(summary.assetKinds.shaderCount == 1);
    }

    {
        full_engine::CookedAssetManifest manifest;
        full_engine::AssetRecord material = makeAsset(20, full_engine::AssetKind::Material);
        material.dependencyCount = 3;
        material.dependencies[0].id = asset(50);
        material.dependencies[0].kind = full_engine::AssetKind::Texture;
        material.dependencies[1].id = asset(40);
        material.dependencies[1].kind = full_engine::AssetKind::Shader;
        material.dependencies[2].id = asset(50);
        material.dependencies[2].kind = full_engine::AssetKind::Texture;
        material.dependencies[3].id = asset(1);
        material.dependencies[3].kind = full_engine::AssetKind::Mesh;
        manifest.assets.push_back(material);

        const full_engine::CookedAssetManifestDependencySummary summary =
            full_engine::summarizeCookedAssetManifestDependencies(manifest);

        assert(summary.genericDependencyReferenceCount == 3);
        assert(sameIds(summary.uniqueGenericDependencies, {asset(40), asset(50)}));
    }

    {
        full_engine::CookedAssetManifest manifest;
        full_engine::TerrainChunkAssetDesc first = makeTerrain(chunk(1), 300, 200, 500);
        first.lodCount = 2;
        first.lods[1].mesh = asset(100);
        first.lods[1].material = asset(200);
        first.lods[1].maxDistanceMeters = 250.0f;
        first.lods[2].mesh = asset(1);
        first.lods[2].material = asset(2);
        first.lods[2].maxDistanceMeters = 500.0f;

        full_engine::TerrainChunkAssetDesc second = makeTerrain(chunk(2), 100, 201, 500);
        full_engine::TerrainChunkAssetDesc fallback = makeTerrain(chunk(3), 300, 201, 0);
        manifest.terrainChunks.push_back(first);
        manifest.terrainChunks.push_back(second);
        manifest.terrainChunks.push_back(fallback);

        const full_engine::CookedAssetManifestDependencySummary summary =
            full_engine::summarizeCookedAssetManifestDependencies(manifest);

        assert(summary.terrainChunkCount == 3);
        assert(summary.terrainLodReferenceCount == 4);
        assert(summary.defaultSplatMapCount == 1);
        assert(sameIds(summary.uniqueTerrainMeshes, {asset(100), asset(300)}));
        assert(sameIds(summary.uniqueTerrainMaterials, {asset(200), asset(201)}));
        assert(sameIds(summary.uniqueTerrainSplatMaps, {asset(500)}));
    }

    {
        full_engine::CookedAssetManifest manifest;
        manifest.assets.push_back(makeAsset(90, full_engine::AssetKind::Mesh));
        manifest.terrainChunks.push_back(makeTerrain(chunk(4), 3, 2, 1));
        const std::size_t assetCount = manifest.assets.size();
        const std::size_t terrainCount = manifest.terrainChunks.size();

        (void)full_engine::summarizeCookedAssetManifestDependencies(manifest);

        assert(manifest.assets.size() == assetCount);
        assert(manifest.terrainChunks.size() == terrainCount);
        assert(manifest.terrainChunks[0].lods[0].mesh == asset(3));
    }

    return 0;
}
