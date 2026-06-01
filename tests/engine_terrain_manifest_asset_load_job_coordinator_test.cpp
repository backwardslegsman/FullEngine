#include "engine/renderer_integration/TerrainManifestAssetLoadJobCoordinator.hpp"
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

void queueManifestLoadRequests(full_engine::TerrainManifestLoadState& state)
{
    state.setManifest(manifest());
    (void)state.planAssetReadiness({});
    (void)state.planAssetLoadRequests();
    (void)state.queueLatestAssetLoadRequests();
}

full_engine::EngineJobRequest customJob()
{
    full_engine::EngineJobRequest request;
    request.id = {9000, 0};
    request.kind = full_engine::EngineJobKind::Custom;
    request.priority = full_engine::EngineJobPriority::High;
    request.payload0 = 9000;
    return request;
}

void testNoPendingLoads(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    full_engine::EngineJobQueue jobs;
    full_engine::RendererAssetHandleCatalog destination;
    CallbackState callbackState;

    const full_engine::TerrainManifestAssetLoadJobCoordinatorResult result =
        full_engine::runTerrainManifestAssetLoadJobs(
            state,
            jobs,
            destination,
            callback,
            &callbackState,
            8);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::NoPendingLoads, "no pending loads reports NoPendingLoads", failures);
    expect(jobs.jobCount() == 0, "no pending loads leaves jobs empty", failures);
    expect(destination.meshHandleCount() == 0, "no pending loads leaves destination unchanged", failures);
}

void testPendingLoadsCompleteAndReplanReady(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs;
    full_engine::RendererAssetHandleCatalog destination;
    CallbackState callbackState;
    callbackState.handles = completeHandles();

    const full_engine::TerrainManifestAssetLoadJobCoordinatorResult result =
        full_engine::runTerrainManifestAssetLoadJobs(
            state,
            jobs,
            destination,
            callback,
            &callbackState,
            8);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::Success, "coordinator succeeds for complete handles", failures);
    expect(result.mirror.summary.queuedCount == 3, "coordinator mirrors three jobs", failures);
    expect(result.jobs.summary.completedCount == 3, "coordinator completes three jobs", failures);
    expect(result.load.status == full_engine::TerrainManifestAssetLoadExecutorStatus::Consumed, "coordinator consumes load queue", failures);
    expect(state.pendingLoadRequestCount() == 0, "coordinator clears load queue after consume", failures);
    expect(jobs.jobCount() == 0, "coordinator removes consumed load jobs", failures);
    expect(result.readiness.summary.readyCount == 3, "coordinator replans ready handles", failures);
    expect(result.summary.finalPendingLoadRequestCount == 0, "coordinator summary tracks final pending requests", failures);
    expect(destination.findMeshHandle(asset(1)) != nullptr, "coordinator loads mesh handle", failures);
    expect(destination.findMaterialHandle(asset(2)) != nullptr, "coordinator loads material handle", failures);
    expect(destination.findTextureHandle(asset(3)) != nullptr, "coordinator loads texture handle", failures);
}

void testMissingCallbackHandleBlocks(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs;
    full_engine::RendererAssetHandleCatalog destination;
    CallbackState callbackState;
    callbackState.handles = completeHandles();
    (void)callbackState.handles.removeMaterialHandle(asset(2));

    const full_engine::TerrainManifestAssetLoadJobCoordinatorResult result =
        full_engine::runTerrainManifestAssetLoadJobs(
            state,
            jobs,
            destination,
            callback,
            &callbackState,
            8);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::JobsBlocked, "missing callback handle blocks jobs", failures);
    expect(result.jobs.summary.blockedCount == 1, "missing callback handle records blocked job", failures);
    expect(state.pendingLoadRequestCount() == 3, "blocked jobs preserve load queue", failures);
    expect(jobs.jobCount() == 3, "blocked jobs preserve mirrored jobs", failures);
    expect(destination.meshHandleCount() == 0, "blocked jobs leave destination unchanged", failures);
}

