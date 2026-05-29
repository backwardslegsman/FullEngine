#include "renderer/scene/Weather.hpp"

#include "renderer/scene/Environment.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void expect(const bool condition, const char* message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

void disabledWeatherProducesNeutralPlan(int& failures)
{
    const full_renderer::EnvironmentDesc environment =
        full_renderer::scene::makeDefaultOpenWorldEnvironmentDesc();
    const full_renderer::WeatherDesc weather =
        full_renderer::scene::makeDefaultWeatherDesc();
    const full_renderer::scene::WeatherRenderPlan plan =
        full_renderer::scene::makeWeatherRenderPlan(weather, environment);

    expect(full_renderer::scene::isValidWeatherDesc(weather),
        "default weather descriptor validates",
        failures);
    expect(plan.neutralState, "disabled weather produces neutral state", failures);
    expect(!plan.weatherEnabled && !plan.windEnabled && !plan.precipitationEnabled,
        "disabled weather disables active hooks",
        failures);
    expect(plan.environment.fogStartMeters == environment.fogStartMeters &&
            plan.environment.fogEndMeters == environment.fogEndMeters,
        "disabled weather preserves base fog distances",
        failures);
    expect(plan.weatherParams[0] == 0.0f && plan.weatherParams[1] == 0.0f,
        "disabled weather uploads neutral wetness params",
        failures);
}

void windPlanningNormalizesAndClamps(int& failures)
{
    full_renderer::WeatherDesc weather;
    weather.enabled = true;
    weather.wind.enabled = true;
    weather.wind.directionWorld[0] = 0.0f;
    weather.wind.directionWorld[1] = 0.0f;
    weather.wind.directionWorld[2] = 0.0f;
    weather.wind.speedMetersPerSecond = 500.0f;

    const full_renderer::scene::WeatherRenderPlan plan =
        full_renderer::scene::makeWeatherRenderPlan(weather, {});

    expect(plan.windEnabled, "enabled wind produces active wind plan", failures);
    expect(plan.windParams[0] == 1.0f && plan.windParams[1] == 0.0f && plan.windParams[2] == 0.0f,
        "zero wind direction falls back to +X",
        failures);
    expect(plan.windParams[3] == 128.0f, "wind speed clamps to implementation maximum", failures);
    expect(plan.clampedValueCount >= 2U, "wind fallback/clamp increments clamp count", failures);
}

void precipitationPlanningValidatesAndClamps(int& failures)
{
    full_renderer::WeatherDesc weather;
    weather.enabled = true;
    weather.precipitation.enabled = true;
    weather.precipitation.type = full_renderer::PrecipitationType::Snow;
    weather.precipitation.intensity = 1.5f;
    weather.precipitation.particleSizeScale = 12.0f;
    weather.precipitation.particleAlphaScale = -1.0f;
    weather.precipitation.usesParticleBatches = false;

    const full_renderer::scene::WeatherRenderPlan plan =
        full_renderer::scene::makeWeatherRenderPlan(weather, {});

    expect(plan.precipitationEnabled, "enabled non-none precipitation is active", failures);
    expect(plan.precipitationType == full_renderer::PrecipitationType::Snow,
        "precipitation type is preserved",
        failures);
    expect(plan.precipitationIntensity == 1.0f,
        "precipitation intensity clamps to one",
        failures);
    expect(plan.precipitationParams[2] == 8.0f,
        "precipitation size scale clamps to implementation maximum",
        failures);
    expect(plan.precipitationParams[3] == 0.0f,
        "precipitation alpha scale clamps to zero",
        failures);
    expect(!plan.precipitationUsesParticleBatches,
        "precipitation particle ownership flag is copied",
        failures);

    weather.precipitation.type = static_cast<full_renderer::PrecipitationType>(99);
    expect(!full_renderer::scene::isValidWeatherDesc(weather),
        "invalid precipitation enum is rejected",
        failures);
}

