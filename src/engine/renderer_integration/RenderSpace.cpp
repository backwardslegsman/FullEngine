#include "engine/renderer_integration/RenderSpace.hpp"

#include <cmath>

namespace full_engine
{
namespace
{
bool isValidLimit(const RenderSpaceLimits& limits) noexcept
{
    return std::isfinite(limits.maxAbsCoordinate) && limits.maxAbsCoordinate > 0.0;
}

bool isWithinLimit(const double value, const RenderSpaceLimits& limits) noexcept
{
    return value >= -limits.maxAbsCoordinate && value <= limits.maxAbsCoordinate;
}

bool isWithinLimit(const WorldVector& value, const RenderSpaceLimits& limits) noexcept
{
    return isWithinLimit(value.x, limits) && isWithinLimit(value.y, limits) && isWithinLimit(value.z, limits);
}

RenderPosition toRenderPositionUnchecked(const WorldVector& value) noexcept
{
    RenderPosition result;
    result.x = static_cast<float>(value.x);
    result.y = static_cast<float>(value.y);
    result.z = static_cast<float>(value.z);
    return result;
}
} // namespace

RenderSpaceResult toRenderPosition(
    const WorldOrigin& origin,
    const WorldPosition& worldPosition,
    RenderPosition& outPosition,
    const RenderSpaceLimits& limits) noexcept
{
    if (!isValidLimit(limits))
    {
        return RenderSpaceResult::InvalidArgument;
    }

    WorldVector relative;
    const WorldOriginResult rebaseResult = rebasePosition(origin, worldPosition, relative);
    if (rebaseResult != WorldOriginResult::Success)
    {
        return RenderSpaceResult::InvalidArgument;
    }

    if (!isWithinLimit(relative, limits))
    {
        return RenderSpaceResult::OutOfRange;
    }

    outPosition = toRenderPositionUnchecked(relative);
    return RenderSpaceResult::Success;
}

RenderSpaceResult toRenderBounds(
    const WorldOrigin& origin,
    const WorldBounds& worldBounds,
    RenderBounds& outBounds,
    const RenderSpaceLimits& limits) noexcept
{
    if (!isValidLimit(limits))
    {
        return RenderSpaceResult::InvalidArgument;
    }

    WorldBounds relative;
    const WorldOriginResult rebaseResult = rebaseBounds(origin, worldBounds, relative);
    if (rebaseResult != WorldOriginResult::Success)
    {
        return RenderSpaceResult::InvalidArgument;
    }

    WorldVector relativeMin;
    relativeMin.x = relative.min.x;
    relativeMin.y = relative.min.y;
    relativeMin.z = relative.min.z;

    WorldVector relativeMax;
    relativeMax.x = relative.max.x;
    relativeMax.y = relative.max.y;
    relativeMax.z = relative.max.z;

    if (!isWithinLimit(relativeMin, limits) || !isWithinLimit(relativeMax, limits))
    {
        return RenderSpaceResult::OutOfRange;
    }

    outBounds.min = toRenderPositionUnchecked(relativeMin);
    outBounds.max = toRenderPositionUnchecked(relativeMax);
    return RenderSpaceResult::Success;
}
} // namespace full_engine
