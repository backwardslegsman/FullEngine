#pragma once

#include "engine/assets/TerrainAssetCatalog.hpp"

namespace full_engine
{
/** @brief Result code for validating terrain asset references against generic asset metadata. */
enum class TerrainAssetDependencyValidationResult
{
    Success,
    InvalidTerrainAssets,
    MissingMeshAsset,
    WrongMeshAssetKind,
    MissingMaterialAsset,
    WrongMaterialAssetKind,
    MissingSplatMapAsset,
    WrongSplatMapAssetKind,
};

/**
 * @brief Aggregate result for terrain asset dependency validation.
 *
 * `terrainValidation` preserves the terrain descriptor shape validation detail
 * when `result` is `InvalidTerrainAssets`.
 */
struct TerrainAssetDependencyValidation
{
    TerrainAssetDependencyValidationResult result = TerrainAssetDependencyValidationResult::InvalidTerrainAssets;
    TerrainAssetValidationResult terrainValidation = TerrainAssetValidationResult::Success;
};

/**
 * @brief Validates terrain asset references against a generic asset catalog.
 *
 * Active terrain LOD mesh references must exist as `AssetKind::Mesh`, active
 * material references must exist as `AssetKind::Material`, and a valid splat
 * map reference must exist as `AssetKind::Texture`. A default splat asset ID is
 * accepted as fallback intent. This function performs no catalog mutation,
 * file IO, renderer handle lookup, or renderer resource validation.
 */
TerrainAssetDependencyValidation validateTerrainAssetDependencies(
    const TerrainChunkAssetDesc& terrainAssets,
    const AssetCatalog& assetCatalog);
} // namespace full_engine
