#include "engine/renderer_integration/TerrainManifestAssetLoadExecutor.hpp"

namespace full_engine
{
namespace
{
bool hasDestinationHandle(
    const TerrainManifestAssetLoadRequest& request,
    const RendererAssetHandleCatalog& handles)
{
    switch (request.kind)
    {
    case AssetKind::Mesh:
        return handles.findMeshHandle(request.id) != nullptr;
    case AssetKind::Material:
        return handles.findMaterialHandle(request.id) != nullptr;
    case AssetKind::Texture:
        return handles.findTextureHandle(request.id) != nullptr;
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        return false;
    }

    return false;
}

RendererAssetHandleCatalogResult addCallbackHandle(
    const TerrainManifestAssetLoadRequest& request,
    const TerrainManifestAssetLoadCallbackResult& callback,
    RendererAssetHandleCatalog& handles)
{
    switch (request.kind)
    {
    case AssetKind::Mesh:
        return handles.addMeshHandle(request.id, callback.mesh);
    case AssetKind::Material:
        return handles.addMaterialHandle(request.id, callback.material);
    case AssetKind::Texture:
        return handles.addTextureHandle(request.id, callback.texture);
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        return RendererAssetHandleCatalogResult::InvalidArgument;
    }

    return RendererAssetHandleCatalogResult::InvalidArgument;
}
} // namespace

TerrainManifestAssetLoadExecutorResult executeTerrainManifestAssetLoadRequests(
    TerrainManifestAssetLoadRequestQueue& queue,
    RendererAssetHandleCatalog& destinationHandles,
    TerrainManifestAssetLoadCallback callback,
    void* const userData)
{
    TerrainManifestAssetLoadExecutorResult result;
    result.callbackRecords.reserve(queue.requests().size());

    RendererAssetHandleCatalog sourceHandles;
    bool canConsume = true;

    for (const TerrainManifestAssetLoadRequest& request : queue.requests())
    {
        TerrainManifestAssetLoadExecutorRecord record;
        record.request = request;

        if (hasDestinationHandle(request, destinationHandles))
        {
            result.callbackRecords.push_back(record);
            continue;
        }

        if (callback == nullptr)
        {
            canConsume = false;
            result.callbackRecords.push_back(record);
            continue;
        }

        record.callbackInvoked = true;
        record.callback = callback(request, userData);
        if (record.callback.status == TerrainManifestAssetLoadCallbackStatus::Loaded)
        {
            record.sourceCatalogResult = addCallbackHandle(request, record.callback, sourceHandles);
            if (record.sourceCatalogResult != RendererAssetHandleCatalogResult::Success)
            {
                canConsume = false;
            }
        }
        else
        {
            canConsume = false;
        }

        result.callbackRecords.push_back(record);
    }

    if (!canConsume)
    {
        result.status = TerrainManifestAssetLoadExecutorStatus::Blocked;
        return result;
    }

    result.consume = consumeTerrainManifestAssetLoadRequests(queue, sourceHandles, destinationHandles);
    result.status = result.consume.consumed
        ? TerrainManifestAssetLoadExecutorStatus::Consumed
        : TerrainManifestAssetLoadExecutorStatus::Blocked;
    return result;
}
} // namespace full_engine
