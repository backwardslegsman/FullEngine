#include "engine/renderer_integration/TerrainManifestAssetLoader.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadRequests.hpp"
#include "engine/renderer_integration/TerrainManifestLoadState.hpp"

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
    return {value};
}

full_engine::AssetRecord assetRecord(const std::uint64_t id, const full_engine::AssetKind kind) noexcept
{
    full_engine::AssetRecord record;
    record.id = asset(id);
    record.kind = kind;
    return record;
}

full_engine::ChunkId chunk(const std::int32_t x) noexcept
{
    return {x, 0, 0};
}

full_engine::TerrainChunkAssetDesc terrainAssets(const full_engine::ChunkId& id)
{
    full_engine::TerrainChunkAssetDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = asset(1);
    desc.lods[0].material = asset(2);
    desc.lods[0].maxDistanceMeters = 64.0f;
    desc.splatMap = asset(3);
    return desc;
}

full_engine::CookedAssetManifest manifest()
{
    full_engine::CookedAssetManifest result;
    result.assets.push_back(assetRecord(1, full_engine::AssetKind::Mesh));
    result.assets.push_back(assetRecord(2, full_engine::AssetKind::Material));
    result.assets.push_back(assetRecord(3, full_engine::AssetKind::Texture));
    result.terrainChunks.push_back(terrainAssets(chunk(1)));
    return result;
}

full_engine::TerrainManifestAssetLoadRequest request(
    const std::uint64_t id,
    const full_engine::AssetKind kind) noexcept
{
    return {asset(id), kind};
}

full_engine::RendererAssetHandleCatalog completeSource()
{
    full_engine::RendererAssetHandleCatalog source;
    (void)source.addMeshHandle(asset(1), full_renderer::MeshHandle{10});
    (void)source.addMaterialHandle(asset(2), full_renderer::MaterialHandle{20});
    (void)source.addTextureHandle(asset(3), full_renderer::TextureHandle{30});
    return source;
}

full_engine::TerrainManifestAssetLoadRequestQueue queueThreeRequests()
{
    full_engine::TerrainManifestAssetLoadRequestQueue queue;
    (void)queue.push(request(1, full_engine::AssetKind::Mesh));
    (void)queue.push(request(2, full_engine::AssetKind::Material));
    (void)queue.push(request(3, full_engine::AssetKind::Texture));
    return queue;
}

void testLoadsPendingRequestsAndClearsQueue(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadRequestQueue queue = queueThreeRequests();
    full_engine::RendererAssetHandleCatalog handles;

    const full_engine::TerrainManifestAssetLoadResult loaded =
        full_engine::consumeTerrainManifestAssetLoadRequests(queue, completeSource(), handles);

    expect(loaded.consumed, "successful fake load consumes pending queue", failures);
    expect(queue.requestCount() == 0, "successful fake load clears queue", failures);
    expect(loaded.records.size() == 3, "fake load records each pending request", failures);
    expect(loaded.summary.loadedCount == 3, "fake load counts loaded requests", failures);
    expect(loaded.records[0].catalogResult == full_engine::RendererAssetHandleCatalogResult::Success,
        "loaded mesh records catalog success",
        failures);
    expect(loaded.records[0].request.kind == full_engine::AssetKind::Mesh, "fake load preserves mesh order", failures);
    expect(loaded.records[1].request.kind == full_engine::AssetKind::Material, "fake load preserves material order", failures);
    expect(loaded.records[2].request.kind == full_engine::AssetKind::Texture, "fake load preserves texture order", failures);
    expect(handles.findMeshHandle(asset(1)) != nullptr, "mesh handle loaded into catalog", failures);
    expect(handles.findMaterialHandle(asset(2)) != nullptr, "material handle loaded into catalog", failures);
    expect(handles.findTextureHandle(asset(3)) != nullptr, "texture handle loaded into catalog", failures);
}

void testMissingFakeHandleLeavesQueuePending(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadRequestQueue queue = queueThreeRequests();
    full_engine::RendererAssetHandleCatalog handles;
    full_engine::RendererAssetHandleCatalog source = completeSource();
    (void)source.removeMaterialHandle(asset(2));

    const full_engine::TerrainManifestAssetLoadResult loaded =
        full_engine::consumeTerrainManifestAssetLoadRequests(queue, source, handles);

    expect(!loaded.consumed, "missing fake handle does not consume queue", failures);
    expect(queue.requestCount() == 3, "missing fake handle leaves queue intact", failures);
    expect(loaded.summary.missingHandleCount == 1, "missing fake handle is counted", failures);
    expect(loaded.records[1].status == full_engine::TerrainManifestAssetLoadStatus::MissingHandle,
        "missing material is diagnosed in order",
        failures);
    expect(handles.meshHandleCount() == 0, "failed fake load does not partially add mesh handles", failures);
    expect(handles.materialHandleCount() == 0, "failed fake load does not add material handles", failures);
    expect(handles.textureHandleCount() == 0, "failed fake load does not add texture handles", failures);
}

