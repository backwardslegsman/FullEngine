#pragma once

#include "engine/assets/AssetCatalog.hpp"

#include <array>
#include <cstdint>

namespace full_engine
{
/** @brief Maximum texture asset references carried by one material source descriptor. */
constexpr std::uint32_t kMaxAssetSourceMaterialTextureRefs = 8;

/** @brief Maximum material sections described by one skinned mesh source descriptor. */
constexpr std::uint32_t kMaxAssetSourceSkinnedMeshSections = 32;

/**
 * @brief Renderer-free local-space bounds for a mesh source.
 *
 * Values are expressed in mesh-local meters using the engine/renderer Y-up
 * convention. They are declarative source metadata only and are not derived or
 * validated against bytes at the source URI.
 */
struct AssetSourceBounds
{
    /** @brief Minimum local-space corner in meters. */
    float min[3] = {};

    /** @brief Maximum local-space corner in meters. */
    float max[3] = {};
};

/** @brief Renderer-free mesh source metadata. */
struct AssetSourceMeshDescriptor
{
    /** @brief Expected vertex count. Zero is invalid. */
    std::uint32_t vertexCount = 0;

    /** @brief Expected index count. Zero is invalid. */
    std::uint32_t indexCount = 0;

    /** @brief Expected mesh-local bounds in meters. */
    AssetSourceBounds localBounds = {};
};

/** @brief Renderer-free skeleton source metadata. */
struct AssetSourceSkeletonDescriptor
{
    /** @brief Expected joint count. Zero is invalid. */
    std::uint32_t jointCount = 0;
};

/** @brief Renderer-free skinned mesh source metadata. */
struct AssetSourceSkinnedMeshSectionDescriptor
{
    /** @brief Material asset expected for this imported skinned mesh section. */
    AssetId materialAssetId = {};

    /** @brief First index in the aggregate skinned index buffer. */
    std::uint32_t firstIndex = 0;

    /** @brief Number of triangle indices in this section. */
    std::uint32_t indexCount = 0;
};

/** @brief Renderer-free skinned mesh source metadata. */
struct AssetSourceSkinnedMeshDescriptor
{
    /** @brief Expected vertex count. Zero is invalid. */
    std::uint32_t vertexCount = 0;

    /** @brief Expected index count. Zero is invalid. */
    std::uint32_t indexCount = 0;

    /** @brief Skeleton asset ID referenced by this skinned mesh. */
    AssetId skeletonAssetId = {};

    /** @brief Expected mesh-local bounds in meters. */
    AssetSourceBounds localBounds = {};

    /** @brief Ordered material sections expected from the imported skinned meshes. */
    std::array<AssetSourceSkinnedMeshSectionDescriptor, kMaxAssetSourceSkinnedMeshSections> sections = {};

    /** @brief Number of active material section descriptors. Zero means no section contract. */
    std::uint32_t sectionCount = 0;
};

/** @brief Renderer-free animation clip source metadata. */
struct AssetSourceAnimationClipDescriptor
{
    /** @brief Skeleton asset ID whose joint order the clip targets. */
    AssetId skeletonAssetId = {};

    /** @brief Expected number of joint transform tracks. Zero is invalid. */
    std::uint32_t trackCount = 0;

    /** @brief Expected clip duration in seconds. Must be finite and positive. */
    float durationSeconds = 0.0f;

    /** @brief Expected source ticks-per-second metadata used during import. */
    float ticksPerSecond = 0.0f;

    /** @brief Absolute tolerance used when matching duration and tick-rate metadata. */
    float metadataTolerance = 0.001f;
};

/** @brief Renderer-free texture source format contract. */
enum class AssetSourceTextureFormat
{
    Unknown,
    Rgba8,
};

/** @brief Renderer-free texture usage semantic. */
enum class AssetSourceTextureSemantic
{
    Unknown,
    Color,
    LinearData,
    NormalMap,
    TerrainSplat,
    ColorGradingLut,
    Debug,
};

/** @brief Renderer-free color-space expectation for texture source data. */
enum class AssetSourceTextureColorSpace
{
    Unknown,
    Srgb,
    Linear,
    EncodedNormal,
};

/** @brief Renderer-free texture source metadata. */
struct AssetSourceTextureDescriptor
{
    /** @brief Expected texture width in texels. Zero is invalid. */
    std::uint32_t width = 0;

    /** @brief Expected texture height in texels. Zero is invalid. */
    std::uint32_t height = 0;

    /** @brief Expected mip count. Zero is invalid. */
    std::uint32_t mipCount = 0;

    /** @brief Expected source pixel format. */
    AssetSourceTextureFormat format = AssetSourceTextureFormat::Unknown;

    /** @brief Expected renderer-facing usage semantic. */
    AssetSourceTextureSemantic semantic = AssetSourceTextureSemantic::Unknown;

