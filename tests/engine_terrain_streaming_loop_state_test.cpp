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

full_engine::TerrainManifestAssetLoadJobCompletion completion(
    const std::uint64_t id,
    const full_engine::AssetKind kind,
    const std::uint32_t handleValue)
{
    full_engine::TerrainManifestAssetLoadJobCompletion result;
    result.request.id = asset(id);
    result.request.kind = kind;
    result.output.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded;
    switch (kind)
    {
    case full_engine::AssetKind::Mesh:
        result.output.mesh = {handleValue};
        break;
    case full_engine::AssetKind::Material:
        result.output.material = {handleValue};
        break;
    case full_engine::AssetKind::Texture:
        result.output.texture = {handleValue};
        break;
    case full_engine::AssetKind::Unknown:
    case full_engine::AssetKind::TerrainChunk:
    case full_engine::AssetKind::Skeleton:
    case full_engine::AssetKind::SkinnedMesh:
    case full_engine::AssetKind::Shader:
        break;
    }
    return result;
}

std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> readyCompletions()
{
    return {
        completion(10, full_engine::AssetKind::Mesh, 10),
        completion(20, full_engine::AssetKind::Material, 20),
        completion(30, full_engine::AssetKind::Texture, 30)};
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
    expect(state.manifestAssetLoadService().requestCount() == 0, "default loop has no retained service work", failures);
    expect(state.externalLoadCompletions().completionCount() == 0, "default loop has no external completions", failures);
    expect(state.latestLoadServiceWorkPackets().packets.empty(), "default loop has no service packets", failures);
    expect(state.latestLoadServiceTickResult().summary.attemptedCount == 0, "default loop has no service tick attempts", failures);
    expect(state.latestDiagnostics().loadService.workPackets.packetizedCount == 0, "default diagnostics have no service packets", failures);
    expect(state.latestDiagnostics().loadService.retainedRequestCount == 0, "default diagnostics have no retained service work", failures);
    expect(
        state.latestDiagnostics().loadService.completionReconcileStatus ==
            full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::NoPendingLoads,
        "default diagnostics have no service completion reconcile",
        failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 0, "default loop has no load requests", failures);
    expect(!state.latestDiagnostics().hasManifest, "default diagnostics report no manifest", failures);
    expect(state.latestDiagnostics().pendingJobCount == 0, "default diagnostics report no jobs", failures);
    expect(state.latestDiagnostics().pendingExternalCompletionCount == 0, "default diagnostics report no external completions", failures);
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
    expect(state.manifestAssetLoadService().requestCount() == 0, "reload clears stale service work", failures);
    expect(state.latestDiagnostics().loadService.retainedRequestCount == 0, "reload clears stale service diagnostics", failures);

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
    expect(state.manifestAssetLoadService().requestCount() == 0, "invalid reload clears service work", failures);
    expect(!state.latestDiagnostics().hasManifest, "invalid reload diagnostics report no manifest", failures);
    expect(state.latestDiagnostics().pendingLoadRequestCount == 0, "invalid reload diagnostics clear load requests", failures);
    expect(state.latestDiagnostics().loadService.retainedRequestCount == 0, "invalid reload diagnostics clear service work", failures);

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

void testExternalCompletionInboxReconcile(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_loop_external_inbox.jsonl";
    writeManifest(path);

    full_engine::TerrainStreamingLoopState state;
    (void)state.reloadManifestAndQueueMissingAssetLoads(path, {});
    (void)state.scheduleAssetLoadJobs();
    const std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> completions =
        readyCompletions();
    const full_engine::TerrainManifestAssetLoadCompletionInboxPublishResult& published =
        state.publishExternalAssetLoadCompletions(completions.data(), completions.size());

    expect(published.summary.publishedCount == 3, "loop external inbox publishes completions", failures);
    expect(state.externalLoadCompletions().completionCount() == 3, "loop external inbox retains completions", failures);
    expect(state.latestDiagnostics().pendingExternalCompletionCount == 3, "loop diagnostics copy external completion count", failures);
    expect(state.latestDiagnostics().externalCompletionPublish.publishedCount == 3, "loop diagnostics copy external publish count", failures);

    full_engine::RendererAssetHandleCatalog destination;
    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult& reconciled =
        state.reconcileExternalAssetLoadCompletions(destination);

    expect(reconciled.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::Success, "loop external inbox reconcile succeeds", failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 0, "loop external inbox reconcile clears load requests", failures);
    expect(state.manifestAssetLoadJobs().jobCount() == 0, "loop external inbox reconcile removes jobs", failures);
    expect(state.externalLoadCompletions().completionCount() == 0, "loop external inbox reconcile clears inbox", failures);
    expect(state.latestDiagnostics().pendingExternalCompletionCount == 0, "loop external inbox reconcile refreshes diagnostics", failures);
    expect(destination.findMeshHandle(asset(10)) != nullptr, "loop external inbox reconcile publishes mesh", failures);
    expect(destination.findMaterialHandle(asset(20)) != nullptr, "loop external inbox reconcile publishes material", failures);
    expect(destination.findTextureHandle(asset(30)) != nullptr, "loop external inbox reconcile publishes texture", failures);

    std::remove(path);
}

void testExternalCompletionInboxBlockedAndCleared(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_loop_external_inbox_blocked.jsonl";
    writeManifest(path);

    full_engine::TerrainStreamingLoopState state;
    (void)state.reloadManifestAndQueueMissingAssetLoads(path, {});
    (void)state.scheduleAssetLoadJobs();
    const full_engine::TerrainManifestAssetLoadJobCompletion meshOnly =
        completion(10, full_engine::AssetKind::Mesh, 10);
    (void)state.publishExternalAssetLoadCompletion(meshOnly);

    full_engine::RendererAssetHandleCatalog destination;
    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult& blocked =
        state.reconcileExternalAssetLoadCompletions(destination);

    expect(blocked.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPending, "loop external inbox partial reconcile is pending", failures);
    expect(state.externalLoadCompletions().completionCount() == 1, "loop external inbox blocked reconcile preserves completion", failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 3, "loop external inbox blocked reconcile preserves load requests", failures);
    expect(state.manifestAssetLoadJobs().jobCount() == 3, "loop external inbox blocked reconcile preserves jobs", failures);
    expect(destination.meshHandleCount() == 0, "loop external inbox blocked reconcile leaves destination unchanged", failures);

    state.clearJobs();
    expect(state.externalLoadCompletions().completionCount() == 0, "clearJobs clears external completion inbox", failures);
    expect(state.latestDiagnostics().pendingExternalCompletionCount == 0, "clearJobs refreshes external completion diagnostics", failures);

    (void)state.reloadManifestAndQueueMissingAssetLoads(path, {});
    (void)state.scheduleAssetLoadJobs();
    (void)state.publishExternalAssetLoadCompletion(meshOnly);
    state.clearManifest();
    expect(state.externalLoadCompletions().completionCount() == 0, "clearManifest clears external completion inbox", failures);

    std::remove(path);
}

void testRetainedLoadServiceReconcilesScheduledJobs(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_loop_service_reconcile.jsonl";
    writeManifest(path);

    full_engine::TerrainStreamingLoopState state;
    (void)state.reloadManifestAndQueueMissingAssetLoads(path, {});
    (void)state.scheduleAssetLoadJobs();

    const full_engine::TerrainManifestAssetLoadServiceEnqueueResult& enqueued =
        state.enqueueScheduledAssetLoadWork();
    CallbackState callback;
    callback.handles = readyHandles();
    const full_engine::TerrainManifestAssetLoadServiceTickResult& tick =
        state.tickAssetLoadService(8, loadCallback, &callback);
    full_engine::RendererAssetHandleCatalog destination;
    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult& reconciled =
        state.reconcileAssetLoadServiceCompletions(destination);

    expect(enqueued.summary.queuedCount == 3, "loop service enqueues scheduled work", failures);
    expect(state.latestLoadServiceWorkPackets().summary.packetizedCount == 3, "loop service stores packet diagnostics", failures);
    expect(tick.summary.loadedCount == 3, "loop service tick loads three requests", failures);
    expect(state.manifestAssetLoadService().completedCount() == 3, "loop service retains completed records", failures);
    expect(state.latestDiagnostics().loadService.workPackets.packetizedCount == 3, "loop service diagnostics copy packet count", failures);
    expect(state.latestDiagnostics().loadService.enqueue.queuedCount == 3, "loop service diagnostics copy enqueue count", failures);
    expect(state.latestDiagnostics().loadService.tick.loadedCount == 3, "loop service diagnostics copy loaded count", failures);
    expect(state.latestDiagnostics().loadService.retainedCompletedCount == 3, "loop service diagnostics copy completed retained count", failures);
    expect(reconciled.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::Success, "loop service completion reconcile succeeds", failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 0, "loop service reconcile clears load requests", failures);
    expect(state.manifestAssetLoadJobs().jobCount() == 0, "loop service reconcile removes scheduled jobs", failures);
    expect(state.manifestAssetLoadService().completions().empty(), "loop service reconcile clears emitted completions", failures);
    expect(state.latestDiagnostics().loadService.retainedCompletionCount == 0, "loop service diagnostics refresh completion count", failures);
    expect(
        state.latestDiagnostics().loadService.completionReconcileStatus ==
            full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::Success,
        "loop service diagnostics copy completion reconcile status",
        failures);
    expect(state.latestDiagnostics().loadService.completionPublish.publishedCount == 3, "loop service diagnostics copy publish count", failures);
    expect(state.latestDiagnostics().loadService.completionReconcile.finalReadyHandleCount == 3, "loop service diagnostics copy nested reconcile count", failures);
    expect(state.latestLoadJobReconcileResult().status == full_engine::TerrainManifestAssetLoadJobReconcileStatus::Success, "loop service reconcile updates lower-level reconcile result", failures);
    expect(state.latestDiagnostics().reconciledLoadJobs.status == full_engine::TerrainManifestAssetLoadJobReconcileStatus::Success, "loop service reconcile updates retained diagnostics", failures);
    expect(state.latestDiagnostics().reconciledLoadJobs.reconcile.finalReadyHandleCount == 3, "loop service reconcile diagnostics copy ready count", failures);
    expect(destination.findMeshHandle(asset(10)) != nullptr, "loop service reconcile publishes mesh destination", failures);
    expect(destination.findMaterialHandle(asset(20)) != nullptr, "loop service reconcile publishes material destination", failures);
    expect(destination.findTextureHandle(asset(30)) != nullptr, "loop service reconcile publishes texture destination", failures);

    std::remove(path);
}

void testRetainedLoadServiceMissingAndFailedCallbacks(std::vector<std::string>& failures)
{
    const char* path = "terrain_streaming_loop_service_blocked.jsonl";
    writeManifest(path);

    full_engine::TerrainStreamingLoopState state;
    (void)state.reloadManifestAndQueueMissingAssetLoads(path, {});
    (void)state.scheduleAssetLoadJobs();
    (void)state.enqueueScheduledAssetLoadWork();

    CallbackState missing;
    missing.handles = readyHandles();
    (void)missing.handles.removeMaterialHandle(asset(20));
    const full_engine::TerrainManifestAssetLoadServiceTickResult& missingTick =
        state.tickAssetLoadService(8, loadCallback, &missing);

    expect(missingTick.summary.loadedCount == 2, "loop service missing tick loads available handles", failures);
    expect(missingTick.summary.missingCount == 1, "loop service missing tick counts missing handle", failures);
    expect(state.manifestAssetLoadService().pendingCount() == 1, "loop service missing tick keeps one pending record", failures);
    expect(state.manifestAssetLoadService().completions().size() == 2, "loop service missing tick emits only loaded completions", failures);
    expect(state.latestDiagnostics().loadService.tick.missingCount == 1, "loop service diagnostics copy missing tick count", failures);
    expect(state.latestDiagnostics().loadService.retainedPendingCount == 1, "loop service diagnostics copy pending retained count", failures);
    expect(state.latestDiagnostics().loadService.retainedCompletionCount == 2, "loop service diagnostics copy emitted completions", failures);

    full_engine::RendererAssetHandleCatalog missingDestination;
    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult& missingReconcile =
        state.reconcileAssetLoadServiceCompletions(missingDestination);

    expect(missingReconcile.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPending, "loop service missing completion reconcile stays pending", failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 3, "loop service missing reconcile preserves load requests", failures);
    expect(state.manifestAssetLoadJobs().jobCount() == 3, "loop service missing reconcile preserves jobs", failures);
    expect(missingDestination.meshHandleCount() == 0, "loop service missing reconcile leaves destination unchanged", failures);
    expect(
        state.latestDiagnostics().loadService.completionReconcileStatus ==
            full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPending,
        "loop service diagnostics copy pending completion reconcile",
        failures);

    state.clearJobs();
    (void)state.scheduleAssetLoadJobs();
    (void)state.enqueueScheduledAssetLoadWork();
    CallbackState failed;
    failed.handles = readyHandles();
    failed.failMaterials = true;
    const full_engine::TerrainManifestAssetLoadServiceTickResult& failedTick =
        state.tickAssetLoadService(8, loadCallback, &failed);
    full_engine::RendererAssetHandleCatalog failedDestination;
    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult& failedReconcile =
        state.reconcileAssetLoadServiceCompletions(failedDestination);

    expect(failedTick.summary.failedCount == 1, "loop service failed tick counts failed callback", failures);
    expect(state.manifestAssetLoadService().failedCount() == 1, "loop service failed tick retains failure", failures);
    expect(failedReconcile.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPublishFailed, "loop service failed completion blocks publish", failures);
    expect(state.manifestLoad().pendingLoadRequestCount() == 3, "loop service failed reconcile preserves load requests", failures);
    expect(state.manifestAssetLoadJobs().jobCount() == 3, "loop service failed reconcile preserves jobs", failures);
    expect(failedDestination.materialHandleCount() == 0, "loop service failed reconcile leaves destination unchanged", failures);
    expect(state.latestDiagnostics().loadService.tick.failedCount == 1, "loop service diagnostics copy failed tick count", failures);
    expect(state.latestDiagnostics().loadService.retainedFailedCount == 1, "loop service diagnostics copy retained failed count", failures);
    expect(
        state.latestDiagnostics().loadService.completionReconcileStatus ==
            full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPublishFailed,
        "loop service diagnostics copy failed completion reconcile",
        failures);

    state.clearJobs();
    expect(state.manifestAssetLoadService().requestCount() == 0, "clearJobs clears retained service records", failures);
    expect(state.latestLoadServiceTickResult().summary.attemptedCount == 0, "clearJobs resets service tick diagnostics", failures);
    expect(state.latestDiagnostics().loadService.retainedRequestCount == 0, "clearJobs resets service diagnostics", failures);
    expect(state.latestDiagnostics().loadService.tick.attemptedCount == 0, "clearJobs resets service tick diagnostics snapshot", failures);

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
    expect(state.manifestAssetLoadService().requestCount() == 0, "clearManifest clears service work", failures);
    expect(state.latestDiagnostics().loadService.retainedRequestCount == 0, "clearManifest clears service diagnostics", failures);
    expect(!state.streamingRuntime().hasLatestPlan(), "clearManifest clears streaming plan", failures);
    expect(state.tickHistoryCount() == 0, "clearManifest clears tick history", failures);

    (void)state.reloadManifestAndQueueMissingAssetLoads(path, {});
    state.clear();
    expect(!state.manifestLoad().hasManifest(), "clear clears manifest", failures);
    expect(state.latestDiagnostics().pendingLoadRequestCount == 0, "clear refreshes diagnostics", failures);
    expect(state.latestDiagnostics().pendingJobCount == 0, "clear clears diagnostic job count", failures);
    expect(state.manifestAssetLoadService().requestCount() == 0, "clear clears service work", failures);
    expect(state.latestDiagnostics().loadService.retainedRequestCount == 0, "clear clears service diagnostics", failures);
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
    testExternalCompletionInboxReconcile(failures);
    testExternalCompletionInboxBlockedAndCleared(failures);
    testRetainedLoadServiceReconcilesScheduledJobs(failures);
    testRetainedLoadServiceMissingAndFailedCallbacks(failures);
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
