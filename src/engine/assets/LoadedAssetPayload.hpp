#pragma once

#include "engine/assets/AssetSourceDescriptor.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace full_engine
{
/** @brief Maximum joints accepted by the first renderer-free skeleton payload contract. */
constexpr std::uint32_t kMaxLoadedSkeletonJoints = 64;

/** @brief Fixed number of joint influences carried by one loaded skinned vertex. */
constexpr std::uint32_t kMaxLoadedSkinningInfluences = 4;

/**
 * @brief Fixed renderer-free vertex shape for loaded mesh asset data.
 *
 * Positions are mesh-local meters in the engine/renderer Y-up convention.
 * Normals are expected to be finite and non-zero. Tangents use the glTF
 * convention where xyz is the local tangent direction and w is the bitangent
 * handedness sign. UV0 values are copied as renderer-facing texture
 * coordinates and must be finite. Colors are linear RGBA in `[0, 1]`. The
 * payload owns copied vectors and does not reference importer buffers,
 * renderer handles, renderer resources, or backend objects.
 */
struct LoadedMeshVertex
{
    /** @brief Mesh-local position in meters. */
    float position[3] = {};

    /** @brief Mesh-local normal direction. */
    float normal[3] = {0.0f, 1.0f, 0.0f};

    /** @brief Primary texture coordinates copied from source UV set 0. */
    float uv0[2] = {};

    /** @brief Linear RGBA vertex color. */
    float colorLinear[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    /** @brief Mesh-local tangent xyz plus glTF-style handedness sign in w. */
    float tangent[4] = {1.0f, 0.0f, 0.0f, 1.0f};
};

/**
 * @brief Renderer-free loaded mesh payload.
 *
 * The mesh stores copied fixed-format vertices and 16-bit triangle indices.
 * It mirrors the current renderer upload contract without including renderer
 * headers or creating renderer resources.
 */
struct LoadedMeshAsset
{
    /** @brief Engine asset identity for this loaded mesh. */
    AssetId id = {};

    /** @brief Copied mesh vertices. */
    std::vector<LoadedMeshVertex> vertices;

    /** @brief Copied 16-bit triangle indices. Count must be a non-zero multiple of three. */
    std::vector<std::uint16_t> indices;

    /** @brief Mesh-local bounds in meters. */
    AssetSourceBounds localBounds = {};
};

/**
 * @brief One renderer-free loaded skeleton joint.
 *
 * Joints are validated by index; `name` is optional copied metadata for
 * diagnostics and future importer tooling. Matrices are column-major floats.
 */
struct LoadedSkeletonJoint
{
    /** @brief Optional copied source joint name. Empty is valid. */
    std::string name;

    /** @brief Parent joint index, or `-1` for the single root joint. */
    std::int32_t parentIndex = -1;

    /** @brief Column-major inverse bind-pose matrix. Values must be finite. */
    float inverseBindPose[16] = {};

    /** @brief Column-major bind/local reference transform for future pose/debug tooling. */
    float referenceTransform[16] = {};
};

/**
 * @brief Renderer-free loaded skeleton payload.
 *
 * The skeleton stores copied hierarchy metadata and bind-pose matrices only.
 * It performs no animation evaluation and owns no renderer resources.
 */
struct LoadedSkeletonAsset
{
    /** @brief Engine asset identity for this loaded skeleton. */
    AssetId id = {};

    /** @brief Ordered joints. Parents must appear before children. */
    std::vector<LoadedSkeletonJoint> joints;
};

/**
 * @brief Renderer-free skinned mesh vertex payload.
 *
 * Positions and normals are mesh-local meters in the engine/renderer Y-up
 * convention. Tangents use the glTF tangent basis convention with xyz as the
 * local tangent direction and w as handedness. UV0 is copied from source
 * texture coordinate set 0. Joint indices and weights mirror the current
 * four-influence renderer contract.
 */
struct LoadedSkinnedMeshVertex
{
    /** @brief Mesh-local position in meters. */
    float position[3] = {};

    /** @brief Mesh-local normal direction. */
    float normal[3] = {0.0f, 1.0f, 0.0f};

    /** @brief Primary texture coordinates copied from source UV set 0. */
    float uv0[2] = {};

    /** @brief Linear RGBA vertex color. */
    float colorLinear[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    /** @brief Up to four joint indices. Values must be less than `kMaxLoadedSkeletonJoints`. */
    std::uint16_t jointIndices[kMaxLoadedSkinningInfluences] = {};

    /** @brief Corresponding non-negative skinning weights. Values must sum to one. */
    float jointWeights[kMaxLoadedSkinningInfluences] = {1.0f, 0.0f, 0.0f, 0.0f};

    /** @brief Mesh-local tangent xyz plus glTF-style handedness sign in w. */
    float tangent[4] = {1.0f, 0.0f, 0.0f, 1.0f};
};

/**
 * @brief One material section in a renderer-free loaded skinned mesh.
 *
 * Sections reference ranges in the aggregate 16-bit triangle index buffer.
 * `materialAssetId` remains an engine asset ID; renderer handles are resolved
 * by later renderer-integration code.
 */
struct LoadedSkinnedMeshSection
{
    /** @brief Material asset ID assigned to this section. */
    AssetId materialAssetId = {};

    /** @brief First index in `LoadedSkinnedMeshAsset::indices`. */
    std::uint32_t firstIndex = 0;

    /** @brief Number of triangle indices in this section. */
    std::uint32_t indexCount = 0;
};

/**
 * @brief Renderer-free loaded skinned mesh payload.
 *
 * The mesh stores copied vertices and indices plus the skeleton asset ID that
 * upload code must resolve before creating renderer-owned skinned mesh
 * resources.
 */
struct LoadedSkinnedMeshAsset
{
    /** @brief Engine asset identity for this loaded skinned mesh. */
    AssetId id = {};

    /** @brief Skeleton asset ID referenced by this skinned mesh. */
    AssetId skeletonAssetId = {};

    /** @brief Copied skinned mesh vertices. */
    std::vector<LoadedSkinnedMeshVertex> vertices;

    /** @brief Copied 16-bit triangle indices. Count must be a non-zero multiple of three. */
    std::vector<std::uint16_t> indices;

    /** @brief Optional material sections over `indices`; empty means one implicit whole-mesh section. */
    std::vector<LoadedSkinnedMeshSection> sections;

    /** @brief Mesh-local bounds in meters. */
    AssetSourceBounds localBounds = {};
};

/** @brief One translation key in a renderer-free loaded animation track. */
struct LoadedAnimationTranslationKey
{
    /** @brief Key time in seconds from clip start. */
    float timeSeconds = 0.0f;

    /** @brief Local-space translation value in meters. */
    float value[3] = {};
};

/** @brief One rotation key in a renderer-free loaded animation track. */
struct LoadedAnimationRotationKey
{
    /** @brief Key time in seconds from clip start. */
    float timeSeconds = 0.0f;

    /** @brief Normalized local-space quaternion stored as x, y, z, w. */
    float value[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};

/** @brief One scale key in a renderer-free loaded animation track. */
struct LoadedAnimationScaleKey
{
    /** @brief Key time in seconds from clip start. */
    float timeSeconds = 0.0f;

    /** @brief Local-space scale value. */
    float value[3] = {1.0f, 1.0f, 1.0f};
};

/**
 * @brief Renderer-free joint transform track in a loaded animation clip.
 *
 * Tracks are keyed by skeleton joint index. Interpolation, blending, root
 * motion extraction, compression, and runtime sampling are intentionally
 * outside this copied payload contract.
 */
struct LoadedAnimationJointTrack
{
    /** @brief Skeleton joint index targeted by this track. */
    std::uint16_t jointIndex = 0;

    /** @brief Copied translation keys ordered by time. */
    std::vector<LoadedAnimationTranslationKey> translations;

    /** @brief Copied rotation keys ordered by time. */
    std::vector<LoadedAnimationRotationKey> rotations;

    /** @brief Copied scale keys ordered by time. */
    std::vector<LoadedAnimationScaleKey> scales;
};

/**
 * @brief Renderer-free loaded skeletal animation clip payload.
 *
 * The clip stores raw local transform keyframes targeting one skeleton asset.
 * It owns copied key data only and performs no evaluation, blending,
 * compression, renderer upload, or runtime animation state mutation.
 */
struct LoadedAnimationClipAsset
{
    /** @brief Engine asset identity for this loaded clip. */
    AssetId id = {};

    /** @brief Skeleton asset ID whose joint order the tracks target. */
    AssetId skeletonAssetId = {};

    /** @brief Clip duration in seconds. */
    float durationSeconds = 0.0f;

    /** @brief Source ticks-per-second metadata used when converting key times. */
    float ticksPerSecond = 0.0f;

    /** @brief Ordered joint tracks. Joint indices must be unique. */
    std::vector<LoadedAnimationJointTrack> tracks;
};

/**
 * @brief Renderer-free loaded texture payload.
 *
 * The v1 payload stores tightly packed, uncompressed, single-mip RGBA8 bytes
 * plus renderer-free semantic/color-space metadata. The engine asset layer
 * owns the byte vector but performs no file IO, decoding, upload, renderer
 * calls, or renderer-resource creation.
 */
struct LoadedTextureAsset
{
    /** @brief Engine asset identity for this loaded texture. */
    AssetId id = {};

    /** @brief Texture width in texels. */
    std::uint32_t width = 0;

    /** @brief Texture height in texels. */
    std::uint32_t height = 0;

    /** @brief Texture mip count. V1 validation accepts only one mip. */
    std::uint32_t mipCount = 0;

    /** @brief Renderer-free pixel format metadata. V1 validation accepts only `Rgba8`. */
    AssetSourceTextureFormat format = AssetSourceTextureFormat::Unknown;

    /** @brief Renderer-free semantic metadata for the loaded bytes. */
    AssetSourceTextureSemantic semantic = AssetSourceTextureSemantic::Unknown;

    /** @brief Renderer-free color-space metadata for the loaded bytes. */
    AssetSourceTextureColorSpace colorSpace = AssetSourceTextureColorSpace::Unknown;

    /** @brief Copied texture bytes. V1 expects at least `width * height * 4` bytes. */
    std::vector<std::uint8_t> bytes;
};

/**
 * @brief Renderer-free loaded material payload.
 *
 * Materials store declarative model/alpha policy and texture asset
 * references. Texture IDs are not renderer handles and are resolved by later
 * renderer-integration code.
 */
struct LoadedMaterialAsset
{
    /** @brief Engine asset identity for this loaded material. */
    AssetId id = {};

    /** @brief Renderer-free material model. */
    AssetSourceMaterialModel model = AssetSourceMaterialModel::Unknown;

    /** @brief Renderer-free material alpha/depth policy. */
    AssetSourceMaterialAlphaMode alphaMode = AssetSourceMaterialAlphaMode::Unknown;

    /** @brief Copied named texture asset references used by this material. */
    std::array<AssetSourceMaterialTextureRef, kMaxAssetSourceMaterialTextureRefs> textureRefs = {};

    /** @brief Number of active entries in `textureRefs`. */
    std::uint32_t textureRefCount = 0;
};

/**
 * @brief Union-style loaded asset payload for loadable asset kinds.
 *
 * Only the payload slot matching `kind` is active. Validation ignores inactive
 * slots. The value owns copied CPU data only; it does not retain file handles,
 * importer state, renderer handles, runtime animation state, or renderer
 * resources.
 */
struct LoadedAssetPayload
{
    /** @brief Active payload kind. Mesh, Texture, Material, Skeleton, SkinnedMesh, and AnimationClip are supported. */
    AssetKind kind = AssetKind::Unknown;

    /** @brief Active when `kind` is `Mesh`. */
    LoadedMeshAsset mesh = {};

    /** @brief Active when `kind` is `Texture`. */
    LoadedTextureAsset texture = {};

    /** @brief Active when `kind` is `Material`. */
    LoadedMaterialAsset material = {};

    /** @brief Active when `kind` is `Skeleton`. */
    LoadedSkeletonAsset skeleton = {};

    /** @brief Active when `kind` is `SkinnedMesh`. */
    LoadedSkinnedMeshAsset skinnedMesh = {};

    /** @brief Active when `kind` is `AnimationClip`. */
    LoadedAnimationClipAsset animationClip = {};
};

/** @brief Validation result for renderer-free loaded asset payloads. */
enum class LoadedAssetPayloadValidationResult
{
    Success,
    InvalidKind,
    InvalidAssetId,
    InvalidMeshVertices,
    InvalidMeshIndices,
    InvalidMeshVertexData,
    InvalidMeshBounds,
    InvalidTextureDimensions,
    InvalidTextureMipCount,
    InvalidTextureFormat,
    InvalidTextureSemantic,
    InvalidTextureColorSpace,
    InvalidTextureByteCount,
    InvalidMaterialModel,
    InvalidMaterialAlphaMode,
    InvalidMaterialTextureCount,
    InvalidMaterialTextureSlot,
    InvalidMaterialTextureRef,
    DuplicateMaterialTextureSlot,
    InvalidSkeletonJointCount,
    InvalidSkeletonHierarchy,
    InvalidSkeletonJointData,
    InvalidSkinnedMeshSkeletonRef,
    InvalidSkinnedMeshVertices,
    InvalidSkinnedMeshIndices,
    InvalidSkinnedMeshVertexData,
    InvalidSkinnedMeshWeights,
    InvalidSkinnedMeshBounds,
    InvalidSkinnedMeshSectionMaterialRef,
    InvalidSkinnedMeshSectionRange,
    InvalidAnimationClipSkeletonRef,
    InvalidAnimationClipDuration,
    InvalidAnimationClipTicksPerSecond,
    InvalidAnimationClipTracks,
    InvalidAnimationClipJointTrack,
    InvalidAnimationClipKeyTimes,
    InvalidAnimationClipKeyData,
};

/** @brief Returns a stable diagnostic name for a loaded payload validation result. */
const char* loadedAssetPayloadValidationResultName(
    LoadedAssetPayloadValidationResult result) noexcept;

/**
 * @brief Validates a renderer-free loaded mesh payload.
 *
 * The function checks only CPU value shape and copied data. It performs no
 * importer work, file IO, renderer calls, renderer handle lookup, or renderer
 * resource creation.
 */
LoadedAssetPayloadValidationResult validateLoadedMeshAsset(
    const LoadedMeshAsset& mesh) noexcept;

/**
 * @brief Validates a renderer-free loaded texture payload.
 *
 * V1 accepts tightly packed single-mip RGBA8 bytes with valid semantic and
 * color-space metadata.
 */
LoadedAssetPayloadValidationResult validateLoadedTextureAsset(
    const LoadedTextureAsset& texture) noexcept;

/**
 * @brief Validates a renderer-free loaded material payload.
 *
 * Texture references are engine asset IDs, not renderer handles.
 */
LoadedAssetPayloadValidationResult validateLoadedMaterialAsset(
    const LoadedMaterialAsset& material) noexcept;

/**
 * @brief Validates a renderer-free loaded skeleton payload.
 *
 * The helper checks hierarchy order and finite matrices only. It performs no
 * animation import, renderer handle lookup, or renderer resource creation.
 */
LoadedAssetPayloadValidationResult validateLoadedSkeletonAsset(
    const LoadedSkeletonAsset& skeleton) noexcept;

/**
 * @brief Validates a renderer-free loaded skinned mesh payload.
 *
 * The helper validates copied CPU value shape, four-influence skinning data,
 * 16-bit triangle indices, and local bounds only.
 */
LoadedAssetPayloadValidationResult validateLoadedSkinnedMeshAsset(
    const LoadedSkinnedMeshAsset& mesh) noexcept;

/**
 * @brief Validates one renderer-free loaded animation clip payload.
 *
 * Validation checks copied key data only. It does not verify the referenced
 * skeleton exists in a catalog and does not evaluate the clip.
 */
LoadedAssetPayloadValidationResult validateLoadedAnimationClipAsset(
    const LoadedAnimationClipAsset& clip) noexcept;

/**
 * @brief Validates the active slot of a loaded asset payload.
 *
 * Inactive slots are ignored. Supported active kinds are Mesh, Texture,
 * Material, Skeleton, and SkinnedMesh.
 */
LoadedAssetPayloadValidationResult validateLoadedAssetPayload(
    const LoadedAssetPayload& payload) noexcept;
} // namespace full_engine
