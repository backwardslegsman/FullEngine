#pragma once

#include "engine/assets/AssetSourceCatalog.hpp"
#include "engine/assets/LoadedAssetPayload.hpp"

namespace full_engine
{
/**
 * @brief Result status for the dev-only loaded asset importer.
 *
 * This importer is a small source-file proof for tests and development assets.
 * It is not a production content format and performs no renderer work, async
 * scheduling, package lookup, URI normalization, or resource creation.
 */
enum class LoadedAssetImportStatus
{
    /** @brief The source file was parsed and converted into a valid loaded payload. */
    Success,

    /** @brief The call arguments were structurally invalid. */
    InvalidArgument,

    /** @brief The source record failed source metadata validation. */
    SourceValidationFailed,

    /** @brief The source URI could not be opened as a filesystem path. */
    IoError,

    /** @brief The dev source file was malformed or used an unknown token/value. */
    ParseError,

    /** @brief Parsed metadata did not match the source record descriptor. */
    DescriptorMismatch,

    /** @brief The parsed payload failed loaded payload validation. */
    PayloadValidationFailed,

    /** @brief The source kind is not supported by this importer. */
    UnsupportedKind,
};

/** @brief Result of importing one dev asset source file into a loaded payload. */
struct LoadedAssetImportResult
{
    /** @brief Import status. */
    LoadedAssetImportStatus status = LoadedAssetImportStatus::InvalidArgument;

    /** @brief Source record validation detail. */
    AssetSourceRecordValidationResult sourceValidation =
        AssetSourceRecordValidationResult::Success;

    /** @brief Loaded payload validation detail. */
    LoadedAssetPayloadValidationResult payloadValidation =
        LoadedAssetPayloadValidationResult::Success;

    /** @brief Imported payload when `status` is `Success`; otherwise diagnostic-only default data. */
    LoadedAssetPayload payload = {};
};

/** @brief Returns a stable diagnostic name for a loaded asset import status. */
const char* loadedAssetImportStatusName(LoadedAssetImportStatus status) noexcept;

/**
 * @brief Imports a renderer-free loaded payload from a tiny dev asset file.
 *
 * `source.uri` is treated as a direct filesystem path for this development
 * importer only. The function validates the source record, parses a strict
 * ASCII fixture format for Mesh/Texture/Material assets, compares parsed
 * metadata against the source descriptor, and validates the resulting
 * `LoadedAssetPayload`.
 *
 * The function copies all payload data by value. It does not include renderer
 * headers, create renderer handles/resources, mutate catalogs, start jobs,
 * perform async work, or interpret production asset/package URIs.
 *
 * @param source Source metadata and filesystem URI to import.
 * @return Imported payload plus ordered validation/import diagnostics.
 */
LoadedAssetImportResult importLoadedAssetPayloadFromDevFile(
    const AssetSourceRecord& source);
} // namespace full_engine
