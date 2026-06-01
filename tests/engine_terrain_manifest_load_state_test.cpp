#include "engine/renderer_integration/TerrainManifestLoadState.hpp"
#include "engine/renderer_integration/TerrainRuntimeController.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
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

full_engine::CookedAssetManifest validManifest()
{
    full_engine::CookedAssetManifest manifest;
    manifest.assets.push_back(assetRecord(1, full_engine::AssetKind::Mesh));
    manifest.assets.push_back(assetRecord(2, full_engine::AssetKind::Material));
    manifest.assets.push_back(assetRecord(3, full_engine::AssetKind::Texture));
    manifest.terrainChunks.push_back(terrainAssets(chunk(1)));
    return manifest;
}

full_engine::CookedAssetManifest invalidManifest()
{
    full_engine::CookedAssetManifest manifest = validManifest();
    manifest.assets[0].id = {};
    return manifest;
}

full_engine::RendererAssetHandleCatalog handles()
{
    full_engine::RendererAssetHandleCatalog catalog;
    (void)catalog.addMeshHandle(asset(1), {10});
    (void)catalog.addMaterialHandle(asset(2), {20});
    (void)catalog.addTextureHandle(asset(3), {30});
    return catalog;
}

full_engine::AssetSourceDescriptor descriptorForKind(const full_engine::AssetKind kind)
{
    full_engine::AssetSourceDescriptor descriptor;
    switch (kind)
    {
    case full_engine::AssetKind::Mesh:
        descriptor.mesh.vertexCount = 4;
        descriptor.mesh.indexCount = 6;
        descriptor.mesh.localBounds.max[0] = 1.0f;
        descriptor.mesh.localBounds.max[1] = 1.0f;
        descriptor.mesh.localBounds.max[2] = 1.0f;
        break;
    case full_engine::AssetKind::Material:
        descriptor.material.model = full_engine::AssetSourceMaterialModel::Basic;
        descriptor.material.alphaMode = full_engine::AssetSourceMaterialAlphaMode::Opaque;
        break;
    case full_engine::AssetKind::Texture:
        descriptor.texture.width = 64;
        descriptor.texture.height = 64;
        descriptor.texture.mipCount = 1;
        descriptor.texture.format = full_engine::AssetSourceTextureFormat::Rgba8;
        descriptor.texture.semantic = full_engine::AssetSourceTextureSemantic::TerrainSplat;
        descriptor.texture.colorSpace = full_engine::AssetSourceTextureColorSpace::Linear;
        break;
    case full_engine::AssetKind::Unknown:
    case full_engine::AssetKind::TerrainChunk:
    case full_engine::AssetKind::Skeleton:
    case full_engine::AssetKind::SkinnedMesh:
    case full_engine::AssetKind::Shader:
        break;
    }
    return descriptor;
}

full_engine::AssetSourceRecord source(
    const std::uint64_t id,
    const full_engine::AssetKind kind,
    const char* const uri)
{
    full_engine::AssetSourceRecord record;
    record.id = asset(id);
    record.kind = kind;
    record.uri = uri;
    record.descriptor = descriptorForKind(kind);
    return record;
}

full_engine::AssetSourceCatalog sourceCatalog()
{
    full_engine::AssetSourceCatalog catalog;
    (void)catalog.addSource(source(1, full_engine::AssetKind::Mesh, "meshes/terrain.mesh"));
    (void)catalog.addSource(source(2, full_engine::AssetKind::Material, "materials/terrain.mat"));
    (void)catalog.addSource(source(3, full_engine::AssetKind::Texture, "textures/terrain_splat.dds"));
    return catalog;
}

full_engine::WorldChunkDesc worldDesc(const full_engine::ChunkId& id) noexcept
{
    full_engine::WorldChunkDesc desc;
    desc.id = id;
    desc.bounds.min = {0.0, 0.0, 0.0};
    desc.bounds.max = {16.0, 4.0, 16.0};
    return desc;
}

