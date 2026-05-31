#pragma once

#include "engine/world/WorldChunkRegistry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>

namespace full_engine
{
/** @brief Maximum terrain LOD asset references accepted by engine terrain asset descriptors. */
constexpr std::uint32_t kMaxTerrainAssetLodLevels = 4;

/**
 * @brief Opaque engine-owned runtime asset identity.
 *
 * Asset IDs identify engine catalog entries or cooked asset references. A
 * default value is invalid. This type does not reference renderer handles,
 * files, importer objects, or live renderer resources.
 */
struct AssetId
{
    std::uint64_t value = 0;
};

/** @brief Returns whether an engine asset ID is non-default. */
bool isValid(AssetId id) noexcept;

/** @brief Compares asset IDs for equality. */
bool operator==(AssetId lhs, AssetId rhs) noexcept;

/** @brief Orders asset IDs deterministically for maps and diagnostics. */
bool operator<(AssetId lhs, AssetId rhs) noexcept;

/** @brief One terrain LOD reference expressed in engine asset IDs. */
struct TerrainAssetLodRef
{
    AssetId mesh = {};
    AssetId material = {};
    float maxDistanceMeters = 0.0f;
};

/**
 * @brief Engine-owned terrain asset references for one world chunk.
 *
 * The descriptor stores asset identity only. It does not own or validate live
 * renderer resources. An invalid/default splat map is allowed so later renderer
 * integration can use the renderer's fallback behavior.
 */
struct TerrainChunkAssetDesc
{
    ChunkId id = {};
    std::array<TerrainAssetLodRef, kMaxTerrainAssetLodLevels> lods = {};
    std::uint32_t lodCount = 0;
    AssetId splatMap = {};
};

/** @brief Result code for terrain asset catalog mutations. */
enum class TerrainAssetCatalogResult
{
    Success,
    AlreadyExists,
    NotFound,
    InvalidArgument,
};

/** @brief Validation result for one terrain chunk asset descriptor. */
enum class TerrainAssetValidationResult
{
    Success,
    InvalidLodCount,
    InvalidMeshAsset,
    InvalidMaterialAsset,
    InvalidDistance,
    UnsortedDistance,
};

/**
 * @brief Validates terrain chunk asset references.
 *
 * Active LOD entries must have valid mesh/material asset IDs, finite
 * non-negative distances, and nondecreasing distance order. Inactive LOD
 * entries and the optional splat map are ignored.
 */
TerrainAssetValidationResult validateTerrainChunkAssets(const TerrainChunkAssetDesc& desc);

/**
 * @brief CPU-only catalog of terrain chunk asset references keyed by `ChunkId`.
 *
 * The catalog stores descriptors by value in deterministic chunk order. It
 * does not perform file IO, async loading, importer work, renderer handle
 * lookup, or renderer resource lifetime management.
 */
class TerrainAssetCatalog
{
public:
    /** @brief Adds a valid asset descriptor for an unmapped chunk. */
    TerrainAssetCatalogResult addChunkAssets(const TerrainChunkAssetDesc& desc);

    /** @brief Replaces an existing chunk asset descriptor with a valid descriptor. */
    TerrainAssetCatalogResult updateChunkAssets(const TerrainChunkAssetDesc& desc);

    /** @brief Removes a chunk asset descriptor. */
    TerrainAssetCatalogResult removeChunkAssets(const ChunkId& id);

    /** @brief Returns a read-only descriptor snapshot, or null if the chunk is missing. */
    const TerrainChunkAssetDesc* findChunkAssets(const ChunkId& id) const;

    /** @brief Returns whether terrain asset references are registered for a chunk. */
    bool contains(const ChunkId& id) const;

    /** @brief Returns the number of registered terrain asset descriptors. */
    std::size_t assetCount() const noexcept;

    /** @brief Removes all registered descriptors without touching renderer resources. */
    void clear() noexcept;

private:
    std::map<ChunkId, TerrainChunkAssetDesc> assets_;
};
} // namespace full_engine
