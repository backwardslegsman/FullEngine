#include "renderer/scene/ColorGrading.hpp"

#include "full_renderer/Handles.hpp"

#include <algorithm>
#include <cmath>

namespace full_renderer::scene
{
namespace
{
constexpr float kMinExposureStops = -8.0f;
constexpr float kMaxExposureStops = 8.0f;
constexpr float kMaxContrast = 4.0f;
constexpr float kMaxSaturation = 4.0f;
constexpr float kMinGamma = 0.1f;
constexpr float kMaxGamma = 4.0f;
constexpr float kMinLift = -1.0f;
constexpr float kMaxLift = 1.0f;
constexpr float kMaxGain = 4.0f;

bool isFinite3(const float values[3]) noexcept
{
    return std::isfinite(values[0]) &&
        std::isfinite(values[1]) &&
        std::isfinite(values[2]);
}

float clampRange(
    const float value,
    const float minimum,
    const float maximum,
    std::uint32_t& clampedValueCount) noexcept
{
    const float clamped = std::clamp(value, minimum, maximum);
    if (clamped != value)
    {
        ++clampedValueCount;
    }
    return clamped;
}

float tonemapOperatorToShaderValue(const TonemapOperator operatorType) noexcept
{
    switch (operatorType)
    {
    case TonemapOperator::Reinhard:
        return 1.0f;
    case TonemapOperator::AcesApproximation:
        return 2.0f;
    case TonemapOperator::None:
        return 0.0f;
    }
    return 0.0f;
}

float debugModeToShaderValue(const ColorGradingDebugMode mode) noexcept
{
    switch (mode)
    {
    case ColorGradingDebugMode::TonemapOnly:
        return 1.0f;
    case ColorGradingDebugMode::GradingOnly:
        return 2.0f;
    case ColorGradingDebugMode::None:
        return 0.0f;
    }
    return 0.0f;
}
} // namespace

bool isValidTonemapOperator(const TonemapOperator operatorType) noexcept
{
    switch (operatorType)
    {
    case TonemapOperator::None:
    case TonemapOperator::Reinhard:
    case TonemapOperator::AcesApproximation:
        return true;
    }
    return false;
}

bool isValidColorGradingDebugMode(const ColorGradingDebugMode mode) noexcept
{
    switch (mode)
    {
    case ColorGradingDebugMode::None:
    case ColorGradingDebugMode::TonemapOnly:
    case ColorGradingDebugMode::GradingOnly:
        return true;
    }
    return false;
}

bool isValidTonemapDesc(const TonemapDesc& desc) noexcept
{
    if (!desc.enabled)
    {
        return isValidTonemapOperator(desc.operatorType) &&
            std::isfinite(desc.exposureStops);
    }

    return isValidTonemapOperator(desc.operatorType) &&
        std::isfinite(desc.exposureStops);
}

bool isValidLutDesc(const LutDesc& desc) noexcept
{
    if (!desc.enabled)
    {
        return std::isfinite(desc.strength);
    }

    return std::isfinite(desc.strength);
}

bool isValidColorGradingDesc(const ColorGradingDesc& desc) noexcept
{
    if (!desc.enabled)
    {
        return true;
    }

    return isValidTonemapDesc(desc.tonemap) &&
        isValidLutDesc(desc.lut) &&
        isValidColorGradingDebugMode(desc.debugMode) &&
        std::isfinite(desc.contrast) &&
        std::isfinite(desc.saturation) &&
        std::isfinite(desc.gamma) &&
        isFinite3(desc.liftLinear) &&
        isFinite3(desc.gainLinear);
}

float clampColorGradingExposureStops(const float exposureStops) noexcept
{
    if (!std::isfinite(exposureStops))
    {
        return 0.0f;
    }
    return std::clamp(exposureStops, kMinExposureStops, kMaxExposureStops);
}

float clampColorGradingContrast(const float contrast) noexcept
{
    if (!std::isfinite(contrast))
    {
        return 1.0f;
    }
    return std::clamp(contrast, 0.0f, kMaxContrast);
}

float clampColorGradingSaturation(const float saturation) noexcept
{
    if (!std::isfinite(saturation))
    {
        return 1.0f;
    }
    return std::clamp(saturation, 0.0f, kMaxSaturation);
}

float clampColorGradingGamma(const float gamma) noexcept
{
    if (!std::isfinite(gamma))
    {
        return 1.0f;
    }
    return std::clamp(gamma, kMinGamma, kMaxGamma);
}

float clampColorGradingLift(const float lift) noexcept
{
    if (!std::isfinite(lift))
    {
        return 0.0f;
    }
    return std::clamp(lift, kMinLift, kMaxLift);
}

float clampColorGradingGain(const float gain) noexcept
{
    if (!std::isfinite(gain))
    {
        return 1.0f;
    }
    return std::clamp(gain, 0.0f, kMaxGain);
}

float clampColorGradingLutStrength(const float strength) noexcept
{
    if (!std::isfinite(strength))
    {
        return 0.0f;
    }
    return std::clamp(strength, 0.0f, 1.0f);
}

ColorGradingDesc makeDefaultColorGradingDesc() noexcept
{
    return {};
}

ColorGradingRenderPlan makeColorGradingRenderPlan(
    const ColorGradingDesc& desc,
    const bool sceneColorTargetAvailable,
    const bool lutTextureAvailable,
    const bool lutSamplingSupported) noexcept
{
    ColorGradingRenderPlan plan;
    if (!desc.enabled || !isValidColorGradingDesc(desc))
    {
        return plan;
    }

    plan.enabled = true;
    plan.passRequired = true;
    plan.sceneColorTargetAvailable = sceneColorTargetAvailable;
    plan.passSubmitted = sceneColorTargetAvailable;
    plan.tonemapEnabled = desc.tonemap.enabled;
    plan.tonemapOperator = desc.tonemap.operatorType;
    plan.debugMode = desc.debugMode;

    const float exposureStops = clampRange(
        desc.tonemap.exposureStops,
        kMinExposureStops,
        kMaxExposureStops,
        plan.clampedValueCount);
    const float contrast = clampRange(desc.contrast, 0.0f, kMaxContrast, plan.clampedValueCount);
    const float saturation = clampRange(desc.saturation, 0.0f, kMaxSaturation, plan.clampedValueCount);
    const float gamma = clampRange(desc.gamma, kMinGamma, kMaxGamma, plan.clampedValueCount);

    for (int channel = 0; channel < 3; ++channel)
    {
        plan.lift[channel] = clampRange(desc.liftLinear[channel], kMinLift, kMaxLift, plan.clampedValueCount);
        plan.gain[channel] = clampRange(desc.gainLinear[channel], 0.0f, kMaxGain, plan.clampedValueCount);
    }

    const float lutStrength = clampRange(desc.lut.strength, 0.0f, 1.0f, plan.clampedValueCount);
    plan.lutRequested = desc.lut.enabled && lutStrength > 0.0f;
    plan.lutSamplingSupported = lutSamplingSupported;
    plan.lutActive = plan.lutRequested && lutTextureAvailable && lutSamplingSupported;
    plan.lutFallback = plan.lutRequested && !plan.lutActive;

    plan.params[0] = std::pow(2.0f, exposureStops);
    plan.params[1] = plan.tonemapEnabled ? tonemapOperatorToShaderValue(desc.tonemap.operatorType) : 0.0f;
    plan.params[2] = plan.tonemapEnabled ? 1.0f : 0.0f;
    plan.params[3] = debugModeToShaderValue(desc.debugMode);
    plan.controls[0] = contrast;
    plan.controls[1] = saturation;
    plan.controls[2] = gamma;
    plan.controls[3] = lutStrength;

    plan.neutralState = !plan.tonemapEnabled &&
        contrast == 1.0f &&
        saturation == 1.0f &&
        gamma == 1.0f &&
        plan.lift[0] == 0.0f &&
        plan.lift[1] == 0.0f &&
        plan.lift[2] == 0.0f &&
        plan.gain[0] == 1.0f &&
        plan.gain[1] == 1.0f &&
        plan.gain[2] == 1.0f &&
        !plan.lutRequested &&
        desc.debugMode == ColorGradingDebugMode::None;
    return plan;
}
} // namespace full_renderer::scene
