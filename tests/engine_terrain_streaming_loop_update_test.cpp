#include "engine/streaming/TerrainStreamingLoopUpdate.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* const message, std::vector<std::string>& failures)
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

full_engine::AssetRecord assetRecord(
    const std::uint64_t id,
    const full_engine::AssetKind kind) noexcept
{
    full_engine::AssetRecord record;
    record.id = asset(id);
    record.kind = kind;
    return record;
}

full_engine::ChunkId chunk(const std::int32_t x = 0) noexcept
{
    return {x, 0, 0};
}

full_engine::WorldChunkDesc worldDesc(const full_engine::ChunkId id = chunk()) noexcept
{
    full_engine::WorldChunkDesc desc;
    desc.id = id;
    desc.bounds.min = {static_cast<double>(id.x) * 16.0, 0.0, 0.0};
    desc.bounds.max = {static_cast<double>(id.x) * 16.0 + 16.0, 4.0, 16.0};
    return desc;
}

full_engine::TerrainChunkAssetDesc terrainAssets(const full_engine::ChunkId id = chunk())
{
    full_engine::TerrainChunkAssetDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = asset(10);
    desc.lods[0].material = asset(20);
    desc.lods[0].maxDistanceMeters = 64.0f;
    desc.splatMap = asset(30);
    return desc;
}

full_engine::CookedAssetManifest validManifest(const full_engine::ChunkId id = chunk())
{
    full_engine::CookedAssetManifest manifest;
    manifest.assets.push_back(assetRecord(10, full_engine::AssetKind::Mesh));
    manifest.assets.push_back(assetRecord(20, full_engine::AssetKind::Material));
    manifest.assets.push_back(assetRecord(30, full_engine::AssetKind::Texture));
    manifest.terrainChunks.push_back(terrainAssets(id));
    return manifest;
}

full_engine::CookedAssetManifest validManifest(const std::vector<full_engine::ChunkId>& ids)
{
    full_engine::CookedAssetManifest manifest;
    manifest.assets.push_back(assetRecord(10, full_engine::AssetKind::Mesh));
    manifest.assets.push_back(assetRecord(20, full_engine::AssetKind::Material));
    manifest.assets.push_back(assetRecord(30, full_engine::AssetKind::Texture));
    for (const full_engine::ChunkId& id : ids)
    {
        manifest.terrainChunks.push_back(terrainAssets(id));
    }
    return manifest;
}

full_engine::RendererAssetHandleCatalog readyHandles()
{
    full_engine::RendererAssetHandleCatalog handles;
    (void)handles.addMeshHandle(asset(10), {10});
    (void)handles.addMaterialHandle(asset(20), {20});
    (void)handles.addTextureHandle(asset(30), {30});
    return handles;
}

full_engine::TerrainChunkResourceDesc terrainResources(const full_engine::ChunkId id = chunk()) noexcept
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

full_engine::TerrainRuntimeStateSnapshot missingSnapshot(const full_engine::ChunkId id = chunk())
{
    full_engine::TerrainRuntimeChunkState state;
    state.id = id;
    state.readiness = full_engine::TerrainRuntimeChunkReadiness::MissingRegistry;

    full_engine::TerrainRuntimeStateSnapshot snapshot;
    snapshot.chunks.push_back(state);
    snapshot.missingRegistryCount = 1;
    return snapshot;
}

full_engine::TerrainRuntimeStateSnapshot missingSnapshot(const std::vector<full_engine::ChunkId>& ids)
{
    full_engine::TerrainRuntimeStateSnapshot snapshot;
    for (const full_engine::ChunkId& id : ids)
    {
        full_engine::TerrainRuntimeChunkState state;
        state.id = id;
        state.readiness = full_engine::TerrainRuntimeChunkReadiness::MissingRegistry;
        snapshot.chunks.push_back(state);
    }
    snapshot.missingRegistryCount = ids.size();
    return snapshot;
}