void testInvalidSuppliedHandleIsRejectedBeforeConsumption(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadRequestQueue queue = queueThreeRequests();
    full_engine::RendererAssetHandleCatalog source = completeSource();
    full_engine::RendererAssetHandleCatalog handles;

    const full_engine::RendererAssetHandleCatalogResult invalidSourceAdd =
        source.addTextureHandle(asset(4), {});
    const full_engine::TerrainManifestAssetLoadResult loaded =
        full_engine::consumeTerrainManifestAssetLoadRequests(queue, source, handles);

    expect(
        invalidSourceAdd == full_engine::RendererAssetHandleCatalogResult::InvalidArgument,
        "invalid supplied handles are rejected before load consumption",
        failures);
    expect(loaded.consumed, "valid source handles still consume queue", failures);
    expect(loaded.summary.catalogRejectedCount == 0, "valid source handles have no catalog rejections", failures);
    expect(queue.requestCount() == 0, "valid source handles clear queue", failures);
    expect(handles.textureHandleCount() == 1, "valid texture handle is still loaded", failures);
}

void testPreExistingHandlesAreAlreadyLoaded(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadRequestQueue queue = queueThreeRequests();
    full_engine::RendererAssetHandleCatalog handles;
    (void)handles.addMeshHandle(asset(1), {99});

    const full_engine::TerrainManifestAssetLoadResult loaded =
        full_engine::consumeTerrainManifestAssetLoadRequests(queue, completeSource(), handles);

    expect(loaded.consumed, "already-loaded handles allow queue consumption", failures);
    expect(queue.requestCount() == 0, "already-loaded batch clears queue", failures);
    expect(loaded.summary.alreadyLoadedCount == 1, "already-loaded handle is counted", failures);
    expect(loaded.summary.loadedCount == 2, "remaining handles are loaded", failures);
    const full_renderer::MeshHandle* mesh = handles.findMeshHandle(asset(1));
    expect(mesh != nullptr && mesh->id == 99, "already-loaded handle is preserved", failures);
}

void testManifestReadinessLoadLoop(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    state.setManifest(manifest());
    full_engine::RendererAssetHandleCatalog handles;

    const full_engine::TerrainManifestAssetReadinessPlan& missingReadiness =
        state.planAssetReadiness(handles);
    const full_engine::TerrainManifestAssetLoadRequestPlan& plan =
        state.planAssetLoadRequests();

    expect(missingReadiness.summary.missingHandleCount == 3, "empty handles produce readiness misses", failures);
    expect(plan.summary.requestCount == 3, "readiness misses produce load requests", failures);

    full_engine::TerrainManifestAssetLoadRequestQueue queue;
    const full_engine::TerrainManifestAssetLoadQueuePushResult pushed = queue.pushPlan(plan);
    expect(pushed.summary.queuedCount == 3, "load plan queues all missing handles", failures);

    const full_engine::TerrainManifestAssetLoadResult loaded =
        full_engine::consumeTerrainManifestAssetLoadRequests(queue, completeSource(), handles);
    expect(loaded.consumed, "fake loader consumes manifest-derived pending requests", failures);

    const full_engine::TerrainManifestAssetReadinessPlan& readyReadiness =
        state.planAssetReadiness(handles);
    const full_engine::TerrainManifestAssetLoadRequestPlan& emptyPlan =
        state.planAssetLoadRequests();

    expect(readyReadiness.summary.readyCount == 3, "loaded fake handles satisfy readiness", failures);
    expect(readyReadiness.summary.missingHandleCount == 0, "loaded fake handles clear readiness misses", failures);
    expect(emptyPlan.requests.empty(), "ready handles produce no further load requests", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testLoadsPendingRequestsAndClearsQueue(failures);
    testMissingFakeHandleLeavesQueuePending(failures);
    testInvalidSuppliedHandleIsRejectedBeforeConsumption(failures);
    testPreExistingHandlesAreAlreadyLoaded(failures);
    testManifestReadinessLoadLoop(failures);

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
