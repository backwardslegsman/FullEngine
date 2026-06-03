#pragma once

#include "engine/assets/AssetSourceCatalog.hpp"
#include "engine/assets/LoadedAssetPayload.hpp"

namespace full_engine
{
/**
 * @brief Options for importing static mesh payloads through Assimp.
 *
 * This importer is renderer-free and synchronous. Options select Assimp
 * post-processing used before converting the first supported static mesh into
 * `LoadedAssetPayload`. The importer does not create renderer resources, own
 * async work, decode textures, import materials, or retain Assimp scene data.
 */
struct AssimpLoadedAssetImportOptions
{
    /** @brief Request Assimp triangle conversion before payload conversion. */
    bool triangulate = true;

    /** @brief Request identical vertex joining before payload conversion. */
    bool joinIdenticalVertices = true;

    /** @brief Request Assimp scene validation before payload conversion. */
    bool validateDataStructure = true;

    /** @brief Generate normals through Assimp when the source mesh omits them. */
    bool generateMissingNormals = false;
};

/** @brief Result status for importing one asset source through Assimp. */
enum class AssimpLoadedAssetImportStatus
{
    Success,
    InvalidArgument,
    SourceValidationFailed,
    IoError,
    ParseError,
    UnsupportedScene,
    DescriptorMismatch,
    PayloadValidationFailed,
    UnsupportedKind,
};

/** @brief Result of importing one Assimp-supported source into a loaded payload. */
struct AssimpLoadedAssetImportResult
{
    /** @brief Import status. */
    AssimpLoadedAssetImportStatus status = AssimpLoadedAssetImportStatus::InvalidArgument;

    /** @brief Source record validation detail. */
    AssetSourceRecordValidationResult sourceValidation =
        AssetSourceRecordValidationResult::Success;

    /** @brief Loaded payload validation detail. */
    LoadedAssetPayloadValidationResult payloadValidation =
        LoadedAssetPayloadValidationResult::Success;

    /** @brief Imported payload when `status` is `Success`; otherwise default diagnostic data. */
    LoadedAssetPayload payload = {};
};

/** @brief Returns a stable diagnostic name for an Assimp import status. */
const char* assimpLoadedAssetImportStatusName(
    AssimpLoadedAssetImportStatus status) noexcept;

/**
 * @brief Imports a renderer-free static mesh payload through Assimp.
 *
 * `source.uri` is treated as a direct filesystem path. V1 supports
 * `AssetKind::Mesh` only and converts one static mesh with positions, normals,
 * optional vertex colors, and 16-bit triangle indices into `LoadedMeshAsset`.
 * Parsed metadata is checked against `source.descriptor.mesh`, and the final
 * payload is validated with `validateLoadedAssetPayload`.
 *
 * The function copies all payload data by value. It performs no renderer
 * calls, renderer handle lookup, renderer resource creation, texture decoding,
 * material import, skeletal import, animation import, async scheduling, or
 * source catalog mutation.
 */
AssimpLoadedAssetImportResult importLoadedAssetPayloadWithAssimp(
    const AssetSourceRecord& source,
    const AssimpLoadedAssetImportOptions& options = {});
} // namespace full_engine
