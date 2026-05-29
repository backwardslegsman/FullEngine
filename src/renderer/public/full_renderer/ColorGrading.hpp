#pragma once

#include "full_renderer/Handles.hpp"

namespace full_renderer
{
/**
 * @brief Tonemap operator used by the backend-private color grading pass.
 *
 * The operator is renderer-facing image adjustment state. It is evaluated only
 * when `ColorGradingDesc::enabled` and `TonemapDesc::enabled` are both true.
 * The renderer does not expose HDR swapchain details or backend resources
 * through this enum.
 */
enum class TonemapOperator
{
    /** @brief Preserve scene color after exposure and basic grading. */
    None,

    /** @brief Apply a simple Reinhard curve, `color / (1 + color)`. */
    Reinhard,

    /** @brief Apply a small ACES-inspired approximation suitable for debug tuning. */
    AcesApproximation
};

/**
 * @brief Debug mode for isolating parts of the color grading pass.
 *
 * These modes affect only the backend-private fullscreen color grading pass.
 * They do not expose intermediate textures and are intended for sample/debug
 * controls rather than editor tooling.
 */
enum class ColorGradingDebugMode
{
    /** @brief Apply the requested tonemap and grading controls normally. */
    None,

    /** @brief Show exposure and tonemap without contrast, saturation, lift, gain, or gamma. */
    TonemapOnly,

    /** @brief Show contrast, saturation, lift, gain, and gamma without exposure tonemapping. */
    GradingOnly
};

/**
 * @brief Frame-local tonemap settings for final scene color adjustment.
 *
 * Values are copied during `IRenderer::submit` and are not retained after the
 * frame. `exposureStops` is an EV-style scalar where `0` is neutral, `1`
 * doubles scene color before tonemapping, and `-1` halves it. Non-finite values
 * are invalid when the owning `ColorGradingDesc` is enabled; finite values are
 * clamped during CPU-side planning.
 *
 * @note Thread safety: Populate and submit on the renderer owner thread.
 */
struct TonemapDesc
{
    /** @brief Enables exposure and tonemap evaluation inside the color grading pass. */
    bool enabled = false;

    /** @brief Tonemap curve used when tonemapping is enabled. */
    TonemapOperator operatorType = TonemapOperator::None;

    /** @brief Exposure in EV stops; `0` is neutral. */
    float exposureStops = 0.0f;
};

/**
 * @brief Optional LUT input for a future backend-private color grading path.
 *
 * The texture handle, when non-zero, must refer to a renderer-owned texture
 * created by the same renderer. The first implementation validates and reports
 * LUT intent but falls back to no LUT when active LUT sampling is unsupported.
 * No backend texture handles or framebuffer objects are exposed through this
 * descriptor.
 *
 * @note Frame validity: The handle is read during `IRenderer::submit`; callers
 * should keep referenced textures live while submitting frames that use them.
 */
struct LutDesc
{
    /** @brief Enables LUT contribution planning. Missing or unsupported LUTs fall back to no LUT. */
    bool enabled = false;

    /** @brief Optional renderer-owned LUT texture. Zero requests the no-LUT fallback. */
    TextureHandle texture;

    /** @brief Blend strength for the LUT contribution in `[0, 1]`. */
    float strength = 0.0f;
};

/**
 * @brief Frame-local final color grading and tonemap settings.
 *
 * The descriptor owns no resources. It is copied during the current
 * `IRenderer::submit` call and never retained by the renderer. Disabled color
 * grading preserves the previous scene output and submits no grading pass
 * unless another backend-private post pass already needs scene presentation.
 * Enabled neutral values are intended to be visually close to the existing
 * output: tonemap disabled, exposure `0`, contrast/saturation/gamma `1`, lift
 * `0`, gain `1`, and LUT strength `0`.
 *
 * The active backend implementation uses the existing private scene color
 * target and a fullscreen pass before selection outlines, debug overlays, and
 * Dear ImGui. LUT texture sampling is an extension point in this milestone:
 * invalid, missing, or unsupported LUT resources fall back to no LUT and report
 * diagnostics through `RendererStats`.
 *
 * @note Thread safety: Populate and submit on the renderer owner thread.
 * @note Frame validity: Values are read only during the current submit call.
 */
struct ColorGradingDesc
{
    /** @brief Enables the backend-private final color adjustment pass. */
    bool enabled = false;

    /** @brief Exposure and tonemap settings used by the pass. */
    TonemapDesc tonemap;

    /** @brief Contrast multiplier; `1` is neutral. */
    float contrast = 1.0f;

    /** @brief Saturation multiplier; `1` is neutral and `0` is grayscale. */
    float saturation = 1.0f;

    /** @brief Output gamma adjustment; `1` is neutral. */
    float gamma = 1.0f;

    /** @brief Linear RGB lift added before contrast/saturation/gamma; `{0,0,0}` is neutral. */
    float liftLinear[3] = {0.0f, 0.0f, 0.0f};

    /** @brief Linear RGB gain multiplied before contrast/saturation/gamma; `{1,1,1}` is neutral. */
    float gainLinear[3] = {1.0f, 1.0f, 1.0f};

    /** @brief Optional LUT intent and strength. Unsupported LUTs fall back to no LUT. */
    LutDesc lut;

    /** @brief Optional debug isolation mode for the color grading fullscreen pass. */
    ColorGradingDebugMode debugMode = ColorGradingDebugMode::None;
};
} // namespace full_renderer
