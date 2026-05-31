#include "engine/streaming/TerrainStreamingManifestCoordinator.hpp"

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

full_engine::CookedAssetManifest manifestFor(const std::vector<full_engine::ChunkId>& ids)
{
    full_engine::CookedAssetManifest manifest;
    manifest.assets.push_back(assetRecord(1, full_engine::AssetKind::Mesh));
    manifest.assets.push_back(assetRecord(2, full_engine::AssetKind::Material));
    manifest.assets.push_back(assetRecord(3, full_engine::AssetKind::Texture));
    for (const full_engine::ChunkId& id : ids)
    {
        manifest.terrainChunks.push_back(terrainAssets(id));
    }
    return manifest;
}

full_engine::RendererAssetHandleCatalog readyHandles()
{
    full_engine::RendererAssetHandleCatalog handles;
    (void)handles.addMeshHandle(asset(1), {10});
    (void)handles.addMaterialHandle(asset(2), {20});
    (void)handles.addTextureHandle(asset(3), {30});
    return handles;
}

full_engine::WorldChunkDesc worldDesc(const full_engine::ChunkId& id, const double offset = 0.0) noexcept
{
    full_engine::WorldChunkDesc desc;
    desc.id = id;
    desc.bounds.min = {offset, 0.0, 0.0};
    desc.bounds.max = {offset + 16.0, 4.0, 16.0};
    return desc;
}

full_engine::TerrainChunkResourceDesc resourceDesc(const full_engine::ChunkId& id)
{
    full_engine::TerrainChunkResourceDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = {10};
    desc.lods[0].material = {20};
    desc.lods[0].maxDistanceMeters = 64.0f;
    desc.splatMap = {30};
    return desc;
}

full_engine::TerrainRuntimeChunkState snapshotState(
    const full_engine::ChunkId& id,
    const bool setup,
    const full_engine::ChunkResidencyState residency = full_engine::ChunkResidencyState::Unloaded)
{
    full_engine::TerrainRuntimeChunkState state;
    state.id = id;
    state.hasRegistry = setup;
    state.hasWorldDesc = setup;
    state.hasResources = setup;
    state.residency = residency;
    state.readiness = setup
        ? full_engine::TerrainRuntimeChunkReadiness::NotResident
        : full_engine::TerrainRuntimeChunkReadiness::MissingRegistry;
    if (setup && residency == full_engine::ChunkResidencyState::Resident)
    {
        state.readiness = full_engine::TerrainRuntimeChunkReadiness::MissingTerrainHandle;
    }
    return state;
}

full_engine::TerrainRuntimeStateSnapshot snapshot(
    const std::vector<full_engine::TerrainRuntimeChunkState>& chunks)
{
    full_engine::TerrainRuntimeStateSnapshot result;
    result.chunks = chunks;
    return result;
}

full_engine::TerrainStreamingPlannerConfig streamingConfig(
    const double chunkSize = 16.0,
    const int loadRadius = 0,
    const int residentRadius = 0)
{
    full_engine::TerrainStreamingPlannerConfig config;
    config.chunkSizeMeters = chunkSize;
    config.loadRadiusChunks = loadRadius;
    config.residentRadiusChunks = residentRadius;
    return config;
}

void addCurrentSetup(
    full_engine::WorldChunkRegistry& registry,
    full_engine::WorldChunkCatalog& worldCatalog,
    full_engine::TerrainResourceCatalog& resources,
    const full_engine::ChunkId& id,
    const bool resident,
    const double worldOffset = 0.0)
{
    (void)registry.createChunk(id);
    (void)worldCatalog.addChunk(worldDesc(id, worldOffset));
    (void)resources.addChunkResources(resourceDesc(id));
    if (resident)
    {
        (void)registry.setResidencyState(id, full_engine::ChunkResidencyState::Loading);
        (void)registry.setResidencyState(id, full_engine::ChunkResidencyState::Resident);
    }
}

struct Fixture
{
    full_engine::TerrainManifestLoadState manifestLoad;
    full_engine::TerrainStreamingRuntimeState streaming;
    full_engine::TerrainRuntimeState runtime;
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
};

full_engine::TerrainStreamingManifestUpdateResult update(
    Fixture& fixture,
    const full_engine::RendererAssetHandleCatalog& handles,
    const std::vector<full_engine::WorldChunkDesc>& worlds,
    const full_engine::TerrainRuntimeStateSnapshot& current,
    const full_engine::TerrainStreamingPlannerConfig& config = streamingConfig())
{
    return full_engine::updateTerrainStreamingFromManifest(
        fixture.manifestLoad,
        handles,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        worlds.empty() ? nullptr : worlds.data(),
        worlds.size(),
        fixture.streaming,
        fixture.runtime,
        config,
        {0.0, 0.0, 0.0},
        current);
}

