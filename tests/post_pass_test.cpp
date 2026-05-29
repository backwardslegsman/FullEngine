#include "renderer/scene/PostPass.hpp"

#include <cstdlib>
#include <iostream>

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

bool containsPass(
    const full_renderer::scene::PostPassPlan& plan,
    const full_renderer::scene::PostPassKind pass) noexcept
{
    for (std::uint32_t index = 0; index < plan.passCount; ++index)
    {
        if (plan.passes[index] == pass)
        {
            return true;
        }
    }

    return false;
}

void directSwapchainPlanHasNoPostWork(int& failures)
{
    full_renderer::scene::PostPassPlanInput input;
    input.viewportWidth = 1280;
    input.viewportHeight = 720;

    const full_renderer::scene::PostPassPlan plan = full_renderer::scene::makePostPassPlan(input);
    expect(plan.validViewport, "valid viewport is reported", failures);
    expect(!plan.sceneTargetRequired, "direct plan does not require scene target", failures);
    expect(plan.presentMode == full_renderer::scene::PostPresentMode::DirectToSwapchain,
        "direct plan reports swapchain present mode",
        failures);
    expect(plan.passCount == 0, "direct plan submits no post passes", failures);
    expect(plan.fullscreenPassCount == 0, "direct plan submits no fullscreen passes", failures);
    expect(plan.skippedPassCount == 0, "direct plan has no skipped requested work", failures);
    expect(plan.skippedPassReasonMask == full_renderer::scene::kPostSkippedPassReasonNone,
        "direct plan has no skipped reason mask",
        failures);
    expect(plan.finalPresentSubmitted, "direct plan reports final output availability", failures);
    expect(full_renderer::scene::isValidPostPassOrdering(plan), "direct ordering validates", failures);
}

void selectionOnlyDoesNotRequireSceneTarget(int& failures)
{
    full_renderer::scene::PostPassPlanInput input;
    input.viewportWidth = 1280;
    input.viewportHeight = 720;
    input.selectionOutlineEnabled = true;
    input.hasSelectedObjects = true;

    const full_renderer::scene::PostPassPlan plan = full_renderer::scene::makePostPassPlan(input);
    expect(!plan.sceneTargetRequired, "selection-only plan does not require scene target", failures);
    expect(!full_renderer::scene::isPostSceneTargetReasonSet(
               plan.sceneTargetReasonMask,
               full_renderer::scene::kPostSceneTargetReasonSelectionOutline),
        "selection mask is not a scene-target reason",
        failures);
    expect(plan.presentMode == full_renderer::scene::PostPresentMode::DirectToSwapchain,
        "selection-only plan keeps direct present mode",
        failures);
    expect(plan.finalPresentSubmitted, "selection-only plan still has a valid final present", failures);
    expect(plan.passCount == 2, "selection-only plan submits mask and outline", failures);
    expect(plan.passes[0] == full_renderer::scene::PostPassKind::SelectionMask,
        "selection mask is first selection pass",
        failures);
    expect(plan.passes[1] == full_renderer::scene::PostPassKind::SelectionOutline,
        "selection outline follows mask",
        failures);
}

void ssaoBlurPlanUsesSceneTargetAndStableOrdering(int& failures)
{
    full_renderer::scene::PostPassPlanInput input;
    input.viewportWidth = 1600;
    input.viewportHeight = 900;
    input.ssaoEnabled = true;
    input.ssaoBlurEnabled = true;
    input.sceneTargetAvailable = true;

    const full_renderer::scene::PostPassPlan plan = full_renderer::scene::makePostPassPlan(input);
    expect(plan.sceneTargetRequired, "SSAO requests intermediate scene target", failures);
    expect(!plan.readableSceneDepthRequired, "SSAO uses its own depth path in post planning", failures);
    expect(full_renderer::scene::isPostSceneTargetReasonSet(
               plan.sceneTargetReasonMask,
               full_renderer::scene::kPostSceneTargetReasonSsao),
        "SSAO scene-target reason is reported",
        failures);
    expect(plan.passCount == 6, "SSAO with blur plans depth, AO, two blur, composite, and present", failures);
    expect(plan.fullscreenPassCount == 5, "SSAO with blur has five fullscreen passes including present", failures);
    expect(plan.passes[0] == full_renderer::scene::PostPassKind::SsaoDepthCapture,
        "SSAO depth is first",
        failures);
    expect(plan.passes[4] == full_renderer::scene::PostPassKind::SsaoComposite,
        "SSAO composite follows blur",
        failures);
    expect(plan.passes[5] == full_renderer::scene::PostPassKind::ScenePresent,
        "scene present follows SSAO",
        failures);
    expect(full_renderer::scene::isValidPostPassOrdering(plan), "SSAO ordering validates", failures);
}

