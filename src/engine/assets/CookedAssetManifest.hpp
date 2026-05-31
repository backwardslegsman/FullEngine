#pragma once

#include "engine/assets/TerrainAssetDependencyValidator.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/**
 * @brief In-memory cooked asset manifest shape for engine asset metadata.
 *
 * The manifest owns renderer-free value records only. It does not contain file
 * paths, serialized bytes, importer state, renderer handles, live resources, or
 * async loading state. Future file parsers can target this shape before
 * catalog validation and runtime resource resolution.
 */
struct CookedAssetManifest
{
    /** @brief Generic asset metadata records declared by the manifest. */
    std::vector<AssetRecord> assets;

    /** @brief Terrain chunk asset-reference records declared by the manifest. */
    std::vector<TerrainChunkAssetDesc> terrainChunks;
};

/**
 * @brief Catalog values built from a validated cooked asset manifest.
 *
 * Catalogs are returned by value. Building a manifest never mutates
 * caller-owned catalogs, renderer handle catalogs, queues, runtime state, or
 * renderer resources.
 */
struct CookedAssetManifestCatalogs
{
    AssetCatalog assets;
    TerrainAssetCatalog terrainAssets;
};

/** @brief Top-level validation result for an in-memory cooked asset manifest. */
enum class CookedAssetManifestValidationResult
{
    Success,
    InvalidAssetRecord,
    DuplicateAssetId,
    InvalidTerrainAssets,
    DuplicateTerrainChunk,
    InvalidTerrainDependencies,
};

/**
 * @brief Detailed validation result for a cooked asset manifest.
 *
 * `assetIndex` and `terrainChunkIndex` identify the first failing manifest
 * record when applicable. They are set to `invalidIndex` when the failure does
 * not apply to that record kind. Nested validation fields preserve lower-level
 * catalog or dependency diagnostics for callers that need specific messages.
 */
struct CookedAssetManifestValidation
{
    static constexpr std::size_t invalidIndex = static_cast<std::size_t>(-1);

    CookedAssetManifestValidationResult result = CookedAssetManifestValidationResult::Success;
    std::size_t assetIndex = invalidIndex;
    std::size_t terrainChunkIndex = invalidIndex;
    AssetRecordValidationResult assetValidation = AssetRecordValidationResult::Success;
    TerrainAssetValidationResult terrainValidation = TerrainAssetValidationResult::Success;
    TerrainAssetDependencyValidationResult terrainDependencyValidation =
        TerrainAssetDependencyValidationResult::Success;
};

/**
 * @brief Result of validating and building catalogs from a cooked manifest.
 *
 * On validation failure, `catalogs` remains default-empty. On success, it
 * contains value catalogs populated from the manifest records.
 */
struct CookedAssetManifestBuildResult
{
    CookedAssetManifestValidation validation;
    CookedAssetManifestCatalogs catalogs;
};

/**
 * @brief Validates manifest records and terrain dependencies deterministically.
 *
 * Generic assets are validated first in manifest order, then terrain chunk
 * asset descriptors are validated in manifest order. The first failure is
 * returned with its record index and nested diagnostic detail.
 */
CookedAssetManifestValidation validateCookedAssetManifest(const CookedAssetManifest& manifest);

/**
 * @brief Builds generic and terrain asset catalogs from a valid manifest.
 *
 * This helper performs validation first. Failed builds return empty catalogs
 * and do not mutate any caller-owned state.
 */
CookedAssetManifestBuildResult buildCatalogsFromCookedAssetManifest(const CookedAssetManifest& manifest);
} // namespace full_engine
