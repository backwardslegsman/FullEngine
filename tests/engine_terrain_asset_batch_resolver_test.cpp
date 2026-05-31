#include "engine/renderer_integration/TerrainAssetResolver.hpp"

#include <cstdint>
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
    return full_engine::AssetId{value};
}

full_renderer::MeshHandle mesh(const std::uint32_t id) noexcept
{
    return full_renderer::MeshHandle{id};
}

full_renderer::MaterialHandle material(const std::uint32_t id) noexcept
{
    return full_renderer::MaterialHandle{id};
}

full_renderer::TextureHandle texture(const std::uint32_t id) noexcept
{
    return full_renderer::TextureHandle{id};
}

full_engine::ChunkId chunk(const int x, const int y = 0, const int z = 0) noexcept
{
    return full_engine::ChunkId{x, y, z};
}

full_engine::TerrainChunkAssetDesc makeAssetDesc(
    const full_engine::ChunkId& id,
    const std::uint32_t lodCount = 1)
{
    full_engine::TerrainChunkAssetDesc desc;
    desc.id = id;
    desc.lodCount = lodCount;
    for (std::uint32_t index = 0; index < lodCount && index < full_engine::kMaxTerrainAssetLodLevels; ++index)
    {
        desc.lods[index].mesh = asset(100 + index);
        desc.lods[index].material = asset(200 + index);
        desc.lods[index].maxDistanceMeters = static_cast<float>((index + 1) * 80);
    }
    return desc;
}

full_engine::TerrainAssetCatalog makeAssetCatalog()
{
    full_engine::TerrainAssetCatalog assets;
    (void)assets.addChunkAssets(makeAssetDesc(chunk(0)));
    (void)assets.addChunkAssets(makeAssetDesc(chunk(1)));
    (void)assets.addChunkAssets(makeAssetDesc(chunk(2)));
    return assets;
}

full_engine::RendererAssetHandleCatalog makeHandleCatalog()
{
    full_engine::RendererAssetHandleCatalog handles;
    for (std::uint32_t index = 0; index < full_engine::kMaxTerrainAssetLodLevels; ++index)
    {
        (void)handles.addMeshHandle(asset(100 + index), mesh(1000 + index));
        (void)handles.addMaterialHandle(asset(200 + index), material(2000 + index));
    }
    (void)handles.addTextureHandle(asset(300), texture(3000));
    return handles;
}

void testValidBatchBuildsResourceCatalog(std::vector<std::string>& failures)
{
    const full_engine::TerrainAssetCatalog assets = makeAssetCatalog();
    const full_engine::RendererAssetHandleCatalog handles = makeHandleCatalog();
    const std::vector<full_engine::ChunkId> ids = {chunk(0), chunk(1)};

    const full_engine::TerrainAssetBatchResolveResult result =
        full_engine::resolveTerrainResourceCatalog(assets, ids.data(), ids.size(), handles);

    expect(result.records.size() == 2, "valid batch emits one record per requested chunk", failures);
    expect(result.summary.resolvedCount == 2, "valid batch counts resolved chunks", failures);
    expect(result.resources.resourceCount() == 2, "valid batch builds resource catalog", failures);
    expect(result.resources.contains(chunk(0)), "resource catalog contains first chunk", failures);
    expect(result.resources.contains(chunk(1)), "resource catalog contains second chunk", failures);
    expect(result.records[0].id == chunk(0), "valid batch preserves first requested id", failures);
    expect(result.records[1].id == chunk(1), "valid batch preserves second requested id", failures);
}

void testRequestedOrderIsPreserved(std::vector<std::string>& failures)
{
    const full_engine::TerrainAssetCatalog assets = makeAssetCatalog();
    const full_engine::RendererAssetHandleCatalog handles = makeHandleCatalog();
    const std::vector<full_engine::ChunkId> ids = {chunk(2), chunk(0), chunk(1)};

    const full_engine::TerrainAssetBatchResolveResult result =
        full_engine::resolveTerrainResourceCatalog(assets, ids.data(), ids.size(), handles);

    expect(result.records.size() == ids.size(), "ordered batch emits all records", failures);
    for (std::size_t index = 0; index < ids.size() && index < result.records.size(); ++index)
    {
        expect(result.records[index].id == ids[index], "batch diagnostics preserve input order", failures);
    }
}

