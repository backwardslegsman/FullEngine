#include "renderer/scene/Ssao.hpp"

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

void defaultSsaoIsDisabledAndStable(int& failures)
{
    const full_renderer::SsaoDesc desc = full_renderer::scene::makeDefaultSsaoDesc();
    expect(!desc.enabled, "default SSAO is disabled", failures);
    expect(full_renderer::scene::isValidSsaoDesc(desc), "default SSAO descriptor validates", failures);

    const full_renderer::scene::SsaoUniformPlan plan = full_renderer::scene::makeSsaoUniformPlan(desc);
    expect(!plan.enabled, "default disabled SSAO produces neutral plan", failures);
    expect(plan.sampleCount == 0, "disabled SSAO uploads no samples", failures);
    expect(plan.intensity == 0.0f, "disabled SSAO uploads neutral intensity", failures);
    expect(desc.halfResolution, "default SSAO requests half-resolution when enabled", failures);
    expect(desc.blurEnabled, "default SSAO requests blur when enabled", failures);
}

void enabledSsaoValidationAndClamping(int& failures)
{
    full_renderer::SsaoDesc desc;
    desc.enabled = true;
    desc.radiusMeters = 1.5f;
    desc.intensity = 0.75f;
    desc.biasMeters = 0.08f;
    desc.power = 1.25f;
    desc.maxDistanceMeters = 90.0f;
    desc.sampleCount = 8;
    desc.halfResolution = true;
    desc.blurEnabled = true;
    desc.blurRadiusPixels = 1.5f;
    expect(full_renderer::scene::isValidSsaoDesc(desc), "enabled valid SSAO descriptor validates", failures);

    full_renderer::scene::SsaoUniformPlan plan = full_renderer::scene::makeSsaoUniformPlan(desc);
    expect(plan.enabled, "enabled SSAO descriptor produces enabled plan", failures);
    expect(plan.halfResolution, "SSAO plan preserves half-resolution mode", failures);
    expect(plan.blurEnabled, "SSAO plan enables blur with positive blur radius", failures);
    expect(plan.blurRadiusPixels == desc.blurRadiusPixels, "SSAO blur radius is preserved when valid", failures);
    expect(plan.radiusPixels > 0.0f, "SSAO radius maps to positive pixel radius", failures);
    expect(plan.intensity == desc.intensity, "SSAO intensity is preserved when valid", failures);
    expect(plan.biasNormalized > 0.0f, "SSAO bias normalizes against max distance", failures);
    expect(plan.power == desc.power, "SSAO power is preserved when valid", failures);
    expect(plan.sampleCount == 8, "SSAO sample count preserves 8-sample mode", failures);

    expect(full_renderer::scene::clampSsaoRadiusMeters(-1.0f) == 0.05f,
        "negative radius clamps to minimum",
        failures);
    expect(full_renderer::scene::clampSsaoIntensity(9.0f) == 4.0f,
        "high intensity clamps",
        failures);
    expect(full_renderer::scene::clampSsaoBiasMeters(-1.0f) == 0.0f,
        "negative bias clamps to zero",
        failures);
    expect(full_renderer::scene::clampSsaoBlurRadiusPixels(99.0f) == 4.0f,
        "high blur radius clamps",
        failures);
    expect(full_renderer::scene::clampSsaoPower(0.0f) == 0.1f,
        "low power clamps",
        failures);
    expect(full_renderer::scene::clampSsaoMaxDistanceMeters(
               std::numeric_limits<float>::quiet_NaN()) == 90.0f,
        "non-finite max distance uses default",
        failures);
    expect(full_renderer::scene::clampSsaoSampleCount(1) == 4,
        "low sample count clamps to 4",
        failures);
    expect(full_renderer::scene::clampSsaoSampleCount(99) == 8,
        "high sample count clamps to 8",
        failures);
}

