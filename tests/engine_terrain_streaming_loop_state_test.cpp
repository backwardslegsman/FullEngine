#include "engine/assets/CookedAssetManifestJson.hpp"
#include "engine/streaming/TerrainStreamingBudgetPolicy.hpp"
#include "engine/streaming/TerrainStreamingLoopState.hpp"

#include <cstdio>
#include <cstdint>
#include <fstream>
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

void writeManifest(const char* const path)
{
    std::remove(path);
    const full_engine::CookedAssetManifestExportResult exported =
        full_engine::exportCookedAssetManifestJsonLines(validManifest(), path);
    (void)exported;
}

full_engine::RendererAssetHandleCatalog readyHandles()
{
    full_engine::RendererAssetHandleCatalog handles;
    (void)handles.addMeshHandle(asset(10), {10});
    (void)handles.addMaterialHandle(asset(20), {20});
    (void)handles.addTextureHandle(asset(30), {30});
    return handles;
}

struct CallbackState
{
    full_engine::RendererAssetHandleCatalog handles;
    bool failMaterials = false;
};

full_engine::TerrainManifestAssetLoadCallbackResult loadCallback(
    const full_engine::TerrainManifestAssetLoadRequest& request,
    void* const userData)
{
    CallbackState& state = *static_cast<CallbackState*>(userData);
    full_engine::TerrainManifestAssetLoadCallbackResult result;

    if (state.failMaterials && request.kind == full_engine::AssetKind::Material)
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

full_engine::WorldChunkDesc worldDesc(const full_engine::ChunkId id = chunk()) noexcept
{
    full_engine::WorldChunkDesc desc;
    desc.id = id;
    desc.bounds.min = {0.0, 0.0, 0.0};
    desc.bounds.max = {16.0, 4.0, 16.0};
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

full_engine::TerrainStreamingPlannerConfig streamingConfig() noexcept
{
    full_engine::TerrainStreamingPlannerConfig config;
    config.chunkSizeMeters = 16.0;
    config.loadRadiusChunks = 0;
    config.residentRadiusChunks = 0;
    return config;
}

void testDefaultState(std::vector<std::string>& failures)
{
    const full_engine::TerrainStreamingLoopState state;
    expect(!state.manifestLoad().hasManifest(), "default loop has no manifest", failures);
    expect(state.manifestAssetLoadJobs().jobCount() == 0, "default loop has no jobs", failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 0, "default loop has no load requests", failures);
    expect(!state.latestDiagnostics().hasManifest, "default diagnostics report no manifest", failures);
    expect(state.latestDiagnostics().pendingJobCount == 0, "default diagnostics report no jobs", failures);
    expect(state.tickHistoryCount() == 0, "default loop has no tick history", failures);
    expect(state.latestTickEvent() == nullptr, "default loop latest tick is null", failures);
    expect(state.latestDiagnostics().tickHistoryCount == 0, "default diagnostics report no tick history", failures);
    expect(
        state.latestDiagnostics().adaptiveBudget.profile == full_engine::TerrainStreamingBudgetProfile::Balanced,
        "default diagnostics expose balanced adaptive budget",
        failures);
    expect(
        state.latestDiagnostics().adaptiveBudget.inspectedTickCount == 0,
        "default diagnostics expose empty adaptive budget history",
        failures);
}

void testReloadSuccessQueuesMissingLoads(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_loop_valid.jsonl";
    writeManifest(path);

    full_engine::TerrainStreamingLoopState state;
    const full_engine::TerrainManifestFileReloadPlanResult& result =
        state.reloadManifestAndQueueMissingAssetLoads(path, {});

    expect(result.load.status == full_engine::TerrainManifestFileLoadStatus::Success, "reload success reports file success", failures);
    expect(state.manifestLoad().hasManifest(), "reload success retains manifest", failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 3, "reload success queues missing load requests", failures);
    expect(state.latestDiagnostics().hasManifest, "reload diagnostics report manifest", failures);
    expect(state.latestDiagnostics().latestFileAssetCount == 3, "reload diagnostics copy asset count", failures);
    expect(state.latestDiagnostics().latestFileTerrainChunkCount == 1, "reload diagnostics copy terrain count", failures);
    expect(state.latestDiagnostics().pendingLoadRequestCount == 3, "reload diagnostics copy pending load requests", failures);
    expect(state.latestDiagnostics().pendingJobCount == 0, "reload clears stale jobs", failures);

    std::remove(path);
}

void testReloadFailureClearsState(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_loop_valid_for_failure.jsonl";
    writeManifest(path);

    full_engine::TerrainStreamingLoopState state;
    (void)state.reloadManifestAndQueueMissingAssetLoads(path, {});
    expect(state.manifestLoad().hasManifest(), "seed reload retained manifest", failures);

    const full_engine::TerrainManifestFileReloadPlanResult& result =
        state.reloadManifestAndQueueMissingAssetLoads("", readyHandles());

    expect(result.load.status == full_engine::TerrainManifestFileLoadStatus::InvalidArgument, "invalid reload reports invalid argument", failures);
    expect(!state.manifestLoad().hasManifest(), "invalid reload clears manifest", failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 0, "invalid reload clears load requests", failures);
    expect(state.manifestAssetLoadJobs().jobCount() == 0, "invalid reload clears jobs", failures);
    expect(!state.latestDiagnostics().hasManifest, "invalid reload diagnostics report no manifest", failures);
    expect(state.latestDiagnostics().pendingLoadRequestCount == 0, "invalid reload diagnostics clear load requests", failures);

    std::remove(path);
}

void testRunLoadJobsSuccess(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_loop_jobs.jsonl";
    writeManifest(path);

    full_engine::TerrainStreamingLoopState state;
    (void)state.reloadManifestAndQueueMissingAssetLoads(path, {});

    CallbackState callback;
    callback.handles = readyHandles();
    full_engine::RendererAssetHandleCatalog destination;
    const full_engine::TerrainManifestAssetLoadJobCoordinatorResult& result =
        state.runAssetLoadJobs(destination, loadCallback, &callback, 3);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::Success, "load jobs success reports success", failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 0, "load jobs success clears load requests", failures);
    expect(state.manifestAssetLoadJobs().jobCount() == 0, "load jobs success clears jobs", failures);
    expect(state.latestDiagnostics().loadJobs.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::Success, "load job diagnostics copy status", failures);
    expect(state.latestDiagnostics().loadJobs.loadConsumed, "load job diagnostics report consumed", failures);
    expect(state.latestDiagnostics().loadJobs.coordinator.finalReadyHandleCount == 3, "load job diagnostics copy ready count", failures);

    std::remove(path);
}

void testRunLoadJobsBlockedPreservesPending(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_loop_jobs_blocked.jsonl";
    writeManifest(path);

    full_engine::TerrainStreamingLoopState state;
    (void)state.reloadManifestAndQueueMissingAssetLoads(path, {});

    CallbackState callback;
    (void)callback.handles.addMeshHandle(asset(10), {10});
    full_engine::RendererAssetHandleCatalog destination;
    const full_engine::TerrainManifestAssetLoadJobCoordinatorResult& result =
        state.runAssetLoadJobs(destination, loadCallback, &callback, 3);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::JobsBlocked, "missing handles block load jobs", failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 3, "blocked jobs preserve load requests", failures);
    expect(state.manifestAssetLoadJobs().jobCount() == 3, "blocked jobs preserve mirrored jobs", failures);
    expect(state.latestDiagnostics().loadJobs.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::JobsBlocked, "blocked diagnostics copy status", failures);
    expect(state.latestDiagnostics().loadJobs.execution.blockedCount > 0, "blocked diagnostics copy blocked count", failures);

    state.clearJobs();
    expect(state.manifestAssetLoadJobs().jobCount() == 0, "clearJobs clears jobs", failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 3, "clearJobs preserves load requests", failures);
    expect(state.latestDiagnostics().pendingJobCount == 0, "clearJobs refreshes job diagnostics", failures);

    std::remove(path);
}