void decalsParticlesColorAndSelectionKeepExpectedOrder(int& failures)
{
    full_renderer::scene::PostPassPlanInput input;
    input.viewportWidth = 1920;
    input.viewportHeight = 1080;
    input.decalsEnabled = true;
    input.activeDecals = true;
    input.particlesEnabled = true;
    input.acceptedParticles = true;
    input.softParticlesEnabled = true;
    input.colorGradingEnabled = true;
    input.selectionOutlineEnabled = true;
    input.hasSelectedObjects = true;
    input.sceneTargetAvailable = true;
    input.sceneDepthAvailable = true;

    const full_renderer::scene::PostPassPlan plan = full_renderer::scene::makePostPassPlan(input);
    expect(plan.sceneTargetRequired, "combined plan requests scene target", failures);
    expect(plan.readableSceneDepthRequired, "decals and soft particles request readable depth", failures);
    expect(plan.sceneDepthAvailable, "scene depth availability is reported", failures);
    expect(plan.presentMode == full_renderer::scene::PostPresentMode::ColorGradedScenePresent,
        "color grading selects graded present mode",
        failures);
    expect(plan.passCount == 6,
        "combined plan includes decal depth, decal composite, particles, color grading, mask, outline",
        failures);
    expect(plan.passes[0] == full_renderer::scene::PostPassKind::DecalDepthCapture,
        "decal depth is first when SSAO is disabled",
        failures);
    expect(plan.passes[2] == full_renderer::scene::PostPassKind::ParticleBillboards,
        "particles follow decals",
        failures);
    expect(plan.passes[3] == full_renderer::scene::PostPassKind::ColorGradingPresent,
        "color grading follows particles",
        failures);
    expect(plan.passes[4] == full_renderer::scene::PostPassKind::SelectionMask,
        "selection mask follows grading",
        failures);
    expect(plan.passes[5] == full_renderer::scene::PostPassKind::SelectionOutline,
        "outline follows selection mask",
        failures);
    expect(full_renderer::scene::isPostSceneTargetReasonSet(
               plan.sceneTargetReasonMask,
               full_renderer::scene::kPostSceneTargetReasonDecals),
        "decal scene-target reason is reported",
        failures);
    expect(full_renderer::scene::isPostSceneTargetReasonSet(
               plan.sceneTargetReasonMask,
               full_renderer::scene::kPostSceneTargetReasonSoftParticles),
        "soft-particle scene-target reason is reported",
        failures);
    expect(!full_renderer::scene::isPostSceneTargetReasonSet(
               plan.sceneTargetReasonMask,
               full_renderer::scene::kPostSceneTargetReasonSelectionOutline),
        "selection outline does not request scene target",
        failures);
    expect(full_renderer::scene::isPostSceneTargetReasonSet(
               plan.sceneTargetReasonMask,
               full_renderer::scene::kPostSceneTargetReasonColorGrading),
        "color grading scene-target reason is reported",
        failures);
    expect(full_renderer::scene::isValidPostPassOrdering(plan), "combined ordering validates", failures);
}

