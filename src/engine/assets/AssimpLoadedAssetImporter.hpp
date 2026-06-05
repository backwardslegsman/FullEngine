#pragma once

#include "engine/assets/AssetSourceCatalog.hpp"
#include "engine/assets/LoadedAssetPayload.hpp"

namespace full_engine
{
/** @brief Selects how Assimp scene data is mapped to loaded skeleton joints. */
enum class AssimpSkeletonSourceMode
{
    /** @brief Build skeletons from `aiMesh::mBones`; preserves existing glTF/skinned behavior. */
    MeshBones,

    /**
     * @brief Build skeletons from scene nodes targeted by animation channels.
     *
     * This mode is intended for FBX characterization/import paths where Assimp
     * exposes animated scene nodes but no mesh skin bones. Imported inverse bind
     * poses are identity matrices because skin bind data is not available.
     */
    AnimatedSceneNodes,
};

/**
 * @brief Options for importing mesh payloads through Assimp.
 *
 * This importer is renderer-free and synchronous. Options select Assimp
 * post-processing and animation conversion behavior used before converting
 * supported Assimp scene data into one `LoadedAssetPayload`. The importer does
 * not create renderer resources, own async work, decode textures, import
 * materials, evaluate animation clips, or retain Assimp scene data.
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

    /**
     * @brief Generate tangent-space data through Assimp when the source mesh omits tangents.
     *
     * Disabled by default so production imports preserve authored tangent bases
     * and fail early when normal-map-ready mesh data is incomplete. Generation
     * depends on valid normals and UV0 data.
     */
    bool generateMissingTangents = false;

    /** @brief Accept missing UV set 0 and fill imported `uv0` with zero. */
    bool defaultMissingUv0ToZero = false;

    /** @brief Zero-based animation index to import for `AssetKind::AnimationClip`. */
    std::uint32_t animationIndex = 0;

    /** @brief Ticks per second used when Assimp reports missing animation tick metadata. */
    double defaultTicksPerSecond = 30.0;

    /** @brief Accept missing translation keys and synthesize one identity translation key. */
    bool allowMissingTranslationKeys = true;

    /** @brief Accept missing rotation keys and synthesize one identity rotation key. */
    bool allowMissingRotationKeys = false;

    /** @brief Accept missing scale keys and synthesize one identity scale key. */
    bool allowMissingScaleKeys = true;

    /** @brief Source used when importing skeletons and animation clip joint maps. */
    AssimpSkeletonSourceMode skeletonSourceMode = AssimpSkeletonSourceMode::MeshBones;
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
 * @brief Imports a renderer-free mesh, skeleton, skinned mesh, or animation clip payload through Assimp.
 *
 * `source.uri` is treated as a direct filesystem path. Static mesh imports
 * combine all supported static meshes in the scene into one `LoadedMeshAsset`.
 * Skeletal imports produce one bind-pose `LoadedSkeletonAsset`, one
 * `LoadedSkinnedMeshAsset`, or one `LoadedAnimationClipAsset` depending on
 * `source.kind`; callers import the skeleton, skinned mesh, and animation clip
 * as separate source records even when they share a glTF file. Skinned mesh
 * imports aggregate convertible skinned meshes and skip unskinned meshes in
 * mixed scenes. Missing UV0 is rejected unless
 * `AssimpLoadedAssetImportOptions::defaultMissingUv0ToZero` is explicitly set.
 * Missing tangents are rejected unless
 * `AssimpLoadedAssetImportOptions::generateMissingTangents` is explicitly set;
 * imported tangents use the glTF xyz plus handedness convention. Mesh order and
 * face order follow Assimp's post-processed scene order. Parsed aggregate
 * metadata is checked against the active source descriptor, and the final
 * payload is validated with `validateLoadedAssetPayload`.
 *
 * `AssimpSkeletonSourceMode::AnimatedSceneNodes` is an opt-in FBX-oriented
 * path for Skeleton and AnimationClip imports only. It derives joint order from
 * animated scene nodes and required ancestors, and uses identity inverse bind
 * poses. That data is useful for CPU animation characterization but is not
 * sufficient for skinned mesh rendering without real weights and bind poses.
 *
 * The function copies all payload data by value. It performs no renderer
 * calls, renderer handle lookup, renderer resource creation, texture decoding,
 * material import, UV1+ import, animation evaluation, async scheduling, or
 * source catalog mutation.
 */
AssimpLoadedAssetImportResult importLoadedAssetPayloadWithAssimp(
    const AssetSourceRecord& source,
    const AssimpLoadedAssetImportOptions& options = {});
} // namespace full_engine