void testReconcileScheduledLoadJobs(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_loop_reconcile.jsonl";
    writeManifest(path);

    full_engine::TerrainStreamingLoopState state;
    (void)state.reloadManifestAndQueueMissingAssetLoads(path, {});
    const full_engine::TerrainManifestAssetLoadJobScheduleResult& scheduled =
        state.scheduleAssetLoadJobs();
    expect(scheduled.status == full_engine::TerrainManifestAssetLoadJobScheduleStatus::Scheduled, "loop schedule load jobs succeeds", failures);
    expect(state.manifestAssetLoadJobs().jobCount() == 3, "loop schedule load jobs queues jobs", failures);

    full_engine::RendererAssetHandleCatalog destination;
    const full_engine::TerrainManifestAssetLoadJobReconcileResult& result =
        state.reconcileScheduledAssetLoadJobs(readyHandles(), destination);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobReconcileStatus::Success, "loop reconcile succeeds", failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 0, "loop reconcile clears load requests", failures);
    expect(state.manifestAssetLoadJobs().jobCount() == 0, "loop reconcile clears scheduled jobs", failures);
    expect(state.latestLoadJobReconcileResult().status == full_engine::TerrainManifestAssetLoadJobReconcileStatus::Success, "loop reconcile stores latest result", failures);
    expect(state.latestDiagnostics().reconciledLoadJobs.status == full_engine::TerrainManifestAssetLoadJobReconcileStatus::Success, "loop reconcile stores diagnostics status", failures);
    expect(state.latestDiagnostics().reconciledLoadJobs.loadConsumed, "loop reconcile diagnostics report consumed", failures);
    expect(state.latestDiagnostics().reconciledLoadJobs.reconcile.finalReadyHandleCount == 3, "loop reconcile diagnostics copy ready count", failures);

    (void)state.reloadManifestAndQueueMissingAssetLoads(path, {});
    (void)state.scheduleAssetLoadJobs();
    full_engine::RendererAssetHandleCatalog incomplete = readyHandles();
    (void)incomplete.removeTextureHandle(asset(30));
    full_engine::RendererAssetHandleCatalog blockedDestination;
    const full_engine::TerrainManifestAssetLoadJobReconcileResult& blocked =
        state.reconcileScheduledAssetLoadJobs(incomplete, blockedDestination);

    expect(blocked.status == full_engine::TerrainManifestAssetLoadJobReconcileStatus::CompletionPending, "loop reconcile missing handle is pending", failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 3, "loop reconcile missing handle preserves load requests", failures);
    expect(state.manifestAssetLoadJobs().jobCount() == 3, "loop reconcile missing handle preserves jobs", failures);
    expect(state.latestDiagnostics().reconciledLoadJobs.status == full_engine::TerrainManifestAssetLoadJobReconcileStatus::CompletionPending, "loop reconcile missing handle stores diagnostics", failures);

    state.clearJobs();
    expect(state.latestDiagnostics().reconciledLoadJobs.status == full_engine::TerrainManifestAssetLoadJobReconcileStatus::NoPendingLoads, "clearJobs resets reconcile diagnostics", failures);

    (void)state.scheduleAssetLoadJobs();
    expect(state.manifestAssetLoadJobs().jobCount() == 3, "loop schedule after clearJobs restores jobs", failures);
    state.clearManifest();
    expect(state.latestDiagnostics().reconciledLoadJobs.status == full_engine::TerrainManifestAssetLoadJobReconcileStatus::NoPendingLoads, "clearManifest resets reconcile diagnostics", failures);

    std::remove(path);
}