void testDefaultState(std::vector<std::string>& failures)
{
    const full_engine::TerrainManifestLoadState state;
    expect(!state.hasManifest(), "default state has no manifest", failures);
    expect(state.manifest().assets.empty(), "default manifest is empty", failures);
    expect(state.latestStage().stagePlan.operations.empty(), "default latest stage is empty", failures);
    expect(state.latestDiagnostics().stage.addCount == 0, "default diagnostics are empty", failures);
    expect(state.latestReadiness().records.empty(), "default readiness is empty", failures);
    expect(state.latestLoadRequests().requests.empty(), "default load requests are empty", failures);
    expect(state.pendingLoadRequestCount() == 0, "default pending load queue is empty", failures);
    expect(state.latestLoadConsumeResult().records.empty(), "default consume diagnostics are empty", failures);
    expect(state.latestLoadExecutorResult().callbackRecords.empty(), "default executor diagnostics are empty", failures);
    expect(!state.hasAssetSources(), "default state has no retained source catalog", failures);
    expect(state.assetSources().sourceCount() == 0, "default retained source catalog is empty", failures);
    expect(state.latestSourceRequests().records.empty(), "default source request diagnostics are empty", failures);
    expect(
        full_engine::terrainManifestLoadStageStatusName(full_engine::TerrainManifestLoadStageStatus::NoManifest) ==
            std::string("NoManifest"),
        "load-stage status names are stable",
        failures);
}

void testNoManifestDoesNotQueue(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    full_engine::TerrainRuntimeState runtime;
    const full_engine::WorldChunkDesc world[] = {worldDesc(chunk(1))};

    const full_engine::TerrainManifestLoadStageResult stage =
        state.stage(handles(), registry, worldCatalog, resources, world, 1);
    const full_engine::TerrainManifestLoadStageResult queued =
        state.queueStage(runtime, handles(), registry, worldCatalog, resources, world, 1);

    expect(stage.status == full_engine::TerrainManifestLoadStageStatus::NoManifest, "stage reports no manifest", failures);
    expect(queued.status == full_engine::TerrainManifestLoadStageStatus::NoManifest, "queue reports no manifest", failures);
    expect(runtime.setupRequestCount() == 0, "no manifest queues no setup", failures);
    expect(runtime.residencyRequestCount() == 0, "no manifest queues no residency", failures);
    expect(state.latestDiagnostics().manifestAssetCount == 0, "no manifest clears latest diagnostics", failures);
    expect(state.planAssetReadiness(handles()).records.empty(), "no manifest plans empty readiness", failures);
    expect(state.planAssetLoadRequests().requests.empty(), "no manifest plans empty load requests", failures);
}

void testSetManifestStagesWithoutQueueing(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    full_engine::CookedAssetManifest manifest = validManifest();
    state.setManifest(manifest);
    manifest.assets.clear();

    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    const full_engine::WorldChunkDesc world[] = {worldDesc(chunk(1))};

    const full_engine::TerrainManifestLoadStageResult result =
        state.stage(handles(), registry, worldCatalog, resources, world, 1);

    expect(state.hasManifest(), "setManifest stores manifest", failures);
    expect(state.manifest().assets.size() == 3, "setManifest copies manifest value", failures);
    expect(result.status == full_engine::TerrainManifestLoadStageStatus::Success, "stage reports state success", failures);
    expect(result.stage.status == full_engine::TerrainManifestRuntimeStageStatus::Success, "stage result succeeds", failures);
    expect(state.latestStage().stagePlan.summary.addCount == 1, "stage stores latest plan", failures);
    expect(state.latestDiagnostics().stage.addCount == 1, "stage stores latest diagnostics", failures);
}

void testReadinessIsStoredAndReset(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    state.setManifest(validManifest());

    const full_engine::TerrainManifestAssetReadinessPlan& readiness = state.planAssetReadiness(handles());
    expect(readiness.summary.requestedCount == 3, "readiness counts retained manifest handle needs", failures);
    expect(readiness.summary.readyCount == 3, "readiness counts available handles", failures);
    expect(state.latestReadiness().summary.readyCount == 3, "readiness is retained", failures);
    expect(state.planAssetLoadRequests().summary.requestCount == 0, "ready manifest produces no load intents", failures);

    state.setManifest(validManifest());
    expect(state.latestReadiness().records.empty(), "setManifest clears stale readiness", failures);
    expect(state.latestLoadRequests().requests.empty(), "setManifest clears stale load requests", failures);

    (void)state.planAssetReadiness(handles());
    (void)state.planAssetLoadRequests();
    state.clearManifest();
    expect(state.latestReadiness().records.empty(), "clearManifest clears readiness", failures);
    expect(state.latestLoadRequests().requests.empty(), "clearManifest clears load requests", failures);
}