void testMissingAndInvalidChunkAssets(std::vector<std::string>& failures)
{
    full_engine::TerrainAssetCatalog assets = makeAssetCatalog();
    (void)assets.addChunkAssets(makeAssetDesc(chunk(4)));
    full_engine::TerrainChunkAssetDesc* invalid =
        const_cast<full_engine::TerrainChunkAssetDesc*>(assets.findChunkAssets(chunk(4)));
    if (invalid != nullptr)
    {
        invalid->lods[0].mesh = {};
    }

    const full_engine::RendererAssetHandleCatalog handles = makeHandleCatalog();
    const std::vector<full_engine::ChunkId> ids = {chunk(99), chunk(4), chunk(0)};

    const full_engine::TerrainAssetBatchResolveResult result =
        full_engine::resolveTerrainResourceCatalog(assets, ids.data(), ids.size(), handles);

    expect(result.records.size() == 3, "failure batch emits requested records", failures);
    expect(
        result.records[0].status == full_engine::TerrainAssetBatchResolveStatus::MissingChunkAssets,
        "batch reports missing chunk assets",
        failures);
    expect(
        result.records[1].status == full_engine::TerrainAssetBatchResolveStatus::InvalidChunkAssets,
        "batch reports invalid chunk assets",
        failures);
    expect(
        result.records[2].status == full_engine::TerrainAssetBatchResolveStatus::Resolved,
        "batch continues after failed chunks",
        failures);
    expect(result.summary.missingChunkAssetsCount == 1, "batch counts missing chunk assets", failures);
    expect(result.summary.invalidChunkAssetsCount == 1, "batch counts invalid chunk assets", failures);
    expect(result.summary.resolvedCount == 1, "batch counts later resolved chunk", failures);
    expect(result.resources.resourceCount() == 1, "failed chunks are left out of built resource catalog", failures);
    expect(result.resources.contains(chunk(0)), "later successful chunk is included in built resource catalog", failures);
}

void testMissingHandleMappings(std::vector<std::string>& failures)
{
    full_engine::TerrainAssetCatalog assets;
    full_engine::TerrainChunkAssetDesc meshMissing = makeAssetDesc(chunk(10));
    meshMissing.lods[0].mesh = asset(900);
    full_engine::TerrainChunkAssetDesc materialMissing = makeAssetDesc(chunk(11));
    materialMissing.lods[0].material = asset(901);
    full_engine::TerrainChunkAssetDesc splatMissing = makeAssetDesc(chunk(12));
    splatMissing.splatMap = asset(902);
    (void)assets.addChunkAssets(meshMissing);
    (void)assets.addChunkAssets(materialMissing);
    (void)assets.addChunkAssets(splatMissing);

    const full_engine::RendererAssetHandleCatalog handles = makeHandleCatalog();
    const std::vector<full_engine::ChunkId> ids = {chunk(10), chunk(11), chunk(12)};

    const full_engine::TerrainAssetBatchResolveResult result =
        full_engine::resolveTerrainResourceCatalog(assets, ids.data(), ids.size(), handles);

    expect(
        result.records[0].status == full_engine::TerrainAssetBatchResolveStatus::MissingMeshHandle,
        "batch reports missing mesh handle",
        failures);
    expect(
        result.records[1].status == full_engine::TerrainAssetBatchResolveStatus::MissingMaterialHandle,
        "batch reports missing material handle",
        failures);
    expect(
        result.records[2].status == full_engine::TerrainAssetBatchResolveStatus::MissingSplatMapHandle,
        "batch reports missing splat handle",
        failures);
    expect(result.summary.missingMeshHandleCount == 1, "batch counts missing mesh handle", failures);
    expect(result.summary.missingMaterialHandleCount == 1, "batch counts missing material handle", failures);
    expect(result.summary.missingSplatMapHandleCount == 1, "batch counts missing splat handle", failures);
    expect(result.resources.resourceCount() == 0, "missing handles do not enter resource catalog", failures);
}

