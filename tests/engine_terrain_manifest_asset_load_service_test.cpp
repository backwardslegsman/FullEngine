#include "engine/renderer_integration/TerrainManifestAssetLoadJobCompletions.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadService.hpp"
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

full_engine::AssetRecord assetRecord(
    const std::uint64_t id,
    const full_engine::AssetKind kind) noexcept
{
    full_engine::AssetRecord record;
    record.id = asset(id);
    record.kind = kind;
    return record;
}

full_engine::TerrainManifestAssetLoadRequest request(
    const std::uint64_t id,
    const full_engine::AssetKind kind) noexcept
{
    full_engine::TerrainManifestAssetLoadRequest result;
    result.id = asset(id);
    result.kind = kind;
    return result;
}

full_engine::TerrainManifestAssetLoadJobWorkPacket packet(
    const std::uint64_t id,
    const full_engine::AssetKind kind,
    const bool mismatchedJobId = false) noexcept
{
    full_engine::TerrainManifestAssetLoadJobWorkPacket result;
    result.request = request(id, kind);
    result.jobId = mismatchedJobId
        ? full_engine::EngineJobId{9999, 0}
        : full_engine::engineJobIdForTerrainManifestAssetLoadRequest(result.request);
    result.priority = full_engine::EngineJobPriority::Normal;
    return result;
}

full_engine::TerrainChunkAssetDesc terrainAssets()
{
    full_engine::TerrainChunkAssetDesc desc;
    desc.id = {1, 0, 0};
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
    result.terrainChunks.push_back(terrainAssets());
    return result;
}

full_engine::RendererAssetHandleCatalog completeHandles()
{
    full_engine::RendererAssetHandleCatalog handles;
    (void)handles.addMeshHandle(asset(1), {10});
    (void)handles.addMaterialHandle(asset(2), {20});
    (void)handles.addTextureHandle(asset(3), {30});
    return handles;
}

void queueManifestLoadRequests(full_engine::TerrainManifestLoadState& state)
{
    state.setManifest(manifest());
    (void)state.planAssetReadiness({});
    (void)state.planAssetLoadRequests();
    (void)state.queueLatestAssetLoadRequests();
}

full_engine::EngineJobQueue schedulePendingJobs(full_engine::TerrainManifestLoadState& state)
{
    full_engine::EngineJobQueue jobs;
    const full_engine::TerrainManifestAssetLoadJobScheduleResult scheduled =
        full_engine::scheduleTerrainManifestAssetLoadJobs(state, jobs);
    (void)scheduled;
    return jobs;
}

struct CallbackState
{
    full_engine::RendererAssetHandleCatalog handles;
    std::uint64_t missingAsset = 0;
    std::uint64_t failedAsset = 0;
    std::uint64_t invalidLoadedAsset = 0;
    std::vector<std::uint64_t> calls;
};

