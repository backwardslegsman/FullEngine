#pragma once

#include "full_renderer/Terrain.hpp"

#include <cmath>
#include <cstddef>

namespace full_renderer::scene
{
struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline bool isFinite(const float value) noexcept
{
    return std::isfinite(value);
}

inline bool isFinite3(const float values[3]) noexcept
{
    return isFinite(values[0]) && isFinite(values[1]) && isFinite(values[2]);
}

inline bool isFinite16(const float values[16]) noexcept
{
    for (std::size_t index = 0; index < 16; ++index)
    {
        if (!isFinite(values[index]))
        {
            return false;
        }
    }

    return true;
}

inline bool isValidAabb(const Aabb& bounds) noexcept
{
    return isFinite3(bounds.min) &&
        isFinite3(bounds.max) &&
        bounds.min[0] <= bounds.max[0] &&
        bounds.min[1] <= bounds.max[1] &&
        bounds.min[2] <= bounds.max[2];
}

inline Vec3 aabbCenter(const Aabb& bounds) noexcept
{
    return {
        (bounds.min[0] + bounds.max[0]) * 0.5f,
        (bounds.min[1] + bounds.max[1]) * 0.5f,
        (bounds.min[2] + bounds.max[2]) * 0.5f};
}

inline float distance(const Vec3 lhs, const Vec3 rhs) noexcept
{
    const float x = lhs.x - rhs.x;
    const float y = lhs.y - rhs.y;
    const float z = lhs.z - rhs.z;
    return std::sqrt(x * x + y * y + z * z);
}

inline Vec3 fromArray(const float values[3]) noexcept
{
    return {values[0], values[1], values[2]};
}

inline void multiplyColumnMajor4x4(const float lhs[16], const float rhs[16], float out[16]) noexcept
{
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            out[column * 4 + row] =
                lhs[0 * 4 + row] * rhs[column * 4 + 0] +
                lhs[1 * 4 + row] * rhs[column * 4 + 1] +
                lhs[2 * 4 + row] * rhs[column * 4 + 2] +
                lhs[3 * 4 + row] * rhs[column * 4 + 3];
        }
    }
}
} // namespace full_renderer::scene
