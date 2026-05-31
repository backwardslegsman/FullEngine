#include "engine/renderer_integration/TerrainManifestAssetReadiness.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

full_engine::ChunkId chunk(const std::int32_t x) noexcept
{
    return {x, 0, 0};
}

full_engine::AssetRecord assetRecord(const std::uint64_t id, const full_engine::AssetKind kind) noexcept
{
    full_engine::AssetRecord record;
    record.id = asset(id);
    record.kind = kind;
    return record;
}

full_engine::TerrainChunkAssetDesc terrainAssets(
    const full_engine::ChunkId& id,
    const std::uint64_t meshId,
    const std::uint64_t materialId,
    const std::uint64_t splatId)
{
    full_engine::TerrainChunkAssetDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = asset(meshId);
    desc.lods[0].material = asset(materialId);
    desc.lods[0].maxDistanceMeters = 64.0f;
    desc.lods[1].mesh = asset(999);
    desc.lods[1].material = asset(998);
    desc.splatMap = asset(splatId);
    return desc;
}

full_engine::RendererAssetHandleCatalog partialHandles()
{
    full_engine::RendererAssetHandleCatalog handles;
    (void)handles.addMeshHandle(asset(1), {10});
    (void)handles.addMaterialHandle(asset(2), {20});
    (void)handles.addTextureHandle(asset(3), {30});
    return handles;
}

const full_engine::TerrainManifestAssetReadinessRecord& recordAt(
    const full_engine::TerrainManifestAssetReadinessPlan& plan,
    const std::size_t index)
{
    return plan.records[index];
}

void testEmptyManifest(std::vector<std::string>& failures)
{
    const full_engine::TerrainManifestAssetReadinessPlan plan =
        full_engine::planTerrainManifestAssetReadiness({}, {});

    expect(plan.records.empty(), "empty manifest has no readiness records", failures);
    expect(plan.summary.requestedCount == 0, "empty manifest has zero requested count", failures);
    expect(plan.summary.readyCount == 0, "empty manifest has zero ready count", failures);
    expect(plan.summary.missingHandleCount == 0, "empty manifest has zero missing count", failures);
}

void testTerrainReferencesAndDefaultSplat(std::vector<std::string>& failures)
{
    full_engine::CookedAssetManifest manifest;
    manifest.terrainChunks.push_back(terrainAssets(chunk(1), 1, 2, 3));
    full_engine::TerrainChunkAssetDesc defaultSplat = terrainAssets(chunk(2), 4, 5, 0);
    defaultSplat.splatMap = {};
    manifest.terrainChunks.push_back(defaultSplat);

    const full_engine::TerrainManifestAssetReadinessPlan plan =
        full_engine::planTerrainManifestAssetReadiness(manifest, partialHandles());

    expect(plan.records.size() == 5, "terrain refs produce mesh/material/valid texture records", failures);
    expect(plan.summary.meshRequestedCount == 2, "mesh requests counted", failures);
    expect(plan.summary.materialRequestedCount == 2, "material requests counted", failures);
    expect(plan.summary.textureRequestedCount == 1, "default splat ignored", failures);
    expect(plan.summary.meshReadyCount == 1, "ready mesh counted", failures);
    expect(plan.summary.meshMissingHandleCount == 1, "missing mesh counted", failures);
    expect(plan.summary.materialReadyCount == 1, "ready material counted", failures);
    expect(plan.summary.materialMissingHandleCount == 1, "missing material counted", failures);
    expect(plan.summary.textureReadyCount == 1, "ready texture counted", failures);
}