void testNoManifest(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::TerrainStreamingManifestUpdateResult result =
        update(fixture, readyHandles(), {}, snapshot({}));

    expect(result.status == full_engine::TerrainStreamingManifestUpdateStatus::NoManifest, "no manifest reports status", failures);
    expect(result.manifestStage.status == full_engine::TerrainManifestLoadStageStatus::NoManifest, "no manifest stage status is copied", failures);
    expect(fixture.runtime.setupRequestCount() == 0, "no manifest queues no setup", failures);
    expect(fixture.runtime.residencyRequestCount() == 0, "no manifest queues no residency", failures);
}

void testMissingHandlesQueueLoadIntent(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId id = chunk(0);
    fixture.manifestLoad.setManifest(manifestFor({id}));
    const std::vector<full_engine::WorldChunkDesc> worlds = {worldDesc(id)};

    const full_engine::TerrainStreamingManifestUpdateResult result =
        update(fixture, {}, worlds, snapshot({}));

    expect(result.status == full_engine::TerrainStreamingManifestUpdateStatus::AssetLoadsPending, "missing handles report pending loads", failures);
    expect(result.readiness.summary.missingHandleCount == 3, "missing handles are counted", failures);
    expect(result.loadRequests.summary.requestCount == 3, "missing handles become load requests", failures);
    expect(result.loadQueue.summary.queuedCount == 3, "missing load requests are queued", failures);
    expect(fixture.manifestLoad.pendingLoadRequestCount() == 3, "manifest state retains pending load requests", failures);
    expect(fixture.runtime.setupRequestCount() == 0, "pending loads queue no runtime setup", failures);
}

void testReadyManifestQueuesAddAndResident(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId id = chunk(0);
    fixture.manifestLoad.setManifest(manifestFor({id}));
    const std::vector<full_engine::WorldChunkDesc> worlds = {worldDesc(id)};

    const full_engine::TerrainStreamingManifestUpdateResult result =
        update(fixture, readyHandles(), worlds, snapshot({}));

    expect(result.status == full_engine::TerrainStreamingManifestUpdateStatus::Success, "ready manifest queues successfully", failures);
    expect(result.manifestStage.stage.stagePlan.summary.addCount == 1, "manifest staging plans add", failures);
    expect(result.streamingPlan.summary.addSetupCount == 1, "streaming plan adds setup", failures);
    expect(result.streamingQueue.summary.queuedSetupAddCount == 1, "streaming queues setup add", failures);
    expect(result.streamingQueue.summary.queuedMakeResidentCount == 1, "streaming queues make resident", failures);
    expect(fixture.runtime.setupRequestCount() == 1, "runtime owns queued setup add", failures);
    expect(fixture.runtime.residencyRequestCount() == 1, "runtime owns queued make resident", failures);
}

void testExistingSetupProducesKeepDiagnostics(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId id = chunk(0);
    fixture.manifestLoad.setManifest(manifestFor({id}));
    addCurrentSetup(fixture.registry, fixture.worldCatalog, fixture.resources, id, true);
    const std::vector<full_engine::WorldChunkDesc> worlds = {worldDesc(id)};

    const full_engine::TerrainStreamingManifestUpdateResult result =
        update(fixture, readyHandles(), worlds, snapshot({snapshotState(id, true, full_engine::ChunkResidencyState::Resident)}));

    expect(result.status == full_engine::TerrainStreamingManifestUpdateStatus::Success, "existing setup update succeeds", failures);
    expect(result.manifestStage.stage.stagePlan.summary.keepCount == 1, "manifest staging keeps existing setup", failures);
    expect(result.streamingPlan.summary.keepSetupCount == 1, "streaming keeps setup", failures);
    expect(result.streamingPlan.summary.keepResidentCount == 1, "streaming keeps resident", failures);
    expect(result.streamingQueue.summary.skippedKeepSetupCount == 1, "queue diagnostics skip keep setup", failures);
    expect(result.streamingQueue.summary.skippedKeepResidentCount == 1, "queue diagnostics skip keep resident", failures);
    expect(fixture.runtime.setupRequestCount() == 0, "keep plan queues no setup", failures);
    expect(fixture.runtime.residencyRequestCount() == 0, "keep plan queues no residency", failures);
}

void testExistingResidentOutsideWindowQueuesUnloadAndRemove(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId far = chunk(5);
    fixture.manifestLoad.setManifest(manifestFor({far}));
    addCurrentSetup(fixture.registry, fixture.worldCatalog, fixture.resources, far, true);
    const std::vector<full_engine::WorldChunkDesc> worlds = {worldDesc(far)};

    const full_engine::TerrainStreamingManifestUpdateResult result =
        update(fixture, readyHandles(), worlds, snapshot({snapshotState(far, true, full_engine::ChunkResidencyState::Resident)}));

    expect(result.status == full_engine::TerrainStreamingManifestUpdateStatus::Success, "far resident update succeeds", failures);
    expect(result.streamingPlan.summary.makeUnloadedCount == 1, "far resident plans unload", failures);
    expect(result.streamingPlan.summary.removeSetupCount == 1, "far setup plans remove", failures);
    expect(result.streamingQueue.summary.queuedMakeUnloadedCount == 1, "far resident queues unload", failures);
    expect(result.streamingQueue.summary.queuedSetupRemoveCount == 1, "far setup queues remove", failures);
    expect(fixture.runtime.setupRequestCount() == 1, "runtime owns setup remove", failures);
    expect(fixture.runtime.residencyRequestCount() == 1, "runtime owns make unloaded", failures);
}

