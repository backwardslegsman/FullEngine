#pragma once

#include "engine/assets/AssimpLoadedAssetImporter.hpp"
#include "engine/assets/LoadedAssetPayload.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace full_engine
{
/** @brief Status for importing a rigid node-attached Assimp scene. */
enum class AssimpRigidNodeSceneImportStatus
{
    Success,
    InvalidArgument,
    IoError,
    ParseError,
    UnsupportedScene,
    PayloadValidationFailed,
};

/** @brief Per-mesh conversion status for rigid node-attached scene import. */
enum class AssimpRigidNodeMeshStatus
{
    Imported,
    InvalidMeshIndex,
    NullMesh,
    NoAnimatedNode,
    MissingPositions,
    MissingNormals,
    MissingTangents,
    MissingUv0,
    NonTriangleFaces,
    TooManyVertices,
    InvalidPayload,
};

/** @brief Input options for importing rigid mesh pieces attached to animated scene nodes. */
struct AssimpRigidNodeSceneImportOptions
{
    /** @brief Direct filesystem path passed to Assimp. */
    std::string path;

    /** @brief Asset ID assigned to the node-derived skeleton payload. */
    AssetId skeletonAssetId = {};

    /** @brief Asset ID assigned to the node-derived animation clip payload. */
    AssetId animationClipAssetId = {};

    /** @brief First asset ID assigned to imported mesh attachments. Later attachments increment by one. */
    AssetId firstMeshAssetId = {};

    /** @brief Assimp conversion options; `AnimatedSceneNodes` is forced for skeleton/clip import. */
    AssimpLoadedAssetImportOptions assimp = {};
};

/**
 * @brief One imported static mesh attached to an animated scene-node joint.
 *
 * The mesh payload owns copied CPU vertex/index data. `jointIndex` targets the
 * imported node-derived skeleton and can be sampled through
 * `sampleLoadedAnimationClip`. This is rigid node animation data only; it does
 * not contain skin weights or bind-pose recovery.
 */
struct AssimpRigidNodeMeshAttachment
{
    /** @brief Mesh asset ID assigned during import. */
    AssetId meshAssetId = {};

    /** @brief Joint index whose sampled model matrix drives this rigid attachment. */
    std::uint16_t jointIndex = 0;

    /** @brief Source Assimp mesh index. */
    std::uint32_t sourceMeshIndex = 0;

    /** @brief Source material index when present, or -1 when unavailable. */
    std::int32_t sourceMaterialIndex = -1;

    /** @brief Copied source node name for diagnostics. */
    std::string sourceNodeName;

    /** @brief Copied source mesh name for diagnostics. */
    std::string sourceMeshName;

    /** @brief Loaded static mesh payload for this rigid attachment. */
    LoadedMeshAsset mesh = {};
};

/** @brief Ordered diagnostic record for one source mesh in rigid-node import. */
struct AssimpRigidNodeMeshRecord
{
    /** @brief Source Assimp mesh index. */
    std::uint32_t sourceMeshIndex = 0;

    /** @brief Conversion status for this mesh. */
    AssimpRigidNodeMeshStatus status = AssimpRigidNodeMeshStatus::NullMesh;

    /** @brief Imported mesh asset ID when `status == Imported`. */
    AssetId meshAssetId = {};

    /** @brief Joint index used by the imported attachment when available. */
    std::uint16_t jointIndex = 0;

    /** @brief Copied source node name for diagnostics. */
    std::string sourceNodeName;
};

/** @brief Aggregate counters for rigid-node scene import. */
struct AssimpRigidNodeSceneImportSummary
{
    std::size_t importedAttachmentCount = 0;
    std::size_t skippedMeshCount = 0;
    std::size_t missingAnimatedNodeCount = 0;
    std::size_t missingAttributeCount = 0;
    std::size_t nonTriangleCount = 0;
    std::size_t tooManyVerticesCount = 0;
    std::size_t invalidPayloadCount = 0;
};

/** @brief Result of importing rigid mesh attachments plus node animation payloads. */
struct AssimpRigidNodeSceneImportResult
{
    /** @brief Overall import status. */
    AssimpRigidNodeSceneImportStatus status = AssimpRigidNodeSceneImportStatus::InvalidArgument;

    /** @brief Loaded payload validation detail for skeleton/clip/mesh failures. */
    LoadedAssetPayloadValidationResult payloadValidation =
        LoadedAssetPayloadValidationResult::Success;

    /** @brief Node-derived skeleton payload when import succeeds. */
    LoadedSkeletonAsset skeleton = {};

    /** @brief Node-derived animation clip payload when import succeeds. */
    LoadedAnimationClipAsset animationClip = {};

    /** @brief Imported rigid mesh attachments in scene traversal order. */
    std::vector<AssimpRigidNodeMeshAttachment> attachments;

    /** @brief One diagnostic record per source mesh reference processed. */
    std::vector<AssimpRigidNodeMeshRecord> meshRecords;

    /** @brief Aggregate import counters. */
    AssimpRigidNodeSceneImportSummary summary = {};
};

/** @brief Returns a stable diagnostic name for a rigid-node scene import status. */
const char* assimpRigidNodeSceneImportStatusName(
    AssimpRigidNodeSceneImportStatus status) noexcept;

/** @brief Returns a stable diagnostic name for a rigid-node mesh conversion status. */
const char* assimpRigidNodeMeshStatusName(
    AssimpRigidNodeMeshStatus status) noexcept;

/**
 * @brief Formats rigid-node import diagnostics as deterministic JSON text.
 *
 * The formatter copies no payload buffers and performs no file IO. Callers may
 * write the returned text to optional diagnostic report locations.
 */
std::string formatAssimpRigidNodeSceneImportResultJson(
    const AssimpRigidNodeSceneImportResult& result);

/**
 * @brief Imports static mesh pieces attached to animated scene nodes.
 *
 * The importer is CPU-only, renderer-free, and synchronous. It reads one
 * Assimp-supported scene file, builds a node-derived skeleton and animation
 * clip from animated scene nodes, then imports source meshes whose owning node
 * maps to that skeleton. Imported mesh pieces remain static `LoadedMeshAsset`
 * payloads; runtime code animates them rigidly with the sampled joint model
 * matrix. No skin weights, bind-pose recovery, renderer handles, renderer
 * resources, material import, texture decoding, async work, or source catalog
 * mutation are performed.
 */
AssimpRigidNodeSceneImportResult importRigidNodeSceneWithAssimp(
    const AssimpRigidNodeSceneImportOptions& options);
} // namespace full_engine
