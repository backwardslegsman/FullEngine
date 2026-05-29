#include "renderer/scene/Weather.hpp"

#include "renderer/scene/Environment.hpp"

#include <algorithm>
#include <cmath>

namespace full_renderer::scene
{
namespace
{
constexpr float kMinimumDirectionLengthSquared = 0.000001f;
constexpr float kMaximumWindSpeedMetersPerSecond = 128.0f;
constexpr float kMaximumPrecipitationScale = 8.0f;
constexpr float kMinimumFogDistanceScale = 0.05f;
constexpr float kMaximumFogDistanceScale = 4.0f;
constexpr float kMinimumFogRangeMeters = 0.001f;

bool isUnitRangeColor(const float* values, const std::uint32_t count) noexcept
{
    for (std::uint32_t index = 0; index < count; ++index)
    {
        if (!std::isfinite(values[index]) || values[index] < 0.0f || values[index] > 1.0f)
        {
            return false;
        }
    }
    return true;
}

bool isFiniteVector(const float* values, const std::uint32_t count) noexcept
{
    for (std::uint32_t index = 0; index < count; ++index)
    {
        if (!std::isfinite(values[index]))
        {
            return false;
        }
    }
    return true;
}

bool isValidPrecipitationType(const PrecipitationType type) noexcept
{
    switch (type)
    {
    case PrecipitationType::None:
    case PrecipitationType::Rain:
    case PrecipitationType::Snow:
    case PrecipitationType::Dust:
        return true;
    }
    return false;
}

float clampUnit(const float value, std::uint32_t& clampedValueCount) noexcept
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    if (clamped != value)
    {
        ++clampedValueCount;
    }
    return clamped;
}

float clampRange(
    const float value,
    const float minimumValue,
    const float maximumValue,
    std::uint32_t& clampedValueCount) noexcept
{
    const float clamped = std::clamp(value, minimumValue, maximumValue);
    if (clamped != value)
    {
        ++clampedValueCount;
    }
    return clamped;
}

void normalizeOrFallback(
    const float source[3],
    const float fallback[3],
    float outDirection[3],
    std::uint32_t& clampedValueCount) noexcept
{
    const float lengthSquared =
        source[0] * source[0] +
        source[1] * source[1] +
        source[2] * source[2];
    if (lengthSquared <= kMinimumDirectionLengthSquared)
    {
        outDirection[0] = fallback[0];
        outDirection[1] = fallback[1];
        outDirection[2] = fallback[2];
        ++clampedValueCount;
        return;
    }

    const float invLength = 1.0f / std::sqrt(lengthSquared);
    outDirection[0] = source[0] * invLength;
    outDirection[1] = source[1] * invLength;
    outDirection[2] = source[2] * invLength;
}

void mixColor3(const float a[3], const float b[3], const float t, float out[3]) noexcept
{
    for (std::uint32_t channel = 0; channel < 3; ++channel)
    {
        out[channel] = a[channel] * (1.0f - t) + b[channel] * t;
    }
}
} // namespace

bool isValidWeatherDesc(const WeatherDesc& desc) noexcept
{
    if (!desc.enabled)
    {
        return true;
    }

    if (!isFiniteVector(desc.wind.directionWorld, 3) ||
        !std::isfinite(desc.wind.speedMetersPerSecond) ||
        !std::isfinite(desc.wind.gustStrength) ||
        !isValidPrecipitationType(desc.precipitation.type) ||
        !isFiniteVector(desc.precipitation.directionWorld, 3) ||
        !std::isfinite(desc.precipitation.intensity) ||
        !isUnitRangeColor(desc.precipitation.particleTintLinear, 4) ||
        !std::isfinite(desc.precipitation.particleSizeScale) ||
        !std::isfinite(desc.precipitation.particleAlphaScale) ||
        !std::isfinite(desc.wetness.amount) ||
        !std::isfinite(desc.wetness.darkeningAmount) ||
        !isUnitRangeColor(desc.fogBlend.weatherFogColorLinear, 3) ||
        !std::isfinite(desc.fogBlend.blendAmount) ||
        !std::isfinite(desc.fogBlend.fogDistanceScale))
    {
        return false;
    }

    return true;
}

WeatherDesc makeDefaultWeatherDesc() noexcept
{
    return {};
}

