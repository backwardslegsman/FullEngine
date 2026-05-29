#include "renderer/scene/Environment.hpp"

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

void defaultEnvironmentIsValidAndUseful(int& failures)
{
    const full_renderer::EnvironmentDesc environment =
        full_renderer::scene::makeDefaultOpenWorldEnvironmentDesc();
    expect(environment.skyEnabled, "default environment enables sky", failures);
    expect(environment.fogEnabled, "default environment enables fog", failures);
    expect(environment.fogEndMeters > environment.fogStartMeters,
        "default fog end exceeds start",
        failures);
    expect(environment.fogStartMeters >= 90.0f,
        "default fog starts beyond nearby validation meshes",
        failures);
    expect(environment.fogColorLinear[0] != environment.skyHorizonColorLinear[0] ||
            environment.fogColorLinear[1] != environment.skyHorizonColorLinear[1] ||
            environment.fogColorLinear[2] != environment.skyHorizonColorLinear[2],
        "default fog color is distinct from sky horizon color",
        failures);
    expect(full_renderer::scene::isValidEnvironmentDesc(environment),
        "default environment validates",
        failures);

    const full_renderer::scene::EnvironmentUniformPlan plan =
        full_renderer::scene::makeEnvironmentUniformPlan(environment);
    expect(plan.skyEnabled && plan.fogEnabled,
        "default uniform plan enables sky and fog",
        failures);
    expect(plan.colors[0][3] == 1.0f && plan.colors[3][3] == 1.0f,
        "environment colors are uploaded as opaque vec4 values",
        failures);
    expect(plan.fogParams[0] == 1.0f && plan.fogParams[2] > 0.0f,
        "enabled fog uploads active inverse range",
        failures);
}

void invalidEnvironmentDescriptorsAreRejected(int& failures)
{
    full_renderer::EnvironmentDesc environment;
    environment.skyZenithColorLinear[0] = -0.1f;
    expect(!full_renderer::scene::isValidEnvironmentDesc(environment),
        "negative sky color is rejected",
        failures);

    environment = {};
    environment.fogColorLinear[2] = 1.2f;
    expect(!full_renderer::scene::isValidEnvironmentDesc(environment),
        "fog color outside unit range is rejected",
        failures);

    environment = {};
    environment.fogStartMeters = 25.0f;
    environment.fogEndMeters = 25.0f;
    expect(!full_renderer::scene::isValidEnvironmentDesc(environment),
        "enabled fog requires positive range",
        failures);

    environment.fogEnabled = false;
    expect(full_renderer::scene::isValidEnvironmentDesc(environment),
        "disabled fog ignores invalid distances",
        failures);
}

void environmentUniformPlanningIsDeterministic(int& failures)
{
    full_renderer::EnvironmentDesc environment;
    environment.skyEnabled = false;
    environment.fogEnabled = false;
    environment.fogStartMeters = std::numeric_limits<float>::quiet_NaN();
    environment.fogEndMeters = -10.0f;

    full_renderer::scene::EnvironmentUniformPlan first =
        full_renderer::scene::makeEnvironmentUniformPlan(environment);
    full_renderer::scene::EnvironmentUniformPlan second =
        full_renderer::scene::makeEnvironmentUniformPlan(environment);

    expect(!first.skyEnabled && !first.fogEnabled,
        "disabled environment produces disabled plan",
        failures);
    expect(first.fogParams[0] == 0.0f && first.fogParams[2] == 0.0f,
        "disabled fog uploads neutral fog parameters",
        failures);
    for (int colorIndex = 0; colorIndex < 4; ++colorIndex)
    {
        for (int channel = 0; channel < 4; ++channel)
        {
            expect(first.colors[colorIndex][channel] == second.colors[colorIndex][channel],
                "environment color planning is deterministic",
                failures);
        }
    }
    for (int index = 0; index < 4; ++index)
    {
        expect(first.fogParams[index] == second.fogParams[index],
            "fog parameter planning is deterministic",
            failures);
    }

    expect(full_renderer::scene::clampFogDistance(-5.0f, 3.0f) == 0.0f,
        "negative fog distance clamps to zero",
        failures);
    expect(full_renderer::scene::clampFogDistance(
               std::numeric_limits<float>::quiet_NaN(),
               3.0f) == 3.0f,
        "non-finite fog distance uses fallback",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    defaultEnvironmentIsValidAndUseful(failures);
    invalidEnvironmentDescriptorsAreRejected(failures);
    environmentUniformPlanningIsDeterministic(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