void testFailedCallbackBlocks(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs;
    full_engine::RendererAssetHandleCatalog destination;
    CallbackState callbackState;
    callbackState.handles = completeHandles();
    callbackState.failedAsset = 2;

    const full_engine::TerrainManifestAssetLoadJobCoordinatorResult result =
        full_engine::runTerrainManifestAssetLoadJobs(
            state,
            jobs,
            destination,
            callback,
            &callbackState,
            8);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::JobsBlocked, "failed callback blocks jobs", failures);
    expect(result.jobs.summary.failedCount == 1, "failed callback records failed job", failures);
    expect(state.pendingLoadRequestCount() == 3, "failed callback preserves load queue", failures);
    expect(jobs.jobCount() == 3, "failed callback preserves mirrored jobs", failures);
}

void testAlreadyLoadedDestinationConsumes(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs;
    full_engine::RendererAssetHandleCatalog destination = completeHandles();

    const full_engine::TerrainManifestAssetLoadJobCoordinatorResult result =
        full_engine::runTerrainManifestAssetLoadJobs(
            state,
            jobs,
            destination,
            nullptr,
            nullptr,
            8);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::Success, "already-loaded destination succeeds without callback", failures);
    expect(result.jobs.summary.completedCount == 3, "already-loaded destination completes jobs", failures);
    expect(result.load.consume.summary.alreadyLoadedCount == 3, "already-loaded destination consumes existing handles", failures);
    expect(state.pendingLoadRequestCount() == 0, "already-loaded destination drains load queue", failures);
    expect(result.readiness.summary.readyCount == 3, "already-loaded destination replans ready", failures);
}

void testMaxJobsPartialBlocksUntilLaterPass(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs;
    full_engine::RendererAssetHandleCatalog destination;
    CallbackState callbackState;
    callbackState.handles = completeHandles();

    const full_engine::TerrainManifestAssetLoadJobCoordinatorResult partial =
        full_engine::runTerrainManifestAssetLoadJobs(
            state,
            jobs,
            destination,
            callback,
            &callbackState,
            1);

    expect(partial.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::JobsBlocked, "partial maxJobs blocks coordinator", failures);
    expect(partial.jobs.summary.completedCount == 1, "partial maxJobs completes one job", failures);
    expect(state.pendingLoadRequestCount() == 3, "partial maxJobs preserves load queue", failures);
    expect(jobs.jobCount() == 3, "partial maxJobs preserves mirrored jobs", failures);

    const full_engine::TerrainManifestAssetLoadJobCoordinatorResult complete =
        full_engine::runTerrainManifestAssetLoadJobs(
            state,
            jobs,
            destination,
            callback,
            &callbackState,
            8);

    expect(complete.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::Success, "later pass completes pending load jobs", failures);
    expect(state.pendingLoadRequestCount() == 0, "later pass consumes load queue", failures);
    expect(jobs.jobCount() == 0, "later pass removes mirrored jobs", failures);
}

void testUnrelatedCustomJobsArePreserved(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs;
    (void)jobs.push(customJob());
    full_engine::RendererAssetHandleCatalog destination;
    CallbackState callbackState;
    callbackState.handles = completeHandles();

    const full_engine::TerrainManifestAssetLoadJobCoordinatorResult result =
        full_engine::runTerrainManifestAssetLoadJobs(
            state,
            jobs,
            destination,
            callback,
            &callbackState,
            8);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::Success, "custom job case still loads assets", failures);
    expect(jobs.jobCount() == 1, "custom job remains after load jobs are removed", failures);
    expect(jobs.jobs()[0].request.kind == full_engine::EngineJobKind::Custom, "remaining job is custom", failures);
    expect(jobs.jobs()[0].request.payload0 == 9000, "custom job payload is preserved", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testNoPendingLoads(failures);
    testPendingLoadsCompleteAndReplanReady(failures);
    testMissingCallbackHandleBlocks(failures);
    testFailedCallbackBlocks(failures);
    testAlreadyLoadedDestinationConsumes(failures);
    testMaxJobsPartialBlocksUntilLaterPass(failures);
    testUnrelatedCustomJobsArePreserved(failures);

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