void testStreamingUpdateQueuesRuntimeIntent(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingLoopState state;
    state.manifestLoad().setManifest(validManifest());

    full_engine::TerrainRuntimeState terrainRuntime;
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    const full_engine::WorldChunkDesc world = worldDesc();
    const full_engine::TerrainRuntimeStateSnapshot current = missingSnapshot();

    const full_engine::TerrainStreamingManifestUpdateResult& result =
        state.updateStreamingFromManifest(
            readyHandles(),
            registry,
            worldCatalog,
            resources,
            &world,
            1,
            terrainRuntime,
            streamingConfig(),
            {0.0, 0.0, 0.0},
            current);

    expect(result.status == full_engine::TerrainStreamingManifestUpdateStatus::Success, "streaming update succeeds", failures);
    expect(terrainRuntime.setupRequestCount() == 1, "streaming update queues setup add", failures);
    expect(terrainRuntime.residencyRequestCount() == 1, "streaming update queues make resident", failures);
    expect(state.streamingRuntime().hasLatestPlan(), "streaming update retains plan", failures);
    expect(state.latestDiagnostics().latestStreamingStatus == full_engine::TerrainStreamingManifestUpdateStatus::Success, "streaming diagnostics copy status", failures);
    expect(state.latestDiagnostics().latestStreamingSummary.queuedSetupAddCount == 1, "streaming diagnostics copy queued setup", failures);
}