    /** @brief Expected color-space interpretation. */
    AssetSourceTextureColorSpace colorSpace = AssetSourceTextureColorSpace::Unknown;
};

/** @brief Renderer-free material source model. */
enum class AssetSourceMaterialModel
{
    Unknown,
    Basic,
    TerrainSplat,
};

/** @brief Renderer-free material alpha/depth policy. */
enum class AssetSourceMaterialAlphaMode
{
    Unknown,
    Opaque,
    AlphaTest,
    AlphaBlend,
};

/** @brief Named material texture slot carried by renderer-free material metadata. */
enum class AssetSourceMaterialTextureSlot
{
    Unknown,
    BaseColor,
    Normal,
    MetallicRoughness,
    Occlusion,
    Emissive,
};

/** @brief One named texture asset reference used by a material source. */
struct AssetSourceMaterialTextureRef
{
    /** @brief Material slot that will later resolve to a renderer texture handle. */
    AssetSourceMaterialTextureSlot slot = AssetSourceMaterialTextureSlot::Unknown;

    /** @brief Engine asset ID for the referenced texture source. */
    AssetId id = {};
};

/** @brief Returns true when two material texture refs name the same slot and asset ID. */
constexpr bool operator==(
    const AssetSourceMaterialTextureRef& lhs,
    const AssetSourceMaterialTextureRef& rhs) noexcept
{
    return lhs.slot == rhs.slot && lhs.id == rhs.id;
}

/** @brief Returns true when two material texture refs differ. */
constexpr bool operator!=(
    const AssetSourceMaterialTextureRef& lhs,
    const AssetSourceMaterialTextureRef& rhs) noexcept
{
    return !(lhs == rhs);
}

/** @brief Renderer-free material source metadata. */
struct AssetSourceMaterialDescriptor
{
    /** @brief Expected material model. */
    AssetSourceMaterialModel model = AssetSourceMaterialModel::Unknown;

    /** @brief Expected material alpha/depth policy. */
    AssetSourceMaterialAlphaMode alphaMode = AssetSourceMaterialAlphaMode::Unknown;

    /** @brief Named texture asset references used by this material source. */
    std::array<AssetSourceMaterialTextureRef, kMaxAssetSourceMaterialTextureRefs> textureRefs = {};

    /** @brief Number of active texture references in `textureRefs`. */
    std::uint32_t textureRefCount = 0;
};

/**
 * @brief Union-style renderer-free source descriptor for loadable asset kinds.
 *
 * Only the descriptor matching the owning `AssetSourceRecord::kind` is active.
 * Inactive fields are ignored by validation. The descriptor is copied by value
 * and does not retain source bytes, importer state, renderer handles, or live
 * renderer resources.
 */
struct AssetSourceDescriptor
{
    /** @brief Mesh metadata used when the source record kind is `Mesh`. */
    AssetSourceMeshDescriptor mesh = {};

    /** @brief Texture metadata used when the source record kind is `Texture`. */
    AssetSourceTextureDescriptor texture = {};

    /** @brief Material metadata used when the source record kind is `Material`. */
    AssetSourceMaterialDescriptor material = {};

    /** @brief Skeleton metadata used when the source record kind is `Skeleton`. */
    AssetSourceSkeletonDescriptor skeleton = {};

    /** @brief Skinned mesh metadata used when the source record kind is `SkinnedMesh`. */
    AssetSourceSkinnedMeshDescriptor skinnedMesh = {};

    /** @brief Animation clip metadata used when the source record kind is `AnimationClip`. */
    AssetSourceAnimationClipDescriptor animationClip = {};
};

/** @brief Validation result for one active source descriptor. */
enum class AssetSourceDescriptorValidationResult
{
    Success,
    InvalidKind,
    InvalidMeshCounts,
    InvalidMeshBounds,
    InvalidTextureDimensions,
    InvalidTextureMipCount,
    InvalidTextureFormat,
    InvalidTextureSemantic,
    InvalidTextureColorSpace,
    InvalidMaterialModel,
    InvalidMaterialAlphaMode,
    InvalidMaterialTextureCount,
    InvalidMaterialTextureSlot,
    InvalidMaterialTextureRef,
    DuplicateMaterialTextureSlot,
    InvalidSkeletonJointCount,
    InvalidSkinnedMeshCounts,
    InvalidSkinnedMeshSkeletonRef,
    InvalidSkinnedMeshBounds,
    InvalidSkinnedMeshSectionCount,
    InvalidSkinnedMeshSectionMaterialRef,
    InvalidSkinnedMeshSectionRange,
    InvalidAnimationClipSkeletonRef,
    InvalidAnimationClipTrackCount,
    InvalidAnimationClipDuration,
    InvalidAnimationClipTicksPerSecond,
    InvalidAnimationClipMetadataTolerance,
};

/** @brief Returns a stable diagnostic name for a descriptor validation result. */
const char* assetSourceDescriptorValidationResultName(
    AssetSourceDescriptorValidationResult result) noexcept;

/**
 * @brief Validates the descriptor field active for `kind`.
 *
 * The helper validates only metadata shape. It performs no source URI lookup,
 * file IO, importer work, dependency catalog lookup, renderer calls, renderer
 * handle checks, or renderer resource creation.
 */
AssetSourceDescriptorValidationResult validateAssetSourceDescriptor(
    AssetKind kind,
    const AssetSourceDescriptor& descriptor) noexcept;
} // namespace full_engine
