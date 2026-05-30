#pragma once

#include "engine/world/WorldOrigin.hpp"

namespace full_engine
{
/** @brief Single-precision renderer-ready position in origin-relative meters. */
struct RenderPosition
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

/** @brief Single-precision renderer-ready axis-aligned bounds in origin-relative meters. */
struct RenderBounds
{
    RenderPosition min = {};
    RenderPosition max = {};
};

/** @brief Result code for engine-to-render-space conversion helpers. */
enum class RenderSpaceResult
{
    Success,
    InvalidArgument,
    OutOfRange,
};

/** @brief Validation limits for converting double-precision engine data to renderer space. */
struct RenderSpaceLimits
{
    double maxAbsCoordinate = 1000000.0;
};

/** @brief Converts an engine world position into renderer-ready relative float coordinates. */
RenderSpaceResult toRenderPosition(
    const WorldOrigin& origin,
    const WorldPosition& worldPosition,
    RenderPosition& outPosition,
    const RenderSpaceLimits& limits = {}) noexcept;

/** @brief Converts engine world bounds into renderer-ready relative float bounds. */
RenderSpaceResult toRenderBounds(
    const WorldOrigin& origin,
    const WorldBounds& worldBounds,
    RenderBounds& outBounds,
    const RenderSpaceLimits& limits = {}) noexcept;
} // namespace full_engine
