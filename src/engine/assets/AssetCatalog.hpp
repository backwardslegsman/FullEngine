#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>

namespace full_engine
{
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

/** @brief Engine-side category for a runtime or cooked asset identity. */
enum class AssetKind
{
    Unknown,
    Mesh,
    Material,
    Texture,
    TerrainChunk,
    Skeleton,
    SkinnedMesh,
    Shader,
};

/** @brief Maximum dependency references stored in one generic asset record. */
constexpr std::uint32_t kMaxAssetDependencies = 8;

/** @brief One declared dependency from an asset to another engine asset ID. */
struct AssetDependencyRef
{
    AssetId id = {};
    AssetKind kind = AssetKind::Unknown;
};

/**
 * @brief Renderer-free metadata record for one engine asset identity.
 *
 * Records describe identity, kind, and intended dependencies only. They do not
 * contain paths, importer state, renderer handles, live resources, or IO state.
 */
struct AssetRecord
{
    AssetId id = {};
    AssetKind kind = AssetKind::Unknown;
    std::array<AssetDependencyRef, kMaxAssetDependencies> dependencies = {};
    std::uint32_t dependencyCount = 0;
};

/** @brief Result code for generic asset catalog mutations. */
enum class AssetCatalogResult
{
    Success,
    AlreadyExists,
    NotFound,
    InvalidArgument,
};

/** @brief Validation result for one generic asset record. */
enum class AssetRecordValidationResult
{
    Success,
    InvalidAssetId,
    InvalidKind,
    InvalidDependencyCount,
    InvalidDependencyId,
    InvalidDependencyKind,
};

/**
 * @brief Validates renderer-free generic asset metadata.
 *
 * The asset ID and kind must be valid. Active dependencies must have valid IDs
 * and kinds. Inactive dependency slots are ignored.
 */
AssetRecordValidationResult validateAssetRecord(const AssetRecord& record);

/**
 * @brief CPU-only catalog of generic engine asset metadata keyed by `AssetId`.
 *
 * The catalog stores records by value in deterministic asset ID order. It does
 * not perform file IO, manifest parsing, importing, async loading, renderer
 * handle lookup, or renderer resource lifetime management.
 */
class AssetCatalog
{
public:
    /** @brief Adds a valid asset record for an unmapped asset ID. */
    AssetCatalogResult addAsset(const AssetRecord& record);

    /** @brief Replaces an existing asset record with a valid record. */
    AssetCatalogResult updateAsset(const AssetRecord& record);

    /** @brief Removes an asset record. */
    AssetCatalogResult removeAsset(AssetId id);

    /** @brief Returns a read-only record snapshot, or null if the asset is missing. */
    const AssetRecord* findAsset(AssetId id) const;

    /** @brief Returns whether metadata is registered for an asset ID. */
    bool contains(AssetId id) const;

    /** @brief Returns the number of registered asset records. */
    std::size_t assetCount() const noexcept;

    /** @brief Removes all registered records without touching runtime resources. */
    void clear() noexcept;

private:
    std::map<AssetId, AssetRecord> assets_;
};
} // namespace full_engine
