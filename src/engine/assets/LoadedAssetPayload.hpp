#pragma once

#include "engine/assets/AssetSourceDescriptor.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace full_engine
{
/**
 * @brief Fixed renderer-free vertex shape for loaded mesh asset data.
 *
 * Positions are mesh-local meters in the engine/renderer Y-up convention.
 * Normals are expected to be finite and non-zero. UV0 values are copied as
 * renderer-facing texture coordinates and must be finite. Colors are linear
 * RGBA in `[0, 1]`. The payload owns copied vectors and does not reference
 * importer buffers, renderer handles, renderer resources, or backend objects.
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
 * importer state, renderer handles, or renderer resources.
 */
struct LoadedAssetPayload
{
    /** @brief Active payload kind. Only Mesh, Texture, and Material are supported. */
    AssetKind kind = AssetKind::Unknown;

    /** @brief Active when `kind` is `Mesh`. */
    LoadedMeshAsset mesh = {};

    /** @brief Active when `kind` is `Texture`. */
    LoadedTextureAsset texture = {};

    /** @brief Active when `kind` is `Material`. */
    LoadedMaterialAsset material = {};
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
 * @brief Validates the active slot of a loaded asset payload.
 *
 * Inactive slots are ignored. Supported active kinds are Mesh, Texture, and
 * Material.
 */
LoadedAssetPayloadValidationResult validateLoadedAssetPayload(
    const LoadedAssetPayload& payload) noexcept;
} // namespace full_engine