WeatherRenderPlan makeWeatherRenderPlan(
    const WeatherDesc& weather,
    const EnvironmentDesc& baseEnvironment) noexcept
{
    WeatherRenderPlan plan;
    plan.environment = baseEnvironment;

    if (!weather.enabled || !isValidWeatherDesc(weather))
    {
        return plan;
    }

    plan.weatherEnabled = true;
    plan.neutralState = false;

    if (weather.wind.enabled)
    {
        constexpr float kFallbackWindDirection[3] = {1.0f, 0.0f, 0.0f};
        float direction[3] = {};
        normalizeOrFallback(weather.wind.directionWorld, kFallbackWindDirection, direction, plan.clampedValueCount);
        plan.windEnabled = true;
        plan.windParams[0] = direction[0];
        plan.windParams[1] = direction[1];
        plan.windParams[2] = direction[2];
        plan.windParams[3] = clampRange(
            weather.wind.speedMetersPerSecond,
            0.0f,
            kMaximumWindSpeedMetersPerSecond,
            plan.clampedValueCount);
    }

    if (weather.precipitation.enabled && weather.precipitation.type != PrecipitationType::None)
    {
        constexpr float kFallbackPrecipitationDirection[3] = {0.0f, -1.0f, 0.0f};
        float direction[3] = {};
        normalizeOrFallback(
            weather.precipitation.directionWorld,
            kFallbackPrecipitationDirection,
            direction,
            plan.clampedValueCount);
        plan.precipitationEnabled = true;
        plan.precipitationUsesParticleBatches = weather.precipitation.usesParticleBatches;
        plan.precipitationType = weather.precipitation.type;
        plan.precipitationIntensity = clampUnit(weather.precipitation.intensity, plan.clampedValueCount);
        plan.precipitationParams[0] = static_cast<float>(weather.precipitation.type);
        plan.precipitationParams[1] = plan.precipitationIntensity;
        plan.precipitationParams[2] = clampRange(
            weather.precipitation.particleSizeScale,
            0.0f,
            kMaximumPrecipitationScale,
            plan.clampedValueCount);
        plan.precipitationParams[3] = clampUnit(
            weather.precipitation.particleAlphaScale,
            plan.clampedValueCount);
    }

    if (weather.wetness.enabled)
    {
        plan.wetnessAmount = clampUnit(weather.wetness.amount, plan.clampedValueCount);
        const float darkeningAmount = clampUnit(weather.wetness.darkeningAmount, plan.clampedValueCount);
        plan.wetnessEnabled = plan.wetnessAmount > 0.0f;
        plan.terrainWetnessActive = plan.wetnessEnabled && weather.wetness.terrainWetnessEnabled;
        plan.meshWetnessActive = plan.wetnessEnabled && weather.wetness.meshWetnessEnabled;
        plan.weatherParams[0] = plan.terrainWetnessActive ? plan.wetnessAmount : 0.0f;
        plan.weatherParams[1] = plan.meshWetnessActive ? plan.wetnessAmount : 0.0f;
        plan.weatherParams[2] = darkeningAmount;
        plan.weatherParams[3] = 1.0f;
    }

    if (weather.fogBlend.enabled && baseEnvironment.fogEnabled && isValidEnvironmentDesc(baseEnvironment))
    {
        plan.fogBlendAmount = clampUnit(weather.fogBlend.blendAmount, plan.clampedValueCount);
        const float fogDistanceScale = clampRange(
            weather.fogBlend.fogDistanceScale,
            kMinimumFogDistanceScale,
            kMaximumFogDistanceScale,
            plan.clampedValueCount);
        if (plan.fogBlendAmount > 0.0f || fogDistanceScale != 1.0f)
        {
            plan.fogBlendEnabled = true;
            mixColor3(
                baseEnvironment.fogColorLinear,
                weather.fogBlend.weatherFogColorLinear,
                plan.fogBlendAmount,
                plan.environment.fogColorLinear);
            plan.environment.fogStartMeters = std::max(0.0f, baseEnvironment.fogStartMeters * fogDistanceScale);
            const float scaledEnd = std::max(0.0f, baseEnvironment.fogEndMeters * fogDistanceScale);
            plan.environment.fogEndMeters = std::max(
                plan.environment.fogStartMeters + kMinimumFogRangeMeters,
                scaledEnd);
        }
    }

    plan.neutralState = !plan.windEnabled &&
        !plan.precipitationEnabled &&
        !plan.wetnessEnabled &&
        !plan.fogBlendEnabled;
    return plan;
}
} // namespace full_renderer::scene