void oneFeatureAtATimeDisableMatrix(int& failures)
{
    {
        full_renderer::scene::PostPassPlanInput input;
        input.viewportWidth = 800;
        input.viewportHeight = 600;
        input.decalsEnabled = true;

        const full_renderer::scene::PostPassPlan plan = full_renderer::scene::makePostPassPlan(input);
        expect(!plan.sceneTargetRequired, "decal enabled with no active decals does not require scene target", failures);
        expect(!containsPass(plan, full_renderer::scene::PostPassKind::ProjectedDecals),
            "decal enabled with no active decals submits no decal pass",
            failures);
        expect(full_renderer::scene::isPostSkippedPassReasonSet(
                   plan.skippedPassReasonMask,
                   full_renderer::scene::kPostSkippedPassReasonNoActiveDecals),
            "no-active-decal skip reason is reported",
            failures);
        expect(plan.finalPresentSubmitted, "empty decal plan preserves final present", failures);
    }

    {
        full_renderer::scene::PostPassPlanInput input;
        input.viewportWidth = 800;
        input.viewportHeight = 600;
        input.particlesEnabled = true;
        input.softParticlesEnabled = true;

        const full_renderer::scene::PostPassPlan plan = full_renderer::scene::makePostPassPlan(input);
        expect(!plan.sceneTargetRequired, "empty soft-particle plan does not require scene target", failures);
        expect(!plan.readableSceneDepthRequired, "empty soft-particle plan does not require depth", failures);
        expect(full_renderer::scene::isPostSkippedPassReasonSet(
                   plan.skippedPassReasonMask,
                   full_renderer::scene::kPostSkippedPassReasonNoAcceptedParticles),
            "no-accepted-particle skip reason is reported",
            failures);
        expect(plan.finalPresentSubmitted, "empty particle plan preserves final present", failures);
    }

    {
        full_renderer::scene::PostPassPlanInput input;
        input.viewportWidth = 800;
        input.viewportHeight = 600;
        input.particlesEnabled = true;
        input.acceptedParticles = true;

        const full_renderer::scene::PostPassPlan plan = full_renderer::scene::makePostPassPlan(input);
        expect(!plan.sceneTargetRequired, "hard particles do not require scene target", failures);
        expect(!plan.readableSceneDepthRequired, "hard particles do not require readable depth", failures);
        expect(plan.presentMode == full_renderer::scene::PostPresentMode::DirectToSwapchain,
            "hard particles keep direct present",
            failures);
        expect(plan.passCount == 1, "hard particles submit only particle pass", failures);
        expect(plan.passes[0] == full_renderer::scene::PostPassKind::ParticleBillboards,
            "hard-particle pass is planned",
            failures);
        expect(plan.finalPresentSubmitted, "hard particles do not depend on SSAO or scene target", failures);
    }

    {
        full_renderer::scene::PostPassPlanInput input;
        input.viewportWidth = 800;
        input.viewportHeight = 600;
        input.particlesEnabled = true;
        input.acceptedParticles = true;
        input.softParticlesEnabled = true;
        input.sceneTargetAvailable = true;
        input.sceneDepthAvailable = true;

        const full_renderer::scene::PostPassPlan plan = full_renderer::scene::makePostPassPlan(input);
        expect(plan.sceneTargetRequired, "soft particles require scene target when particles are accepted", failures);
        expect(plan.readableSceneDepthRequired, "soft particles require readable depth when particles are accepted", failures);
        expect(plan.passCount == 2, "soft particles plan particle pass and scene present", failures);
        expect(plan.passes[0] == full_renderer::scene::PostPassKind::ParticleBillboards,
            "soft particle pass precedes present",
            failures);
        expect(plan.passes[1] == full_renderer::scene::PostPassKind::ScenePresent,
            "soft particles present scene target",
            failures);
    }

    {
        full_renderer::scene::PostPassPlanInput input;
        input.viewportWidth = 800;
        input.viewportHeight = 600;
        input.colorGradingEnabled = true;
        input.sceneTargetAvailable = true;

        const full_renderer::scene::PostPassPlan plan = full_renderer::scene::makePostPassPlan(input);
        expect(plan.sceneTargetRequired, "color grading requires scene target", failures);
        expect(plan.presentMode == full_renderer::scene::PostPresentMode::ColorGradedScenePresent,
            "color grading selects color-graded present",
            failures);
        expect(plan.passCount == 1, "color grading alone submits one present pass", failures);
        expect(plan.passes[0] == full_renderer::scene::PostPassKind::ColorGradingPresent,
            "color grading present is planned",
            failures);
    }
}