full_engine::TerrainManifestAssetLoadCallbackResult callbackFromState(
    const full_engine::TerrainManifestAssetLoadRequest& request,
    void* const userData)
{
    CallbackState& state = *static_cast<CallbackState*>(userData);
    state.calls.push_back(request.id.value);

    full_engine::TerrainManifestAssetLoadCallbackResult result;
    if (request.id.value == state.missingAsset)
    {
        result.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Missing;
        return result;
    }
    if (request.id.value == state.failedAsset)
    {
        result.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Failed;
        return result;
    }

    result.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded;
    if (request.id.value == state.invalidLoadedAsset)
    {
        return result;
    }

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

full_engine::TerrainManifestAssetLoadService serviceFromScheduledJobs(
    full_engine::TerrainManifestLoadState& state,
    full_engine::EngineJobQueue& jobs)
{
    full_engine::TerrainManifestAssetLoadService service;
    jobs = schedulePendingJobs(state);
    const full_engine::TerrainManifestAssetLoadJobWorkPacketResult packets =
        full_engine::buildTerrainManifestAssetLoadJobWorkPackets(jobs);
    (void)service.enqueueWorkPackets(packets);
    return service;
}

void testDefaultServiceIsEmpty(std::vector<std::string>& failures)
{
    const full_engine::TerrainManifestAssetLoadService service;

    expect(service.requestCount() == 0, "default service has no requests", failures);
    expect(service.pendingCount() == 0, "default service has no pending work", failures);
    expect(service.completedCount() == 0, "default service has no completed work", failures);
    expect(service.failedCount() == 0, "default service has no failed work", failures);
    expect(service.completions().empty(), "default service has no completions", failures);
    expect(std::string(full_engine::terrainManifestAssetLoadServiceTickStatusName(
               full_engine::TerrainManifestAssetLoadServiceTickStatus::Idle)) == "Idle", "tick status name is stable", failures);
}

void testValidPacketsEnqueueAndDeduplicate(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadService service;
    const full_engine::TerrainManifestAssetLoadJobWorkPacket packets[] = {
        packet(1, full_engine::AssetKind::Mesh),
        packet(2, full_engine::AssetKind::Material),
        packet(3, full_engine::AssetKind::Texture),
        packet(1, full_engine::AssetKind::Mesh),
    };

    const full_engine::TerrainManifestAssetLoadServiceEnqueueResult result =
        service.enqueueWorkPackets(packets, 4);

    expect(result.records.size() == 4, "enqueue emits one record per packet", failures);
    expect(result.summary.queuedCount == 3, "enqueue queues three unique packets", failures);
    expect(result.summary.alreadyQueuedCount == 1, "enqueue deduplicates repeated packet", failures);
    expect(result.summary.invalidPacketCount == 0, "valid enqueue reports no invalid packets", failures);
    expect(service.requestCount() == 3, "service retains three unique requests", failures);
    expect(service.pendingCount() == 3, "service starts retained requests pending", failures);
    expect(service.records()[0].packet.request.kind == full_engine::AssetKind::Mesh, "enqueue preserves mesh order", failures);
    expect(service.records()[1].packet.request.kind == full_engine::AssetKind::Material, "enqueue preserves material order", failures);
    expect(service.records()[2].packet.request.kind == full_engine::AssetKind::Texture, "enqueue preserves texture order", failures);
    expect(std::string(full_engine::terrainManifestAssetLoadServiceEnqueueStatusName(result.records[0].status)) == "Queued", "enqueue status name is stable", failures);
}

void testInvalidPacketsDoNotMutateService(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadService service;
    const full_engine::TerrainManifestAssetLoadJobWorkPacket packets[] = {
        packet(0, full_engine::AssetKind::Mesh),
        packet(4, full_engine::AssetKind::Shader),
        packet(5, full_engine::AssetKind::Texture, true),
    };

    const full_engine::TerrainManifestAssetLoadServiceEnqueueResult result =
        service.enqueueWorkPackets(packets, 3);

    expect(result.summary.invalidPacketCount == 3, "invalid packets are counted", failures);
    expect(result.summary.queuedCount == 0, "invalid packets queue nothing", failures);
    expect(service.requestCount() == 0, "invalid packets do not mutate service", failures);

    const full_engine::TerrainManifestAssetLoadServiceEnqueueResult nullResult =
        service.enqueueWorkPackets(nullptr, 2);
    expect(nullResult.summary.invalidPacketCount == 2, "null nonempty packet array reports invalid", failures);
    expect(service.requestCount() == 0, "null nonempty packet array does not mutate service", failures);
}

void testZeroBudgetAndNullCallbackDoNotProgress(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadService service;
    const full_engine::TerrainManifestAssetLoadJobWorkPacket mesh = packet(1, full_engine::AssetKind::Mesh);
    (void)service.enqueueWorkPackets(&mesh, 1);

    CallbackState state;
    state.handles = completeHandles();
    const full_engine::TerrainManifestAssetLoadServiceTickResult zeroBudget =
        service.tick(0, callbackFromState, &state);
    const full_engine::TerrainManifestAssetLoadServiceTickResult nullCallback =
        service.tick(1, nullptr, nullptr);

    expect(zeroBudget.status == full_engine::TerrainManifestAssetLoadServiceTickStatus::Idle, "zero budget tick is idle", failures);
    expect(nullCallback.status == full_engine::TerrainManifestAssetLoadServiceTickStatus::Blocked, "null callback blocks pending work", failures);
    expect(state.calls.empty(), "zero budget does not invoke callback", failures);
    expect(service.pendingCount() == 1, "blocked tick preserves pending work", failures);
    expect(service.completions().empty(), "blocked tick emits no completions", failures);
}

void testLoadedResultsProduceCompletions(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadService service;
    const full_engine::TerrainManifestAssetLoadJobWorkPacket packets[] = {
        packet(1, full_engine::AssetKind::Mesh),
        packet(2, full_engine::AssetKind::Material),
        packet(3, full_engine::AssetKind::Texture),
    };
    (void)service.enqueueWorkPackets(packets, 3);

    CallbackState state;
    state.handles = completeHandles();
    const full_engine::TerrainManifestAssetLoadServiceTickResult result =
        service.tick(8, callbackFromState, &state);

    expect(result.status == full_engine::TerrainManifestAssetLoadServiceTickStatus::Progressed, "loaded tick progresses", failures);
    expect(result.summary.attemptedCount == 3, "loaded tick attempts three requests", failures);
    expect(result.summary.loadedCount == 3, "loaded tick counts three loaded handles", failures);
    expect(result.summary.completedCount == 3, "loaded tick completes retained requests", failures);
    expect(result.summary.pendingCount == 0, "loaded tick leaves no pending requests", failures);
    expect(service.completedCount() == 3, "service retained completed count is three", failures);
    expect(service.completions().size() == 3, "loaded tick emits three completions", failures);
    expect(service.completions()[0].request.kind == full_engine::AssetKind::Mesh, "completion order preserves mesh", failures);
    expect(service.completions()[1].request.kind == full_engine::AssetKind::Material, "completion order preserves material", failures);
    expect(service.completions()[2].request.kind == full_engine::AssetKind::Texture, "completion order preserves texture", failures);
}

void testMissingResultsStayPendingAndLaterSucceed(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadService service;
    const full_engine::TerrainManifestAssetLoadJobWorkPacket mesh = packet(1, full_engine::AssetKind::Mesh);
    (void)service.enqueueWorkPackets(&mesh, 1);

    CallbackState state;
    state.handles = completeHandles();
    state.missingAsset = 1;
    const full_engine::TerrainManifestAssetLoadServiceTickResult missing =
        service.tick(1, callbackFromState, &state);
    const bool noCompletionAfterMissing = service.completions().empty();
    state.missingAsset = 0;
    const full_engine::TerrainManifestAssetLoadServiceTickResult loaded =
        service.tick(1, callbackFromState, &state);

    expect(missing.summary.missingCount == 1, "missing tick counts one missing request", failures);
    expect(missing.summary.pendingCount == 1, "missing tick leaves request pending", failures);
    expect(noCompletionAfterMissing, "missing tick emits no completions", failures);
    expect(loaded.summary.loadedCount == 1, "later tick loads pending request", failures);
    expect(service.completedCount() == 1, "later tick completes request", failures);
    expect(service.completions().size() == 1, "later tick emits completion", failures);
    expect(service.records()[0].attemptCount == 2, "service tracks both attempts", failures);
}

void testFailedResultsCanBeRetried(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadService service;
    const full_engine::TerrainManifestAssetLoadJobWorkPacket material = packet(2, full_engine::AssetKind::Material);
    (void)service.enqueueWorkPackets(&material, 1);

    CallbackState state;
    state.handles = completeHandles();
    state.failedAsset = 2;
    const full_engine::TerrainManifestAssetLoadServiceTickResult failed =
        service.tick(1, callbackFromState, &state);
    const std::size_t completionCountAfterFailed = service.completions().size();
    const std::size_t retried = service.retryFailed();
    state.failedAsset = 0;
    const full_engine::TerrainManifestAssetLoadServiceTickResult loaded =
        service.tick(1, callbackFromState, &state);

    expect(failed.summary.failedCount == 1, "failed tick counts failed request", failures);
    expect(failed.summary.retainedFailedCount == 1, "failed tick retains failure", failures);
    expect(completionCountAfterFailed == 1, "failed tick emits failed completion", failures);
    expect(service.completions()[0].output.status == full_engine::TerrainManifestAssetLoadCallbackStatus::Failed, "failed completion preserves failed callback", failures);
    expect(retried == 1, "retryFailed resets one retained failure", failures);
    expect(loaded.summary.loadedCount == 1, "retried request can load", failures);
    expect(service.completedCount() == 1, "retried request completes", failures);
    expect(service.completions().size() == 2, "retry load appends second completion", failures);
}

void testInvalidLoadedHandleIsFailedWork(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadService service;
    const full_engine::TerrainManifestAssetLoadJobWorkPacket texture = packet(3, full_engine::AssetKind::Texture);
    (void)service.enqueueWorkPackets(&texture, 1);

    CallbackState state;
    state.handles = completeHandles();
    state.invalidLoadedAsset = 3;
    const full_engine::TerrainManifestAssetLoadServiceTickResult result =
        service.tick(1, callbackFromState, &state);

    expect(result.summary.failedCount == 1, "loaded invalid handle is failed work", failures);
    expect(service.failedCount() == 1, "invalid loaded handle leaves failed retained state", failures);
    expect(service.completions().size() == 1, "invalid loaded handle emits blocking completion", failures);
    expect(service.completions()[0].output.status == full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded, "invalid completion preserves loaded callback status", failures);
}

void testClearOperationsResetDocumentedState(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadService service;
    const full_engine::TerrainManifestAssetLoadJobWorkPacket mesh = packet(1, full_engine::AssetKind::Mesh);
    (void)service.enqueueWorkPackets(&mesh, 1);

    CallbackState state;
    state.handles = completeHandles();
    (void)service.tick(1, callbackFromState, &state);
    service.clearCompletions();
    expect(service.requestCount() == 1, "clearCompletions preserves retained records", failures);
    expect(service.completedCount() == 1, "clearCompletions preserves completed status", failures);
    expect(service.completions().empty(), "clearCompletions removes completion output", failures);

    service.clear();
    expect(service.requestCount() == 0, "clear removes retained records", failures);
    expect(service.completions().empty(), "clear removes completions", failures);
}

void testEndToEndPacketsServiceCompletionsReconcileReady(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs;
    full_engine::TerrainManifestAssetLoadService service = serviceFromScheduledJobs(state, jobs);

    CallbackState callbacks;
    callbacks.handles = completeHandles();
    const full_engine::TerrainManifestAssetLoadServiceTickResult tick =
        service.tick(8, callbackFromState, &callbacks);
    full_engine::RendererAssetHandleCatalog destination;
    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult reconcile =
        full_engine::reconcileTerrainManifestAssetLoadJobCompletions(
            state,
            jobs,
            service.completions().data(),
            service.completions().size(),
            destination);

    expect(tick.summary.loadedCount == 3, "end-to-end service loads three requests", failures);
    expect(reconcile.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::Success, "service completions reconcile successfully", failures);
    expect(reconcile.reconcile.summary.removedScheduledJobCount == 3, "service reconcile removes scheduled jobs", failures);
    expect(state.pendingLoadRequestCount() == 0, "service reconcile clears retained load requests", failures);
    expect(jobs.jobCount() == 0, "service reconcile clears scheduled load jobs", failures);
    expect(destination.findMeshHandle(asset(1)) != nullptr, "service reconcile publishes mesh handle", failures);
    expect(destination.findMaterialHandle(asset(2)) != nullptr, "service reconcile publishes material handle", failures);
    expect(destination.findTextureHandle(asset(3)) != nullptr, "service reconcile publishes texture handle", failures);

    const full_engine::TerrainManifestAssetReadinessPlan& readiness = state.planAssetReadiness(destination);
    expect(readiness.summary.readyCount == 3, "service reconcile replans readiness ready", failures);
    const full_engine::TerrainManifestAssetLoadRequestPlan& noMoreLoads = state.planAssetLoadRequests();
    expect(noMoreLoads.summary.requestCount == 0, "service reconcile produces no further load requests", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testDefaultServiceIsEmpty(failures);
    testValidPacketsEnqueueAndDeduplicate(failures);
    testInvalidPacketsDoNotMutateService(failures);
    testZeroBudgetAndNullCallbackDoNotProgress(failures);
    testLoadedResultsProduceCompletions(failures);
    testMissingResultsStayPendingAndLaterSucceed(failures);
    testFailedResultsCanBeRetried(failures);
    testInvalidLoadedHandleIsFailedWork(failures);
    testClearOperationsResetDocumentedState(failures);
    testEndToEndPacketsServiceCompletionsReconcileReady(failures);

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
