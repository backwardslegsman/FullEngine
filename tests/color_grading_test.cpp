#include "renderer/scene/ColorGrading.hpp"

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

void defaultColorGradingIsNeutral(int& failures)
{
    const full_renderer::ColorGradingDesc desc =
        full_renderer::scene::makeDefaultColorGradingDesc();
    expect(!desc.enabled, "default color grading is disabled", failures);
    expect(full_renderer::scene::isValidColorGradingDesc(desc),
        "default color grading descriptor validates",
        failures);

    const full_renderer::scene::ColorGradingRenderPlan plan =
        full_renderer::scene::makeColorGradingRenderPlan(desc, true, false, false);
    expect(!plan.enabled, "disabled color grading produces disabled plan", failures);
    expect(!plan.passSubmitted, "disabled color grading submits no fullscreen pass", failures);
    expect(plan.neutralState, "disabled color grading is neutral", failures);
}

void enabledNeutralColorGradingRequestsSceneTarget(int& failures)
{
    full_renderer::ColorGradingDesc desc =
        full_renderer::scene::makeDefaultColorGradingDesc();
    desc.enabled = true;

    const full_renderer::scene::ColorGradingRenderPlan plan =
        full_renderer::scene::makeColorGradingRenderPlan(desc, true, false, false);
    expect(plan.enabled, "enabled neutral color grading is active", failures);
    expect(plan.passRequired, "enabled color grading requires final pass planning", failures);
    expect(plan.passSubmitted, "enabled color grading submits when scene color is available", failures);
    expect(plan.sceneColorTargetAvailable, "scene color target availability is reported", failures);
    expect(plan.neutralState, "enabled neutral values remain neutral", failures);
    expect(plan.params[0] == 1.0f, "neutral exposure uploads scale one", failures);
    expect(plan.controls[0] == 1.0f &&
            plan.controls[1] == 1.0f &&
            plan.controls[2] == 1.0f,
        "neutral grading controls upload identity values",
        failures);

    const full_renderer::scene::ColorGradingRenderPlan missingSceneTarget =
        full_renderer::scene::makeColorGradingRenderPlan(desc, false, false, false);
    expect(missingSceneTarget.enabled, "missing scene target keeps descriptor active in stats", failures);
    expect(missingSceneTarget.passRequired, "missing scene target still reports pass requirement", failures);
    expect(!missingSceneTarget.passSubmitted, "missing scene target prevents pass submission", failures);
}

void colorGradingValidationRejectsInvalidEnabledValues(int& failures)
{
    full_renderer::ColorGradingDesc desc =
        full_renderer::scene::makeDefaultColorGradingDesc();
    desc.enabled = true;
    desc.tonemap.operatorType = static_cast<full_renderer::TonemapOperator>(99);
    expect(!full_renderer::scene::isValidColorGradingDesc(desc),
        "invalid tonemap operator is rejected",
        failures);

    desc = full_renderer::scene::makeDefaultColorGradingDesc();
    desc.enabled = true;
    desc.contrast = std::numeric_limits<float>::quiet_NaN();
    expect(!full_renderer::scene::isValidColorGradingDesc(desc),
        "non-finite contrast is rejected when color grading is enabled",
        failures);

    desc = full_renderer::scene::makeDefaultColorGradingDesc();
    desc.enabled = true;
    desc.debugMode = static_cast<full_renderer::ColorGradingDebugMode>(99);
    expect(!full_renderer::scene::isValidColorGradingDesc(desc),
        "invalid color grading debug mode is rejected",
        failures);

    desc = full_renderer::scene::makeDefaultColorGradingDesc();
    desc.enabled = false;
    desc.gamma = std::numeric_limits<float>::quiet_NaN();
    expect(full_renderer::scene::isValidColorGradingDesc(desc),
        "disabled color grading ignores invalid nested values",
        failures);
}

