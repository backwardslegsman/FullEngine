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

void RendererAssetHandleCatalog::clear() noexcept
{
    meshes_.clear();
    materials_.clear();
    textures_.clear();
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
} // namespace full_engine