void wetnessAndFogBlendPlanRenderState(int& failures)
{
    full_renderer::EnvironmentDesc environment;
    environment.fogEnabled = true;
    environment.fogColorLinear[0] = 0.2f;
    environment.fogColorLinear[1] = 0.4f;
    environment.fogColorLinear[2] = 0.6f;
    environment.fogStartMeters = 100.0f;
    environment.fogEndMeters = 200.0f;

    full_renderer::WeatherDesc weather;
    weather.enabled = true;
    weather.wetness.enabled = true;
    weather.wetness.amount = 0.5f;
    weather.wetness.darkeningAmount = 0.25f;
    weather.wetness.meshWetnessEnabled = false;
    weather.fogBlend.enabled = true;
    weather.fogBlend.weatherFogColorLinear[0] = 0.8f;
    weather.fogBlend.weatherFogColorLinear[1] = 0.8f;
    weather.fogBlend.weatherFogColorLinear[2] = 0.8f;
    weather.fogBlend.blendAmount = 0.5f;
    weather.fogBlend.fogDistanceScale = 0.5f;

    const full_renderer::scene::WeatherRenderPlan plan =
        full_renderer::scene::makeWeatherRenderPlan(weather, environment);

    expect(plan.wetnessEnabled && plan.terrainWetnessActive && !plan.meshWetnessActive,
        "wetness honors terrain and mesh toggles",
        failures);
    expect(plan.weatherParams[0] == 0.5f && plan.weatherParams[1] == 0.0f &&
            plan.weatherParams[2] == 0.25f,
        "wetness uniforms pack terrain amount, mesh amount, and darkening",
        failures);
    expect(plan.fogBlendEnabled && plan.fogBlendAmount == 0.5f,
        "fog blend reports active blend",
        failures);
    expect(plan.environment.fogStartMeters == 50.0f &&
            plan.environment.fogEndMeters == 100.0f,
        "fog blend scales start and end distances",
        failures);
    expect(std::fabs(plan.environment.fogColorLinear[0] - 0.5f) < 0.0001f,
        "fog blend mixes fog color deterministically",
        failures);
}

void invalidWeatherDescriptorsAreRejected(int& failures)
{
    full_renderer::WeatherDesc weather;
    weather.enabled = true;
    weather.wind.speedMetersPerSecond = std::numeric_limits<float>::quiet_NaN();
    expect(!full_renderer::scene::isValidWeatherDesc(weather),
        "non-finite wind speed is rejected",
        failures);

    weather = {};
    weather.enabled = true;
    weather.precipitation.particleTintLinear[0] = 1.2f;
    expect(!full_renderer::scene::isValidWeatherDesc(weather),
        "precipitation color outside unit range is rejected",
        failures);

    weather = {};
    weather.enabled = false;
    weather.fogBlend.fogDistanceScale = std::numeric_limits<float>::quiet_NaN();
    expect(full_renderer::scene::isValidWeatherDesc(weather),
        "disabled weather ignores invalid nested values",
        failures);
}

void weatherPlanningIsDeterministic(int& failures)
{
    full_renderer::WeatherDesc weather;
    weather.enabled = true;
    weather.wind.enabled = true;
    weather.wind.directionWorld[0] = 2.0f;
    weather.wind.speedMetersPerSecond = 4.0f;
    weather.wetness.enabled = true;
    weather.wetness.amount = 0.75f;

    const full_renderer::scene::WeatherRenderPlan first =
        full_renderer::scene::makeWeatherRenderPlan(weather, {});
    const full_renderer::scene::WeatherRenderPlan second =
        full_renderer::scene::makeWeatherRenderPlan(weather, {});

    for (int index = 0; index < 4; ++index)
    {
        expect(first.weatherParams[index] == second.weatherParams[index],
            "weather params are deterministic",
            failures);
        expect(first.windParams[index] == second.windParams[index],
            "wind params are deterministic",
            failures);
    }
    expect(first.wetnessAmount == second.wetnessAmount &&
            first.clampedValueCount == second.clampedValueCount,
        "weather plan summary is deterministic",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    disabledWeatherProducesNeutralPlan(failures);
    windPlanningNormalizesAndClamps(failures);
    precipitationPlanningValidatesAndClamps(failures);
    wetnessAndFogBlendPlanRenderState(failures);
    invalidWeatherDescriptorsAreRejected(failures);
    weatherPlanningIsDeterministic(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
