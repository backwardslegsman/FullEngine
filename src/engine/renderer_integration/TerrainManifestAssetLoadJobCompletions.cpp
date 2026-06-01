#include "engine/renderer_integration/TerrainManifestAssetLoadJobCompletions.hpp"

#include "engine/renderer_integration/TerrainManifestLoadState.hpp"

namespace full_engine
{
namespace
{
bool isSupportedCompletionKind(const AssetKind kind) noexcept
{
    return kind == AssetKind::Mesh ||
        kind == AssetKind::Material ||
        kind == AssetKind::Texture;
}

void incrementSummary(
    TerrainManifestAssetLoadJobCompletionSummary& summary,
    const TerrainManifestAssetLoadJobCompletionStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadJobCompletionStatus::Published:
        ++summary.publishedCount;
        break;
    case TerrainManifestAssetLoadJobCompletionStatus::AlreadyPublished:
        ++summary.alreadyPublishedCount;
        break;
    case TerrainManifestAssetLoadJobCompletionStatus::InvalidRequest:
        ++summary.invalidRequestCount;
        break;
    case TerrainManifestAssetLoadJobCompletionStatus::MissingHandle:
        ++summary.missingHandleCount;
        break;
    case TerrainManifestAssetLoadJobCompletionStatus::CatalogRejected:
        ++summary.catalogRejectedCount;
        break;
    }
}

bool hasPublishedHandle(
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

bool completionHasValidHandle(const TerrainManifestAssetLoadJobCompletion& completion) noexcept
{
    switch (completion.request.kind)
    {
    case AssetKind::Mesh:
        return full_renderer::isValid(completion.output.mesh);
    case AssetKind::Material:
        return full_renderer::isValid(completion.output.material);
    case AssetKind::Texture:
        return full_renderer::isValid(completion.output.texture);
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        return false;
    }

    return false;
}

RendererAssetHandleCatalogResult publishCompletionHandle(
    const TerrainManifestAssetLoadJobCompletion& completion,
    RendererAssetHandleCatalog& handles)
{
    const TerrainManifestAssetLoadRequest& request = completion.request;
    switch (request.kind)
    {
    case AssetKind::Mesh:
        return handles.addMeshHandle(request.id, completion.output.mesh);
    case AssetKind::Material:
        return handles.addMaterialHandle(request.id, completion.output.material);
    case AssetKind::Texture:
        return handles.addTextureHandle(request.id, completion.output.texture);
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        return RendererAssetHandleCatalogResult::InvalidArgument;
    }

    return RendererAssetHandleCatalogResult::InvalidArgument;
}

bool publishingFailed(const TerrainManifestAssetLoadJobCompletionPublishResult& result) noexcept
{
    return result.summary.invalidRequestCount > 0 ||
        result.summary.missingHandleCount > 0 ||
        result.summary.catalogRejectedCount > 0;
}

TerrainManifestAssetLoadJobCompletionReconcileStatus completionStatusFromReconcile(
    const TerrainManifestAssetLoadJobReconcileStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadJobReconcileStatus::Success:
        return TerrainManifestAssetLoadJobCompletionReconcileStatus::Success;
    case TerrainManifestAssetLoadJobReconcileStatus::NoPendingLoads:
        return TerrainManifestAssetLoadJobCompletionReconcileStatus::NoPendingLoads;
    case TerrainManifestAssetLoadJobReconcileStatus::CompletionPending:
        return TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPending;
    case TerrainManifestAssetLoadJobReconcileStatus::LoadConsumeBlocked:
        return TerrainManifestAssetLoadJobCompletionReconcileStatus::LoadConsumeBlocked;
    }

    return TerrainManifestAssetLoadJobCompletionReconcileStatus::LoadConsumeBlocked;
}
} // namespace

const char* terrainManifestAssetLoadJobCompletionStatusName(
    const TerrainManifestAssetLoadJobCompletionStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadJobCompletionStatus::Published:
        return "Published";
    case TerrainManifestAssetLoadJobCompletionStatus::AlreadyPublished:
        return "AlreadyPublished";
    case TerrainManifestAssetLoadJobCompletionStatus::InvalidRequest:
        return "InvalidRequest";
    case TerrainManifestAssetLoadJobCompletionStatus::MissingHandle:
        return "MissingHandle";
    case TerrainManifestAssetLoadJobCompletionStatus::CatalogRejected:
        return "CatalogRejected";
    }

    return "Unknown";
}