void testMissingReadinessBuildsLoadRequests(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    state.setManifest(validManifest());

    const full_engine::TerrainManifestAssetReadinessPlan& readiness = state.planAssetReadiness({});
    const full_engine::TerrainManifestAssetLoadRequestPlan& loadRequests = state.planAssetLoadRequests();

    expect(readiness.summary.missingHandleCount == 3, "missing handles counted before load planning", failures);
    expect(loadRequests.summary.requestCount == 3, "missing handles become load requests", failures);
    expect(loadRequests.summary.meshRequestCount == 1, "missing mesh becomes load request", failures);
    expect(loadRequests.summary.materialRequestCount == 1, "missing material becomes load request", failures);
    expect(loadRequests.summary.textureRequestCount == 1, "missing texture becomes load request", failures);
    expect(state.latestLoadRequests().requests.size() == 3, "load requests are retained", failures);
}

void testLoadRequestsCanBeQueuedAndReset(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    state.setManifest(validManifest());

    (void)state.planAssetReadiness({});
    (void)state.planAssetLoadRequests();

    const full_engine::TerrainManifestAssetLoadQueuePushResult& firstQueue =
        state.queueLatestAssetLoadRequests();
    expect(firstQueue.summary.queuedCount == 3, "first load request queue records new requests", failures);
    expect(firstQueue.summary.alreadyQueuedCount == 0, "first load request queue has no duplicates", failures);
    expect(state.pendingLoadRequestCount() == 3, "load state retains pending load queue", failures);
    expect(state.loadRequestQueue().summary().requestCount == 3, "load queue exposes summary", failures);

    const full_engine::TerrainManifestAssetLoadQueuePushResult& secondQueue =
        state.queueLatestAssetLoadRequests();
    expect(secondQueue.summary.queuedCount == 0, "second load request queue adds no duplicates", failures);
    expect(secondQueue.summary.alreadyQueuedCount == 3, "second load request queue reports duplicates", failures);
    expect(state.pendingLoadRequestCount() == 3, "duplicate queue preserves pending count", failures);

    state.setManifest(validManifest());
    expect(state.pendingLoadRequestCount() == 0, "setManifest clears pending load queue", failures);
    expect(state.latestLoadRequestQueueResult().records.empty(), "setManifest clears latest queue diagnostics", failures);
    expect(state.latestLoadConsumeResult().records.empty(), "setManifest clears latest consume diagnostics", failures);
    expect(state.latestLoadExecutorResult().callbackRecords.empty(), "setManifest clears latest executor diagnostics", failures);

    (void)state.planAssetReadiness({});
    (void)state.planAssetLoadRequests();
    (void)state.queueLatestAssetLoadRequests();
    full_engine::RendererAssetHandleCatalog source = handles();
    full_engine::RendererAssetHandleCatalog destination;
    (void)state.consumePendingAssetLoadRequests(source, destination);
    state.clearManifest();
    expect(state.pendingLoadRequestCount() == 0, "clearManifest clears pending load queue", failures);
    expect(state.latestLoadRequestQueueResult().records.empty(), "clearManifest clears queue diagnostics", failures);
    expect(state.latestLoadConsumeResult().records.empty(), "clearManifest clears consume diagnostics", failures);
    expect(state.latestLoadExecutorResult().callbackRecords.empty(), "clearManifest clears executor diagnostics", failures);
}

void testAssetSourcesMapLatestLoadRequests(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    state.setManifest(validManifest());
    state.setAssetSources(sourceCatalog());

    (void)state.planAssetReadiness({});
    (void)state.planAssetLoadRequests();
    const full_engine::TerrainManifestAssetSourceRequestPlan& sources =
        state.planAssetSources();

    expect(state.hasAssetSources(), "setAssetSources retains source catalog", failures);
    expect(state.assetSources().sourceCount() == 3, "setAssetSources stores source records", failures);
    expect(sources.summary.mappedCount == 3, "source planning maps load requests", failures);
    expect(sources.summary.missingSourceCount == 0, "source planning has no missing sources", failures);
    expect(sources.records.size() == 3, "source planning returns records", failures);
    expect(sources.records[0].source.uri == "meshes/terrain.mesh", "source planning copies mesh source", failures);
    expect(sources.records[1].source.uri == "materials/terrain.mat", "source planning copies material source", failures);
    expect(sources.records[2].source.uri == "textures/terrain_splat.dds", "source planning copies texture source", failures);
    expect(state.pendingLoadRequestCount() == 0, "source planning does not queue load requests", failures);

    (void)state.queueLatestAssetLoadRequests();
    expect(state.pendingLoadRequestCount() == 3, "source planning preserves load requests after queueing", failures);
}

