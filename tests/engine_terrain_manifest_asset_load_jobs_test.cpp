#include "engine/jobs/JobQueue.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadExecutor.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadJobs.hpp"
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

full_engine::AssetKind kindFromPayload(const std::uint64_t value) noexcept
{
    return static_cast<full_engine::AssetKind>(value);
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

struct FakeLoaderState
{
    full_engine::RendererAssetHandleCatalog handles;
    std::uint64_t failedAsset = 0;
    std::vector<std::uint64_t> jobCalls;
    std::vector<std::uint64_t> executorCalls;
};

bool hasFakeHandle(
    const full_engine::RendererAssetHandleCatalog& handles,
    const full_engine::AssetId id,
    const full_engine::AssetKind kind) noexcept
{
    switch (kind)
    {
    case full_engine::AssetKind::Mesh:
        return handles.findMeshHandle(id) != nullptr;
    case full_engine::AssetKind::Material:
        return handles.findMaterialHandle(id) != nullptr;
    case full_engine::AssetKind::Texture:
        return handles.findTextureHandle(id) != nullptr;
    case full_engine::AssetKind::Unknown:
    case full_engine::AssetKind::TerrainChunk:
    case full_engine::AssetKind::Skeleton:
    case full_engine::AssetKind::SkinnedMesh:
    case full_engine::AssetKind::Shader:
        return false;
    }

    return false;
}

full_engine::EngineJobStatus fakeJobCallback(
    const full_engine::EngineJobRequest& request,
    void* const userData)
{
    FakeLoaderState& state = *static_cast<FakeLoaderState*>(userData);
    state.jobCalls.push_back(request.payload0);

    const full_engine::AssetId id = asset(request.payload0);
    if (id.value == state.failedAsset)
    {
        return full_engine::EngineJobStatus::Failed;
    }

    return hasFakeHandle(state.handles, id, kindFromPayload(request.payload1))
        ? full_engine::EngineJobStatus::Completed
        : full_engine::EngineJobStatus::Blocked;
}

full_engine::TerrainManifestAssetLoadCallbackResult fakeExecutorCallback(
    const full_engine::TerrainManifestAssetLoadRequest& request,
    void* const userData)
{
    FakeLoaderState& state = *static_cast<FakeLoaderState*>(userData);
    state.executorCalls.push_back(request.id.value);

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

void queueManifestLoadRequests(full_engine::TerrainManifestLoadState& state)
{
    state.setManifest(manifest());
    (void)state.planAssetReadiness({});
    (void)state.planAssetLoadRequests();
    (void)state.queueLatestAssetLoadRequests();
}

full_engine::EngineJobQueue mirrorPendingJobs(full_engine::TerrainManifestLoadState& state)
{
    full_engine::EngineJobQueue jobs;
    const full_engine::TerrainManifestAssetLoadJobMirrorResult mirrored =
        full_engine::mirrorTerrainManifestAssetLoadRequestsToJobs(state.loadRequestQueue(), jobs);
    (void)mirrored;
    return jobs;
}

void testJobDrivenLoadReplansReady(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    expect(state.pendingLoadRequestCount() == 3, "load state queues three missing handles", failures);

    full_engine::EngineJobQueue jobs = mirrorPendingJobs(state);
    expect(jobs.jobCount() == 3, "pending load requests mirror into jobs", failures);
    expect(jobs.summary().manifestAssetLoadCount == 3, "mirrored jobs are manifest asset loads", failures);

    FakeLoaderState loader;
    loader.handles = completeHandles();
    const full_engine::EngineJobExecutionResult executed =
        full_engine::runReadyJobs(jobs, 8, fakeJobCallback, &loader);

    expect(executed.summary.completedCount == 3, "all mirrored jobs complete", failures);
    expect(jobs.jobCount() == 0, "completed mirrored jobs are removed", failures);
    expect(loader.jobCalls.size() == 3, "job callback records every load job", failures);
    expect(loader.jobCalls[0] == 1, "job callback preserves mesh order", failures);
    expect(loader.jobCalls[1] == 2, "job callback preserves material order", failures);
    expect(loader.jobCalls[2] == 3, "job callback preserves texture order", failures);

    full_engine::RendererAssetHandleCatalog destination;
    const full_engine::TerrainManifestAssetLoadExecutorResult& consumed =
        state.executePendingAssetLoadRequests(destination, fakeExecutorCallback, &loader);

    expect(consumed.status == full_engine::TerrainManifestAssetLoadExecutorStatus::Consumed, "executor consumes after jobs complete", failures);
    expect(state.pendingLoadRequestCount() == 0, "load request queue drains after executor consume", failures);
    expect(destination.findMeshHandle(asset(1)) != nullptr, "destination receives mesh handle", failures);
    expect(destination.findMaterialHandle(asset(2)) != nullptr, "destination receives material handle", failures);
    expect(destination.findTextureHandle(asset(3)) != nullptr, "destination receives texture handle", failures);

    const full_engine::TerrainManifestAssetReadinessPlan& readiness = state.planAssetReadiness(destination);
    expect(readiness.summary.readyCount == 3, "readiness replans ready after job-driven load", failures);
    const full_engine::TerrainManifestAssetLoadRequestPlan& noMoreLoads = state.planAssetLoadRequests();
    expect(noMoreLoads.summary.requestCount == 0, "ready handles produce no further load requests", failures);
}

void testMissingFakeHandleKeepsQueuesPending(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs = mirrorPendingJobs(state);

    FakeLoaderState loader;
    loader.handles = completeHandles();
    (void)loader.handles.removeMaterialHandle(asset(2));

    const full_engine::EngineJobExecutionResult executed =
        full_engine::runReadyJobs(jobs, 8, fakeJobCallback, &loader);

    expect(executed.summary.completedCount == 2, "available fake handles complete their jobs", failures);
    expect(executed.summary.blockedCount == 1, "missing fake handle blocks one job", failures);
    expect(jobs.jobCount() == 1, "blocked job remains pending", failures);
    expect(state.pendingLoadRequestCount() == 3, "load request queue remains pending after blocked job", failures);

    full_engine::RendererAssetHandleCatalog destination;
    const full_engine::TerrainManifestAssetLoadExecutorResult& consumed =
        state.executePendingAssetLoadRequests(destination, fakeExecutorCallback, &loader);

    expect(consumed.status == full_engine::TerrainManifestAssetLoadExecutorStatus::Blocked, "executor blocks when fake handle remains missing", failures);
    expect(state.pendingLoadRequestCount() == 3, "blocked executor preserves load request queue", failures);
    expect(destination.meshHandleCount() == 0, "blocked executor leaves destination unchanged", failures);
}

void testFailedFakeLoadKeepsReadinessMissing(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs = mirrorPendingJobs(state);

    FakeLoaderState loader;
    loader.handles = completeHandles();
    loader.failedAsset = 2;

    const full_engine::EngineJobExecutionResult executed =
        full_engine::runReadyJobs(jobs, 8, fakeJobCallback, &loader);

    expect(executed.summary.failedCount == 1, "failed fake load records one failed job", failures);
    expect(jobs.jobCount() == 1, "failed job remains pending", failures);

    full_engine::RendererAssetHandleCatalog destination;
    const full_engine::TerrainManifestAssetLoadExecutorResult& consumed =
        state.executePendingAssetLoadRequests(destination, fakeExecutorCallback, &loader);

    expect(consumed.status == full_engine::TerrainManifestAssetLoadExecutorStatus::Blocked, "failed fake load blocks executor", failures);
    expect(state.pendingLoadRequestCount() == 3, "failed fake load preserves load request queue", failures);
    const full_engine::TerrainManifestAssetReadinessPlan& readiness = state.planAssetReadiness(destination);
    expect(readiness.summary.missingHandleCount == 3, "failed fake load prevents readiness from becoming ready", failures);
}

void testAlreadyLoadedDestinationDrainsLoadQueue(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);

    full_engine::RendererAssetHandleCatalog destination = completeHandles();
    const full_engine::TerrainManifestAssetLoadExecutorResult& consumed =
        state.executePendingAssetLoadRequests(destination, nullptr, nullptr);

    expect(consumed.status == full_engine::TerrainManifestAssetLoadExecutorStatus::Consumed, "already-loaded destination consumes queue", failures);
    expect(consumed.consume.summary.alreadyLoadedCount == 3, "already-loaded destination reports existing handles", failures);
    expect(state.pendingLoadRequestCount() == 0, "already-loaded destination drains load queue", failures);
    expect(consumed.callbackRecords.size() == 3, "already-loaded destination records every pending request", failures);
    expect(!consumed.callbackRecords[0].callbackInvoked, "already-loaded destination skips callback", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testJobDrivenLoadReplansReady(failures);
    testMissingFakeHandleKeepsQueuesPending(failures);
    testFailedFakeLoadKeepsReadinessMissing(failures);
    testAlreadyLoadedDestinationDrainsLoadQueue(failures);

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
