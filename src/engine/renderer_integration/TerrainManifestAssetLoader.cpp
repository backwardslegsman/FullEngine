#include "engine/renderer_integration/TerrainManifestAssetLoader.hpp"

namespace full_engine
{
namespace
{
void incrementSummary(TerrainManifestAssetLoadSummary& summary, const TerrainManifestAssetLoadStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadStatus::Loaded:
        ++summary.loadedCount;
        break;
    case TerrainManifestAssetLoadStatus::AlreadyLoaded:
        ++summary.alreadyLoadedCount;
        break;
    case TerrainManifestAssetLoadStatus::MissingHandle:
        ++summary.missingHandleCount;
        break;
    case TerrainManifestAssetLoadStatus::CatalogRejected:
        ++summary.catalogRejectedCount;
        break;
    }
}

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

bool hasSourceHandle(
    const TerrainManifestAssetLoadRequest& request,
    const RendererAssetHandleCatalog& handles)
{
    return hasDestinationHandle(request, handles);
}

RendererAssetHandleCatalogResult addSourceHandle(
    const TerrainManifestAssetLoadRequest& request,
    const RendererAssetHandleCatalog& sourceHandles,
    RendererAssetHandleCatalog& destinationHandles)
{
    switch (request.kind)
    {
    case AssetKind::Mesh:
    {
        const full_renderer::MeshHandle* handle = sourceHandles.findMeshHandle(request.id);
        return handle != nullptr ? destinationHandles.addMeshHandle(request.id, *handle)
                                 : RendererAssetHandleCatalogResult::NotFound;
    }
    case AssetKind::Material:
    {
        const full_renderer::MaterialHandle* handle = sourceHandles.findMaterialHandle(request.id);
        return handle != nullptr ? destinationHandles.addMaterialHandle(request.id, *handle)
                                 : RendererAssetHandleCatalogResult::NotFound;
    }
    case AssetKind::Texture:
    {
        const full_renderer::TextureHandle* handle = sourceHandles.findTextureHandle(request.id);
        return handle != nullptr ? destinationHandles.addTextureHandle(request.id, *handle)
                                 : RendererAssetHandleCatalogResult::NotFound;
    }
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        return RendererAssetHandleCatalogResult::InvalidArgument;
    }

    return RendererAssetHandleCatalogResult::InvalidArgument;
}

void removeCopiedHandle(
    const TerrainManifestAssetLoadRequest& request,
    RendererAssetHandleCatalog& handles)
{
    switch (request.kind)
    {
    case AssetKind::Mesh:
        (void)handles.removeMeshHandle(request.id);
        break;
    case AssetKind::Material:
        (void)handles.removeMaterialHandle(request.id);
        break;
    case AssetKind::Texture:
        (void)handles.removeTextureHandle(request.id);
        break;
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        break;
    }
}
} // namespace

TerrainManifestAssetLoadResult consumeTerrainManifestAssetLoadRequests(
    TerrainManifestAssetLoadRequestQueue& queue,
    const RendererAssetHandleCatalog& sourceHandles,
    RendererAssetHandleCatalog& destinationHandles)
{
    TerrainManifestAssetLoadResult result;
    result.records.reserve(queue.requests().size());

    bool canConsume = true;
    for (const TerrainManifestAssetLoadRequest& request : queue.requests())
    {
        TerrainManifestAssetLoadRecord record;
        record.request = request;

        if (hasDestinationHandle(request, destinationHandles))
        {
            record.status = TerrainManifestAssetLoadStatus::AlreadyLoaded;
        }
        else if (hasSourceHandle(request, sourceHandles))
        {
            record.status = TerrainManifestAssetLoadStatus::Loaded;
        }
        else
        {
            record.status = TerrainManifestAssetLoadStatus::MissingHandle;
            canConsume = false;
        }

        incrementSummary(result.summary, record.status);
        result.records.push_back(record);
    }

    if (!canConsume)
    {
        return result;
    }

    std::vector<TerrainManifestAssetLoadRequest> copiedRequests;
    copiedRequests.reserve(result.records.size());

    for (TerrainManifestAssetLoadRecord& record : result.records)
    {
        if (record.status == TerrainManifestAssetLoadStatus::AlreadyLoaded)
        {
            continue;
        }

        record.catalogResult = addSourceHandle(record.request, sourceHandles, destinationHandles);
        if (record.catalogResult != RendererAssetHandleCatalogResult::Success)
        {
            record.status = TerrainManifestAssetLoadStatus::CatalogRejected;
            --result.summary.loadedCount;
            ++result.summary.catalogRejectedCount;

            for (const TerrainManifestAssetLoadRequest& copied : copiedRequests)
            {
                removeCopiedHandle(copied, destinationHandles);
            }

            return result;
        }

        copiedRequests.push_back(record.request);
    }

    queue.clear();
    result.consumed = true;
    return result;
}
} // namespace full_engine
