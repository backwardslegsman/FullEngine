#pragma once

#include "full_renderer/Renderer.hpp"

#include <cstdint>

namespace full_renderer::scene
{
struct WeatherRenderPlan
{
    EnvironmentDesc environment;
    float weatherParams[4] = {};
    float windParams[4] = {};
    float precipitationParams[4] = {};
    bool weatherEnabled = false;
    bool windEnabled = false;
    bool precipitationEnabled = false;
    bool precipitationUsesParticleBatches = false;
    bool wetnessEnabled = false;
    bool terrainWetnessActive = false;
    bool meshWetnessActive = false;
    bool fogBlendEnabled = false;
    bool neutralState = true;
    PrecipitationType precipitationType = PrecipitationType::None;
    float precipitationIntensity = 0.0f;
    float wetnessAmount = 0.0f;
    float fogBlendAmount = 0.0f;
    std::uint32_t clampedValueCount = 0;
};

bool isValidWeatherDesc(const WeatherDesc& desc) noexcept;
WeatherDesc makeDefaultWeatherDesc() noexcept;
WeatherRenderPlan makeWeatherRenderPlan(
    const WeatherDesc& weather,
    const EnvironmentDesc& baseEnvironment) noexcept;
} // namespace full_renderer::scene
