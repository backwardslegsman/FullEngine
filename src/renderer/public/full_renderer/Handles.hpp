#pragma once

#include <cstdint>

namespace full_renderer
{
/**
 * @brief Opaque handle for a renderer-owned mesh resource.
 *
 * Handles are owned by the renderer instance that created them. A default or
 * zero handle is always invalid. A non-zero handle can become stale after
 * destruction, renderer shutdown, or use with a different renderer instance.
 * Destroying invalid or stale handles is ignored; submitting them is rejected
 * or skipped according to the owning API and reported through debug stats where
 * practical. Public handles never expose backend resources.
 */
struct MeshHandle
{
    /** @brief Zero is always invalid; non-zero values are assigned by the renderer. */
    std::uint32_t id = 0;
};

/**
 * @brief Opaque handle for a renderer-owned material resource.
 *
 * The same invalid/stale lifetime rules as `MeshHandle` apply. Materials may
 * reference texture handles and those textures must remain live while materials
 * that use them can be submitted.
 */
struct MaterialHandle
{
    /** @brief Zero is always invalid; non-zero values are assigned by the renderer. */
    std::uint32_t id = 0;
};

/**
 * @brief Opaque handle for a renderer-owned texture resource.
 *
 * The renderer copies texture creation data before returning from creation.
 * Destroyed or stale texture handles fall back or reject according to the
 * descriptor using them; no backend texture ID is exposed.
 */
struct TextureHandle
{
    /** @brief Zero is always invalid; non-zero values are assigned by the renderer. */
    std::uint32_t id = 0;
};

/**
 * @brief Opaque handle for renderer-owned skeleton metadata.
 *
 * Skeleton handles follow the same invalid/stale ownership rules as other
 * public renderer handles and never expose backend or animation-system storage.
 */
struct SkeletonHandle
{
    /** @brief Zero is always invalid; non-zero values are assigned by the renderer. */
    std::uint32_t id = 0;
};

/**
 * @brief Opaque handle for a renderer-owned skinned mesh resource.
 *
 * The renderer copies skinned mesh creation data before returning from
 * creation. Destroyed or stale handles are rejected by submissions and reported
 * through debug diagnostics where practical.
 */
struct SkinnedMeshHandle
{
    /** @brief Zero is always invalid; non-zero values are assigned by the renderer. */
    std::uint32_t id = 0;
};

/**
 * @brief Returns whether a mesh handle may refer to a renderer-owned resource.
 *
 * This check only identifies the public invalid sentinel. A non-zero handle can
 * still be stale if the resource was destroyed or belongs to another renderer.
 */
constexpr bool isValid(const MeshHandle handle) noexcept
{
    return handle.id != 0;
}

/**
 * @brief Returns whether a material handle may refer to a renderer-owned resource.
 *
 * This check only identifies the public invalid sentinel. A non-zero handle can
 * still be stale if the resource was destroyed or belongs to another renderer.
 */
constexpr bool isValid(const MaterialHandle handle) noexcept
{
    return handle.id != 0;
}

/**
 * @brief Returns whether a texture handle may refer to a renderer-owned resource.
 *
 * This check only identifies the public invalid sentinel. A non-zero handle can
 * still be stale if the resource was destroyed or belongs to another renderer.
 */
constexpr bool isValid(const TextureHandle handle) noexcept
{
    return handle.id != 0;
}

/**
 * @brief Returns whether a skeleton handle may refer to renderer-owned metadata.
 *
 * This check only identifies the public invalid sentinel. A non-zero handle can
 * still be stale if the skeleton was destroyed or belongs to another renderer.
 */
constexpr bool isValid(const SkeletonHandle handle) noexcept
{
    return handle.id != 0;
}

/**
 * @brief Returns whether a skinned mesh handle may refer to a renderer-owned resource.
 *
 * This check only identifies the public invalid sentinel. A non-zero handle can
 * still be stale if the resource was destroyed or belongs to another renderer.
 */
constexpr bool isValid(const SkinnedMeshHandle handle) noexcept
{
    return handle.id != 0;
}
} // namespace full_renderer
