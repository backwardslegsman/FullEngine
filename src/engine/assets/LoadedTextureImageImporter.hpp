#pragma once

#include "engine/assets/AssetSourceCatalog.hpp"
#include "engine/assets/LoadedAssetPayload.hpp"

namespace full_engine
{
/**
 * @brief Result status for importing one direct texture image file.
 *
 * The importer is renderer-free and synchronous. It decodes source image bytes
 * into the existing single-mip RGBA8 `LoadedTextureAsset` payload contract and
 * performs no renderer calls, renderer handle lookup, resource creation,
 * package lookup, mip generation, compression, glTF material parsing, or async
 * scheduling.
 */
enum class LoadedTextureImageImportStatus
{
    Success,
    InvalidArgument,
    SourceValidationFailed,
    IoError,
    DecodeError,
    DescriptorMismatch,
    PayloadValidationFailed,
    UnsupportedKind,
};

/** @brief Result of importing one direct image file into a loaded texture payload. */
struct LoadedTextureImageImportResult
{
    /** @brief Import status. */
    LoadedTextureImageImportStatus status = LoadedTextureImageImportStatus::InvalidArgument;

    /** @brief Source record validation detail. */
    AssetSourceRecordValidationResult sourceValidation =
        AssetSourceRecordValidationResult::Success;

    /** @brief Loaded payload validation detail. */
    LoadedAssetPayloadValidationResult payloadValidation =
        LoadedAssetPayloadValidationResult::Success;

    /** @brief Imported texture payload when `status` is `Success`; otherwise diagnostic-only data. */
    LoadedAssetPayload payload = {};
};

/** @brief Returns a stable diagnostic name for a texture image import status. */
const char* loadedTextureImageImportStatusName(
    LoadedTextureImageImportStatus status) noexcept;

/**
 * @brief Imports a renderer-free texture payload from a direct image file.
 *
 * `source.uri` is treated as a direct filesystem path. V1 supports
 * `AssetKind::Texture` only and decodes images into tightly packed RGBA8 bytes
 * with one mip. Width, height, mip count, and format are checked against
 * `source.descriptor.texture`; semantic and color-space metadata are copied
 * from that descriptor and validated as authored policy.
 *
 * The function copies decoded bytes by value. It does not expose stb headers,
 * retain decoder memory, normalize paths, infer color policy, resolve glTF
 * image references, create renderer resources, mutate catalogs, or start jobs.
 */
LoadedTextureImageImportResult importLoadedTexturePayloadFromImageFile(
    const AssetSourceRecord& source);
} // namespace full_engine
