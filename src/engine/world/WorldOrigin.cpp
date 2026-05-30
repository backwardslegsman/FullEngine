#include "engine/world/WorldOrigin.hpp"

#include <cmath>

namespace full_engine
{
namespace
{
WorldVector subtract(const WorldPosition& lhs, const WorldPosition& rhs) noexcept
{
    WorldVector result;
    result.x = lhs.x - rhs.x;
    result.y = lhs.y - rhs.y;
    result.z = lhs.z - rhs.z;
    return result;
}

WorldPosition toPosition(const WorldVector& vector) noexcept
{
    WorldPosition result;
    result.x = vector.x;
    result.y = vector.y;
    result.z = vector.z;
    return result;
}

bool hasOrderedExtents(const WorldBounds& bounds) noexcept
{
    return bounds.min.x <= bounds.max.x && bounds.min.y <= bounds.max.y && bounds.min.z <= bounds.max.z;
}
} // namespace

bool isFinite(const WorldPosition& position) noexcept
{
    return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

bool isFinite(const WorldBounds& bounds) noexcept
{
    return isFinite(bounds.min) && isFinite(bounds.max) && hasOrderedExtents(bounds);
}

WorldOrigin makeAbsoluteOrigin() noexcept
{
    WorldOrigin origin;
    origin.mode = OriginMode::Absolute;
    origin.originWorld = {};
    return origin;
}

WorldOrigin makeCameraRelativeOrigin(const WorldPosition& cameraWorld) noexcept
{
    WorldOrigin origin;
    origin.mode = OriginMode::CameraRelative;
    origin.originWorld = cameraWorld;
    return origin;
}

WorldOriginResult rebasePosition(
    const WorldOrigin& origin,
    const WorldPosition& worldPosition,
    WorldVector& outRelative) noexcept
{
    if (!isFinite(origin.originWorld) || !isFinite(worldPosition))
    {
        return WorldOriginResult::InvalidArgument;
    }

    switch (origin.mode)
    {
    case OriginMode::Absolute:
        outRelative = subtract(worldPosition, {});
        return WorldOriginResult::Success;
    case OriginMode::CameraRelative:
        outRelative = subtract(worldPosition, origin.originWorld);
        return WorldOriginResult::Success;
    }

    return WorldOriginResult::InvalidArgument;
}

WorldOriginResult rebaseBounds(
    const WorldOrigin& origin,
    const WorldBounds& worldBounds,
    WorldBounds& outRelativeBounds) noexcept
{
    if (!isFinite(origin.originWorld) || !isFinite(worldBounds))
    {
        return WorldOriginResult::InvalidArgument;
    }

    WorldVector relativeMin;
    WorldVector relativeMax;
    const WorldOriginResult minResult = rebasePosition(origin, worldBounds.min, relativeMin);
    const WorldOriginResult maxResult = rebasePosition(origin, worldBounds.max, relativeMax);
    if (minResult != WorldOriginResult::Success || maxResult != WorldOriginResult::Success)
    {
        return WorldOriginResult::InvalidArgument;
    }

    outRelativeBounds.min = toPosition(relativeMin);
    outRelativeBounds.max = toPosition(relativeMax);
    return WorldOriginResult::Success;
}
} // namespace full_engine