full_engine::TerrainRuntimeStateSnapshot renderableSnapshot(const full_engine::ChunkId id = chunk())
{
    full_engine::TerrainRuntimeChunkState state;
    state.id = id;
    state.hasRegistry = true;
    state.hasWorldDesc = true;
    state.hasResources = true;
    state.hasTerrainHandle = true;
    state.residency = full_engine::ChunkResidencyState::Resident;
    state.readiness = full_engine::TerrainRuntimeChunkReadiness::Renderable;

    full_engine::TerrainRuntimeStateSnapshot snapshot;
    snapshot.chunks.push_back(state);
    snapshot.renderableCount = 1;
    return snapshot;
}

full_engine::TerrainStreamingPlannerConfig streamingConfig() noexcept
{
    full_engine::TerrainStreamingPlannerConfig config;
    config.chunkSizeMeters = 16.0;
    config.loadRadiusChunks = 0;
    config.residentRadiusChunks = 0;
    return config;
}

class FakeRenderer final : public full_renderer::IRenderer
{
public:
    std::uint32_t nextTerrainId = 10;
    bool failCreate = false;
    int createCalls = 0;
    int updateCalls = 0;
    int destroyCalls = 0;

    full_renderer::RendererResult initialize(const full_renderer::RendererInitDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    void shutdown() noexcept override {}

    bool isInitialized() const noexcept override
    {
        return true;
    }

    full_renderer::MeshHandle createMesh(const full_renderer::MeshDesc&) override
    {
        return {};
    }

    void destroyMesh(full_renderer::MeshHandle) noexcept override {}

    full_renderer::SkeletonHandle createSkeleton(const full_renderer::SkeletonDesc&) override
    {
        return {};
    }

    void destroySkeleton(full_renderer::SkeletonHandle) noexcept override {}

    full_renderer::SkinnedMeshHandle createSkinnedMesh(const full_renderer::SkinnedMeshDesc&) override
    {
        return {};
    }

    void destroySkinnedMesh(full_renderer::SkinnedMeshHandle) noexcept override {}

    full_renderer::TextureHandle createTexture(const full_renderer::TextureDesc&) override
    {
        return {};
    }

    void destroyTexture(full_renderer::TextureHandle) noexcept override {}

    full_renderer::MaterialHandle createMaterial(const full_renderer::MaterialDesc&) override
    {
        return {};
    }

    void destroyMaterial(full_renderer::MaterialHandle) noexcept override {}

    full_renderer::TerrainChunkHandle createTerrainChunk(const full_renderer::TerrainChunkDesc&) override
    {
        ++createCalls;
        if (failCreate)
        {
            return {};
        }
        return {nextTerrainId++, 1};
    }

    full_renderer::RendererResult updateTerrainChunk(
        full_renderer::TerrainChunkHandle,
        const full_renderer::TerrainChunkDesc&) override
    {
        ++updateCalls;
        return full_renderer::RendererResult::Success;
    }

    void destroyTerrainChunk(full_renderer::TerrainChunkHandle) noexcept override
    {
        ++destroyCalls;
    }

    full_renderer::RendererResult resize(const full_renderer::RendererResizeDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult beginFrame(const full_renderer::FrameDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult submit(const full_renderer::RenderPacket&) override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult endFrame() override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererStats getStats() const noexcept override
    {
        return {};
    }

    full_renderer::TerrainStats getTerrainStats() const noexcept override
    {
        return {};
    }

    std::uint32_t copyTerrainDebugInfo(full_renderer::TerrainChunkDebugInfo*, std::uint32_t) const noexcept override
    {
        return 0;
    }

    std::uint32_t copyTerrainBatchDebugInfo(full_renderer::TerrainBatchDebugInfo*, std::uint32_t) const noexcept override
    {
        return 0;
    }

    std::uint32_t copyTerrainShadowCasterDebugInfo(
        full_renderer::TerrainChunkDebugInfo*,
        std::uint32_t) const noexcept override
    {
        return 0;
    }
};

struct Fixture
{
    full_engine::TerrainStreamingLoopState loop;
    full_engine::TerrainRuntimeState runtime;
    FakeRenderer renderer;
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    full_engine::ChunkTerrainHandleMap handles;
};

full_engine::TerrainStreamingLoopUpdateResult runUpdate(
    Fixture& fixture,
    const full_engine::RendererAssetHandleCatalog& assetHandles,
    const full_engine::WorldChunkDesc* const worldDescs,
    const std::size_t worldDescCount,
    const full_engine::ChunkId* const trackedIds,
    const std::size_t trackedIdCount,
    const full_engine::TerrainStreamingPlannerConfig& config,
    const full_engine::TerrainRuntimeStateSnapshot& snapshot)
{
    return full_engine::updateTerrainStreamingLoop(
        fixture.loop,
        fixture.runtime,
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles,
        assetHandles,
        worldDescs,
        worldDescCount,
        trackedIds,
        trackedIdCount,
        config,
        {0.0, 0.0, 0.0},
        snapshot);
}

full_engine::TerrainStreamingLoopUpdateResult runUpdate(
    Fixture& fixture,
    const full_engine::RendererAssetHandleCatalog& assetHandles,
    const full_engine::WorldChunkDesc* const worldDescs,
    const std::size_t worldDescCount,
    const full_engine::ChunkId* const trackedIds,
    const std::size_t trackedIdCount,
    const full_engine::TerrainStreamingPlannerConfig& config,
    const full_engine::TerrainRuntimeStateSnapshot& snapshot,
    const full_engine::TerrainStreamingLoopUpdateOptions& options)
{
    return full_engine::updateTerrainStreamingLoop(
        fixture.loop,
        fixture.runtime,
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles,
        assetHandles,
        worldDescs,
        worldDescCount,
        trackedIds,
        trackedIdCount,
        config,
        {0.0, 0.0, 0.0},
        snapshot,
        options);
}

void seedCurrentRenderableSetup(Fixture& fixture, const full_engine::ChunkId id)
{
    (void)fixture.registry.createChunk(id);
    (void)fixture.registry.setResidencyState(id, full_engine::ChunkResidencyState::Loading);
    (void)fixture.registry.setResidencyState(id, full_engine::ChunkResidencyState::Resident);
    (void)fixture.worldCatalog.addChunk(worldDesc(id));
    (void)fixture.resources.addChunkResources(terrainResources(id));
    (void)fixture.handles.mapChunk(id, {77, 1});
}

void testNoManifestBlocks(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId id = chunk();
    const full_engine::WorldChunkDesc world = worldDesc(id);
    const full_engine::TerrainRuntimeStateSnapshot snapshot = missingSnapshot(id);

    const full_engine::TerrainStreamingLoopUpdateResult result =
        runUpdate(fixture, readyHandles(), &world, 1, &id, 1, streamingConfig(), snapshot);

    expect(result.status == full_engine::TerrainStreamingLoopUpdateStatus::StreamingBlocked, "no manifest blocks loop update", failures);
    expect(result.streaming.status == full_engine::TerrainStreamingManifestUpdateStatus::NoManifest, "no manifest preserves streaming status", failures);
    expect(!result.runtimeUpdateRan, "no manifest does not run runtime update", failures);
    expect(result.setupRequestsBeforeRuntime == 0, "no manifest has no setup requests before runtime", failures);
    expect(result.setupRequestsAfterRuntime == 0, "no manifest has no setup requests after runtime", failures);
}

void testMissingHandlesBlocksAndQueuesLoadIntent(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId id = chunk();
    fixture.loop.manifestLoad().setManifest(validManifest(id));
    const full_engine::WorldChunkDesc world = worldDesc(id);
    const full_engine::TerrainRuntimeStateSnapshot snapshot = missingSnapshot(id);

    const full_engine::TerrainStreamingLoopUpdateResult result =
        runUpdate(fixture, {}, &world, 1, &id, 1, streamingConfig(), snapshot);

    expect(result.status == full_engine::TerrainStreamingLoopUpdateStatus::StreamingBlocked, "missing handles block loop update", failures);
    expect(result.streaming.status == full_engine::TerrainStreamingManifestUpdateStatus::AssetLoadsPending, "missing handles report asset loads pending", failures);
    expect(!result.runtimeUpdateRan, "missing handles do not run runtime update", failures);
    expect(fixture.loop.manifestLoad().pendingLoadRequestCount() == 3, "missing handles queue load requests", failures);
    expect(fixture.runtime.setupRequestCount() == 0, "missing handles do not queue setup", failures);
    expect(fixture.runtime.residencyRequestCount() == 0, "missing handles do not queue residency", failures);
}

void testReadyManifestAppliesRuntime(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId id = chunk();
    fixture.loop.manifestLoad().setManifest(validManifest(id));
    const full_engine::WorldChunkDesc world = worldDesc(id);
    const full_engine::TerrainRuntimeStateSnapshot snapshot = missingSnapshot(id);

    const full_engine::TerrainStreamingLoopUpdateResult result =
        runUpdate(fixture, readyHandles(), &world, 1, &id, 1, streamingConfig(), snapshot);

    expect(result.status == full_engine::TerrainStreamingLoopUpdateStatus::Success, "ready manifest update succeeds", failures);
    expect(result.streaming.status == full_engine::TerrainStreamingManifestUpdateStatus::Success, "ready manifest streaming succeeds", failures);
    expect(result.runtimeUpdateRan, "ready manifest runs runtime update", failures);
    expect(result.setupRequestsBeforeRuntime == 1, "ready manifest reports queued setup before runtime", failures);
    expect(result.residencyRequestsBeforeRuntime == 1, "ready manifest reports queued residency before runtime", failures);
    expect(result.setupRequestsAfterRuntime == 0, "ready manifest clears setup queue", failures);
    expect(result.residencyRequestsAfterRuntime == 0, "ready manifest clears residency queue", failures);
    expect(result.runtime.status == full_engine::TerrainRuntimeUpdateStatus::Success, "runtime update succeeds", failures);
    expect(fixture.registry.contains(id), "runtime creates registry chunk", failures);
    expect(fixture.handles.contains(id), "runtime maps terrain handle", failures);
    expect(fixture.renderer.createCalls == 1, "runtime creates terrain once", failures);
    expect(fixture.runtime.hasLatestSnapshot(), "runtime update tracks snapshot", failures);
    expect(fixture.loop.tickHistoryCount() == 1, "loop update appends tick history", failures);
    expect(fixture.loop.latestTickEvent() != nullptr, "loop update stores latest tick", failures);
    if (fixture.loop.latestTickEvent() != nullptr)
    {
        expect(fixture.loop.latestTickEvent()->runtimeUpdateRan, "tick event records runtime update", failures);
        expect(fixture.loop.latestTickEvent()->streaming.queuedSetupAddCount == 1, "tick event copies streaming summary", failures);
        expect(fixture.loop.latestTickEvent()->runtimeSubmission.createdCount == 1, "tick event copies runtime submission", failures);
    }
}

void testBudgetedStreamingQueuesAndAppliesPartialBatch(std::vector<std::string>& failures)
{
    Fixture fixture;
    const std::vector<full_engine::ChunkId> ids = {chunk(0), chunk(1)};
    fixture.loop.manifestLoad().setManifest(validManifest(ids));
    const full_engine::WorldChunkDesc worlds[] = {worldDesc(ids[0]), worldDesc(ids[1])};
    const full_engine::TerrainRuntimeStateSnapshot snapshot = missingSnapshot(ids);
    full_engine::TerrainStreamingPlannerConfig config = streamingConfig();
    config.loadRadiusChunks = 1;
    config.residentRadiusChunks = 1;

    full_engine::TerrainStreamingLoopUpdateOptions options;
    options.budgets.queue.maxSetupAdds = 1;
    options.budgets.queue.maxMakeResident = 1;
    const full_engine::TerrainStreamingLoopUpdateResult result =
        runUpdate(fixture, readyHandles(), worlds, 2, ids.data(), ids.size(), config, snapshot, options);

    expect(result.status == full_engine::TerrainStreamingLoopUpdateStatus::Success, "budgeted streaming update succeeds", failures);
    expect(result.runtimeUpdateRan, "budgeted streaming applies partial runtime batch", failures);
    expect(result.setupRequestsBeforeRuntime == 1, "budgeted streaming queues one setup before runtime", failures);
    expect(result.residencyRequestsBeforeRuntime == 1, "budgeted streaming queues one residency before runtime", failures);
    expect(result.streaming.summary.deferredSetupAddCount == 1, "budgeted streaming defers one setup add", failures);
    expect(result.streaming.summary.deferredMakeResidentCount == 1, "budgeted streaming defers one make resident", failures);
    expect(fixture.renderer.createCalls == 1, "budgeted streaming creates one terrain chunk", failures);
    expect(fixture.handles.mappedCount() == 1, "budgeted streaming maps one terrain handle", failures);
    expect(fixture.loop.latestTickEvent() != nullptr, "budgeted streaming stores latest tick", failures);
    if (fixture.loop.latestTickEvent() != nullptr)
    {
        expect(fixture.loop.latestTickEvent()->streaming.deferredSetupAddCount == 1, "tick event copies deferred setup add", failures);
        expect(fixture.loop.latestTickEvent()->streamingQueue.deferredMakeResidentCount == 1, "tick event copies deferred residency", failures);
    }
}

void testZeroQueueBudgetSkipsRuntimeUpdate(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId id = chunk();
    fixture.loop.manifestLoad().setManifest(validManifest(id));
    const full_engine::WorldChunkDesc world = worldDesc(id);
    const full_engine::TerrainRuntimeStateSnapshot snapshot = missingSnapshot(id);

    full_engine::TerrainStreamingLoopUpdateOptions options;
    options.budgets.queue.maxSetupAdds = 0;
    options.budgets.queue.maxMakeResident = 0;
    const full_engine::TerrainStreamingLoopUpdateResult result =
        runUpdate(fixture, readyHandles(), &world, 1, &id, 1, streamingConfig(), snapshot, options);

    expect(result.status == full_engine::TerrainStreamingLoopUpdateStatus::Success, "zero queue budget succeeds", failures);
    expect(!result.runtimeUpdateRan, "zero queue budget skips runtime update", failures);
    expect(result.setupRequestsBeforeRuntime == 0, "zero queue budget queues no setup before runtime", failures);
    expect(result.residencyRequestsBeforeRuntime == 0, "zero queue budget queues no residency before runtime", failures);
    expect(result.streaming.summary.deferredSetupAddCount == 1, "zero queue budget defers setup add", failures);
    expect(result.streaming.summary.deferredMakeResidentCount == 1, "zero queue budget defers make resident", failures);
    expect(fixture.renderer.createCalls == 0, "zero queue budget does not call renderer", failures);
    expect(fixture.loop.latestTickEvent() != nullptr, "zero queue budget stores tick", failures);
    if (fixture.loop.latestTickEvent() != nullptr)
    {
        expect(!fixture.loop.latestTickEvent()->runtimeUpdateRan, "zero queue tick records skipped runtime", failures);
        expect(fixture.loop.latestTickEvent()->setupRequestsBeforeRuntime == 0, "zero queue tick copies setup count", failures);
    }
}

void testPipelineCreateBudgetLimitsRuntimeWork(std::vector<std::string>& failures)
{
    Fixture fixture;
    const std::vector<full_engine::ChunkId> ids = {chunk(0), chunk(1)};
    fixture.loop.manifestLoad().setManifest(validManifest(ids));
    const full_engine::WorldChunkDesc worlds[] = {worldDesc(ids[0]), worldDesc(ids[1])};
    const full_engine::TerrainRuntimeStateSnapshot snapshot = missingSnapshot(ids);
    full_engine::TerrainStreamingPlannerConfig config = streamingConfig();
    config.loadRadiusChunks = 1;
    config.residentRadiusChunks = 1;

    full_engine::TerrainStreamingLoopUpdateOptions options;
    options.budgets.maxPipelineCreates = 1;
    const full_engine::TerrainStreamingLoopUpdateResult result =
        runUpdate(fixture, readyHandles(), worlds, 2, ids.data(), ids.size(), config, snapshot, options);

    expect(result.status == full_engine::TerrainStreamingLoopUpdateStatus::Success, "pipeline create budget succeeds", failures);
    expect(result.runtimeUpdateRan, "pipeline create budget still runs runtime", failures);
    expect(result.setupRequestsBeforeRuntime == 2, "pipeline create budget preserves queued setup count", failures);
    expect(result.residencyRequestsBeforeRuntime == 2, "pipeline create budget preserves queued residency count", failures);
    expect(result.runtime.pipeline.lifecycle.summary.createCount == 1, "pipeline create budget plans one create", failures);
    expect(result.runtime.pipeline.lifecycle.summary.deferredCreateCount == 1, "pipeline create budget defers one create", failures);
    expect(fixture.renderer.createCalls == 1, "pipeline create budget calls renderer once", failures);
    expect(fixture.handles.mappedCount() == 1, "pipeline create budget maps one handle", failures);
    expect(fixture.loop.latestTickEvent() != nullptr, "pipeline budget stores tick", failures);
    if (fixture.loop.latestTickEvent() != nullptr)
    {
        expect(fixture.loop.latestTickEvent()->runtimeLifecycle.deferredCreateCount == 1, "tick event copies deferred create", failures);
    }
}

void testReadyManifestWithoutPendingRuntimeSkipsRuntimeUpdate(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId id = chunk();
    fixture.loop.manifestLoad().setManifest(validManifest(id));
    seedCurrentRenderableSetup(fixture, id);
    const full_engine::WorldChunkDesc world = worldDesc(id);
    const full_engine::TerrainRuntimeStateSnapshot snapshot = renderableSnapshot(id);

    const full_engine::TerrainStreamingLoopUpdateResult result =
        runUpdate(fixture, readyHandles(), &world, 1, &id, 1, streamingConfig(), snapshot);

    expect(result.status == full_engine::TerrainStreamingLoopUpdateStatus::Success, "stable manifest update succeeds", failures);
    expect(!result.runtimeUpdateRan, "stable manifest skips runtime update", failures);
    expect(result.setupRequestsBeforeRuntime == 0, "stable manifest queues no setup", failures);
    expect(result.residencyRequestsBeforeRuntime == 0, "stable manifest queues no residency", failures);
    expect(fixture.renderer.createCalls == 0, "stable manifest does not touch renderer", failures);
}

void testInvalidStreamingConfigBlocks(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId id = chunk();
    fixture.loop.manifestLoad().setManifest(validManifest(id));
    const full_engine::WorldChunkDesc world = worldDesc(id);
    const full_engine::TerrainRuntimeStateSnapshot snapshot = missingSnapshot(id);
    full_engine::TerrainStreamingPlannerConfig config = streamingConfig();
    config.chunkSizeMeters = 0.0;

    const full_engine::TerrainStreamingLoopUpdateResult result =
        runUpdate(fixture, readyHandles(), &world, 1, &id, 1, config, snapshot);

    expect(result.status == full_engine::TerrainStreamingLoopUpdateStatus::StreamingBlocked, "invalid streaming config blocks", failures);
    expect(result.streaming.status == full_engine::TerrainStreamingManifestUpdateStatus::StreamingQueueBlocked, "invalid streaming config reports queue blocked", failures);
    expect(!result.runtimeUpdateRan, "invalid streaming config does not run runtime", failures);
    expect(fixture.runtime.setupRequestCount() == 0, "invalid streaming config leaves setup queue empty", failures);
}

void testRuntimeSetupFailureMapsStatus(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId id = chunk();
    fixture.loop.manifestLoad().setManifest(validManifest(id));
    const full_engine::WorldChunkDesc world = worldDesc(id);
    const full_engine::TerrainRuntimeStateSnapshot snapshot = missingSnapshot(id);

    full_engine::TerrainChunkResourceDesc invalidResources = terrainResources({5, 0, 0});
    invalidResources.lodCount = 0;
    fixture.runtime.queueSetupAdd(worldDesc({5, 0, 0}), invalidResources);

    const full_engine::TerrainStreamingLoopUpdateResult result =
        runUpdate(fixture, readyHandles(), &world, 1, &id, 1, streamingConfig(), snapshot);

    expect(result.status == full_engine::TerrainStreamingLoopUpdateStatus::RuntimeSetupFailed, "setup failure maps loop status", failures);
    expect(result.runtimeUpdateRan, "setup failure ran runtime update", failures);
    expect(result.runtime.status == full_engine::TerrainRuntimeUpdateStatus::SetupFailed, "setup failure preserves runtime status", failures);
    expect(result.setupRequestsBeforeRuntime == 2, "setup failure reports preexisting plus streaming setup requests", failures);
    expect(result.setupRequestsAfterRuntime == 0, "setup failure clears setup requests", failures);
}

void testRuntimePipelineFailureMapsStatus(std::vector<std::string>& failures)
{
    Fixture fixture;
    fixture.renderer.failCreate = true;
    const full_engine::ChunkId id = chunk();
    fixture.loop.manifestLoad().setManifest(validManifest(id));
    const full_engine::WorldChunkDesc world = worldDesc(id);
    const full_engine::TerrainRuntimeStateSnapshot snapshot = missingSnapshot(id);

    const full_engine::TerrainStreamingLoopUpdateResult result =
        runUpdate(fixture, readyHandles(), &world, 1, &id, 1, streamingConfig(), snapshot);

    expect(result.status == full_engine::TerrainStreamingLoopUpdateStatus::RuntimePipelineFailed, "pipeline failure maps loop status", failures);
    expect(result.runtimeUpdateRan, "pipeline failure ran runtime update", failures);
    expect(result.runtime.status == full_engine::TerrainRuntimeUpdateStatus::PipelineFailed, "pipeline failure preserves runtime status", failures);
    expect(result.setupRequestsAfterRuntime == 0, "pipeline failure clears setup requests", failures);
    expect(result.residencyRequestsAfterRuntime == 0, "pipeline failure clears residency requests", failures);
    expect(!fixture.handles.contains(id), "pipeline failure does not map terrain handle", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testNoManifestBlocks(failures);
    testMissingHandlesBlocksAndQueuesLoadIntent(failures);
    testReadyManifestAppliesRuntime(failures);
    testBudgetedStreamingQueuesAndAppliesPartialBatch(failures);
    testZeroQueueBudgetSkipsRuntimeUpdate(failures);
    testPipelineCreateBudgetLimitsRuntimeWork(failures);
    testReadyManifestWithoutPendingRuntimeSkipsRuntimeUpdate(failures);
    testInvalidStreamingConfigBlocks(failures);
    testRuntimeSetupFailureMapsStatus(failures);
    testRuntimePipelineFailureMapsStatus(failures);

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
