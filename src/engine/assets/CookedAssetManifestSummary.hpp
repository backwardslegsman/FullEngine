#pragma once

#include "engine/assets/CookedAssetManifest.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Count of generic manifest asset records by engine asset kind. */
struct AssetKindSummary
{
    std::size_t meshCount = 0;
    std::size_t materialCount = 0;
    std::size_t textureCount = 0;
    std::size_t terrainChunkCount = 0;
    std::size_t skeletonCount = 0;
    std::size_t skinnedMeshCount = 0;
    std::size_t shaderCount = 0;
};

/**
 * @brief Renderer-free summary of asset references declared by a cooked manifest.
 *
 * The summary reports declared records and referenced asset IDs only. It does
 * not validate the manifest, build catalogs, resolve renderer handles, load
 * files, or retain ownership of manifest data.
 */
struct CookedAssetManifestDependencySummary
{
    std::size_t genericAssetCount = 0;
    std::size_t terrainChunkCount = 0;
    AssetKindSummary assetKinds = {};
    std::size_t genericDependencyReferenceCount = 0;
    std::size_t terrainLodReferenceCount = 0;
    std::size_t defaultSplatMapCount = 0;
    std::vector<AssetId> uniqueGenericDependencies;
    std::vector<AssetId> uniqueTerrainMeshes;
    std::vector<AssetId> uniqueTerrainMaterials;
    std::vector<AssetId> uniqueTerrainSplatMaps;
};

/**
 * @brief Summarizes declared asset references in a cooked asset manifest.
 *
 * Only active dependency and LOD slots are inspected. Default splat map IDs
 * are counted as fallback intent and are not included in unique texture
 * references. Output ID vectors are sorted deterministically.
 */
CookedAssetManifestDependencySummary summarizeCookedAssetManifestDependencies(
    const CookedAssetManifest& manifest);
} // namespace full_engine
