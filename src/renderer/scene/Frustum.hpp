#pragma once

#include "full_renderer/Terrain.hpp"

namespace full_renderer::scene
{
struct Plane
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct Frustum
{
    Plane planes[6];
};

Frustum extractFrustumFromViewProjection(const float viewProjection[16]) noexcept;
bool intersects(const Frustum& frustum, const Aabb& bounds) noexcept;
} // namespace full_renderer::scene