void testDuplicateRequestedIdsReportCatalogFailure(std::vector<std::string>& failures)
{
    const full_engine::TerrainAssetCatalog assets = makeAssetCatalog();
    const full_engine::RendererAssetHandleCatalog handles = makeHandleCatalog();
    const std::vector<full_engine::ChunkId> ids = {chunk(0), chunk(0)};

    const full_engine::TerrainAssetBatchResolveResult result =
        full_engine::resolveTerrainResourceCatalog(assets, ids.data(), ids.size(), handles);

    expect(result.records.size() == 2, "duplicate request emits two records", failures);
    expect(
        result.records[0].status == full_engine::TerrainAssetBatchResolveStatus::Resolved,
        "first duplicate request resolves",
        failures);
    expect(
        result.records[1].status == full_engine::TerrainAssetBatchResolveStatus::ResourceCatalogFailed,
        "second duplicate request reports resource catalog failure",
        failures);
    expect(
        result.records[1].resourceResult == full_engine::TerrainResourceResult::AlreadyExists,
        "duplicate request preserves resource catalog failure detail",
        failures);
    expect(result.summary.resolvedCount == 1, "duplicate request counts first resolve", failures);
    expect(result.summary.resourceCatalogFailedCount == 1, "duplicate request counts catalog failure", failures);
    expect(result.resources.resourceCount() == 1, "duplicate request stores only one resource descriptor", failures);
}

void testNullIdsAreDeterministic(std::vector<std::string>& failures)
{
    const full_engine::TerrainAssetCatalog assets = makeAssetCatalog();
    const full_engine::RendererAssetHandleCatalog handles = makeHandleCatalog();

    const full_engine::TerrainAssetBatchResolveResult result =
        full_engine::resolveTerrainResourceCatalog(assets, nullptr, 3, handles);

    expect(result.records.empty(), "null id input emits no records", failures);
    expect(result.summary.missingChunkAssetsCount == 3, "null id input counts missing chunk assets", failures);
    expect(result.resources.resourceCount() == 0, "null id input builds empty resource catalog", failures);
    expect(assets.assetCount() == 3, "null id input does not mutate terrain asset catalog", failures);
}

void testInputsAreNotMutated(std::vector<std::string>& failures)
{
    const full_engine::TerrainAssetCatalog assets = makeAssetCatalog();
    const full_engine::RendererAssetHandleCatalog handles = makeHandleCatalog();
    const std::vector<full_engine::ChunkId> ids = {chunk(0), chunk(1)};

    const full_engine::TerrainAssetBatchResolveResult result =
        full_engine::resolveTerrainResourceCatalog(assets, ids.data(), ids.size(), handles);

    expect(result.summary.resolvedCount == 2, "batch before mutation check resolves", failures);
    expect(assets.assetCount() == 3, "batch does not mutate terrain asset catalog", failures);
    expect(handles.meshHandleCount() == full_engine::kMaxTerrainAssetLodLevels, "batch does not mutate mesh handles", failures);
    expect(handles.materialHandleCount() == full_engine::kMaxTerrainAssetLodLevels, "batch does not mutate material handles", failures);
    expect(handles.textureHandleCount() == 1, "batch does not mutate texture handles", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testValidBatchBuildsResourceCatalog(failures);
    testRequestedOrderIsPreserved(failures);
    testMissingAndInvalidChunkAssets(failures);
    testMissingHandleMappings(failures);
    testDuplicateRequestedIdsReportCatalogFailure(failures);
    testNullIdsAreDeterministic(failures);
    testInputsAreNotMutated(failures);

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