void invalidEnabledSsaoDescriptorsAreRejected(int& failures)
{
    full_renderer::SsaoDesc desc;
    desc.enabled = true;
    desc.radiusMeters = -1.0f;
    expect(!full_renderer::scene::isValidSsaoDesc(desc), "negative enabled radius is rejected", failures);

    desc = {};
    desc.enabled = true;
    desc.intensity = 6.0f;
    expect(!full_renderer::scene::isValidSsaoDesc(desc), "out-of-range enabled intensity is rejected", failures);

    desc = {};
    desc.enabled = true;
    desc.blurRadiusPixels = -1.0f;
    expect(!full_renderer::scene::isValidSsaoDesc(desc), "negative enabled blur radius is rejected", failures);

    desc = {};
    desc.enabled = true;
    desc.power = std::numeric_limits<float>::infinity();
    expect(!full_renderer::scene::isValidSsaoDesc(desc), "non-finite enabled power is rejected", failures);

    desc = {};
    desc.enabled = false;
    desc.radiusMeters = std::numeric_limits<float>::quiet_NaN();
    expect(full_renderer::scene::isValidSsaoDesc(desc), "disabled SSAO ignores invalid tuning values", failures);
}

void ssaoPlanningIsDeterministic(int& failures)
{
    full_renderer::SsaoDesc desc;
    desc.enabled = true;
    desc.radiusMeters = 2.0f;
    desc.intensity = 1.2f;
    desc.biasMeters = 0.05f;
    desc.power = 1.8f;
    desc.maxDistanceMeters = 120.0f;
    desc.sampleCount = 8;
    desc.debugVisualize = true;

    const full_renderer::scene::SsaoUniformPlan first = full_renderer::scene::makeSsaoUniformPlan(desc);
    const full_renderer::scene::SsaoUniformPlan second = full_renderer::scene::makeSsaoUniformPlan(desc);
    expect(first.enabled == second.enabled, "SSAO enabled planning is deterministic", failures);
    expect(first.radiusPixels == second.radiusPixels, "SSAO radius planning is deterministic", failures);
    expect(first.biasNormalized == second.biasNormalized, "SSAO bias planning is deterministic", failures);
    expect(first.sampleCount == second.sampleCount, "SSAO sample planning is deterministic", failures);
    expect(first.debugVisualize && second.debugVisualize, "SSAO debug visualize flag is retained", failures);

    desc.blurEnabled = false;
    const full_renderer::scene::SsaoUniformPlan noBlur = full_renderer::scene::makeSsaoUniformPlan(desc);
    expect(!noBlur.blurEnabled, "disabled SSAO blur produces raw AO plan", failures);

    desc.blurEnabled = true;
    desc.blurRadiusPixels = 0.0f;
    const full_renderer::scene::SsaoUniformPlan zeroBlur = full_renderer::scene::makeSsaoUniformPlan(desc);
    expect(!zeroBlur.blurEnabled, "zero blur radius produces raw AO plan", failures);
}

void ssaoTargetDimensionsHandleHalfResolution(int& failures)
{
    full_renderer::scene::SsaoTargetDimensions dimensions =
        full_renderer::scene::makeSsaoTargetDimensions(641, 479, true);
    expect(dimensions.width == 321 && dimensions.height == 240,
        "odd viewport dimensions round half-resolution AO up",
        failures);

    dimensions = full_renderer::scene::makeSsaoTargetDimensions(1, 1, true);
    expect(dimensions.width == 1 && dimensions.height == 1,
        "tiny viewport dimensions clamp to one AO pixel",
        failures);

    dimensions = full_renderer::scene::makeSsaoTargetDimensions(641, 479, false);
    expect(dimensions.width == 641 && dimensions.height == 479,
        "full-resolution AO preserves viewport dimensions",
        failures);

    dimensions = full_renderer::scene::makeSsaoTargetDimensions(0, 479, true);
    expect(dimensions.width == 0 && dimensions.height == 0,
        "invalid viewport dimensions produce zero AO dimensions",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    defaultSsaoIsDisabledAndStable(failures);
    enabledSsaoValidationAndClamping(failures);
    invalidEnabledSsaoDescriptorsAreRejected(failures);
    ssaoPlanningIsDeterministic(failures);
    ssaoTargetDimensionsHandleHalfResolution(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
