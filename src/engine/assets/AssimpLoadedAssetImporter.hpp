#pragma once

#include "engine/assets/AssetSourceCatalog.hpp"
#include "engine/assets/LoadedAssetPayload.hpp"

namespace full_engine
{
/**
 * @brief Options for importing static mesh payloads through Assimp.
 *
 * This importer is renderer-free and synchronous. Options select Assimp
 * post-processing used before converting supported static meshes into one
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

    /** @brief Accept missing UV set 0 and fill imported `uv0` with zero. */
    bool defaultMissingUv0ToZero = false;
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
 * `AssetKind::Mesh` only and combines all supported static meshes in the scene
 * into one `LoadedMeshAsset` with positions, normals, UV set 0, optional
 * color set 0, and 16-bit triangle indices. Missing UV0 is rejected unless
 * `AssimpLoadedAssetImportOptions::defaultMissingUv0ToZero` is explicitly set.
 * Mesh order and face order follow Assimp's post-processed scene order. Parsed
 * aggregate metadata is checked against `source.descriptor.mesh`, and the
 * final payload is validated with `validateLoadedAssetPayload`.
 *
 * The function copies all payload data by value. It performs no renderer
 * calls, renderer handle lookup, renderer resource creation, texture decoding,
 * material import, tangent import, UV1+ import, skeletal import, animation import, async
 * scheduling, or source catalog mutation.
 */
AssimpLoadedAssetImportResult importLoadedAssetPayloadWithAssimp(
    const AssetSourceRecord& source,
    const AssimpLoadedAssetImportOptions& options = {});
} // namespace full_engine
