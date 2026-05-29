#pragma once

#include "renderer/scene/Frustum.hpp"

namespace full_renderer::terrain
{
inline bool isChunkVisible(const scene::Frustum& frustum, const Aabb& bounds) noexcept
{
    return scene::intersects(frustum, bounds);
}
} // namespace full_renderer::terrain