void testAssetSourceMissingAndReset(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    state.setManifest(validManifest());

    full_engine::AssetSourceCatalog partial;
    (void)partial.addSource(source(1, full_engine::AssetKind::Mesh, "meshes/terrain.mesh"));
    (void)partial.addSource(source(2, full_engine::AssetKind::Texture, "textures/wrong_kind.dds"));
    state.setAssetSources(std::move(partial));

    (void)state.planAssetReadiness({});
    (void)state.planAssetLoadRequests();
    const full_engine::TerrainManifestAssetSourceRequestPlan& planned =
        state.planAssetSources();

    expect(planned.summary.mappedCount == 1, "source planning maps available source", failures);
    expect(planned.summary.missingSourceCount == 2, "source planning reports missing and wrong-kind sources", failures);
    expect(planned.summary.invalidRequestCount == 0, "source planning has no invalid requests", failures);

    state.clearAssetSources();
    expect(!state.hasAssetSources(), "clearAssetSources clears retained source flag", failures);
    expect(state.assetSources().sourceCount() == 0, "clearAssetSources clears source records", failures);
    expect(state.latestSourceRequests().records.empty(), "clearAssetSources clears source diagnostics", failures);
    expect(state.latestLoadRequests().requests.size() == 3, "clearAssetSources preserves load request plan", failures);

    state.setAssetSources(sourceCatalog());
    (void)state.planAssetSources();
    state.setManifest(validManifest());
    expect(!state.hasAssetSources(), "setManifest clears retained source catalog", failures);
    expect(state.latestSourceRequests().records.empty(), "setManifest clears source diagnostics", failures);

    state.setAssetSources(sourceCatalog());
    (void)state.planAssetReadiness({});
    (void)state.planAssetLoadRequests();
    (void)state.planAssetSources();
    state.clearManifest();
    expect(!state.hasAssetSources(), "clearManifest clears retained source catalog", failures);
    expect(state.latestSourceRequests().records.empty(), "clearManifest clears source diagnostics", failures);
}

void testLoadRequestsCanBeConsumed(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    state.setManifest(validManifest());

    (void)state.planAssetReadiness({});
    (void)state.planAssetLoadRequests();
    (void)state.queueLatestAssetLoadRequests();

    full_engine::RendererAssetHandleCatalog destination;
    const full_engine::TerrainManifestAssetLoadResult& consumed =
        state.consumePendingAssetLoadRequests(handles(), destination);

    expect(consumed.consumed, "state consume clears satisfied pending requests", failures);
    expect(consumed.summary.loadedCount == 3, "state consume counts loaded handles", failures);
    expect(consumed.records.size() == 3, "state consume stores ordered records", failures);
    expect(state.pendingLoadRequestCount() == 0, "state consume clears owned pending queue", failures);
    expect(destination.findMeshHandle(asset(1)) != nullptr, "state consume copies mesh handle", failures);
    expect(destination.findMaterialHandle(asset(2)) != nullptr, "state consume copies material handle", failures);
    expect(destination.findTextureHandle(asset(3)) != nullptr, "state consume copies texture handle", failures);
    expect(state.latestLoadConsumeResult().summary.loadedCount == 3, "state retains consume diagnostics", failures);
}

void testLoadConsumeMissingHandlesCanRetry(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    state.setManifest(validManifest());

    (void)state.planAssetReadiness({});
    (void)state.planAssetLoadRequests();
    (void)state.queueLatestAssetLoadRequests();

    full_engine::RendererAssetHandleCatalog missingMaterial = handles();
    (void)missingMaterial.removeMaterialHandle(asset(2));
    full_engine::RendererAssetHandleCatalog destination;

    const full_engine::TerrainManifestAssetLoadResult& failed =
        state.consumePendingAssetLoadRequests(missingMaterial, destination);
    expect(!failed.consumed, "missing handle consume leaves queue pending", failures);
    expect(failed.summary.missingHandleCount == 1, "missing handle consume records miss", failures);
    expect(state.pendingLoadRequestCount() == 3, "missing handle consume keeps pending requests", failures);
    expect(destination.meshHandleCount() == 0, "missing handle consume does not partially mutate destination", failures);

    const full_engine::TerrainManifestAssetLoadResult& retried =
        state.consumePendingAssetLoadRequests(handles(), destination);
    expect(retried.consumed, "state consume can retry after handles appear", failures);
    expect(state.pendingLoadRequestCount() == 0, "retry consume clears pending queue", failures);
    expect(destination.materialHandleCount() == 1, "retry consume copies material handle", failures);
}

