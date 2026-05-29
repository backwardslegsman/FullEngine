#include "renderer/scene/Frustum.hpp"

#include <cmath>

namespace full_renderer::scene
{
namespace
{
Plane makePlane(const float x, const float y, const float z, const float w) noexcept
{
    const float length = std::sqrt(x * x + y * y + z * z);
    if (length <= 0.000001f)
    {
        return {};
    }

    const float invLength = 1.0f / length;
    return {x * invLength, y * invLength, z * invLength, w * invLength};
}

float planeDistance(const Plane& plane, const float x, const float y, const float z) noexcept
{
    return plane.x * x + plane.y * y + plane.z * z + plane.w;
}
} // namespace

Frustum extractFrustumFromViewProjection(const float m[16]) noexcept
{
    Frustum frustum;

    frustum.planes[0] = makePlane(m[3] + m[0], m[7] + m[4], m[11] + m[8], m[15] + m[12]);
    frustum.planes[1] = makePlane(m[3] - m[0], m[7] - m[4], m[11] - m[8], m[15] - m[12]);
    frustum.planes[2] = makePlane(m[3] + m[1], m[7] + m[5], m[11] + m[9], m[15] + m[13]);
    frustum.planes[3] = makePlane(m[3] - m[1], m[7] - m[5], m[11] - m[9], m[15] - m[13]);
    frustum.planes[4] = makePlane(m[3] + m[2], m[7] + m[6], m[11] + m[10], m[15] + m[14]);
    frustum.planes[5] = makePlane(m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14]);

    return frustum;
}

bool intersects(const Frustum& frustum, const Aabb& bounds) noexcept
{
    for (const Plane& plane : frustum.planes)
    {
        const float x = plane.x >= 0.0f ? bounds.max[0] : bounds.min[0];
        const float y = plane.y >= 0.0f ? bounds.max[1] : bounds.min[1];
        const float z = plane.z >= 0.0f ? bounds.max[2] : bounds.min[2];

        if (planeDistance(plane, x, y, z) < 0.0f)
        {
            return false;
        }
    }

    return true;
}
} // namespace full_renderer::scene
