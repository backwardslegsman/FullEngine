#pragma once

#include <cstdint>

namespace full_renderer
{
/**
 * @brief Per-submission fade mode for externally controlled structure visibility.
 *
 * Fade mode is renderer-facing render state only. The caller or future engine
 * decides which objects fade and submits the desired value each frame; the
 * renderer does not perform building, roof, camera obstruction, room, or
 * gameplay visibility logic.
 */
enum class FadeMode
{
    /** @brief Disable fade and preserve ordinary opaque rendering. */
    None,

    /**
     * @brief Blend the object alpha by the submitted visibility value.
     *
     * Alpha fade uses backend alpha blending with depth testing enabled and
     * depth writes disabled for the affected draw. It is simple but not sorted
     * by the renderer, so intersecting transparent structure fades may show
     * conventional transparency artifacts.
     */
    Alpha,

    /**
     * @brief Use a stable screen-space dither pattern for opaque-style fading.
     *
     * Dithered fade keeps the ordinary opaque depth/write state and discards
     * fragments according to the submitted visibility value. The pattern is
     * deterministic for a frame and avoids full transparency sorting, but can
     * show stipple aliasing at low visibility.
     */
    Dithered
};

/**
 * @brief Frame-local fade settings for one static, instanced, or skinned draw.
 *
 * This descriptor owns no resources and is copied as part of normal render
 * submission data. `visibility` is a scalar in `[0, 1]` where `1` means fully
 * visible and `0` means fully faded by the selected mode. Values are validated
 * during `IRenderer::submit` and clamped during CPU-side render planning.
 *
 * @note Thread safety: Populate and submit this descriptor on the renderer
 * owner thread.
 * @note Frame validity: Values are read only for the current submit call; the
 * renderer never retains pointers to caller-owned fade state.
 */
struct FadeDesc
{
    /** @brief Enables fade planning for this draw or instanced batch. */
    bool enabled = false;

    /** @brief Visibility scalar in `[0, 1]`; non-finite values are invalid. */
    float visibility = 1.0f;

    /** @brief Backend-neutral fade mode used for this submission. */
    FadeMode mode = FadeMode::Dithered;
};
} // namespace full_renderer