void testTickHistoryRetainsRecentEvents(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingLoopState state;

    for (std::size_t index = 0; index < full_engine::kTerrainStreamingTickHistoryCapacity + 3; ++index)
    {
        full_engine::TerrainStreamingTickEvent event;
        event.streamingStatus = index % 2 == 0
            ? full_engine::TerrainStreamingManifestUpdateStatus::Success
            : full_engine::TerrainStreamingManifestUpdateStatus::AssetLoadsPending;
        event.runtimeUpdateRan = index % 3 == 0;
        event.streaming.deferredSetupAddCount = index;
        event.streamingQueue.deferredMakeResidentCount = index + 1;
        event.runtimeLifecycle.deferredCreateCount = index + 2;
        state.appendTickEvent(event);
    }

    expect(
        state.tickHistoryCount() == full_engine::kTerrainStreamingTickHistoryCapacity,
        "tick history retains fixed capacity",
        failures);
    expect(
        state.latestDiagnostics().tickHistoryCount == full_engine::kTerrainStreamingTickHistoryCapacity,
        "tick history diagnostics copy count",
        failures);
    expect(
        state.latestDiagnostics().adaptiveBudget.profile == full_engine::TerrainStreamingBudgetProfile::CatchUp,
        "tick history diagnostics copy adaptive profile",
        failures);
    expect(
        state.latestDiagnostics().adaptiveBudget.inspectedTickCount == 5,
        "tick history diagnostics copy adaptive inspected count",
        failures);
    expect(
        state.latestDiagnostics().adaptiveBudget.deferredWorkCount > 0,
        "tick history diagnostics copy adaptive pressure",
        failures);

    const std::vector<full_engine::TerrainStreamingTickEvent> events = state.tickEvents();
    expect(events.size() == full_engine::kTerrainStreamingTickHistoryCapacity, "tick history event snapshot size matches count", failures);
    expect(events.front().sequence == 4, "tick history drops oldest overflow events", failures);
    expect(
        events.back().sequence == full_engine::kTerrainStreamingTickHistoryCapacity + 3,
        "tick history keeps newest sequence",
        failures);
    expect(events.front().streaming.deferredSetupAddCount == 3, "oldest retained event keeps copied streaming counters", failures);
    expect(state.latestTickEvent() != nullptr, "tick history latest event exists", failures);
    if (state.latestTickEvent() != nullptr)
    {
        expect(
            state.latestTickEvent()->runtimeLifecycle.deferredCreateCount ==
                full_engine::kTerrainStreamingTickHistoryCapacity + 4,
            "latest tick stores copied lifecycle counters",
            failures);
    }

    state.clearTickHistory();
    expect(state.tickHistoryCount() == 0, "clearTickHistory clears event count", failures);
    expect(state.latestTickEvent() == nullptr, "clearTickHistory clears latest pointer", failures);
    expect(state.latestDiagnostics().tickHistoryCount == 0, "clearTickHistory refreshes diagnostics", failures);
    expect(
        state.latestDiagnostics().adaptiveBudget.profile == full_engine::TerrainStreamingBudgetProfile::Balanced,
        "clearTickHistory refreshes adaptive diagnostics",
        failures);

    full_engine::TerrainStreamingTickEvent event;
    state.appendTickEvent(event);
    expect(state.latestTickEvent() != nullptr && state.latestTickEvent()->sequence == 1, "tick history clear restarts sequence", failures);
    expect(
        state.latestDiagnostics().adaptiveBudget.profile == full_engine::TerrainStreamingBudgetProfile::Conservative,
        "zero-pressure tick selects conservative diagnostics",
        failures);
}

void testClearResetsOwnedState(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_loop_clear.jsonl";
    writeManifest(path);

    full_engine::TerrainStreamingLoopState state;
    (void)state.reloadManifestAndQueueMissingAssetLoads(path, {});
    state.clearManifest();

    expect(!state.manifestLoad().hasManifest(), "clearManifest clears manifest", failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 0, "clearManifest clears load requests", failures);
    expect(state.manifestAssetLoadJobs().jobCount() == 0, "clearManifest clears jobs", failures);
    expect(!state.streamingRuntime().hasLatestPlan(), "clearManifest clears streaming plan", failures);
    expect(state.tickHistoryCount() == 0, "clearManifest clears tick history", failures);

    (void)state.reloadManifestAndQueueMissingAssetLoads(path, {});
    state.clear();
    expect(!state.manifestLoad().hasManifest(), "clear clears manifest", failures);
    expect(state.latestDiagnostics().pendingLoadRequestCount == 0, "clear refreshes diagnostics", failures);
    expect(state.latestDiagnostics().pendingJobCount == 0, "clear clears diagnostic job count", failures);
    expect(state.latestDiagnostics().tickHistoryCount == 0, "clear clears diagnostic tick count", failures);

    std::remove(path);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testDefaultState(failures);
    testReloadSuccessQueuesMissingLoads(failures);
    testReloadFailureClearsState(failures);
    testRunLoadJobsSuccess(failures);
    testRunLoadJobsBlockedPreservesPending(failures);
    testReconcileScheduledLoadJobs(failures);
    testStreamingUpdateQueuesRuntimeIntent(failures);
    testTickHistoryRetainsRecentEvents(failures);
    testClearResetsOwnedState(failures);

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