const char* terrainManifestAssetLoadJobCompletionReconcileStatusName(
    const TerrainManifestAssetLoadJobCompletionReconcileStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadJobCompletionReconcileStatus::Success:
        return "Success";
    case TerrainManifestAssetLoadJobCompletionReconcileStatus::NoPendingLoads:
        return "NoPendingLoads";
    case TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPublishFailed:
        return "CompletionPublishFailed";
    case TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPending:
        return "CompletionPending";
    case TerrainManifestAssetLoadJobCompletionReconcileStatus::LoadConsumeBlocked:
        return "LoadConsumeBlocked";
    }

    return "Unknown";
}

TerrainManifestAssetLoadJobCompletionPublishResult publishTerrainManifestAssetLoadJobCompletions(
    const TerrainManifestAssetLoadJobCompletion* const completions,
    const std::size_t completionCount,
    RendererAssetHandleCatalog& completedHandles)
{
    TerrainManifestAssetLoadJobCompletionPublishResult result;
    if (completions == nullptr && completionCount > 0)
    {
        result.summary.invalidRequestCount = completionCount;
        return result;
    }

    result.records.reserve(completionCount);
    for (std::size_t index = 0; index < completionCount; ++index)
    {
        TerrainManifestAssetLoadJobCompletionRecord record;
        record.completion = completions[index];

        const TerrainManifestAssetLoadRequest& request = record.completion.request;
        if (!isValid(request.id) || !isSupportedCompletionKind(request.kind))
        {
            record.status = TerrainManifestAssetLoadJobCompletionStatus::InvalidRequest;
        }
        else if (record.completion.output.status != TerrainManifestAssetLoadCallbackStatus::Loaded)
        {
            record.status = TerrainManifestAssetLoadJobCompletionStatus::MissingHandle;
        }
        else if (!completionHasValidHandle(record.completion))
        {
            record.status = TerrainManifestAssetLoadJobCompletionStatus::MissingHandle;
        }
        else if (hasPublishedHandle(request, completedHandles))
        {
            record.status = TerrainManifestAssetLoadJobCompletionStatus::AlreadyPublished;
        }
        else
        {
            record.catalogResult = publishCompletionHandle(record.completion, completedHandles);
            if (record.catalogResult == RendererAssetHandleCatalogResult::Success)
            {
                record.status = TerrainManifestAssetLoadJobCompletionStatus::Published;
            }
            else if (record.catalogResult == RendererAssetHandleCatalogResult::AlreadyExists)
            {
                record.status = TerrainManifestAssetLoadJobCompletionStatus::AlreadyPublished;
            }
            else if (record.catalogResult == RendererAssetHandleCatalogResult::InvalidArgument)
            {
                record.status = TerrainManifestAssetLoadJobCompletionStatus::MissingHandle;
            }
            else
            {
                record.status = TerrainManifestAssetLoadJobCompletionStatus::CatalogRejected;
            }
        }

        incrementSummary(result.summary, record.status);
        result.records.push_back(record);
    }

    return result;
}

TerrainManifestAssetLoadJobCompletionReconcileResult reconcileTerrainManifestAssetLoadJobCompletions(
    TerrainManifestLoadState& manifestLoad,
    EngineJobQueue& jobs,
    const TerrainManifestAssetLoadJobCompletion* const completions,
    const std::size_t completionCount,
    RendererAssetHandleCatalog& destinationHandles)
{
    TerrainManifestAssetLoadJobCompletionReconcileResult result;

    if (manifestLoad.pendingLoadRequestCount() == 0)
    {
        result.status = TerrainManifestAssetLoadJobCompletionReconcileStatus::NoPendingLoads;
        result.reconcile = reconcileTerrainManifestAssetLoadJobs(
            manifestLoad,
            jobs,
            RendererAssetHandleCatalog{},
            destinationHandles);
        return result;
    }

    RendererAssetHandleCatalog completedHandles;
    result.publish = publishTerrainManifestAssetLoadJobCompletions(
        completions,
        completionCount,
        completedHandles);
    if (publishingFailed(result.publish))
    {
        result.status = TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPublishFailed;
        return result;
    }

    result.reconcile = reconcileTerrainManifestAssetLoadJobs(
        manifestLoad,
        jobs,
        completedHandles,
        destinationHandles);
    result.status = completionStatusFromReconcile(result.reconcile.status);
    return result;
}
} // namespace full_engine
