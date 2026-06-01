#include "engine/renderer_integration/TerrainManifestAssetLoadJobCompletions.hpp"
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

full_engine::TerrainManifestAssetLoadRequest request(
    const std::uint64_t id,
    const full_engine::AssetKind kind) noexcept
{
    full_engine::TerrainManifestAssetLoadRequest result;
    result.id = asset(id);
    result.kind = kind;
    return result;
}

full_engine::TerrainManifestAssetLoadJobCompletion meshCompletion(
    const std::uint64_t id,
    const std::uint16_t handle) noexcept
{
    full_engine::TerrainManifestAssetLoadJobCompletion completion;
    completion.request = request(id, full_engine::AssetKind::Mesh);
    completion.output.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded;
    completion.output.mesh = {handle};
    return completion;
}

full_engine::TerrainManifestAssetLoadJobCompletion materialCompletion(
    const std::uint64_t id,
    const std::uint16_t handle) noexcept
{
    full_engine::TerrainManifestAssetLoadJobCompletion completion;
    completion.request = request(id, full_engine::AssetKind::Material);
    completion.output.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded;
    completion.output.material = {handle};
    return completion;
}

full_engine::TerrainManifestAssetLoadJobCompletion textureCompletion(
    const std::uint64_t id,
    const std::uint16_t handle) noexcept
{
    full_engine::TerrainManifestAssetLoadJobCompletion completion;
    completion.request = request(id, full_engine::AssetKind::Texture);
    completion.output.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded;
    completion.output.texture = {handle};
    return completion;
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

void testValidCompletionsPublishHandles(std::vector<std::string>& failures)
{
    full_engine::RendererAssetHandleCatalog completed;
    const std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> completions = {
        meshCompletion(1, 10),
        materialCompletion(2, 20),
        textureCompletion(3, 30),
    };

    const full_engine::TerrainManifestAssetLoadJobCompletionPublishResult result =
        full_engine::publishTerrainManifestAssetLoadJobCompletions(
            completions.data(),
            completions.size(),
            completed);

    expect(result.summary.publishedCount == 3, "valid completions publish three handles", failures);
    expect(result.records.size() == 3, "valid completions preserve source record count", failures);
    expect(result.records[0].status == full_engine::TerrainManifestAssetLoadJobCompletionStatus::Published, "mesh completion is published", failures);
    expect(result.records[1].completion.request.kind == full_engine::AssetKind::Material, "completion order is preserved", failures);
    expect(completed.findMeshHandle(asset(1)) != nullptr, "published mesh handle is available", failures);
    expect(completed.findMaterialHandle(asset(2)) != nullptr, "published material handle is available", failures);
    expect(completed.findTextureHandle(asset(3)) != nullptr, "published texture handle is available", failures);
    expect(std::string(full_engine::terrainManifestAssetLoadJobCompletionStatusName(result.records[0].status)) == "Published", "completion status name is stable", failures);
}

void testInvalidRequestsAndMissingHandlesAreRejected(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadJobCompletion invalidId = meshCompletion(0, 10);
    invalidId.request.id = {};

    full_engine::TerrainManifestAssetLoadJobCompletion unsupportedKind = meshCompletion(4, 40);
    unsupportedKind.request.kind = full_engine::AssetKind::Shader;

    full_engine::TerrainManifestAssetLoadJobCompletion missingStatus = materialCompletion(2, 20);
    missingStatus.output.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Missing;

    full_engine::TerrainManifestAssetLoadJobCompletion defaultHandle = meshCompletion(1, 0);
    defaultHandle.output.mesh = {};

    const std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> completions = {
        invalidId,
        unsupportedKind,
        missingStatus,
        defaultHandle,
    };

    full_engine::RendererAssetHandleCatalog completed;
    const full_engine::TerrainManifestAssetLoadJobCompletionPublishResult result =
        full_engine::publishTerrainManifestAssetLoadJobCompletions(
            completions.data(),
            completions.size(),
            completed);

    expect(result.summary.invalidRequestCount == 2, "invalid requests are counted", failures);
    expect(result.summary.missingHandleCount == 2, "missing/default handles are counted", failures);
    expect(result.summary.publishedCount == 0, "invalid completions publish no handles", failures);
    expect(completed.meshHandleCount() == 0, "invalid completions leave mesh catalog empty", failures);
    expect(result.records[0].status == full_engine::TerrainManifestAssetLoadJobCompletionStatus::InvalidRequest, "invalid id record is rejected", failures);
    expect(result.records[2].status == full_engine::TerrainManifestAssetLoadJobCompletionStatus::MissingHandle, "missing callback output is rejected", failures);
}

void testDuplicateCompletionsAreAlreadyPublished(std::vector<std::string>& failures)
{
    full_engine::RendererAssetHandleCatalog completed;
    const std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> completions = {
        meshCompletion(1, 10),
        meshCompletion(1, 11),
    };

    const full_engine::TerrainManifestAssetLoadJobCompletionPublishResult result =
        full_engine::publishTerrainManifestAssetLoadJobCompletions(
            completions.data(),
            completions.size(),
            completed);

    expect(result.summary.publishedCount == 1, "first duplicate completion publishes", failures);
    expect(result.summary.alreadyPublishedCount == 1, "second duplicate completion is already published", failures);
    expect(result.records[1].status == full_engine::TerrainManifestAssetLoadJobCompletionStatus::AlreadyPublished, "duplicate record reports already published", failures);
    expect(completed.meshHandleCount() == 1, "duplicate completions keep one mesh mapping", failures);
}

void testNullCompletionsWithCountAreInvalid(std::vector<std::string>& failures)
{
    full_engine::RendererAssetHandleCatalog completed;
    const full_engine::TerrainManifestAssetLoadJobCompletionPublishResult result =
        full_engine::publishTerrainManifestAssetLoadJobCompletions(nullptr, 2, completed);

    expect(result.records.empty(), "null completion array emits no records", failures);
    expect(result.summary.invalidRequestCount == 2, "null completion array counts invalid inputs", failures);
    expect(completed.meshHandleCount() == 0, "null completion array leaves catalog unchanged", failures);
}

void testReconcileValidCompletionsConsumesAndRemovesJobs(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs;
    (void)jobs.push(customJob());
    (void)full_engine::scheduleTerrainManifestAssetLoadJobs(state, jobs);

    full_engine::RendererAssetHandleCatalog destination;
    const std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> completions = {
        meshCompletion(1, 10),
        materialCompletion(2, 20),
        textureCompletion(3, 30),
    };

    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult result =
        full_engine::reconcileTerrainManifestAssetLoadJobCompletions(
            state,
            jobs,
            completions.data(),
            completions.size(),
            destination);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::Success, "completion reconcile succeeds", failures);
    expect(result.publish.summary.publishedCount == 3, "completion reconcile publishes three handles", failures);
    expect(result.reconcile.status == full_engine::TerrainManifestAssetLoadJobReconcileStatus::Success, "nested reconcile succeeds", failures);
    expect(result.reconcile.load.consumed, "completion reconcile consumes retained queue", failures);
    expect(result.reconcile.summary.removedScheduledJobCount == 3, "completion reconcile removes scheduled load jobs", failures);
    expect(result.reconcile.readiness.summary.readyCount == 3, "completion reconcile replans ready", failures);
    expect(state.pendingLoadRequestCount() == 0, "completion reconcile clears retained load requests", failures);
    expect(jobs.jobCount() == 1, "completion reconcile preserves unrelated job", failures);
    expect(jobs.jobs()[0].request.kind == full_engine::EngineJobKind::Custom, "completion reconcile leaves custom job", failures);
    expect(destination.findMeshHandle(asset(1)) != nullptr, "completion reconcile writes mesh destination", failures);
    expect(destination.findMaterialHandle(asset(2)) != nullptr, "completion reconcile writes material destination", failures);
    expect(destination.findTextureHandle(asset(3)) != nullptr, "completion reconcile writes texture destination", failures);
    expect(std::string(full_engine::terrainManifestAssetLoadJobCompletionReconcileStatusName(result.status)) == "Success", "completion reconcile status name is stable", failures);
}