void testLoadConsumeAlreadyLoadedPreservesDestination(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    state.setManifest(validManifest());

    (void)state.planAssetReadiness({});
    (void)state.planAssetLoadRequests();
    (void)state.queueLatestAssetLoadRequests();

    full_engine::RendererAssetHandleCatalog destination;
    (void)destination.addMeshHandle(asset(1), {99});

    const full_engine::TerrainManifestAssetLoadResult& consumed =
        state.consumePendingAssetLoadRequests(handles(), destination);

    expect(consumed.consumed, "already loaded destination can consume queue", failures);
    expect(consumed.summary.alreadyLoadedCount == 1, "already loaded destination is counted", failures);
    expect(consumed.summary.loadedCount == 2, "remaining destination handles are loaded", failures);
    const full_renderer::MeshHandle* mesh = destination.findMeshHandle(asset(1));
    expect(mesh != nullptr && mesh->id == 99, "already loaded destination handle is preserved", failures);
    expect(state.pendingLoadRequestCount() == 0, "already loaded consume clears queue", failures);
}

void testQueueStageQueuesRuntimeIntent(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    state.setManifest(validManifest());

    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    full_engine::TerrainRuntimeState runtime;
    const full_engine::WorldChunkDesc world[] = {worldDesc(chunk(1))};

    const full_engine::TerrainManifestLoadStageResult result =
        state.queueStage(runtime, handles(), registry, worldCatalog, resources, world, 1);

    expect(result.status == full_engine::TerrainManifestLoadStageStatus::Success, "queue stage reports state success", failures);
    expect(result.stage.queue.summary.queuedSetupCount == 1, "queue stage queues setup", failures);
    expect(result.stage.queue.summary.queuedMakeResidentCount == 1, "queue stage queues make resident", failures);
    expect(runtime.setupRequestCount() == 1, "runtime receives setup request", failures);
    expect(runtime.residencyRequestCount() == 1, "runtime receives residency request", failures);
    expect(state.latestDiagnostics().queue.queuedSetupCount == 1, "queue stage stores diagnostics", failures);
}

void testInvalidManifestStoresFailure(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    state.setManifest(invalidManifest());

    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    const full_engine::WorldChunkDesc world[] = {worldDesc(chunk(1))};

    const full_engine::TerrainManifestLoadStageResult result =
        state.stage(handles(), registry, worldCatalog, resources, world, 1);

    expect(result.status == full_engine::TerrainManifestLoadStageStatus::Success, "invalid manifest still stages loaded value", failures);
    expect(
        result.stage.status == full_engine::TerrainManifestRuntimeStageStatus::InvalidManifest,
        "invalid manifest stores staging failure",
        failures);
    expect(
        state.latestDiagnostics().status == full_engine::TerrainManifestRuntimeStageStatus::InvalidManifest,
        "invalid manifest stores failure diagnostics",
        failures);
}

void testClearAndReplaceResetState(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    state.setManifest(validManifest());

    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    full_engine::TerrainRuntimeState runtime;
    const full_engine::WorldChunkDesc world[] = {worldDesc(chunk(1))};
    (void)state.queueStage(runtime, handles(), registry, worldCatalog, resources, world, 1);

    state.setManifest(validManifest());
    expect(state.latestDiagnostics().queue.queuedSetupCount == 0, "replace clears stale diagnostics", failures);
    expect(state.latestStage().stagePlan.operations.empty(), "replace clears stale stage", failures);
    expect(runtime.setupRequestCount() == 1, "replace does not touch external queues", failures);

    state.clearManifest();
    expect(!state.hasManifest(), "clear removes manifest", failures);
    expect(state.latestDiagnostics().queue.queuedSetupCount == 0, "clear resets diagnostics", failures);
    expect(runtime.setupRequestCount() == 1, "clear does not touch external queues", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testDefaultState(failures);
    testNoManifestDoesNotQueue(failures);
    testSetManifestStagesWithoutQueueing(failures);
    testReadinessIsStoredAndReset(failures);
    testMissingReadinessBuildsLoadRequests(failures);
    testLoadRequestsCanBeQueuedAndReset(failures);
    testAssetSourcesMapLatestLoadRequests(failures);
    testAssetSourceMissingAndReset(failures);
    testLoadRequestsCanBeConsumed(failures);
    testLoadConsumeMissingHandlesCanRetry(failures);
    testLoadConsumeAlreadyLoadedPreservesDestination(failures);
    testQueueStageQueuesRuntimeIntent(failures);
    testInvalidManifestStoresFailure(failures);
    testClearAndReplaceResetState(failures);

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
