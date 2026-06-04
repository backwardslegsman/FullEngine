#pragma once

#include "engine/assets/AssetSourceCatalog.hpp"
#include "engine/assets/LoadedAssetPayload.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace full_engine
{
/**
 * @brief Options for extracting material and texture source metadata from glTF.
 *
 * The extractor is renderer-free and synchronous. It reads glTF material
 * bindings through Assimp, maps direct image references into
 * `AssetSourceRecord` texture sources, and emits `LoadedMaterialAsset`
 * payloads whose named texture references are engine asset IDs. It performs no
 * image decoding beyond metadata inspection, renderer calls, renderer handle
 * lookup, renderer resource creation, material graph evaluation, async work,
 * or source catalog mutation.
 */
struct GltfMaterialAssetImportOptions
{
    /** @brief First material asset ID assigned to glTF material index zero. */
    AssetId firstMaterialId = {};

    /** @brief First texture asset ID assigned to the first unique referenced image. */
    AssetId firstTextureId = {};

    /** @brief Material model assigned to extracted material payloads. */
    AssetSourceMaterialModel materialModel = AssetSourceMaterialModel::Basic;

    /** @brief Texture semantic assigned to extracted base-color texture sources. */
    AssetSourceTextureSemantic baseColorTextureSemantic = AssetSourceTextureSemantic::Color;

    /** @brief Texture color-space assigned to extracted base-color texture sources. */
    AssetSourceTextureColorSpace baseColorTextureColorSpace = AssetSourceTextureColorSpace::Srgb;
};

/** @brief Top-level result status for glTF material source extraction. */
enum class GltfMaterialAssetImportStatus
{
    Success,
    InvalidArgument,
    IoError,
    ParseError,
    UnsupportedScene,
};

/** @brief Per-material extraction status. */
enum class GltfMaterialAssetImportRecordStatus
{
    Planned,
    NoBaseColorTexture,
    TextureInfoFailed,
    SourceValidationFailed,
    PayloadValidationFailed,
};

/** @brief Ordered diagnostic record for one glTF material. */
struct GltfMaterialAssetImportRecord
{
    /** @brief Per-material extraction status. */
    GltfMaterialAssetImportRecordStatus status = GltfMaterialAssetImportRecordStatus::Planned;

    /** @brief Zero-based Assimp/glTF material index. */
    std::uint32_t materialIndex = 0;

    /** @brief Assigned material asset ID. */
    AssetId materialId = {};

    /** @brief Assigned base-color texture asset ID when a texture was mapped. */
    AssetId baseColorTextureId = {};

    /** @brief Resolved direct image URI when a base-color texture was mapped. */
    std::string baseColorTextureUri;

    /** @brief Named texture refs assigned to this material payload. */
    std::array<AssetSourceMaterialTextureRef, kMaxAssetSourceMaterialTextureRefs> textureRefs = {};

    /** @brief Number of active entries in `textureRefs`. */
    std::uint32_t textureRefCount = 0;

    /** @brief Texture source validation detail for mapped base-color textures. */
    AssetSourceRecordValidationResult textureSourceValidation =
        AssetSourceRecordValidationResult::Success;

    /** @brief Material source validation detail. */
    AssetSourceRecordValidationResult materialSourceValidation =
        AssetSourceRecordValidationResult::Success;

    /** @brief Material payload validation detail. */
    LoadedAssetPayloadValidationResult payloadValidation =
        LoadedAssetPayloadValidationResult::Success;
};

/** @brief Summary counters for glTF material source extraction. */
struct GltfMaterialAssetImportSummary
{
    std::uint32_t plannedMaterialCount = 0;
    std::uint32_t noBaseColorTextureCount = 0;
    std::uint32_t textureInfoFailedCount = 0;
    std::uint32_t sourceValidationFailedCount = 0;
    std::uint32_t payloadValidationFailedCount = 0;
    std::uint32_t emittedTextureSourceCount = 0;
    std::uint32_t emittedMaterialSourceCount = 0;
    std::uint32_t emittedMaterialPayloadCount = 0;
};

/** @brief Result of extracting renderer-free material payloads and texture sources from one glTF file. */
struct GltfMaterialAssetImportResult
{
    /** @brief Top-level import status. */
    GltfMaterialAssetImportStatus status = GltfMaterialAssetImportStatus::InvalidArgument;

    /** @brief Ordered per-material diagnostic records. */
    std::vector<GltfMaterialAssetImportRecord> records;

    /** @brief Summary counters. */
    GltfMaterialAssetImportSummary summary = {};

    /** @brief Texture and material source records emitted in dependency order. */
    std::vector<AssetSourceRecord> sourceRecords;

    /** @brief Material payloads emitted in glTF material order. */
    std::vector<LoadedAssetPayload> materialPayloads;
};

/** @brief Returns a stable diagnostic name for a glTF material import status. */
const char* gltfMaterialAssetImportStatusName(
    GltfMaterialAssetImportStatus status) noexcept;

/** @brief Returns a stable diagnostic name for a glTF material import record status. */
const char* gltfMaterialAssetImportRecordStatusName(
    GltfMaterialAssetImportRecordStatus status) noexcept;

/**
 * @brief Extracts material payloads and direct texture source records from a glTF file.
 *
 * `uri` is treated as a direct filesystem path to a glTF file. Base-color
 * texture image paths are resolved relative to that file and inspected for
 * width/height only; actual image decoding remains owned by
 * `importLoadedTexturePayloadFromImageFile`. Returned source records and
 * payloads are copied values. The function does not mutate catalogs, retain
 * Assimp scene pointers, resolve renderer handles, create resources, or run
 * async work.
 */
GltfMaterialAssetImportResult importGltfMaterialAssetSources(
    const std::string& uri,
    const GltfMaterialAssetImportOptions& options);
} // namespace full_engine