void testFailedPublishingLeavesReconcileStateUnchanged(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs;
    (void)full_engine::scheduleTerrainManifestAssetLoadJobs(state, jobs);

    full_engine::TerrainManifestAssetLoadJobCompletion missingMaterial = materialCompletion(2, 20);
    missingMaterial.output.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Missing;
    const std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> completions = {
        meshCompletion(1, 10),
        missingMaterial,
        textureCompletion(3, 30),
    };
    full_engine::RendererAssetHandleCatalog destination;

    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult result =
        full_engine::reconcileTerrainManifestAssetLoadJobCompletions(
            state,
            jobs,
            completions.data(),
            completions.size(),
            destination);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPublishFailed, "failed publishing blocks reconcile", failures);
    expect(result.publish.summary.missingHandleCount == 1, "failed publishing reports missing handle", failures);
    expect(result.reconcile.status == full_engine::TerrainManifestAssetLoadJobReconcileStatus::NoPendingLoads, "failed publishing does not attempt nested reconcile", failures);
    expect(state.pendingLoadRequestCount() == 3, "failed publishing preserves retained load requests", failures);
    expect(jobs.jobCount() == 3, "failed publishing preserves scheduled jobs", failures);
    expect(destination.meshHandleCount() == 0, "failed publishing leaves destination mesh catalog unchanged", failures);
    expect(destination.materialHandleCount() == 0, "failed publishing leaves destination material catalog unchanged", failures);
    expect(destination.textureHandleCount() == 0, "failed publishing leaves destination texture catalog unchanged", failures);
}