void allEnabledMinusOneKeepsValidPresentation(int& failures)
{
    const auto makeAllEnabled = []() {
        full_renderer::scene::PostPassPlanInput input;
        input.viewportWidth = 1024;
        input.viewportHeight = 768;
        input.ssaoEnabled = true;
        input.ssaoBlurEnabled = true;
        input.decalsEnabled = true;
        input.activeDecals = true;
        input.particlesEnabled = true;
        input.acceptedParticles = true;
        input.softParticlesEnabled = true;
        input.colorGradingEnabled = true;
        input.selectionOutlineEnabled = true;
        input.hasSelectedObjects = true;
        input.sceneTargetAvailable = true;
        input.sceneDepthAvailable = true;
        return input;
    };

    for (int disabledFeature = 0; disabledFeature < 5; ++disabledFeature)
    {
        full_renderer::scene::PostPassPlanInput input = makeAllEnabled();
        switch (disabledFeature)
        {
        case 0:
            input.ssaoEnabled = false;
            input.ssaoBlurEnabled = false;
            break;
        case 1:
            input.decalsEnabled = false;
            input.activeDecals = false;
            break;
        case 2:
            input.particlesEnabled = false;
            input.acceptedParticles = false;
            input.softParticlesEnabled = false;
            break;
        case 3:
            input.colorGradingEnabled = false;
            break;
        case 4:
            input.selectionOutlineEnabled = false;
            input.hasSelectedObjects = false;
            break;
        default:
            break;
        }

        const full_renderer::scene::PostPassPlan plan = full_renderer::scene::makePostPassPlan(input);
        expect(plan.finalPresentSubmitted, "all-enabled-minus-one plan has final present", failures);
        expect(full_renderer::scene::isValidPostPassOrdering(plan), "all-enabled-minus-one ordering validates", failures);
    }
}

void missingResourcesAreReported(int& failures)
{
    full_renderer::scene::PostPassPlanInput input;
    input.viewportWidth = 640;
    input.viewportHeight = 480;
    input.decalsEnabled = true;
    input.activeDecals = true;
    input.sceneTargetAvailable = false;
    input.sceneDepthAvailable = false;

    const full_renderer::scene::PostPassPlan plan = full_renderer::scene::makePostPassPlan(input);
    expect(plan.sceneTargetRequired, "decal plan requires scene target", failures);
    expect(plan.readableSceneDepthRequired, "decal plan requires readable depth", failures);
    expect(plan.invalidResourceCount == 2,
        "missing scene target and readable depth are counted",
        failures);
    expect(full_renderer::scene::isPostSkippedPassReasonSet(
               plan.skippedPassReasonMask,
               full_renderer::scene::kPostSkippedPassReasonSceneTargetUnavailable),
        "missing scene target skip reason is reported",
        failures);
    expect(full_renderer::scene::isPostSkippedPassReasonSet(
               plan.skippedPassReasonMask,
               full_renderer::scene::kPostSkippedPassReasonReadableDepthUnavailable),
        "missing readable depth skip reason is reported",
        failures);
    expect(!plan.finalPresentSubmitted, "missing scene target prevents final present submission", failures);
}

