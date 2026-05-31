#pragma once

#include "engine/assets/TerrainAssetCatalog.hpp"
#include "engine/renderer_integration/TerrainResourceCatalog.hpp"

#include <cstddef>
#include <map>

namespace full_engine
{
/** @brief Result code for renderer asset handle catalog mutations. */
enum class RendererAssetHandleCatalogResult
{
    Success,
    AlreadyExists,
    NotFound,
    InvalidArgument,
};

/**
 * @brief CPU-only lookup table from engine asset IDs to public renderer handles.
 *
 * The catalog stores externally supplied renderer handles by value. It does not
 * create, destroy, update, or prove liveness of renderer resources; it only
 * rejects public invalid sentinels before terrain asset resolution.
 */
class RendererAssetHandleCatalog
{
public:
    /** @brief Adds a mesh handle for a previously unmapped engine asset ID. */
    RendererAssetHandleCatalogResult addMeshHandle(AssetId id, full_renderer::MeshHandle handle);

    /** @brief Replaces an existing mesh handle mapping. */
    RendererAssetHandleCatalogResult updateMeshHandle(AssetId id, full_renderer::MeshHandle handle);

    /** @brief Removes a mesh handle mapping. */
    RendererAssetHandleCatalogResult removeMeshHandle(AssetId id);

    /** @brief Returns the mesh handle mapped to an asset ID, or null if missing. */
    const full_renderer::MeshHandle* findMeshHandle(AssetId id) const;

    /** @brief Returns the number of mapped mesh handles. */
    std::size_t meshHandleCount() const noexcept;

    /** @brief Adds a material handle for a previously unmapped engine asset ID. */
    RendererAssetHandleCatalogResult addMaterialHandle(AssetId id, full_renderer::MaterialHandle handle);

    /** @brief Replaces an existing material handle mapping. */
    RendererAssetHandleCatalogResult updateMaterialHandle(AssetId id, full_renderer::MaterialHandle handle);

    /** @brief Removes a material handle mapping. */
    RendererAssetHandleCatalogResult removeMaterialHandle(AssetId id);

    /** @brief Returns the material handle mapped to an asset ID, or null if missing. */
    const full_renderer::MaterialHandle* findMaterialHandle(AssetId id) const;

    /** @brief Returns the number of mapped material handles. */
    std::size_t materialHandleCount() const noexcept;

    /** @brief Adds a texture handle for a previously unmapped engine asset ID. */
    RendererAssetHandleCatalogResult addTextureHandle(AssetId id, full_renderer::TextureHandle handle);

    /** @brief Replaces an existing texture handle mapping. */
    RendererAssetHandleCatalogResult updateTextureHandle(AssetId id, full_renderer::TextureHandle handle);

    /** @brief Removes a texture handle mapping. */
    RendererAssetHandleCatalogResult removeTextureHandle(AssetId id);

    /** @brief Returns the texture handle mapped to an asset ID, or null if missing. */
    const full_renderer::TextureHandle* findTextureHandle(AssetId id) const;

    /** @brief Returns the number of mapped texture handles. */
    std::size_t textureHandleCount() const noexcept;

    /** @brief Removes all handle mappings without touching renderer resources. */
    void clear() noexcept;

private:
    std::map<AssetId, full_renderer::MeshHandle> meshes_;
    std::map<AssetId, full_renderer::MaterialHandle> materials_;
    std::map<AssetId, full_renderer::TextureHandle> textures_;
};

/** @brief Result status for terrain asset ID to renderer handle resolution. */
enum class TerrainAssetResolveStatus
{
    Success,
    MissingChunkAssets,
    InvalidChunkAssets,
    MissingMeshHandle,
    MissingMaterialHandle,
    MissingSplatMapHandle,
};

/**
 * @brief Result of resolving one terrain asset descriptor to renderer handles.
 *
 * On success, `resources` is a valid terrain resource descriptor. On failure,
 * `resources` remains default-initialized so callers do not accidentally
 * consume a partial descriptor.
 */
struct TerrainAssetResolveResult
{
    TerrainAssetResolveStatus status = TerrainAssetResolveStatus::MissingChunkAssets;
    TerrainAssetValidationResult assetValidation = TerrainAssetValidationResult::Success;
    TerrainChunkResourceDesc resources = {};
};

/**
 * @brief Resolves one engine terrain asset descriptor into renderer handle resources.
 *
 * Active LOD mesh/material asset IDs must be present in `handles`. A default
 * splat asset ID resolves to a default texture handle so the renderer can use
 * its terrain splat fallback. Valid splat asset IDs must be present in the
 * texture handle mappings.
 */
TerrainAssetResolveResult resolveTerrainChunkResources(
    const TerrainChunkAssetDesc& assets,
    const RendererAssetHandleCatalog& handles);

/**
 * @brief Looks up one chunk asset descriptor and resolves it into renderer resources.
 *
 * Missing chunk asset descriptors return `MissingChunkAssets`. The function
 * performs no catalog mutation and does not touch renderer resource lifetime.
 */
TerrainAssetResolveResult resolveTerrainChunkResources(
    const TerrainAssetCatalog& assets,
    const ChunkId& id,
    const RendererAssetHandleCatalog& handles);
} // namespace full_engine