void testEmptyCompletionsReconcileAlreadyLoadedDestination(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    queueManifestLoadRequests(state);
    full_engine::EngineJobQueue jobs;
    (void)full_engine::scheduleTerrainManifestAssetLoadJobs(state, jobs);
    full_engine::RendererAssetHandleCatalog destination = completeHandles();

    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult result =
        full_engine::reconcileTerrainManifestAssetLoadJobCompletions(
            state,
            jobs,
            nullptr,
            0,
            destination);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::Success, "empty completions reconcile already-loaded destination", failures);
    expect(result.publish.records.empty(), "empty completions publish no records", failures);
    expect(result.reconcile.load.summary.alreadyLoadedCount == 3, "already-loaded destination satisfies retained requests", failures);
    expect(state.pendingLoadRequestCount() == 0, "already-loaded completion reconcile clears load queue", failures);
    expect(jobs.jobCount() == 0, "already-loaded completion reconcile removes scheduled load jobs", failures);
}

void testNoPendingLoadsReportsNoPending(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    full_engine::EngineJobQueue jobs;
    full_engine::RendererAssetHandleCatalog destination;
    const std::vector<full_engine::TerrainManifestAssetLoadJobCompletion> completions = {
        meshCompletion(1, 10),
    };

    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult result =
        full_engine::reconcileTerrainManifestAssetLoadJobCompletions(
            state,
            jobs,
            completions.data(),
            completions.size(),
            destination);

    expect(result.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::NoPendingLoads, "no pending completion reconcile reports NoPendingLoads", failures);
    expect(result.publish.records.empty(), "no pending completion reconcile does not publish completions", failures);
    expect(destination.meshHandleCount() == 0, "no pending completion reconcile leaves destination unchanged", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testValidCompletionsPublishHandles(failures);
    testInvalidRequestsAndMissingHandlesAreRejected(failures);
    testDuplicateCompletionsAreAlreadyPublished(failures);
    testNullCompletionsWithCountAreInvalid(failures);
    testReconcileValidCompletionsConsumesAndRemovesJobs(failures);
    testFailedPublishingLeavesReconcileStateUnchanged(failures);
    testEmptyCompletionsReconcileAlreadyLoadedDestination(failures);
    testNoPendingLoadsReportsNoPending(failures);

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