void colorGradingClampsForRenderPlanning(int& failures)
{
    full_renderer::ColorGradingDesc desc =
        full_renderer::scene::makeDefaultColorGradingDesc();
    desc.enabled = true;
    desc.tonemap.enabled = true;
    desc.tonemap.operatorType = full_renderer::TonemapOperator::AcesApproximation;
    desc.tonemap.exposureStops = 99.0f;
    desc.contrast = 99.0f;
    desc.saturation = -2.0f;
    desc.gamma = 99.0f;
    desc.liftLinear[0] = -3.0f;
    desc.liftLinear[1] = 0.5f;
    desc.liftLinear[2] = 3.0f;
    desc.gainLinear[0] = -3.0f;
    desc.gainLinear[1] = 2.0f;
    desc.gainLinear[2] = 9.0f;
    desc.lut.enabled = true;
    desc.lut.strength = 2.0f;

    const full_renderer::scene::ColorGradingRenderPlan plan =
        full_renderer::scene::makeColorGradingRenderPlan(desc, true, false, false);
    expect(plan.clampedValueCount >= 8,
        "out-of-range color grading values are counted as clamped",
        failures);
    expect(plan.params[0] == 256.0f, "exposure clamps to eight stops", failures);
    expect(plan.params[1] == 2.0f, "ACES approximation uploads shader operator two", failures);
    expect(plan.controls[0] == 4.0f, "contrast clamps to maximum", failures);
    expect(plan.controls[1] == 0.0f, "saturation clamps to minimum", failures);
    expect(plan.controls[2] == 4.0f, "gamma clamps to maximum", failures);
    expect(plan.lift[0] == -1.0f && plan.lift[2] == 1.0f,
        "lift clamps per channel",
        failures);
    expect(plan.gain[0] == 0.0f && plan.gain[2] == 4.0f,
        "gain clamps per channel",
        failures);
    expect(plan.lutRequested, "LUT request is retained after strength clamping", failures);
    expect(plan.lutFallback, "unsupported LUT sampling falls back to no LUT", failures);
}

void lutPlanningReportsActiveAndFallbackStates(int& failures)
{
    full_renderer::ColorGradingDesc desc =
        full_renderer::scene::makeDefaultColorGradingDesc();
    desc.enabled = true;
    desc.lut.enabled = true;
    desc.lut.texture.id = 42;
    desc.lut.strength = 0.5f;

    full_renderer::scene::ColorGradingRenderPlan plan =
        full_renderer::scene::makeColorGradingRenderPlan(desc, true, true, true);
    expect(plan.lutRequested, "valid LUT request is reported", failures);
    expect(plan.lutActive, "available supported LUT is active", failures);
    expect(!plan.lutFallback, "active LUT does not report fallback", failures);
    expect(plan.controls[3] == 0.5f, "LUT strength uploads into controls", failures);

    plan = full_renderer::scene::makeColorGradingRenderPlan(desc, true, false, true);
    expect(plan.lutRequested, "missing LUT texture still reports requested LUT", failures);
    expect(!plan.lutActive, "missing LUT texture is inactive", failures);
    expect(plan.lutFallback, "missing LUT texture reports fallback", failures);

    plan = full_renderer::scene::makeColorGradingRenderPlan(desc, true, true, false);
    expect(!plan.lutActive, "unsupported LUT sampling is inactive", failures);
    expect(plan.lutFallback, "unsupported LUT sampling reports fallback", failures);
}

void colorGradingPlanningIsDeterministic(int& failures)
{
    full_renderer::ColorGradingDesc desc =
        full_renderer::scene::makeDefaultColorGradingDesc();
    desc.enabled = true;
    desc.tonemap.enabled = true;
    desc.tonemap.operatorType = full_renderer::TonemapOperator::Reinhard;
    desc.tonemap.exposureStops = -1.0f;
    desc.contrast = 1.2f;
    desc.saturation = 0.8f;
    desc.gamma = 1.1f;
    desc.debugMode = full_renderer::ColorGradingDebugMode::TonemapOnly;

    const full_renderer::scene::ColorGradingRenderPlan first =
        full_renderer::scene::makeColorGradingRenderPlan(desc, true, false, false);
    const full_renderer::scene::ColorGradingRenderPlan second =
        full_renderer::scene::makeColorGradingRenderPlan(desc, true, false, false);
    expect(first.params[0] == second.params[0] &&
            first.params[1] == second.params[1] &&
            first.params[3] == second.params[3],
        "color grading params are deterministic",
        failures);
    expect(first.controls[0] == second.controls[0] &&
            first.controls[1] == second.controls[1] &&
            first.controls[2] == second.controls[2],
        "color grading controls are deterministic",
        failures);
    expect(first.passSubmitted == second.passSubmitted &&
            first.debugMode == second.debugMode,
        "color grading plan summary is deterministic",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    defaultColorGradingIsNeutral(failures);
    enabledNeutralColorGradingRequestsSceneTarget(failures);
    colorGradingValidationRejectsInvalidEnabledValues(failures);
    colorGradingClampsForRenderPlanning(failures);
    lutPlanningReportsActiveAndFallbackStates(failures);
    colorGradingPlanningIsDeterministic(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
