#pragma once

namespace full_engine
{
/** @brief Double-precision absolute engine world position in meters. */
struct WorldPosition
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/** @brief Double-precision relative engine world vector in meters. */
struct WorldVector
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/** @brief Double-precision engine-owned axis-aligned world bounds in meters. */
struct WorldBounds
{
    WorldPosition min = {};
    WorldPosition max = {};
};

/** @brief Origin policy used before engine state is submitted to renderer integration. */
enum class OriginMode
{
    Absolute,
    CameraRelative,
};

/** @brief Engine-owned large-world origin descriptor. */
struct WorldOrigin
{
    OriginMode mode = OriginMode::Absolute;
    WorldPosition originWorld = {};
};

/** @brief Result code for large-world origin helper operations. */
enum class WorldOriginResult
{
    Success,
    InvalidArgument,
};

/** @brief Returns whether all position components are finite. */
bool isFinite(const WorldPosition& position) noexcept;

/** @brief Returns whether all bound components are finite and min <= max on each axis. */
bool isFinite(const WorldBounds& bounds) noexcept;

/** @brief Creates an absolute origin at world zero. */
WorldOrigin makeAbsoluteOrigin() noexcept;

/** @brief Creates a camera-relative origin using the supplied world position. */
WorldOrigin makeCameraRelativeOrigin(const WorldPosition& cameraWorld) noexcept;

/** @brief Converts an absolute world position to origin-relative engine coordinates. */
WorldOriginResult rebasePosition(
    const WorldOrigin& origin,
    const WorldPosition& worldPosition,
    WorldVector& outRelative) noexcept;

/** @brief Converts absolute world bounds to origin-relative engine bounds. */
WorldOriginResult rebaseBounds(
    const WorldOrigin& origin,
    const WorldBounds& worldBounds,
    WorldBounds& outRelativeBounds) noexcept;
} // namespace full_engine
