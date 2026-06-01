#include "engine/renderer_integration/TerrainManifestAssetLoadJobCompletions.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadJobWorkPackets.hpp"
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

full_engine::TerrainManifestAssetLoadJobCompletion completionFromRequest(
    const full_engine::TerrainManifestAssetLoadRequest& request,
    const full_engine::RendererAssetHandleCatalog& sourceHandles,
    const bool fail)
{
    full_engine::TerrainManifestAssetLoadJobCompletion completion;
    completion.request = request;
    completion.output.status = fail
        ? full_engine::TerrainManifestAssetLoadCallbackStatus::Failed
        : full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded;

    if (fail)
    {
        return completion;
    }

    switch (request.kind)
    {
    case full_engine::AssetKind::Mesh:
        if (const full_renderer::MeshHandle* const handle = sourceHandles.findMeshHandle(request.id))
        {
            completion.output.mesh = *handle;
            return completion;
        }
        break;
    case full_engine::AssetKind::Material:
        if (const full_renderer::MaterialHandle* const handle = sourceHandles.findMaterialHandle(request.id))
        {
            completion.output.material = *handle;
            return completion;
        }
        break;
    case full_engine::AssetKind::Texture:
        if (const full_renderer::TextureHandle* const handle = sourceHandles.findTextureHandle(request.id))
        {
            completion.output.texture = *handle;
            return completion;
        }
        break;
    case full_engine::AssetKind::Unknown:
    case full_engine::AssetKind::TerrainChunk:
    case full_engine::AssetKind::Skeleton:
    case full_engine::AssetKind::SkinnedMesh:
    case full_engine::AssetKind::Shader:
        break;
    }

    completion.output.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Missing;
    return completion;
}

std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> runFakeExternalWorker(
    const full_engine::EngineJobQueue& jobs,
    const full_engine::RendererAssetHandleCatalog& sourceHandles,
    const std::uint64_t failedAsset = 0)
{
    std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> completions;
    const full_engine::TerrainManifestAssetLoadJobWorkPacketResult packets =
        full_engine::buildTerrainManifestAssetLoadJobWorkPackets(jobs);
    for (const full_engine::TerrainManifestAssetLoadJobWorkPacket& packet : packets.packets)
    {
        completions.push_back(completionFromRequest(
            packet.request,
            sourceHandles,
            packet.request.id.value == failedAsset));
    }
    return completions;
}

full_engine::EngineJobQueue schedulePendingJobs(full_engine::TerrainManifestLoadState& state)
{
    full_engine::EngineJobQueue jobs;
    const full_engine::TerrainManifestAssetLoadJobScheduleResult scheduled =
        full_engine::scheduleTerrainManifestAssetLoadJobs(state, jobs);
    (void)scheduled;
    return jobs;
}

void testScheduledJobsProduceCompletionsAndReadinessReady(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs = schedulePendingJobs(state);
    (void)jobs.push(customJob());

    const std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> completions =
        runFakeExternalWorker(jobs, completeHandles());
    full_engine::RendererAssetHandleCatalog destination;
    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult result =
        full_engine::reconcileTerrainManifestAssetLoadJobCompletions(
            state,
            jobs,
            completions.data(),
            completions.size(),
            destination);

    expect(completions.size() == 3, "worker emits one completion per scheduled load job", failures);
    expect(completions[0].request.kind == full_engine::AssetKind::Mesh, "worker preserves mesh completion order", failures);
    expect(completions[1].request.kind == full_engine::AssetKind::Material, "worker preserves material completion order", failures);
    expect(completions[2].request.kind == full_engine::AssetKind::Texture, "worker preserves texture completion order", failures);
    expect(result.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::Success, "completion worker reconcile succeeds", failures);
    expect(result.publish.summary.publishedCount == 3, "worker completions publish three handles", failures);
    expect(result.reconcile.load.consumed, "worker completions consume retained load queue", failures);
    expect(result.reconcile.summary.removedScheduledJobCount == 3, "worker completions remove scheduled load jobs", failures);
    expect(result.reconcile.readiness.summary.readyCount == 3, "worker completions replan readiness ready", failures);
    expect(state.pendingLoadRequestCount() == 0, "worker completions clear retained load requests", failures);
    expect(jobs.jobCount() == 1, "worker completions preserve unrelated job", failures);
    expect(jobs.jobs()[0].request.kind == full_engine::EngineJobKind::Custom, "remaining job is custom", failures);
    expect(destination.findMeshHandle(asset(1)) != nullptr, "worker completions publish mesh destination", failures);
    expect(destination.findMaterialHandle(asset(2)) != nullptr, "worker completions publish material destination", failures);
    expect(destination.findTextureHandle(asset(3)) != nullptr, "worker completions publish texture destination", failures);

    const full_engine::TerrainManifestAssetReadinessPlan& readiness =
        state.planAssetReadiness(destination);
    expect(readiness.summary.readyCount == 3, "replanned readiness stays ready", failures);
    const full_engine::TerrainManifestAssetLoadRequestPlan& noMoreLoads =
        state.planAssetLoadRequests();
    expect(noMoreLoads.summary.requestCount == 0, "ready handles produce no more load requests", failures);
}