void testMissingWorldDescReportsStageFailure(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId id = chunk(0);
    fixture.manifestLoad.setManifest(manifestFor({id}));

    const full_engine::TerrainStreamingManifestUpdateResult result =
        update(fixture, readyHandles(), {}, snapshot({}));

    expect(result.status == full_engine::TerrainStreamingManifestUpdateStatus::ManifestStageFailed, "missing world desc reports stage failure", failures);
    expect(result.manifestStage.stage.status == full_engine::TerrainManifestRuntimeStageStatus::MissingWorldDesc, "missing world desc stage status is preserved", failures);
    expect(fixture.runtime.setupRequestCount() == 0, "stage failure queues no setup", failures);
}

void testUnsupportedStageChangesBlockQueueing(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId id = chunk(0);
    fixture.manifestLoad.setManifest(manifestFor({id}));
    addCurrentSetup(fixture.registry, fixture.worldCatalog, fixture.resources, id, false, 25.0);
    const std::vector<full_engine::WorldChunkDesc> worlds = {worldDesc(id, 0.0)};

    const full_engine::TerrainStreamingManifestUpdateResult result =
        update(fixture, readyHandles(), worlds, snapshot({snapshotState(id, true)}));

    expect(result.status == full_engine::TerrainStreamingManifestUpdateStatus::UnsupportedStageChanges, "changed descriptors block queueing", failures);
    expect(result.manifestStage.stage.stagePlan.summary.changedUnsupportedCount == 1, "unsupported change is counted", failures);
    expect(fixture.runtime.setupRequestCount() == 0, "unsupported change queues no setup", failures);
    expect(fixture.runtime.residencyRequestCount() == 0, "unsupported change queues no residency", failures);
}

void testInvalidStreamingConfigBlocksQueueing(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId id = chunk(0);
    fixture.manifestLoad.setManifest(manifestFor({id}));
    const std::vector<full_engine::WorldChunkDesc> worlds = {worldDesc(id)};

    const full_engine::TerrainStreamingManifestUpdateResult result =
        update(fixture, readyHandles(), worlds, snapshot({}), streamingConfig(0.0, 0, 0));

    expect(result.status == full_engine::TerrainStreamingManifestUpdateStatus::StreamingQueueBlocked, "invalid streaming plan blocks queueing", failures);
    expect(result.streamingQueue.status == full_engine::TerrainStreamingQueueStatus::BlockedInvalidPlan, "blocked streaming queue status is preserved", failures);
    expect(fixture.runtime.setupRequestCount() == 0, "invalid streaming config queues no setup", failures);
    expect(fixture.runtime.residencyRequestCount() == 0, "invalid streaming config queues no residency", failures);
}

void testStatusNames(std::vector<std::string>& failures)
{
    expect(std::string(full_engine::terrainStreamingManifestUpdateStatusName(full_engine::TerrainStreamingManifestUpdateStatus::Success)) == "Success", "success name is stable", failures);
    expect(std::string(full_engine::terrainStreamingManifestUpdateStatusName(full_engine::TerrainStreamingManifestUpdateStatus::NoManifest)) == "NoManifest", "no manifest name is stable", failures);
    expect(std::string(full_engine::terrainStreamingManifestUpdateStatusName(full_engine::TerrainStreamingManifestUpdateStatus::AssetLoadsPending)) == "AssetLoadsPending", "asset loads pending name is stable", failures);
    expect(std::string(full_engine::terrainStreamingManifestUpdateStatusName(full_engine::TerrainStreamingManifestUpdateStatus::ManifestStageFailed)) == "ManifestStageFailed", "manifest stage failed name is stable", failures);
    expect(std::string(full_engine::terrainStreamingManifestUpdateStatusName(full_engine::TerrainStreamingManifestUpdateStatus::UnsupportedStageChanges)) == "UnsupportedStageChanges", "unsupported stage changes name is stable", failures);
    expect(std::string(full_engine::terrainStreamingManifestUpdateStatusName(full_engine::TerrainStreamingManifestUpdateStatus::StreamingQueueBlocked)) == "StreamingQueueBlocked", "streaming queue blocked name is stable", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testNoManifest(failures);
    testMissingHandlesQueueLoadIntent(failures);
    testReadyManifestQueuesAddAndResident(failures);
    testExistingSetupProducesKeepDiagnostics(failures);
    testExistingResidentOutsideWindowQueuesUnloadAndRemove(failures);
    testMissingWorldDescReportsStageFailure(failures);
    testUnsupportedStageChangesBlockQueueing(failures);
    testInvalidStreamingConfigBlocksQueueing(failures);
    testStatusNames(failures);

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
