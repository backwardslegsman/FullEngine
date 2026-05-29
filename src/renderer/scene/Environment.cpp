#include "renderer/scene/Environment.hpp"

#include <algorithm>
#include <cmath>

namespace full_renderer::scene
{
namespace
{
constexpr float kMinimumFogRangeMeters = 0.001f;

bool isUnitRangeColor3(const float values[3]) noexcept
{
    for (int index = 0; index < 3; ++index)
    {
        if (!std::isfinite(values[index]) || values[index] < 0.0f || values[index] > 1.0f)
        {
            return false;
        }
    }
    return true;
}
} // namespace

bool isValidEnvironmentDesc(const EnvironmentDesc& desc) noexcept
{
    if (!isUnitRangeColor3(desc.skyZenithColorLinear) ||
        !isUnitRangeColor3(desc.skyHorizonColorLinear) ||
        !isUnitRangeColor3(desc.skyGroundColorLinear) ||
        !isUnitRangeColor3(desc.fogColorLinear))
    {
        return false;
    }

    if (!desc.fogEnabled)
    {
        return true;
    }

    return std::isfinite(desc.fogStartMeters) &&
        std::isfinite(desc.fogEndMeters) &&
        desc.fogStartMeters >= 0.0f &&
        desc.fogEndMeters > desc.fogStartMeters + kMinimumFogRangeMeters;
}

float clampFogDistance(const float value, const float fallback) noexcept
{
    if (!std::isfinite(value))
    {
        return fallback;
    }
    return std::max(0.0f, value);
}

EnvironmentUniformPlan makeEnvironmentUniformPlan(const EnvironmentDesc& desc) noexcept
{
    EnvironmentUniformPlan plan;
    plan.skyEnabled = desc.skyEnabled;
    plan.fogEnabled = desc.fogEnabled && isValidEnvironmentDesc(desc);

    for (int channel = 0; channel < 3; ++channel)
    {
        plan.colors[0][channel] = desc.skyZenithColorLinear[channel];
        plan.colors[1][channel] = desc.skyHorizonColorLinear[channel];
        plan.colors[2][channel] = desc.skyGroundColorLinear[channel];
        plan.colors[3][channel] = desc.fogColorLinear[channel];
    }
    plan.colors[0][3] = 1.0f;
    plan.colors[1][3] = 1.0f;
    plan.colors[2][3] = 1.0f;
    plan.colors[3][3] = 1.0f;

    const float fogStart = clampFogDistance(desc.fogStartMeters, 0.0f);
    const float fogEnd = std::max(
        fogStart + kMinimumFogRangeMeters,
        clampFogDistance(desc.fogEndMeters, fogStart + kMinimumFogRangeMeters));
    plan.fogParams[0] = plan.fogEnabled ? 1.0f : 0.0f;
    plan.fogParams[1] = fogStart;
    plan.fogParams[2] = plan.fogEnabled ? 1.0f / (fogEnd - fogStart) : 0.0f;
    plan.fogParams[3] = fogEnd;
    return plan;
}

EnvironmentDesc makeDefaultOpenWorldEnvironmentDesc() noexcept
{
    return {};
}
} // namespace full_renderer::scene
