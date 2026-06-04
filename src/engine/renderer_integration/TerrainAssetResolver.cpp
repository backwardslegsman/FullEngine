#include "engine/renderer_integration/TerrainAssetResolver.hpp"

namespace full_engine
{
namespace
{
template <typename Handle>
RendererAssetHandleCatalogResult addHandle(
    std::map<AssetId, Handle>& handles,
    const AssetId id,
    const Handle handle)
{
    if (!isValid(id) || !full_renderer::isValid(handle))
    {
        return RendererAssetHandleCatalogResult::InvalidArgument;
    }

    const auto inserted = handles.emplace(id, handle);
    return inserted.second ? RendererAssetHandleCatalogResult::Success
                           : RendererAssetHandleCatalogResult::AlreadyExists;
}

template <typename Handle>
RendererAssetHandleCatalogResult updateHandle(
    std::map<AssetId, Handle>& handles,
    const AssetId id,
    const Handle handle)
{
    if (!isValid(id) || !full_renderer::isValid(handle))
    {
        return RendererAssetHandleCatalogResult::InvalidArgument;
    }

    auto found = handles.find(id);
    if (found == handles.end())
    {
        return RendererAssetHandleCatalogResult::NotFound;
    }

    found->second = handle;
    return RendererAssetHandleCatalogResult::Success;
}

template <typename Handle>
RendererAssetHandleCatalogResult removeHandle(std::map<AssetId, Handle>& handles, const AssetId id)
{
    if (!isValid(id))
    {
        return RendererAssetHandleCatalogResult::InvalidArgument;
    }

    return handles.erase(id) > 0 ? RendererAssetHandleCatalogResult::Success
                                 : RendererAssetHandleCatalogResult::NotFound;
}

template <typename Handle>
const Handle* findHandle(const std::map<AssetId, Handle>& handles, const AssetId id)
{
    const auto found = handles.find(id);
    return found == handles.end() ? nullptr : &found->second;
}

TerrainAssetBatchResolveStatus batchStatusFromResolve(const TerrainAssetResolveStatus status) noexcept
{
    switch (status)
    {
    case TerrainAssetResolveStatus::Success:
        return TerrainAssetBatchResolveStatus::Resolved;
    case TerrainAssetResolveStatus::MissingChunkAssets:
        return TerrainAssetBatchResolveStatus::MissingChunkAssets;
    case TerrainAssetResolveStatus::InvalidChunkAssets:
        return TerrainAssetBatchResolveStatus::InvalidChunkAssets;
    case TerrainAssetResolveStatus::MissingMeshHandle:
        return TerrainAssetBatchResolveStatus::MissingMeshHandle;
    case TerrainAssetResolveStatus::MissingMaterialHandle:
        return TerrainAssetBatchResolveStatus::MissingMaterialHandle;
    case TerrainAssetResolveStatus::MissingSplatMapHandle:
        return TerrainAssetBatchResolveStatus::MissingSplatMapHandle;
    }

    return TerrainAssetBatchResolveStatus::MissingChunkAssets;
}

void incrementSummary(
    TerrainAssetBatchResolveSummary& summary,
    const TerrainAssetBatchResolveStatus status,
    const std::size_t amount = 1) noexcept
{
    switch (status)
    {
    case TerrainAssetBatchResolveStatus::Resolved:
        summary.resolvedCount += amount;
        break;
    case TerrainAssetBatchResolveStatus::MissingChunkAssets:
        summary.missingChunkAssetsCount += amount;
        break;
    case TerrainAssetBatchResolveStatus::InvalidChunkAssets:
        summary.invalidChunkAssetsCount += amount;
        break;
    case TerrainAssetBatchResolveStatus::MissingMeshHandle:
        summary.missingMeshHandleCount += amount;
        break;
    case TerrainAssetBatchResolveStatus::MissingMaterialHandle:
        summary.missingMaterialHandleCount += amount;
        break;
    case TerrainAssetBatchResolveStatus::MissingSplatMapHandle:
        summary.missingSplatMapHandleCount += amount;
        break;
    case TerrainAssetBatchResolveStatus::ResourceCatalogFailed:
        summary.resourceCatalogFailedCount += amount;
        break;
    }
}
} // namespace

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::addMeshHandle(
    const AssetId id,
    const full_renderer::MeshHandle handle)
{
    return addHandle(meshes_, id, handle);
}

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::updateMeshHandle(
    const AssetId id,
    const full_renderer::MeshHandle handle)
{
    return updateHandle(meshes_, id, handle);
}

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::removeMeshHandle(const AssetId id)
{
    return removeHandle(meshes_, id);
}

const full_renderer::MeshHandle* RendererAssetHandleCatalog::findMeshHandle(const AssetId id) const
{
    return findHandle(meshes_, id);
}

std::size_t RendererAssetHandleCatalog::meshHandleCount() const noexcept
{
    return meshes_.size();
}

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::addMaterialHandle(
    const AssetId id,
    const full_renderer::MaterialHandle handle)
{
    return addHandle(materials_, id, handle);
}

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::updateMaterialHandle(
    const AssetId id,
    const full_renderer::MaterialHandle handle)
{
    return updateHandle(materials_, id, handle);
}

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::removeMaterialHandle(const AssetId id)
{
    return removeHandle(materials_, id);
}

const full_renderer::MaterialHandle* RendererAssetHandleCatalog::findMaterialHandle(const AssetId id) const
{
    return findHandle(materials_, id);
}

std::size_t RendererAssetHandleCatalog::materialHandleCount() const noexcept
{
    return materials_.size();
}

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::addTextureHandle(
    const AssetId id,
    const full_renderer::TextureHandle handle)
{
    return addHandle(textures_, id, handle);
}

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::updateTextureHandle(
    const AssetId id,
    const full_renderer::TextureHandle handle)
{
    return updateHandle(textures_, id, handle);
}

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::removeTextureHandle(const AssetId id)
{
    return removeHandle(textures_, id);
}

const full_renderer::TextureHandle* RendererAssetHandleCatalog::findTextureHandle(const AssetId id) const
{
    return findHandle(textures_, id);
}

std::size_t RendererAssetHandleCatalog::textureHandleCount() const noexcept
{
    return textures_.size();
}

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::addSkeletonHandle(
    const AssetId id,
    const full_renderer::SkeletonHandle handle)
{
    return addHandle(skeletons_, id, handle);
}

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::updateSkeletonHandle(
    const AssetId id,
    const full_renderer::SkeletonHandle handle)
{
    return updateHandle(skeletons_, id, handle);
}

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::removeSkeletonHandle(const AssetId id)
{
    return removeHandle(skeletons_, id);
}

const full_renderer::SkeletonHandle* RendererAssetHandleCatalog::findSkeletonHandle(const AssetId id) const
{
    return findHandle(skeletons_, id);
}

std::size_t RendererAssetHandleCatalog::skeletonHandleCount() const noexcept
{
    return skeletons_.size();
}

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::addSkinnedMeshHandle(
    const AssetId id,
    const full_renderer::SkinnedMeshHandle handle)
{
    return addHandle(skinnedMeshes_, id, handle);
}

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::updateSkinnedMeshHandle(
    const AssetId id,
    const full_renderer::SkinnedMeshHandle handle)
{
    return updateHandle(skinnedMeshes_, id, handle);
}

RendererAssetHandleCatalogResult RendererAssetHandleCatalog::removeSkinnedMeshHandle(const AssetId id)
{
    return removeHandle(skinnedMeshes_, id);
}

const full_renderer::SkinnedMeshHandle* RendererAssetHandleCatalog::findSkinnedMeshHandle(const AssetId id) const
{
    return findHandle(skinnedMeshes_, id);
}

std::size_t RendererAssetHandleCatalog::skinnedMeshHandleCount() const noexcept
{
    return skinnedMeshes_.size();
}

void RendererAssetHandleCatalog::clear() noexcept
{
    meshes_.clear();
    materials_.clear();
    textures_.clear();
    skeletons_.clear();
    skinnedMeshes_.clear();
}

TerrainAssetResolveResult resolveTerrainChunkResources(
    const TerrainChunkAssetDesc& assets,
    const RendererAssetHandleCatalog& handles)
{
    TerrainAssetResolveResult result;

    result.assetValidation = validateTerrainChunkAssets(assets);
    if (result.assetValidation != TerrainAssetValidationResult::Success)
    {
        result.status = TerrainAssetResolveStatus::InvalidChunkAssets;
        return result;
    }

    TerrainChunkResourceDesc resolved;
    resolved.id = assets.id;
    resolved.lodCount = assets.lodCount;

    for (std::uint32_t index = 0; index < assets.lodCount; ++index)
    {
        const TerrainAssetLodRef& sourceLod = assets.lods[index];
        TerrainResourceLod& targetLod = resolved.lods[index];

        const full_renderer::MeshHandle* mesh = handles.findMeshHandle(sourceLod.mesh);
        if (mesh == nullptr)
        {
            result.status = TerrainAssetResolveStatus::MissingMeshHandle;
            return result;
        }

        const full_renderer::MaterialHandle* material = handles.findMaterialHandle(sourceLod.material);
        if (material == nullptr)
        {
            result.status = TerrainAssetResolveStatus::MissingMaterialHandle;
            return result;
        }

        targetLod.mesh = *mesh;
        targetLod.material = *material;
        targetLod.maxDistanceMeters = sourceLod.maxDistanceMeters;
    }

    if (isValid(assets.splatMap))
    {
        const full_renderer::TextureHandle* texture = handles.findTextureHandle(assets.splatMap);
        if (texture == nullptr)
        {
            result.status = TerrainAssetResolveStatus::MissingSplatMapHandle;
            return result;
        }

        resolved.splatMap = *texture;
    }

    result.status = TerrainAssetResolveStatus::Success;
    result.resources = resolved;
    return result;
}

TerrainAssetResolveResult resolveTerrainChunkResources(
    const TerrainAssetCatalog& assets,
    const ChunkId& id,
    const RendererAssetHandleCatalog& handles)
{
    const TerrainChunkAssetDesc* desc = assets.findChunkAssets(id);
    if (desc == nullptr)
    {
        return {};
    }

    return resolveTerrainChunkResources(*desc, handles);
}

TerrainAssetBatchResolveResult resolveTerrainResourceCatalog(
    const TerrainAssetCatalog& assets,
    const ChunkId* ids,
    const std::size_t idCount,
    const RendererAssetHandleCatalog& handles)
{
    TerrainAssetBatchResolveResult result;
    if (ids == nullptr)
    {
        if (idCount > 0)
        {
            incrementSummary(
                result.summary,
                TerrainAssetBatchResolveStatus::MissingChunkAssets,
                idCount);
        }
        return result;
    }

    result.records.reserve(idCount);
    for (std::size_t index = 0; index < idCount; ++index)
    {
        TerrainAssetBatchResolveRecord record;
        record.id = ids[index];
        record.sourceResolve = resolveTerrainChunkResources(assets, record.id, handles);
        record.status = batchStatusFromResolve(record.sourceResolve.status);

        if (record.status == TerrainAssetBatchResolveStatus::Resolved)
        {
            record.resourceResult = result.resources.addChunkResources(record.sourceResolve.resources);
            if (record.resourceResult != TerrainResourceResult::Success)
            {
                record.status = TerrainAssetBatchResolveStatus::ResourceCatalogFailed;
            }
        }

        incrementSummary(result.summary, record.status);
        result.records.push_back(record);
    }

    return result;
}
} // namespace full_engine
