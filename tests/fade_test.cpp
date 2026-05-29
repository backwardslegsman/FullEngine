#include "renderer/scene/Fade.hpp"

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

void defaultFadeIsNeutral(int& failures)
{
    const full_renderer::FadeDesc desc = full_renderer::scene::makeDefaultFadeDesc();
    const full_renderer::scene::FadeRenderState state =
        full_renderer::scene::makeFadeRenderState(desc);

    expect(full_renderer::scene::isValidFadeDesc(desc),
        "default fade descriptor validates",
        failures);
    expect(!state.enabled && !state.active && state.fullyVisible,
        "default fade state is neutral and fully visible",
        failures);
    expect(state.params[0] == 1.0f && state.params[1] == 0.0f && state.params[3] == 0.0f,
        "neutral fade uploads fully visible disabled params",
        failures);
}

void fadeVisibilityClampsForPlanning(int& failures)
{
    full_renderer::FadeDesc desc;
    desc.enabled = true;
    desc.mode = full_renderer::FadeMode::Dithered;
    desc.visibility = -2.0f;
    full_renderer::scene::FadeRenderState state = full_renderer::scene::makeFadeRenderState(desc);
    expect(state.visibility == 0.0f && state.fullyHidden && state.active,
        "negative visibility clamps to hidden active dither state",
        failures);

    desc.visibility = 3.0f;
    state = full_renderer::scene::makeFadeRenderState(desc);
    expect(state.visibility == 1.0f && state.fullyVisible && !state.active,
        "visibility above one clamps to fully visible inactive state",
        failures);
}

void invalidFadeDescriptorsAreRejected(int& failures)
{
    full_renderer::FadeDesc desc;
    desc.enabled = true;
    desc.visibility = std::numeric_limits<float>::quiet_NaN();
    expect(!full_renderer::scene::isValidFadeDesc(desc),
        "enabled fade rejects NaN visibility",
        failures);

    desc = {};
    desc.enabled = true;
    desc.mode = static_cast<full_renderer::FadeMode>(99);
    expect(!full_renderer::scene::isValidFadeDesc(desc),
        "enabled fade rejects invalid mode enum",
        failures);

    desc.enabled = false;
    expect(full_renderer::scene::isValidFadeDesc(desc),
        "disabled fade ignores invalid nested mode",
        failures);
}

void alphaAndDitherModesPlanDistinctStates(int& failures)
{
    full_renderer::FadeDesc alpha;
    alpha.enabled = true;
    alpha.mode = full_renderer::FadeMode::Alpha;
    alpha.visibility = 0.4f;
    const full_renderer::scene::FadeRenderState alphaState =
        full_renderer::scene::makeFadeRenderState(alpha);
    expect(alphaState.alphaMode && !alphaState.ditherMode && alphaState.params[1] == 1.0f,
        "alpha mode maps to alpha shader mode",
        failures);

    full_renderer::FadeDesc dither = alpha;
    dither.mode = full_renderer::FadeMode::Dithered;
    const full_renderer::scene::FadeRenderState ditherState =
        full_renderer::scene::makeFadeRenderState(dither);
    expect(ditherState.ditherMode && !ditherState.alphaMode && ditherState.params[1] == 2.0f,
        "dither mode maps to dither shader mode",
        failures);
}

void renderPlanAggregatesCounts(int& failures)
{
    full_renderer::scene::FadeRenderPlan plan;

    full_renderer::FadeDesc visible;
    visible.enabled = true;
    visible.visibility = 1.0f;
    full_renderer::scene::accumulateFadeRenderState(
        full_renderer::scene::makeFadeRenderState(visible),
        plan);

    full_renderer::FadeDesc partial;
    partial.enabled = true;
    partial.mode = full_renderer::FadeMode::Dithered;
    partial.visibility = 0.5f;
    full_renderer::scene::accumulateFadeRenderState(
        full_renderer::scene::makeFadeRenderState(partial),
        plan);

    full_renderer::FadeDesc hidden;
    hidden.enabled = true;
    hidden.mode = full_renderer::FadeMode::Alpha;
    hidden.visibility = 0.0f;
    full_renderer::scene::accumulateFadeRenderState(
        full_renderer::scene::makeFadeRenderState(hidden),
        plan);

    expect(plan.submittedCount == 3,
        "fade render plan counts submitted enabled descriptors",
        failures);
    expect(plan.activeCount == 2,
        "fade render plan counts active non-visible descriptors",
        failures);
    expect(plan.fullyVisibleCount == 1 && plan.partiallyFadedCount == 1 && plan.fullyHiddenCount == 1,
        "fade render plan separates visible, partial, and hidden states",
        failures);
    expect(plan.alphaDrawCount == 1 && plan.ditherDrawCount == 1,
        "fade render plan counts alpha and dither draws",
        failures);
}

void fadePlanningIsDeterministic(int& failures)
{
    full_renderer::FadeDesc desc;
    desc.enabled = true;
    desc.mode = full_renderer::FadeMode::Dithered;
    desc.visibility = 0.375f;

    const full_renderer::scene::FadeRenderState first =
        full_renderer::scene::makeFadeRenderState(desc);
    const full_renderer::scene::FadeRenderState second =
        full_renderer::scene::makeFadeRenderState(desc);
    for (int index = 0; index < 4; ++index)
    {
        expect(first.params[index] == second.params[index],
            "fade params are deterministic",
            failures);
    }
    expect(first.visibility == second.visibility && first.active == second.active,
        "fade summary state is deterministic",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    defaultFadeIsNeutral(failures);
    fadeVisibilityClampsForPlanning(failures);
    invalidFadeDescriptorsAreRejected(failures);
    alphaAndDitherModesPlanDistinctStates(failures);
    renderPlanAggregatesCounts(failures);
    fadePlanningIsDeterministic(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
