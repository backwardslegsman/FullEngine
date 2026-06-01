#include "engine/streaming/TerrainStreamingSchedulerTick.hpp"
#include "engine/streaming/TerrainStreamingSchedulerTickDiagnostics.hpp"

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
    desc.lods[0].mesh = asset(1);
    desc.lods[0].material = asset(2);
    desc.lods[0].maxDistanceMeters = 64.0f;
    desc.splatMap = asset(3);
    return desc;
}

full_engine::CookedAssetManifest manifest(const full_engine::ChunkId id = chunk())
{
    full_engine::CookedAssetManifest result;
    result.assets.push_back(assetRecord(1, full_engine::AssetKind::Mesh));
    result.assets.push_back(assetRecord(2, full_engine::AssetKind::Material));
    result.assets.push_back(assetRecord(3, full_engine::AssetKind::Texture));
    result.terrainChunks.push_back(terrainAssets(id));
    return result;
}

full_engine::RendererAssetHandleCatalog readyHandles()
{
    full_engine::RendererAssetHandleCatalog handles;
    (void)handles.addMeshHandle(asset(1), {10});
    (void)handles.addMaterialHandle(asset(2), {20});
    (void)handles.addTextureHandle(asset(3), {30});
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

void appendPressureTick(full_engine::TerrainStreamingLoopState& loop, const std::size_t deferredWork = 1)
{
    full_engine::TerrainStreamingTickEvent event;
    event.streamingStatus = full_engine::TerrainStreamingManifestUpdateStatus::Success;
    event.runtimeStatus = full_engine::TerrainRuntimeUpdateStatus::Success;
    event.streaming.deferredSetupAddCount = deferredWork;
    loop.appendTickEvent(event);
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
    full_engine::ChunkTerrainHandleMap terrainHandles;
};

struct CallbackState
{
    full_engine::RendererAssetHandleCatalog handles;
    std::uint64_t failedAsset = 0;
    std::vector<std::uint64_t> calls;
};

full_engine::TerrainManifestAssetLoadCallbackResult callback(
    const full_engine::TerrainManifestAssetLoadRequest& request,
    void* const userData)
{
    CallbackState& state = *static_cast<CallbackState*>(userData);
    state.calls.push_back(request.id.value);

    full_engine::TerrainManifestAssetLoadCallbackResult result;
    if (request.id.value == state.failedAsset)
    {
        result.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Failed;
        return result;
    }

    result.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded;
    switch (request.kind)
    {
    case full_engine::AssetKind::Mesh:
        if (const full_renderer::MeshHandle* const handle = state.handles.findMeshHandle(request.id))
        {
            result.mesh = *handle;
            return result;
        }
        break;
    case full_engine::AssetKind::Material:
        if (const full_renderer::MaterialHandle* const handle = state.handles.findMaterialHandle(request.id))
        {
            result.material = *handle;
            return result;
        }
        break;
    case full_engine::AssetKind::Texture:
        if (const full_renderer::TextureHandle* const handle = state.handles.findTextureHandle(request.id))
        {
            result.texture = *handle;
            return result;
        }
        break;
    case full_engine::AssetKind::Unknown:
    case full_engine::AssetKind::TerrainChunk:
    case full_engine::AssetKind::Skeleton:
    case full_engine::AssetKind::SkinnedMesh:
    case full_engine::AssetKind::Shader:
        break;
    }

    result.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Missing;
    return result;
}

void preparePendingLoadRequests(
    Fixture& fixture,
    const full_engine::ChunkId id = chunk())
{
    fixture.loop.manifestLoad().setManifest(manifest(id));
    const full_engine::WorldChunkDesc desc = worldDesc(id);
    (void)fixture.loop.updateStreamingFromManifest(
        full_engine::RendererAssetHandleCatalog{},
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        &desc,
        1,
        fixture.runtime,
        streamingConfig(),
        {0.0, 0.0, 0.0},
        missingSnapshot(id));
}

full_engine::TerrainStreamingSchedulerTickResult runTick(
    Fixture& fixture,
    full_engine::RendererAssetHandleCatalog& assetHandles,
    CallbackState& callbackState,
    const full_engine::WorldChunkDesc& desc,
    const full_engine::TerrainRuntimeStateSnapshot& snapshot,
    const full_engine::TerrainStreamingSchedulerTickOptions& options = {})
{
    const full_engine::ChunkId trackedId = desc.id;
    return full_engine::runTerrainStreamingSchedulerTick(
        fixture.loop,
        fixture.runtime,
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.terrainHandles,
        assetHandles,
        &desc,
        1,
        &trackedId,
        1,
        streamingConfig(),
        {0.0, 0.0, 0.0},
        snapshot,
        callback,
        &callbackState,
        options);
}

void registerRenderableChunk(Fixture& fixture, const full_engine::ChunkId id)
{
    (void)fixture.registry.createChunk(id);
    (void)fixture.registry.setResidencyState(id, full_engine::ChunkResidencyState::Loading);
    (void)fixture.registry.setResidencyState(id, full_engine::ChunkResidencyState::Resident);
    (void)fixture.worldCatalog.addChunk(worldDesc(id));
    (void)fixture.resources.addChunkResources(terrainResources(id));
    (void)fixture.terrainHandles.mapChunk(id, {50, 1});
}

void testIdleDecisionRunsNoPhases(std::vector<std::string>& failures)
{
    Fixture fixture;
    full_engine::RendererAssetHandleCatalog assetHandles;
    CallbackState callbackState;
    const full_engine::WorldChunkDesc desc = worldDesc();

    const full_engine::TerrainStreamingSchedulerTickResult result =
        runTick(fixture, assetHandles, callbackState, desc, missingSnapshot());

    expect(result.status == full_engine::TerrainStreamingSchedulerTickStatus::Idle, "idle tick returns idle", failures);
    expect(!result.loadJobsRan, "idle tick does not run load jobs", failures);
    expect(!result.streamingRan, "idle tick does not run streaming", failures);
    expect(fixture.renderer.createCalls == 0, "idle tick does not touch renderer", failures);
}

void testPendingLoadRequestsRunLoadJobsOnly(std::vector<std::string>& failures)
{
    Fixture fixture;
    preparePendingLoadRequests(fixture);
    full_engine::RendererAssetHandleCatalog assetHandles;
    CallbackState callbackState;
    callbackState.handles = readyHandles();
    const full_engine::WorldChunkDesc desc = worldDesc();
    full_engine::TerrainStreamingSchedulerTickOptions options;
    options.scheduler.normalMaxAssetLoadJobs = 8;

    const full_engine::TerrainStreamingSchedulerTickResult result =
        runTick(fixture, assetHandles, callbackState, desc, missingSnapshot(), options);

    expect(result.status == full_engine::TerrainStreamingSchedulerTickStatus::Success, "load-only tick succeeds", failures);
    expect(result.loadJobsRan, "load-only tick runs load jobs", failures);
    expect(!result.streamingRan, "load-only tick does not run streaming", failures);
    expect(result.loadJobs.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::Success, "load-only tick stores load result", failures);
    expect(fixture.loop.manifestLoad().pendingLoadRequestCount() == 0, "load-only tick clears pending load requests", failures);
    expect(assetHandles.findMeshHandle(asset(1)) != nullptr, "load-only tick writes mesh handle", failures);
    expect(fixture.renderer.createCalls == 0, "load-only tick does not touch terrain renderer", failures);
}

void testDeferredPressureRunsStreamingOnly(std::vector<std::string>& failures)
{
    Fixture fixture;
    fixture.loop.manifestLoad().setManifest(manifest());
    appendPressureTick(fixture.loop);
    full_engine::RendererAssetHandleCatalog assetHandles = readyHandles();
    CallbackState callbackState;
    const full_engine::WorldChunkDesc desc = worldDesc();

    const full_engine::TerrainStreamingSchedulerTickResult result =
        runTick(fixture, assetHandles, callbackState, desc, missingSnapshot());

    expect(result.status == full_engine::TerrainStreamingSchedulerTickStatus::Success, "streaming-only tick succeeds", failures);
    expect(!result.loadJobsRan, "streaming-only tick does not run load jobs", failures);
    expect(result.streamingRan, "streaming-only tick runs streaming", failures);
    expect(result.streaming.runtimeUpdateRan, "streaming-only tick applies runtime update", failures);
    const full_engine::TerrainStreamingTickEvent* const tick = fixture.loop.latestTickEvent();
    expect(tick != nullptr, "streaming-only tick records history", failures);
    if (tick != nullptr)
    {
        expect(tick->scheduler.hasSchedulerDecision, "streaming-only tick annotates scheduler decision", failures);
        expect(tick->scheduler.decisionStatus == result.decision.status, "streaming-only history copies decision status", failures);
        expect(tick->scheduler.decisionReason == result.decision.reason, "streaming-only history copies decision reason", failures);
        expect(tick->scheduler.streamingRan, "streaming-only history records streaming phase", failures);
        expect(tick->budgetProfile == result.decision.budgetProfile, "streaming-only history uses scheduler profile", failures);
    }
    expect(fixture.renderer.createCalls == 1, "streaming-only tick creates terrain", failures);
    expect(fixture.terrainHandles.contains(chunk()), "streaming-only tick maps terrain handle", failures);
}

void testCombinedLoadAndStreamingRunsInOrder(std::vector<std::string>& failures)
{
    Fixture fixture;
    preparePendingLoadRequests(fixture);
    appendPressureTick(fixture.loop);
    full_engine::RendererAssetHandleCatalog assetHandles;
    CallbackState callbackState;
    callbackState.handles = readyHandles();
    const full_engine::WorldChunkDesc desc = worldDesc();
    full_engine::TerrainStreamingSchedulerTickOptions options;
    options.scheduler.normalMaxAssetLoadJobs = 8;

    const full_engine::TerrainStreamingSchedulerTickResult result =
        runTick(fixture, assetHandles, callbackState, desc, missingSnapshot(), options);

    expect(result.status == full_engine::TerrainStreamingSchedulerTickStatus::Success, "combined tick succeeds", failures);
    expect(result.loadJobsRan, "combined tick runs load jobs", failures);
    expect(result.streamingRan, "combined tick runs streaming", failures);
    expect(assetHandles.findTextureHandle(asset(3)) != nullptr, "combined tick loads handles before streaming", failures);
    expect(fixture.renderer.createCalls == 1, "combined tick creates terrain after loading handles", failures);
    expect(fixture.loop.manifestLoad().pendingLoadRequestCount() == 0, "combined tick clears load requests", failures);
}

void testScheduleOnlyMirrorsLoadJobsWithoutExecution(std::vector<std::string>& failures)
{
    Fixture fixture;
    preparePendingLoadRequests(fixture);
    appendPressureTick(fixture.loop);
    full_engine::RendererAssetHandleCatalog assetHandles;
    CallbackState callbackState;
    callbackState.handles = readyHandles();
    const full_engine::WorldChunkDesc desc = worldDesc();
    full_engine::TerrainStreamingSchedulerTickOptions options;
    options.loadJobMode = full_engine::TerrainStreamingSchedulerLoadJobMode::ScheduleOnly;

    const full_engine::TerrainStreamingSchedulerTickResult result =
        runTick(fixture, assetHandles, callbackState, desc, missingSnapshot(), options);

    expect(result.status == full_engine::TerrainStreamingSchedulerTickStatus::Success, "schedule-only tick succeeds", failures);
    expect(!result.loadJobsRan, "schedule-only tick does not execute load jobs", failures);
    expect(result.loadJobsScheduled, "schedule-only tick schedules load jobs", failures);
    expect(!result.streamingRan, "schedule-only tick stops before streaming", failures);
    expect(callbackState.calls.empty(), "schedule-only tick does not invoke load callback", failures);
    expect(result.scheduledLoadJobs.status == full_engine::TerrainManifestAssetLoadJobScheduleStatus::Scheduled, "schedule-only tick stores schedule result", failures);
    expect(result.scheduledLoadJobs.mirror.summary.queuedCount == 3, "schedule-only tick mirrors pending loads", failures);
    expect(result.scheduledLoadJobs.finalPendingLoadRequestCount == 3, "schedule-only tick leaves load requests pending", failures);
    expect(fixture.loop.manifestLoad().pendingLoadRequestCount() == 3, "schedule-only tick preserves retained load queue", failures);
    expect(fixture.loop.manifestAssetLoadJobs().jobCount() == 3, "schedule-only tick leaves jobs queued for external execution", failures);
    expect(assetHandles.meshHandleCount() == 0, "schedule-only tick leaves handle catalog unchanged", failures);
    expect(fixture.renderer.createCalls == 0, "schedule-only tick does not touch renderer", failures);
    expect(fixture.loop.latestDiagnostics().scheduledLoadJobs.status == full_engine::TerrainManifestAssetLoadJobScheduleStatus::Scheduled, "schedule-only tick updates loop diagnostics", failures);
}

void testScheduleOnlyDeduplicatesAlreadyQueuedJobs(std::vector<std::string>& failures)
{
    Fixture fixture;
    preparePendingLoadRequests(fixture);
    full_engine::RendererAssetHandleCatalog assetHandles;
    CallbackState callbackState;
    const full_engine::WorldChunkDesc desc = worldDesc();
    full_engine::TerrainStreamingSchedulerTickOptions options;
    options.loadJobMode = full_engine::TerrainStreamingSchedulerLoadJobMode::ScheduleOnly;

    const full_engine::TerrainStreamingSchedulerTickResult first =
        runTick(fixture, assetHandles, callbackState, desc, missingSnapshot(), options);
    const full_engine::TerrainStreamingSchedulerTickResult second =
        runTick(fixture, assetHandles, callbackState, desc, missingSnapshot(), options);

    expect(first.status == full_engine::TerrainStreamingSchedulerTickStatus::Success, "first schedule-only tick succeeds", failures);
    expect(second.status == full_engine::TerrainStreamingSchedulerTickStatus::Success, "second schedule-only tick succeeds", failures);
    expect(second.scheduledLoadJobs.mirror.summary.alreadyQueuedCount == 3, "second schedule-only tick reports already queued jobs", failures);
    expect(second.scheduledLoadJobs.mirror.summary.queuedCount == 0, "second schedule-only tick queues no duplicates", failures);
    expect(fixture.loop.manifestAssetLoadJobs().jobCount() == 3, "schedule-only duplicate pass preserves job count", failures);
    expect(callbackState.calls.empty(), "schedule-only duplicate pass still does not invoke callback", failures);
}

void testBlockedLoadJobsStopBeforeStreaming(std::vector<std::string>& failures)
{
    Fixture fixture;
    preparePendingLoadRequests(fixture);
    appendPressureTick(fixture.loop);
    full_engine::RendererAssetHandleCatalog assetHandles;
    CallbackState callbackState;
    callbackState.handles = readyHandles();
    (void)callbackState.handles.removeMaterialHandle(asset(2));
    const full_engine::WorldChunkDesc desc = worldDesc();

    const full_engine::TerrainStreamingSchedulerTickResult result =
        runTick(fixture, assetHandles, callbackState, desc, missingSnapshot());

    expect(result.status == full_engine::TerrainStreamingSchedulerTickStatus::LoadJobsBlocked, "blocked load jobs map tick status", failures);
    expect(result.loadJobsRan, "blocked load jobs were attempted", failures);
    expect(!result.streamingRan, "blocked load jobs stop before streaming", failures);
    expect(fixture.loop.manifestLoad().pendingLoadRequestCount() == 3, "blocked load jobs preserve pending requests", failures);
    expect(fixture.renderer.createCalls == 0, "blocked load jobs do not touch renderer", failures);
}

void testStreamingBlockedMapsStatus(std::vector<std::string>& failures)
{
    Fixture fixture;
    appendPressureTick(fixture.loop);
    full_engine::RendererAssetHandleCatalog assetHandles;
    CallbackState callbackState;
    const full_engine::WorldChunkDesc desc = worldDesc();

    const full_engine::TerrainStreamingSchedulerTickResult result =
        runTick(fixture, assetHandles, callbackState, desc, missingSnapshot());

    expect(result.status == full_engine::TerrainStreamingSchedulerTickStatus::StreamingBlocked, "streaming blocked maps status", failures);
    expect(!result.loadJobsRan, "streaming blocked tick does not run load jobs", failures);
    expect(result.streamingRan, "streaming blocked tick attempts streaming", failures);
    expect(result.streaming.streaming.status == full_engine::TerrainStreamingManifestUpdateStatus::NoManifest, "streaming blocked preserves underlying status", failures);
}

void testRuntimeSetupFailureMapsStatus(std::vector<std::string>& failures)
{
    Fixture fixture;
    fixture.loop.manifestLoad().setManifest(manifest());
    appendPressureTick(fixture.loop);
    full_engine::RendererAssetHandleCatalog assetHandles = readyHandles();
    CallbackState callbackState;
    const full_engine::WorldChunkDesc desc = worldDesc();
    full_engine::TerrainChunkResourceDesc invalidResources = terrainResources();
    invalidResources.lodCount = 0;
    fixture.runtime.queueSetupAdd(desc, invalidResources);

    const full_engine::TerrainStreamingSchedulerTickResult result =
        runTick(fixture, assetHandles, callbackState, desc, missingSnapshot());

    expect(result.status == full_engine::TerrainStreamingSchedulerTickStatus::RuntimeSetupFailed, "setup failure maps scheduler tick status", failures);
    expect(result.streamingRan, "setup failure runs streaming phase", failures);
    expect(result.streaming.runtime.status == full_engine::TerrainRuntimeUpdateStatus::SetupFailed, "setup failure preserves runtime status", failures);
}

void testRuntimeResidencyFailureMapsStatus(std::vector<std::string>& failures)
{
    const full_engine::ChunkId id = chunk();
    Fixture fixture;
    fixture.loop.manifestLoad().setManifest(manifest(id));
    registerRenderableChunk(fixture, id);
    const full_engine::WorldChunkInfo* const info = fixture.registry.findChunk(id);
    if (info != nullptr)
    {
        const_cast<full_engine::WorldChunkInfo*>(info)->residency =
            static_cast<full_engine::ChunkResidencyState>(99);
    }
    fixture.runtime.queueMakeResident(id);
    appendPressureTick(fixture.loop);
    full_engine::RendererAssetHandleCatalog assetHandles = readyHandles();
    CallbackState callbackState;
    const full_engine::WorldChunkDesc desc = worldDesc(id);

    const full_engine::TerrainStreamingSchedulerTickResult result =
        runTick(fixture, assetHandles, callbackState, desc, renderableSnapshot(id));

    expect(result.status == full_engine::TerrainStreamingSchedulerTickStatus::RuntimeResidencyFailed, "residency failure maps scheduler tick status", failures);
    expect(result.streaming.runtime.status == full_engine::TerrainRuntimeUpdateStatus::ResidencyFailed, "residency failure preserves runtime status", failures);
}

void testRuntimePipelineFailureMapsStatus(std::vector<std::string>& failures)
{
    Fixture fixture;
    fixture.loop.manifestLoad().setManifest(manifest());
    fixture.renderer.failCreate = true;
    appendPressureTick(fixture.loop);
    full_engine::RendererAssetHandleCatalog assetHandles = readyHandles();
    CallbackState callbackState;
    const full_engine::WorldChunkDesc desc = worldDesc();

    const full_engine::TerrainStreamingSchedulerTickResult result =
        runTick(fixture, assetHandles, callbackState, desc, missingSnapshot());

    expect(result.status == full_engine::TerrainStreamingSchedulerTickStatus::RuntimePipelineFailed, "pipeline failure maps scheduler tick status", failures);
    expect(result.streaming.runtime.status == full_engine::TerrainRuntimeUpdateStatus::PipelineFailed, "pipeline failure preserves runtime status", failures);
    expect(!fixture.terrainHandles.contains(chunk()), "pipeline failure does not map handle", failures);
}

void testExplicitMaxJobOverrideIsHonored(std::vector<std::string>& failures)
{
    Fixture fixture;
    preparePendingLoadRequests(fixture);
    full_engine::RendererAssetHandleCatalog assetHandles;
    CallbackState callbackState;
    callbackState.handles = readyHandles();
    const full_engine::WorldChunkDesc desc = worldDesc();
    full_engine::TerrainStreamingSchedulerTickOptions options;
    options.overrideMaxAssetLoadJobs = true;
    options.maxAssetLoadJobs = 1;

    const full_engine::TerrainStreamingSchedulerTickResult result =
        runTick(fixture, assetHandles, callbackState, desc, missingSnapshot(), options);

    expect(result.status == full_engine::TerrainStreamingSchedulerTickStatus::LoadJobsBlocked, "partial max job override blocks tick", failures);
    expect(result.loadJobs.jobs.summary.completedCount == 1, "partial max job override completes one job", failures);
    expect(fixture.loop.manifestLoad().pendingLoadRequestCount() == 3, "partial max job override preserves load queue", failures);
}

void testSchedulerTickDiagnosticsDefaultAndCopiesCounters(std::vector<std::string>& failures)
{
    {
        const full_engine::TerrainStreamingSchedulerTickResult result;
        const full_engine::TerrainStreamingSchedulerTickDiagnostics diagnostics =
            full_engine::makeTerrainStreamingSchedulerTickDiagnostics(result);

        expect(diagnostics.status == full_engine::TerrainStreamingSchedulerTickStatus::Idle, "default diagnostics status", failures);
        expect(diagnostics.decisionStatus == full_engine::TerrainStreamingSchedulerStatus::Idle, "default diagnostics decision status", failures);
        expect(diagnostics.decisionReason == full_engine::TerrainStreamingSchedulerReason::NoWork, "default diagnostics reason", failures);
        expect(diagnostics.history.hasSchedulerDecision, "default diagnostics has history decision", failures);
        expect(!diagnostics.loadJobsRan, "default diagnostics load phase skipped", failures);
        expect(!diagnostics.loadJobsScheduled, "default diagnostics schedule phase skipped", failures);
        expect(!diagnostics.streamingRan, "default diagnostics streaming phase skipped", failures);
        expect(diagnostics.loadJobExecution.completedCount == 0, "default diagnostics load counters zero", failures);
        expect(diagnostics.scheduledLoadJobMirror.queuedCount == 0, "default diagnostics schedule counters zero", failures);
        expect(diagnostics.streamingSummary.streamingPlanOperationCount == 0, "default diagnostics streaming counters zero", failures);
    }

    full_engine::TerrainStreamingSchedulerTickResult result;
    result.status = full_engine::TerrainStreamingSchedulerTickStatus::Success;
    result.decision.status = full_engine::TerrainStreamingSchedulerStatus::RunStreamingAndAssetLoadJobs;
    result.decision.reason = full_engine::TerrainStreamingSchedulerReason::CatchUp;
    result.decision.budgetProfile = full_engine::TerrainStreamingBudgetProfile::CatchUp;
    result.decision.pendingLoadRequestCount = 1;
    result.decision.pendingJobCount = 2;
    result.decision.deferredWorkCount = 3;
    result.decision.peakDeferredWorkCount = 4;
    result.decision.runtimeBacklogCount = 5;
    result.decision.pressureCount = 6;
    result.decision.maxAssetLoadJobs = 7;
    result.loadJobsRan = true;
    result.loadJobsScheduled = true;
    result.streamingRan = true;
    result.loadJobs.status = full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::Success;
    result.loadJobs.mirror.summary.queuedCount = 8;
    result.loadJobs.jobs.summary.completedCount = 9;
    result.loadJobs.load.consume.summary.loadedCount = 10;
    result.loadJobs.load.consume.consumed = true;
    result.loadJobs.summary.finalReadyHandleCount = 11;
    result.loadJobs.readiness.summary.readyCount = 12;
    result.loadJobs.mirror.records.push_back({});
    result.loadJobs.jobs.records.push_back({});
    result.loadJobs.load.consume.records.push_back({});
    result.loadJobs.readiness.records.push_back({});
    result.scheduledLoadJobs.status = full_engine::TerrainManifestAssetLoadJobScheduleStatus::Scheduled;
    result.scheduledLoadJobs.mirror.summary.queuedCount = 31;
    result.scheduledLoadJobs.mirror.summary.alreadyQueuedCount = 32;
    result.scheduledLoadJobs.mirror.summary.invalidArgumentCount = 33;
    result.scheduledLoadJobs.initialPendingLoadRequestCount = 34;
    result.scheduledLoadJobs.finalPendingLoadRequestCount = 35;
    result.scheduledLoadJobs.pendingJobCount = 36;
    result.streaming.status = full_engine::TerrainStreamingLoopUpdateStatus::Success;
    result.streaming.streaming.status = full_engine::TerrainStreamingManifestUpdateStatus::Success;
    result.streaming.runtime.status = full_engine::TerrainRuntimeUpdateStatus::Success;
    result.streaming.runtimeUpdateRan = true;
    result.streaming.setupRequestsBeforeRuntime = 13;
    result.streaming.residencyRequestsBeforeRuntime = 14;
    result.streaming.setupRequestsAfterRuntime = 15;
    result.streaming.residencyRequestsAfterRuntime = 16;
    result.streaming.streaming.summary.streamingPlanOperationCount = 17;
    result.streaming.streaming.streamingQueue.summary.queuedSetupAddCount = 18;

    const std::size_t sourceMirrorRecordCount = result.loadJobs.mirror.records.size();
    const std::size_t sourceJobRecordCount = result.loadJobs.jobs.records.size();
    const std::size_t sourceLoadRecordCount = result.loadJobs.load.consume.records.size();
    const std::size_t sourceReadinessRecordCount = result.loadJobs.readiness.records.size();
    const full_engine::TerrainStreamingSchedulerTickDiagnostics diagnostics =
        full_engine::makeTerrainStreamingSchedulerTickDiagnostics(result);

    expect(diagnostics.status == full_engine::TerrainStreamingSchedulerTickStatus::Success, "diagnostics copies status", failures);
    expect(diagnostics.decisionStatus == full_engine::TerrainStreamingSchedulerStatus::RunStreamingAndAssetLoadJobs, "diagnostics copies decision status", failures);
    expect(diagnostics.decisionReason == full_engine::TerrainStreamingSchedulerReason::CatchUp, "diagnostics copies decision reason", failures);
    expect(diagnostics.budgetProfile == full_engine::TerrainStreamingBudgetProfile::CatchUp, "diagnostics copies profile", failures);
    expect(diagnostics.pendingLoadRequestCount == 1, "diagnostics copies pending load count", failures);
    expect(diagnostics.pendingJobCount == 2, "diagnostics copies pending job count", failures);
    expect(diagnostics.deferredWorkCount == 3, "diagnostics copies deferred count", failures);
    expect(diagnostics.peakDeferredWorkCount == 4, "diagnostics copies peak deferred count", failures);
    expect(diagnostics.runtimeBacklogCount == 5, "diagnostics copies runtime backlog", failures);
    expect(diagnostics.pressureCount == 6, "diagnostics copies pressure", failures);
    expect(diagnostics.maxAssetLoadJobs == 7, "diagnostics copies max jobs", failures);
    expect(diagnostics.history.hasSchedulerDecision, "diagnostics history marks decision present", failures);
    expect(diagnostics.history.status == diagnostics.status, "diagnostics history copies status", failures);
    expect(diagnostics.history.decisionStatus == diagnostics.decisionStatus, "diagnostics history copies decision status", failures);
    expect(diagnostics.history.decisionReason == diagnostics.decisionReason, "diagnostics history copies decision reason", failures);
    expect(diagnostics.history.budgetProfile == diagnostics.budgetProfile, "diagnostics history copies profile", failures);
    expect(diagnostics.history.pressureCount == diagnostics.pressureCount, "diagnostics history copies pressure", failures);
    expect(diagnostics.loadJobsRan, "diagnostics copies load phase bool", failures);
    expect(diagnostics.loadJobsScheduled, "diagnostics copies schedule phase bool", failures);
    expect(diagnostics.streamingRan, "diagnostics copies streaming phase bool", failures);
    expect(diagnostics.loadJobStatus == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::Success, "diagnostics copies load status", failures);
    expect(diagnostics.loadJobMirror.queuedCount == 8, "diagnostics copies mirror counters", failures);
    expect(diagnostics.loadJobExecution.completedCount == 9, "diagnostics copies job execution", failures);
    expect(diagnostics.loadConsume.loadedCount == 10, "diagnostics copies load consume", failures);
    expect(diagnostics.loadConsumed, "diagnostics copies consumed flag", failures);
    expect(diagnostics.loadJobCoordinator.finalReadyHandleCount == 11, "diagnostics copies coordinator summary", failures);
    expect(diagnostics.loadReadiness.readyCount == 12, "diagnostics copies readiness summary", failures);
    expect(diagnostics.scheduledLoadJobStatus == full_engine::TerrainManifestAssetLoadJobScheduleStatus::Scheduled, "diagnostics copies schedule status", failures);
    expect(diagnostics.scheduledLoadJobMirror.queuedCount == 31, "diagnostics copies schedule queued count", failures);
    expect(diagnostics.scheduledLoadJobMirror.alreadyQueuedCount == 32, "diagnostics copies schedule already queued count", failures);
    expect(diagnostics.scheduledLoadJobMirror.invalidArgumentCount == 33, "diagnostics copies schedule invalid count", failures);
    expect(diagnostics.scheduledInitialPendingLoadRequestCount == 34, "diagnostics copies schedule initial pending", failures);
    expect(diagnostics.scheduledFinalPendingLoadRequestCount == 35, "diagnostics copies schedule final pending", failures);
    expect(diagnostics.scheduledPendingJobCount == 36, "diagnostics copies schedule pending jobs", failures);
    expect(diagnostics.streamingStatus == full_engine::TerrainStreamingLoopUpdateStatus::Success, "diagnostics copies streaming status", failures);
    expect(diagnostics.manifestStreamingStatus == full_engine::TerrainStreamingManifestUpdateStatus::Success, "diagnostics copies manifest status", failures);
    expect(diagnostics.runtimeStatus == full_engine::TerrainRuntimeUpdateStatus::Success, "diagnostics copies runtime status", failures);
    expect(diagnostics.runtimeUpdateRan, "diagnostics copies runtime update bool", failures);
    expect(diagnostics.setupRequestsBeforeRuntime == 13, "diagnostics copies setup before", failures);
    expect(diagnostics.residencyRequestsBeforeRuntime == 14, "diagnostics copies residency before", failures);
    expect(diagnostics.setupRequestsAfterRuntime == 15, "diagnostics copies setup after", failures);
    expect(diagnostics.residencyRequestsAfterRuntime == 16, "diagnostics copies residency after", failures);
    expect(diagnostics.streamingSummary.streamingPlanOperationCount == 17, "diagnostics copies streaming summary", failures);
    expect(diagnostics.streamingQueue.queuedSetupAddCount == 18, "diagnostics copies queue summary", failures);
    expect(result.loadJobs.mirror.records.size() == sourceMirrorRecordCount, "diagnostics does not mutate mirror records", failures);
    expect(result.loadJobs.jobs.records.size() == sourceJobRecordCount, "diagnostics does not mutate job records", failures);
    expect(result.loadJobs.load.consume.records.size() == sourceLoadRecordCount, "diagnostics does not mutate load records", failures);
    expect(result.loadJobs.readiness.records.size() == sourceReadinessRecordCount, "diagnostics does not mutate readiness records", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testIdleDecisionRunsNoPhases(failures);
    testPendingLoadRequestsRunLoadJobsOnly(failures);
    testDeferredPressureRunsStreamingOnly(failures);
    testCombinedLoadAndStreamingRunsInOrder(failures);
    testScheduleOnlyMirrorsLoadJobsWithoutExecution(failures);
    testScheduleOnlyDeduplicatesAlreadyQueuedJobs(failures);
    testBlockedLoadJobsStopBeforeStreaming(failures);
    testStreamingBlockedMapsStatus(failures);
    testRuntimeSetupFailureMapsStatus(failures);
    testRuntimeResidencyFailureMapsStatus(failures);
    testRuntimePipelineFailureMapsStatus(failures);
    testExplicitMaxJobOverrideIsHonored(failures);
    testSchedulerTickDiagnosticsDefaultAndCopiesCounters(failures);

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