void testDuplicatesGenericDependenciesAndOrdering(std::vector<std::string>& failures)
{
    full_engine::CookedAssetManifest manifest;
    full_engine::AssetRecord owner = assetRecord(100, full_engine::AssetKind::TerrainChunk);
    owner.dependencyCount = 5;
    owner.dependencies[0] = {asset(7), full_engine::AssetKind::Mesh};
    owner.dependencies[1] = {asset(6), full_engine::AssetKind::Mesh};
    owner.dependencies[2] = {asset(8), full_engine::AssetKind::Material};
    owner.dependencies[3] = {asset(9), full_engine::AssetKind::Texture};
    owner.dependencies[4] = {asset(10), full_engine::AssetKind::Skeleton};
    owner.dependencies[5] = {asset(11), full_engine::AssetKind::Mesh};
    manifest.assets.push_back(owner);
    manifest.terrainChunks.push_back(terrainAssets(chunk(1), 7, 8, 9));
    manifest.terrainChunks.push_back(terrainAssets(chunk(2), 6, 8, 9));

    full_engine::RendererAssetHandleCatalog handles;
    (void)handles.addMeshHandle(asset(6), {60});
    (void)handles.addTextureHandle(asset(9), {90});

    const full_engine::TerrainManifestAssetReadinessPlan plan =
        full_engine::planTerrainManifestAssetReadiness(manifest, handles);

    expect(plan.records.size() == 4, "duplicates are deduplicated and non-renderer deps ignored", failures);
    expect(recordAt(plan, 0).kind == full_engine::TerrainManifestAssetHandleKind::Mesh, "record 0 is mesh", failures);
    expect(recordAt(plan, 0).id == asset(6), "mesh records sorted by id", failures);
    expect(recordAt(plan, 0).status == full_engine::TerrainManifestAssetReadinessStatus::Ready, "mesh 6 ready", failures);
    expect(recordAt(plan, 1).kind == full_engine::TerrainManifestAssetHandleKind::Mesh, "record 1 is mesh", failures);
    expect(recordAt(plan, 1).id == asset(7), "mesh 7 follows mesh 6", failures);
    expect(recordAt(plan, 1).status == full_engine::TerrainManifestAssetReadinessStatus::MissingHandle, "mesh 7 missing", failures);
    expect(recordAt(plan, 2).kind == full_engine::TerrainManifestAssetHandleKind::Material, "materials follow meshes", failures);
    expect(recordAt(plan, 2).id == asset(8), "material id sorted", failures);
    expect(recordAt(plan, 2).status == full_engine::TerrainManifestAssetReadinessStatus::MissingHandle, "material missing", failures);
    expect(recordAt(plan, 3).kind == full_engine::TerrainManifestAssetHandleKind::Texture, "textures follow materials", failures);
    expect(recordAt(plan, 3).id == asset(9), "texture id sorted", failures);
    expect(recordAt(plan, 3).status == full_engine::TerrainManifestAssetReadinessStatus::Ready, "texture ready", failures);
}

void testInactiveSlotsIgnored(std::vector<std::string>& failures)
{
    full_engine::CookedAssetManifest manifest;
    full_engine::AssetRecord owner = assetRecord(100, full_engine::AssetKind::TerrainChunk);
    owner.dependencyCount = 1;
    owner.dependencies[0] = {asset(1), full_engine::AssetKind::Mesh};
    owner.dependencies[1] = {asset(2), full_engine::AssetKind::Material};
    manifest.assets.push_back(owner);

    full_engine::TerrainChunkAssetDesc desc = terrainAssets(chunk(1), 3, 4, 0);
    desc.lodCount = 0;
    desc.splatMap = {};
    manifest.terrainChunks.push_back(desc);

    const full_engine::TerrainManifestAssetReadinessPlan plan =
        full_engine::planTerrainManifestAssetReadiness(manifest, {});

    expect(plan.records.size() == 1, "inactive dependency and LOD slots ignored", failures);
    expect(plan.records[0].id == asset(1), "only active dependency remains", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testEmptyManifest(failures);
    testTerrainReferencesAndDefaultSplat(failures);
    testDuplicatesGenericDependenciesAndOrdering(failures);
    testInactiveSlotsIgnored(failures);

    if (!failures.empty())
    {
        for (const std::string& failure : failures)
        {
            std::cerr << failure << '\n';
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
