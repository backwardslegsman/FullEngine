#pragma once

#include "full_renderer/ColorGrading.hpp"

#include <cstdint>

namespace full_renderer::scene
{
struct ColorGradingRenderPlan
{
    float params[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float controls[4] = {1.0f, 1.0f, 1.0f, 0.0f};
    float lift[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float gain[4] = {1.0f, 1.0f, 1.0f, 0.0f};
    TonemapOperator tonemapOperator = TonemapOperator::None;
    ColorGradingDebugMode debugMode = ColorGradingDebugMode::None;
    bool enabled = false;
    bool tonemapEnabled = false;
    bool passRequired = false;
    bool passSubmitted = false;
    bool sceneColorTargetAvailable = false;
    bool neutralState = true;
    bool lutRequested = false;
    bool lutActive = false;
    bool lutFallback = false;
    bool lutSamplingSupported = false;
    std::uint32_t clampedValueCount = 0;
};

bool isValidTonemapOperator(TonemapOperator operatorType) noexcept;
bool isValidColorGradingDebugMode(ColorGradingDebugMode mode) noexcept;
bool isValidTonemapDesc(const TonemapDesc& desc) noexcept;
bool isValidLutDesc(const LutDesc& desc) noexcept;
bool isValidColorGradingDesc(const ColorGradingDesc& desc) noexcept;
float clampColorGradingExposureStops(float exposureStops) noexcept;
float clampColorGradingContrast(float contrast) noexcept;
float clampColorGradingSaturation(float saturation) noexcept;
float clampColorGradingGamma(float gamma) noexcept;
float clampColorGradingLift(float lift) noexcept;
float clampColorGradingGain(float gain) noexcept;
float clampColorGradingLutStrength(float strength) noexcept;
ColorGradingDesc makeDefaultColorGradingDesc() noexcept;
ColorGradingRenderPlan makeColorGradingRenderPlan(
    const ColorGradingDesc& desc,
    bool sceneColorTargetAvailable,
    bool lutTextureAvailable,
    bool lutSamplingSupported) noexcept;
} // namespace full_renderer::scene
