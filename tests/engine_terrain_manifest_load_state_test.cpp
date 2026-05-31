#include "engine/renderer_integration/TerrainManifestLoadState.hpp"
#include "engine/renderer_integration/TerrainRuntimeController.hpp"

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
