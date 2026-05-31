#include "engine/renderer_integration/TerrainManifestAssetLoadExecutor.hpp"
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

full_engine::TerrainManifestAssetLoadRequest request(
    const std::uint64_t id,
    const full_engine::AssetKind kind) noexcept
{
    return {asset(id), kind};
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

full_engine::TerrainManifestAssetLoadRequestQueue queueThreeRequests()
{
    full_engine::TerrainManifestAssetLoadRequestQueue queue;
    (void)queue.push(request(1, full_engine::AssetKind::Mesh));
    (void)queue.push(request(2, full_engine::AssetKind::Material));
    (void)queue.push(request(3, full_engine::AssetKind::Texture));
    return queue;
}

struct CallbackState
{
    full_engine::RendererAssetHandleCatalog handles;
    full_engine::TerrainManifestAssetLoadCallbackStatus overrideStatus =
        full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded;
    bool returnInvalidHandles = false;
    int callCount = 0;
};

full_engine::RendererAssetHandleCatalog completeHandles()
{
    full_engine::RendererAssetHandleCatalog handles;
    (void)handles.addMeshHandle(asset(1), {10});
    (void)handles.addMaterialHandle(asset(2), {20});
    (void)handles.addTextureHandle(asset(3), {30});
    return handles;
}

full_engine::TerrainManifestAssetLoadCallbackResult catalogCallback(
    const full_engine::TerrainManifestAssetLoadRequest& request,
    void* const userData)
{
    CallbackState& state = *static_cast<CallbackState*>(userData);
    ++state.callCount;

    full_engine::TerrainManifestAssetLoadCallbackResult result;
    result.status = state.overrideStatus;
    if (result.status != full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded)
    {
        return result;
    }

    if (state.returnInvalidHandles)
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

void testCallbackLoadsAndConsumesQueue(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadRequestQueue queue = queueThreeRequests();
    full_engine::RendererAssetHandleCatalog destination;
    CallbackState state;
    state.handles = completeHandles();

    const full_engine::TerrainManifestAssetLoadExecutorResult result =
        full_engine::executeTerrainManifestAssetLoadRequests(queue, destination, catalogCallback, &state);

    expect(result.status == full_engine::TerrainManifestAssetLoadExecutorStatus::Consumed, "callback executor consumes queue", failures);
    expect(result.consume.consumed, "callback executor delegates final consume", failures);
    expect(queue.requestCount() == 0, "callback executor clears consumed queue", failures);
    expect(result.callbackRecords.size() == 3, "callback executor records every request", failures);
    expect(result.callbackRecords[0].request.kind == full_engine::AssetKind::Mesh, "callback executor preserves mesh order", failures);
    expect(result.callbackRecords[1].request.kind == full_engine::AssetKind::Material, "callback executor preserves material order", failures);
    expect(result.callbackRecords[2].request.kind == full_engine::AssetKind::Texture, "callback executor preserves texture order", failures);
    expect(state.callCount == 3, "callback executor invokes callback for missing destination handles", failures);
    expect(destination.findMeshHandle(asset(1)) != nullptr, "callback executor loads mesh handle", failures);
    expect(destination.findMaterialHandle(asset(2)) != nullptr, "callback executor loads material handle", failures);
    expect(destination.findTextureHandle(asset(3)) != nullptr, "callback executor loads texture handle", failures);
}

void testAlreadyLoadedSkipsCallback(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadRequestQueue queue = queueThreeRequests();
    full_engine::RendererAssetHandleCatalog destination;
    (void)destination.addMeshHandle(asset(1), {99});
    CallbackState state;
    state.handles = completeHandles();

    const full_engine::TerrainManifestAssetLoadExecutorResult result =
        full_engine::executeTerrainManifestAssetLoadRequests(queue, destination, catalogCallback, &state);

    expect(result.status == full_engine::TerrainManifestAssetLoadExecutorStatus::Consumed, "already-loaded executor consumes queue", failures);
    expect(state.callCount == 2, "already-loaded destination skips one callback", failures);
    expect(!result.callbackRecords[0].callbackInvoked, "already-loaded record marks callback skipped", failures);
    expect(result.consume.summary.alreadyLoadedCount == 1, "already-loaded destination counted by consume", failures);
    const full_renderer::MeshHandle* const mesh = destination.findMeshHandle(asset(1));
    expect(mesh != nullptr && mesh->id == 99, "already-loaded destination handle is preserved", failures);
}

void testNullCallbackBlocksMissingDestination(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadRequestQueue queue = queueThreeRequests();
    full_engine::RendererAssetHandleCatalog destination;

    const full_engine::TerrainManifestAssetLoadExecutorResult result =
        full_engine::executeTerrainManifestAssetLoadRequests(queue, destination, nullptr);

    expect(result.status == full_engine::TerrainManifestAssetLoadExecutorStatus::Blocked, "null callback blocks executor", failures);
    expect(!result.consume.consumed, "null callback does not consume queue", failures);
    expect(queue.requestCount() == 3, "null callback leaves queue pending", failures);
    expect(destination.meshHandleCount() == 0, "null callback leaves destination unchanged", failures);
}

void testCallbackMissingOrFailedBlocksBatch(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadRequestQueue missingQueue = queueThreeRequests();
    full_engine::RendererAssetHandleCatalog missingDestination;
    CallbackState missingState;
    missingState.handles = completeHandles();
    (void)missingState.handles.removeMaterialHandle(asset(2));

    const full_engine::TerrainManifestAssetLoadExecutorResult missing =
        full_engine::executeTerrainManifestAssetLoadRequests(missingQueue, missingDestination, catalogCallback, &missingState);
    expect(missing.status == full_engine::TerrainManifestAssetLoadExecutorStatus::Blocked, "missing callback handle blocks executor", failures);
    expect(missingQueue.requestCount() == 3, "missing callback handle leaves queue pending", failures);
    expect(missingDestination.meshHandleCount() == 0, "missing callback handle leaves destination unchanged", failures);

    full_engine::TerrainManifestAssetLoadRequestQueue failedQueue = queueThreeRequests();
    full_engine::RendererAssetHandleCatalog failedDestination;
    CallbackState failedState;
    failedState.handles = completeHandles();
    failedState.overrideStatus = full_engine::TerrainManifestAssetLoadCallbackStatus::Failed;

    const full_engine::TerrainManifestAssetLoadExecutorResult failed =
        full_engine::executeTerrainManifestAssetLoadRequests(failedQueue, failedDestination, catalogCallback, &failedState);
    expect(failed.status == full_engine::TerrainManifestAssetLoadExecutorStatus::Blocked, "failed callback blocks executor", failures);
    expect(failedQueue.requestCount() == 3, "failed callback leaves queue pending", failures);
    expect(failedDestination.meshHandleCount() == 0, "failed callback leaves destination unchanged", failures);
}

void testInvalidCallbackHandleBlocksBatch(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadRequestQueue queue = queueThreeRequests();
    full_engine::RendererAssetHandleCatalog destination;
    CallbackState state;
    state.handles = completeHandles();
    state.returnInvalidHandles = true;

    const full_engine::TerrainManifestAssetLoadExecutorResult result =
        full_engine::executeTerrainManifestAssetLoadRequests(queue, destination, catalogCallback, &state);

    expect(result.status == full_engine::TerrainManifestAssetLoadExecutorStatus::Blocked, "invalid callback handle blocks executor", failures);
    expect(queue.requestCount() == 3, "invalid callback handle leaves queue pending", failures);
    expect(destination.meshHandleCount() == 0, "invalid callback handle leaves destination unchanged", failures);
    expect(
        result.callbackRecords[0].sourceCatalogResult == full_engine::RendererAssetHandleCatalogResult::InvalidArgument,
        "invalid callback handle records source catalog rejection",
        failures);
}

void testLoadStateExecutesPendingRequests(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    state.setManifest(manifest());
    (void)state.planAssetReadiness({});
    (void)state.planAssetLoadRequests();
    (void)state.queueLatestAssetLoadRequests();

    full_engine::RendererAssetHandleCatalog destination;
    CallbackState callbackState;
    callbackState.handles = completeHandles();

    const full_engine::TerrainManifestAssetLoadExecutorResult& executed =
        state.executePendingAssetLoadRequests(destination, catalogCallback, &callbackState);

    expect(executed.status == full_engine::TerrainManifestAssetLoadExecutorStatus::Consumed, "load state executor consumes queue", failures);
    expect(state.pendingLoadRequestCount() == 0, "load state executor clears pending requests", failures);
    expect(state.latestLoadExecutorResult().consume.consumed, "load state stores executor diagnostics", failures);
    expect(state.latestLoadConsumeResult().consumed, "load state mirrors consume diagnostics", failures);

    const full_engine::TerrainManifestAssetReadinessPlan& ready = state.planAssetReadiness(destination);
    expect(ready.summary.readyCount == 3, "load state executor makes handles ready", failures);

    state.setManifest(manifest());
    expect(state.latestLoadExecutorResult().callbackRecords.empty(), "setManifest clears executor diagnostics", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testCallbackLoadsAndConsumesQueue(failures);
    testAlreadyLoadedSkipsCallback(failures);
    testNullCallbackBlocksMissingDestination(failures);
    testCallbackMissingOrFailedBlocksBatch(failures);
    testInvalidCallbackHandleBlocksBatch(failures);
    testLoadStateExecutesPendingRequests(failures);

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
