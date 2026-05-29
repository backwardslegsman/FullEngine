#include "renderer/scene/Ssao.hpp"

#include <algorithm>
#include <cmath>

namespace full_renderer::scene
{
namespace
{
constexpr float kMinRadiusMeters = 0.05f;
constexpr float kMaxRadiusMeters = 8.0f;
constexpr float kMaxIntensity = 4.0f;
constexpr float kMaxBiasMeters = 2.0f;
constexpr float kMaxBlurRadiusPixels = 4.0f;
constexpr float kMinPower = 0.1f;
constexpr float kMaxPower = 8.0f;
constexpr float kMinMaxDistanceMeters = 1.0f;
constexpr float kMaxMaxDistanceMeters = 1000.0f;
constexpr float kPixelsPerMeter = 12.0f;
constexpr float kMaxRadiusPixels = 32.0f;
} // namespace

float clampSsaoRadiusMeters(const float radiusMeters) noexcept
{
    if (!std::isfinite(radiusMeters))
    {
        return 1.5f;
    }
    return std::min(std::max(radiusMeters, kMinRadiusMeters), kMaxRadiusMeters);
}

float clampSsaoBlurRadiusPixels(const float blurRadiusPixels) noexcept
{
    if (!std::isfinite(blurRadiusPixels))
    {
        return 1.0f;
    }
    return std::min(std::max(blurRadiusPixels, 0.0f), kMaxBlurRadiusPixels);
}

float clampSsaoIntensity(const float intensity) noexcept
{
    if (!std::isfinite(intensity))
    {
        return 0.55f;
    }
    return std::min(std::max(intensity, 0.0f), kMaxIntensity);
}

float clampSsaoBiasMeters(const float biasMeters) noexcept
{
    if (!std::isfinite(biasMeters))
    {
        return 0.08f;
    }
    return std::min(std::max(biasMeters, 0.0f), kMaxBiasMeters);
}

float clampSsaoPower(const float power) noexcept
{
    if (!std::isfinite(power))
    {
        return 1.4f;
    }
    return std::min(std::max(power, kMinPower), kMaxPower);
}

float clampSsaoMaxDistanceMeters(const float maxDistanceMeters) noexcept
{
    if (!std::isfinite(maxDistanceMeters))
    {
        return 90.0f;
    }
    return std::min(std::max(maxDistanceMeters, kMinMaxDistanceMeters), kMaxMaxDistanceMeters);
}

std::uint32_t clampSsaoSampleCount(const std::uint32_t sampleCount) noexcept
{
    return sampleCount <= 4U ? 4U : 8U;
}

bool isValidSsaoDesc(const SsaoDesc& desc) noexcept
{
    if (!desc.enabled)
    {
        return true;
    }

    return std::isfinite(desc.radiusMeters) &&
        desc.radiusMeters > 0.0f &&
        std::isfinite(desc.intensity) &&
        desc.intensity >= 0.0f &&
        desc.intensity <= kMaxIntensity &&
        std::isfinite(desc.blurRadiusPixels) &&
        desc.blurRadiusPixels >= 0.0f &&
        std::isfinite(desc.biasMeters) &&
        desc.biasMeters >= 0.0f &&
        std::isfinite(desc.power) &&
        desc.power >= kMinPower &&
        desc.power <= kMaxPower &&
        std::isfinite(desc.maxDistanceMeters) &&
        desc.maxDistanceMeters > 0.0f &&
        desc.sampleCount > 0;
}

SsaoTargetDimensions makeSsaoTargetDimensions(
    const std::uint32_t viewportWidth,
    const std::uint32_t viewportHeight,
    const bool halfResolution) noexcept
{
    SsaoTargetDimensions dimensions;
    if (viewportWidth == 0 || viewportHeight == 0)
    {
        return dimensions;
    }

    if (!halfResolution)
    {
        dimensions.width = viewportWidth;
        dimensions.height = viewportHeight;
        return dimensions;
    }

    dimensions.width = std::max(1U, (viewportWidth + 1U) / 2U);
    dimensions.height = std::max(1U, (viewportHeight + 1U) / 2U);
    return dimensions;
}

SsaoUniformPlan makeSsaoUniformPlan(const SsaoDesc& desc) noexcept
{
    SsaoUniformPlan plan;
    plan.enabled = desc.enabled && isValidSsaoDesc(desc);
    if (!plan.enabled)
    {
        return plan;
    }

    const float radiusMeters = clampSsaoRadiusMeters(desc.radiusMeters);
    plan.halfResolution = desc.halfResolution;
    plan.blurRadiusPixels = clampSsaoBlurRadiusPixels(desc.blurRadiusPixels);
    plan.blurEnabled = desc.blurEnabled && plan.blurRadiusPixels > 0.0f;
    plan.radiusPixels = std::min(radiusMeters * kPixelsPerMeter, kMaxRadiusPixels);
    plan.intensity = clampSsaoIntensity(desc.intensity);
    const float maxDistanceMeters = clampSsaoMaxDistanceMeters(desc.maxDistanceMeters);
    plan.maxDistanceMeters = std::max(maxDistanceMeters, radiusMeters + 0.001f);
    plan.inverseMaxDistanceMeters = 1.0f / plan.maxDistanceMeters;
    plan.biasNormalized = clampSsaoBiasMeters(desc.biasMeters) * plan.inverseMaxDistanceMeters;
    plan.power = clampSsaoPower(desc.power);
    plan.sampleCount = clampSsaoSampleCount(desc.sampleCount);
    plan.debugVisualize = desc.debugVisualize;
    return plan;
}

SsaoDesc makeDefaultSsaoDesc() noexcept
{
    return {};
}
} // namespace full_renderer::scene