void viewportResourcePlanningIsStable(int& failures)
{
    full_renderer::scene::PostSceneTargetDimensions dimensions =
        full_renderer::scene::makePostSceneTargetDimensions(641, 479);
    expect(dimensions.valid && dimensions.width == 641 && dimensions.height == 479,
        "odd viewport dimensions are preserved",
        failures);

    dimensions = full_renderer::scene::makePostSceneTargetDimensions(1, 1);
    expect(dimensions.valid && dimensions.width == 1 && dimensions.height == 1,
        "tiny viewport dimensions remain valid",
        failures);

    dimensions = full_renderer::scene::makePostSceneTargetDimensions(0, 720);
    expect(!dimensions.valid && dimensions.width == 0 && dimensions.height == 0,
        "zero viewport dimensions are invalid",
        failures);

    full_renderer::scene::PostSceneTargetReconfigurePlan reconfigure =
        full_renderer::scene::makePostSceneTargetReconfigurePlan(1280, 720, false, 0, 0, false);
    expect(!reconfigure.reconfigureRequired &&
            reconfigure.reason == full_renderer::scene::PostSceneTargetReconfigureReason::NotRequired,
        "disabled scene target does not request reconfiguration",
        failures);

    reconfigure = full_renderer::scene::makePostSceneTargetReconfigurePlan(1280, 720, true, 0, 0, false);
    expect(reconfigure.reconfigureRequired &&
            reconfigure.reason == full_renderer::scene::PostSceneTargetReconfigureReason::MissingResource,
        "missing scene target resource requests reconfiguration",
        failures);

    reconfigure = full_renderer::scene::makePostSceneTargetReconfigurePlan(1920, 1080, true, 1280, 720, true);
    expect(reconfigure.reconfigureRequired &&
            reconfigure.reason == full_renderer::scene::PostSceneTargetReconfigureReason::DimensionChanged,
        "viewport size change requests reconfiguration",
        failures);

    reconfigure = full_renderer::scene::makePostSceneTargetReconfigurePlan(1920, 1080, true, 1920, 1080, true);
    expect(!reconfigure.reconfigureRequired &&
            reconfigure.reason == full_renderer::scene::PostSceneTargetReconfigureReason::Unchanged,
        "same-size valid target does not request reconfiguration",
        failures);

    reconfigure = full_renderer::scene::makePostSceneTargetReconfigurePlan(0, 1080, true, 1920, 1080, true);
    expect(!reconfigure.reconfigureRequired &&
            reconfigure.reason == full_renderer::scene::PostSceneTargetReconfigureReason::InvalidViewport,
        "invalid viewport blocks reconfiguration",
        failures);
}

void planningIsDeterministic(int& failures)
{
    full_renderer::scene::PostPassPlanInput input;
    input.viewportWidth = 800;
    input.viewportHeight = 600;
    input.forceSceneTarget = true;
    input.ssaoEnabled = true;
    input.decalsEnabled = true;
    input.activeDecals = true;
    input.selectionOutlineEnabled = true;
    input.hasSelectedObjects = true;
    input.sceneTargetAvailable = true;
    input.sceneDepthAvailable = true;

    const full_renderer::scene::PostPassPlan first = full_renderer::scene::makePostPassPlan(input);
    const full_renderer::scene::PostPassPlan second = full_renderer::scene::makePostPassPlan(input);
    expect(first.passCount == second.passCount &&
            first.fullscreenPassCount == second.fullscreenPassCount &&
            first.sceneTargetReasonMask == second.sceneTargetReasonMask &&
            first.skippedPassReasonMask == second.skippedPassReasonMask &&
            first.presentMode == second.presentMode,
        "post-pass planning summary is deterministic",
        failures);
    for (std::uint32_t index = 0; index < first.passCount; ++index)
    {
        expect(first.passes[index] == second.passes[index],
            "post-pass order is deterministic",
            failures);
    }
}
} // namespace

int main()
{
    int failures = 0;
    directSwapchainPlanHasNoPostWork(failures);
    selectionOnlyDoesNotRequireSceneTarget(failures);
    ssaoBlurPlanUsesSceneTargetAndStableOrdering(failures);
    decalsParticlesColorAndSelectionKeepExpectedOrder(failures);
    oneFeatureAtATimeDisableMatrix(failures);
    allEnabledMinusOneKeepsValidPresentation(failures);
    missingResourcesAreReported(failures);
    viewportResourcePlanningIsStable(failures);
    planningIsDeterministic(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