void testMissingWorkerHandlePreservesPendingState(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs = schedulePendingJobs(state);
    full_engine::RendererAssetHandleCatalog source = completeHandles();
    (void)source.removeMaterialHandle(asset(2));

    const std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> completions =
        runFakeExternalWorker(jobs, source);
    full_engine::RendererAssetHandleCatalog destination;
    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult result =
        full_engine::reconcileTerrainManifestAssetLoadJobCompletions(
            state,
            jobs,
            completions.data(),
            completions.size(),
            destination);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPublishFailed, "missing worker handle blocks completion reconcile", failures);
    expect(result.publish.summary.missingHandleCount == 1, "missing worker handle is counted", failures);
    expect(state.pendingLoadRequestCount() == 3, "missing worker handle preserves retained load requests", failures);
    expect(jobs.jobCount() == 3, "missing worker handle preserves scheduled jobs", failures);
    expect(destination.meshHandleCount() == 0, "missing worker handle leaves destination unchanged", failures);
}

void testFailedWorkerCompletionPreservesPendingState(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs = schedulePendingJobs(state);

    const std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> completions =
        runFakeExternalWorker(jobs, completeHandles(), 2);
    full_engine::RendererAssetHandleCatalog destination;
    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult result =
        full_engine::reconcileTerrainManifestAssetLoadJobCompletions(
            state,
            jobs,
            completions.data(),
            completions.size(),
            destination);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPublishFailed, "failed worker completion blocks reconcile", failures);
    expect(result.publish.summary.missingHandleCount == 1, "failed worker completion is treated as missing handle", failures);
    expect(state.pendingLoadRequestCount() == 3, "failed worker completion preserves retained load requests", failures);
    expect(jobs.jobCount() == 3, "failed worker completion preserves scheduled jobs", failures);
    expect(destination.materialHandleCount() == 0, "failed worker completion leaves destination unchanged", failures);
}

void testEmptyWorkerCompletionsReconcileAlreadyLoadedDestination(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs = schedulePendingJobs(state);
    full_engine::RendererAssetHandleCatalog destination = completeHandles();

    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult result =
        full_engine::reconcileTerrainManifestAssetLoadJobCompletions(
            state,
            jobs,
            nullptr,
            0,
            destination);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::Success, "empty completions reconcile already-loaded destination", failures);
    expect(result.reconcile.load.summary.alreadyLoadedCount == 3, "already-loaded destination satisfies pending requests", failures);
    expect(state.pendingLoadRequestCount() == 0, "already-loaded destination clears retained load requests", failures);
    expect(jobs.jobCount() == 0, "already-loaded destination removes scheduled load jobs", failures);
    expect(result.reconcile.readiness.summary.readyCount == 3, "already-loaded destination replans ready", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testScheduledJobsProduceCompletionsAndReadinessReady(failures);
    testMissingWorkerHandlePreservesPendingState(failures);
    testFailedWorkerCompletionPreservesPendingState(failures);
    testEmptyWorkerCompletionsReconcileAlreadyLoadedDestination(failures);

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
