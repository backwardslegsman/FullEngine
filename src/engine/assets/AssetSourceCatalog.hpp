#pragma once

#include "engine/assets/AssetCatalog.hpp"
#include "engine/assets/AssetSourceDescriptor.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace full_engine
{
/**
 * @brief Renderer-free source descriptor for one loadable engine asset.
 *
 * The URI is an opaque caller-owned identifier copied into the catalog. It may
 * represent a relative path, package entry, virtual asset key, or other loader
 * source convention. This record does not contain serialized bytes, importer
 * state, renderer handles, live renderer resources, or async ownership.
 */
struct AssetSourceRecord
{
    /** @brief Engine asset ID whose source is described. A default ID is invalid. */
    AssetId id = {};

    /** @brief Expected loadable asset kind. Mesh, Material, Texture, Skeleton, and SkinnedMesh are valid. */
    AssetKind kind = AssetKind::Unknown;

    /** @brief Opaque source URI copied by value. Empty URIs are invalid. */
    std::string uri;

    /** @brief Renderer-free expected source contents for this asset kind. */
    AssetSourceDescriptor descriptor = {};
};

/** @brief Result code for source catalog mutations. */
enum class AssetSourceCatalogResult
{
    Success,
    AlreadyExists,
    NotFound,
    InvalidArgument,
};

/** @brief Validation result for one asset source record. */
enum class AssetSourceRecordValidationResult
{
    Success,
    InvalidAssetId,
    InvalidKind,
    EmptyUri,
    InvalidDescriptor,
};

/** @brief Returns a stable diagnostic name for a source catalog result. */
const char* assetSourceCatalogResultName(AssetSourceCatalogResult result) noexcept;

/** @brief Returns a stable diagnostic name for a source record validation result. */
const char* assetSourceRecordValidationResultName(AssetSourceRecordValidationResult result) noexcept;

/**
 * @brief Validates renderer-free source metadata for a loadable asset.
 *
 * Valid records use a non-default asset ID, a loadable kind (`Mesh`,
 * `Material`, `Texture`, `Skeleton`, or `SkinnedMesh`), a non-empty URI, and
 * valid descriptor metadata for the active kind. The URI is not normalized,
 * opened, parsed, or interpreted by this helper. Descriptor metadata is
 * declarative and is not checked against source bytes.
 */
AssetSourceRecordValidationResult validateAssetSourceRecord(const AssetSourceRecord& record);

/**
 * @brief CPU-only catalog of loadable asset source descriptors.
 *
 * The catalog stores source records by value in deterministic asset ID order.
 * It performs no file IO, path normalization, package parsing, importing,
 * async work, renderer handle lookup, or renderer resource creation.
 *
 * @note Thread safety: Not thread-safe. Callers must serialize mutation and
 * access.
 */
class AssetSourceCatalog
{
public:
    /** @brief Adds a valid source record for an unmapped asset ID. */
    AssetSourceCatalogResult addSource(const AssetSourceRecord& record);

    /** @brief Replaces an existing source record with a valid record. */
    AssetSourceCatalogResult updateSource(const AssetSourceRecord& record);

    /** @brief Removes a source record for a valid asset ID. */
    AssetSourceCatalogResult removeSource(AssetId id);

    /** @brief Returns a read-only source record, or null if the asset is missing. */
    const AssetSourceRecord* findSource(AssetId id) const;

    /** @brief Returns whether a source record is registered for a valid asset ID. */
    bool contains(AssetId id) const;

    /** @brief Returns the number of registered source records. */
    std::size_t sourceCount() const noexcept;

    /** @brief Returns a copied snapshot of source records in deterministic asset ID order. */
    std::vector<AssetSourceRecord> sources() const;

    /** @brief Removes all registered source records. */
    void clear() noexcept;

private:
    std::map<AssetId, AssetSourceRecord> sources_;
};
} // namespace full_engine
