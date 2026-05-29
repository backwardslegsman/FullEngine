#pragma once

#include "full_renderer/Renderer.hpp"

#include <cstdint>

namespace full_renderer::resources
{
/** @brief Returns true when the texture semantic enum value is supported by the renderer contract. */
bool isValidTextureSemantic(TextureSemantic semantic) noexcept;

/** @brief Returns true when the texture color-space enum value is supported by the renderer contract. */
bool isValidTextureColorSpace(TextureColorSpace colorSpace) noexcept;

/**
 * @brief Returns true when a texture semantic and color-space pairing follows the asset contract.
 *
 * The check is intentionally backend-agnostic. It validates authoring/import
 * intent but does not expose or inspect backend texture resources.
 */
bool isTextureColorSpaceCompatible(TextureSemantic semantic, TextureColorSpace colorSpace) noexcept;

/**
 * @brief Validates the renderer-facing mesh asset contract.
 *
 * Mesh data must be finite, indexed as triangles with 16-bit indices, use
 * usable non-zero normals, and avoid degenerate triangles. Pointers are only
 * inspected for the duration of the call.
 */
RendererResult validateMeshAssetContract(const MeshDesc& desc) noexcept;

/**
 * @brief Computes local mesh bounds for validation and import-tool tests.
 *
 * @return `true` when bounds were computed from valid finite positions.
 */
bool computeMeshLocalBounds(const MeshDesc& desc, Aabb& outBounds) noexcept;

/**
 * @brief Returns true when indexed mesh triangles contain a degenerate triangle.
 *
 * Invalid descriptors are treated as having degenerate geometry because callers
 * should validate structure before relying on triangle-level diagnostics.
 */
bool meshHasDegenerateTriangles(const MeshDesc& desc) noexcept;

/**
 * @brief Validates the renderer-facing texture asset contract.
 *
 * The current runtime path accepts uncompressed, single-mip, tightly packed
 * RGBA8 2D texture data. Semantic and color-space metadata are checked for
 * consistency so import tools and engine code can share the same rules.
 */
RendererResult validateTextureAssetContract(const TextureDesc& desc) noexcept;
} // namespace full_renderer::resources
