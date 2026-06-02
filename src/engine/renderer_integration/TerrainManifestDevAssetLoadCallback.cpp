#include "engine/renderer_integration/TerrainManifestDevAssetLoadCallback.hpp"

#include "engine/assets/LoadedAssetImporter.hpp"
#include "engine/renderer_integration/LoadedAssetUploadExecutor.hpp"
#include "engine/renderer_integration/LoadedAssetUploadPlan.hpp"

#include "full_renderer/Handles.hpp"

#include <vector>

namespace full_engine
{
namespace
{
TerrainManifestAssetLoadCallbackResult missingResult() noexcept
{
    TerrainManifestAssetLoadCallbackResult result;
    result.status = TerrainManifestAssetLoadCallbackStatus::Missing;
    return result;
}

TerrainManifestAssetLoadCallbackResult failedResult() noexcept
{
    TerrainManifestAssetLoadCallbackResult result;
    result.status = TerrainManifestAssetLoadCallbackStatus::Failed;
    return result;
}

bool setExistingHandle(
    const TerrainManifestAssetLoadRequest& request,
    const RendererAssetHandleCatalog& handles,
    TerrainManifestAssetLoadCallbackResult& result) noexcept
{
    switch (request.kind)
    {
    case AssetKind::Mesh:
        if (const full_renderer::MeshHandle* const handle = handles.findMeshHandle(request.id))
        {
            result.status = TerrainManifestAssetLoadCallbackStatus::Loaded;
            result.mesh = *handle;
            return true;
        }
        break;
    case AssetKind::Material:
        if (const full_renderer::MaterialHandle* const handle = handles.findMaterialHandle(request.id))
        {
            result.status = TerrainManifestAssetLoadCallbackStatus::Loaded;
            result.material = *handle;
            return true;
        }
        break;
    case AssetKind::Texture:
        if (const full_renderer::TextureHandle* const handle = handles.findTextureHandle(request.id))
        {
            result.status = TerrainManifestAssetLoadCallbackStatus::Loaded;
            result.texture = *handle;
            return true;
        }
        break;
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        break;
    }

    return false;
}

TerrainManifestAssetLoadCallbackResult resultFromCompletedHandles(
    const TerrainManifestAssetLoadRequest& request,
    const RendererAssetHandleCatalog& completedHandles) noexcept
{
    TerrainManifestAssetLoadCallbackResult result = failedResult();
    return setExistingHandle(request, completedHandles, result) ? result : failedResult();
}

LoadedAssetUploadExecuteStatus uploadStatusForRequest(
    const TerrainManifestAssetLoadRequest& request,
    const LoadedAssetUploadExecuteResult& result) noexcept
{
    if (result.records.size() != 1)
    {
        return LoadedAssetUploadExecuteStatus::SkippedUnplanned;
    }

    const LoadedAssetUploadExecuteRecord& record = result.records[0];
    if (!(record.id == request.id) || record.kind != request.kind)
    {
        return LoadedAssetUploadExecuteStatus::SkippedUnplanned;
    }

    return record.status;
}

bool canImportAndUpload(const AssetKind kind) noexcept
{
    return kind == AssetKind::Mesh ||
        kind == AssetKind::Texture ||
        kind == AssetKind::Material;
}
} // namespace

TerrainManifestAssetLoadCallbackResult terrainManifestDevAssetLoadCallback(
    const TerrainManifestAssetLoadRequest& request,
    void* const userData)
{
    TerrainManifestDevAssetLoadContext* const context =
        static_cast<TerrainManifestDevAssetLoadContext*>(userData);
    if (context == nullptr ||
        context->sources == nullptr ||
        context->renderer == nullptr ||
        context->completedHandles == nullptr ||
        !isValid(request.id))
    {
        return failedResult();
    }

    TerrainManifestAssetLoadCallbackResult existing = missingResult();
    if (context->alreadyLoadedHandles != nullptr &&
        setExistingHandle(request, *context->alreadyLoadedHandles, existing))
    {
        return existing;
    }
    if (setExistingHandle(request, *context->completedHandles, existing))
    {
        return existing;
    }

    if (!canImportAndUpload(request.kind))
    {
        return failedResult();
    }

    const AssetSourceRecord* const source = context->sources->findSource(request.id);
    if (source == nullptr)
    {
        return missingResult();
    }
    if (source->kind != request.kind)
    {
        return failedResult();
    }

    const LoadedAssetImportResult imported = importLoadedAssetPayloadFromDevFile(*source);
    if (imported.status != LoadedAssetImportStatus::Success)
    {
        return failedResult();
    }

    LoadedAssetUploadPlan plan = buildLoadedAssetUploadPlan(&imported.payload, 1);
    if (plan.records.size() != 1 ||
        plan.records[0].status != LoadedAssetUploadStatus::Planned)
    {
        return failedResult();
    }

    const LoadedAssetUploadExecuteResult executed =
        executeLoadedAssetUploadPlan(*context->renderer, plan, *context->completedHandles);
    const LoadedAssetUploadExecuteStatus status = uploadStatusForRequest(request, executed);
    if (status == LoadedAssetUploadExecuteStatus::MissingTextureHandle)
    {
        return missingResult();
    }
    if (status != LoadedAssetUploadExecuteStatus::Uploaded &&
        status != LoadedAssetUploadExecuteStatus::AlreadyMapped)
    {
        return failedResult();
    }

    return resultFromCompletedHandles(request, *context->completedHandles);
}

TerrainManifestDevAssetLoadWorkerResult runTerrainManifestDevAssetLoadWorker(
    const EngineJobQueue& scheduledJobs,
    TerrainManifestAssetLoadCompletionInbox& destination,
    TerrainManifestDevAssetLoadContext& context)
{
    TerrainManifestDevAssetLoadWorkerResult result;
    result.packets = buildTerrainManifestAssetLoadJobWorkPackets(scheduledJobs);

    std::vector<TerrainManifestAssetLoadJobCompletion> completions;
    completions.reserve(result.packets.packets.size());
    for (const TerrainManifestAssetLoadJobWorkPacket& packet : result.packets.packets)
    {
        TerrainManifestAssetLoadJobCompletion completion;
        completion.request = packet.request;
        completion.output = terrainManifestDevAssetLoadCallback(packet.request, &context);
        completions.push_back(completion);
    }

    result.publish = publishTerrainManifestAssetLoadWorkerCompletions(
        destination,
        completions.data(),
        completions.size());
    return result;
}
} // namespace full_engine
