#include "renderer/scene/PostPass.hpp"

namespace full_renderer::scene
{
namespace
{
void markSkipped(PostPassPlan& plan, const PostSkippedPassReason reason) noexcept
{
    const std::uint32_t reasonBit = static_cast<std::uint32_t>(reason);
    if ((plan.skippedPassReasonMask & reasonBit) == 0U)
    {
        plan.skippedPassReasonMask |= reasonBit;
        ++plan.skippedPassCount;
    }
}

void appendPass(PostPassPlan& plan, const PostPassKind pass, const bool fullscreen) noexcept
{
    if (plan.passCount >= PostPassPlan::kMaxPasses)
    {
        markSkipped(plan, kPostSkippedPassReasonPassLimit);
        return;
    }

    plan.passes[plan.passCount] = pass;
    ++plan.passCount;
    if (fullscreen)
    {
        ++plan.fullscreenPassCount;
    }
}

int passIndex(const PostPassPlan& plan, const PostPassKind pass) noexcept
{
    for (std::uint32_t index = 0; index < plan.passCount; ++index)
    {
        if (plan.passes[index] == pass)
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}

bool orderedIfPresent(const PostPassPlan& plan, const PostPassKind before, const PostPassKind after) noexcept
{
    const int beforeIndex = passIndex(plan, before);
    const int afterIndex = passIndex(plan, after);
    return beforeIndex < 0 || afterIndex < 0 || beforeIndex < afterIndex;
}
} // namespace

bool isPostSceneTargetReasonSet(const std::uint32_t reasonMask, const PostSceneTargetReason reason) noexcept
{
    return (reasonMask & static_cast<std::uint32_t>(reason)) != 0U;
}

bool isPostSkippedPassReasonSet(const std::uint32_t reasonMask, const PostSkippedPassReason reason) noexcept
{
    return (reasonMask & static_cast<std::uint32_t>(reason)) != 0U;
}

const char* postPassKindName(const PostPassKind pass) noexcept
{
    switch (pass)
    {
    case PostPassKind::SsaoDepthCapture:
        return "SSAO depth";
    case PostPassKind::SsaoEvaluate:
        return "SSAO evaluate";
    case PostPassKind::SsaoBlurHorizontal:
        return "SSAO blur H";
    case PostPassKind::SsaoBlurVertical:
        return "SSAO blur V";
    case PostPassKind::SsaoComposite:
        return "SSAO composite";
    case PostPassKind::DecalDepthCapture:
        return "Decal depth";
    case PostPassKind::ProjectedDecals:
        return "Projected decals";
    case PostPassKind::ParticleBillboards:
        return "Particles";
    case PostPassKind::ScenePresent:
        return "Scene present";
    case PostPassKind::ColorGradingPresent:
        return "Color grading present";
    case PostPassKind::SelectionMask:
        return "Selection mask";
    case PostPassKind::SelectionOutline:
        return "Selection outline";
    }

    return "Unknown";
}

const char* postPresentModeName(const PostPresentMode mode) noexcept
{
    switch (mode)
    {
    case PostPresentMode::DirectToSwapchain:
        return "direct swapchain";
    case PostPresentMode::ScenePresent:
        return "scene present";
    case PostPresentMode::ColorGradedScenePresent:
        return "color graded scene present";
    }

    return "unknown";
}

PostSceneTargetDimensions makePostSceneTargetDimensions(
    const std::uint32_t viewportWidth,
    const std::uint32_t viewportHeight) noexcept
{
    PostSceneTargetDimensions dimensions;
    dimensions.valid = viewportWidth > 0U && viewportHeight > 0U;
    dimensions.width = dimensions.valid ? viewportWidth : 0U;
    dimensions.height = dimensions.valid ? viewportHeight : 0U;
    return dimensions;
}

PostSceneTargetReconfigurePlan makePostSceneTargetReconfigurePlan(
    const std::uint32_t viewportWidth,
    const std::uint32_t viewportHeight,
    const bool targetRequired,
    const std::uint32_t currentWidth,
    const std::uint32_t currentHeight,
    const bool currentResourceValid) noexcept
{
    PostSceneTargetReconfigurePlan plan;
    plan.targetRequired = targetRequired;
    plan.dimensions = makePostSceneTargetDimensions(viewportWidth, viewportHeight);

    if (!targetRequired)
    {
        plan.reason = PostSceneTargetReconfigureReason::NotRequired;
        return plan;
    }

    if (!plan.dimensions.valid)
    {
        plan.reason = PostSceneTargetReconfigureReason::InvalidViewport;
        return plan;
    }

    if (!currentResourceValid)
    {
        plan.reason = PostSceneTargetReconfigureReason::MissingResource;
        plan.reconfigureRequired = true;
        return plan;
    }

    if (currentWidth != plan.dimensions.width || currentHeight != plan.dimensions.height)
    {
        plan.reason = PostSceneTargetReconfigureReason::DimensionChanged;
        plan.reconfigureRequired = true;
        return plan;
    }

    plan.reason = PostSceneTargetReconfigureReason::Unchanged;
    return plan;
}

bool isValidPostPassOrdering(const PostPassPlan& plan) noexcept
{
    if (plan.passCount > PostPassPlan::kMaxPasses)
    {
        return false;
    }

    return orderedIfPresent(plan, PostPassKind::SsaoDepthCapture, PostPassKind::SsaoEvaluate) &&
        orderedIfPresent(plan, PostPassKind::SsaoEvaluate, PostPassKind::SsaoBlurHorizontal) &&
        orderedIfPresent(plan, PostPassKind::SsaoBlurHorizontal, PostPassKind::SsaoBlurVertical) &&
        orderedIfPresent(plan, PostPassKind::SsaoBlurVertical, PostPassKind::SsaoComposite) &&
        orderedIfPresent(plan, PostPassKind::SsaoComposite, PostPassKind::ProjectedDecals) &&
        orderedIfPresent(plan, PostPassKind::ProjectedDecals, PostPassKind::ParticleBillboards) &&
        orderedIfPresent(plan, PostPassKind::ParticleBillboards, PostPassKind::ScenePresent) &&
        orderedIfPresent(plan, PostPassKind::ParticleBillboards, PostPassKind::ColorGradingPresent) &&
        orderedIfPresent(plan, PostPassKind::ScenePresent, PostPassKind::SelectionMask) &&
        orderedIfPresent(plan, PostPassKind::ColorGradingPresent, PostPassKind::SelectionMask) &&
        orderedIfPresent(plan, PostPassKind::SelectionMask, PostPassKind::SelectionOutline);
}

PostPassPlan makePostPassPlan(const PostPassPlanInput& input) noexcept
{
    PostPassPlan plan;
    plan.viewportWidth = input.viewportWidth;
    plan.viewportHeight = input.viewportHeight;
    const PostSceneTargetDimensions dimensions =
        makePostSceneTargetDimensions(input.viewportWidth, input.viewportHeight);
    plan.validViewport = dimensions.valid;
    plan.sceneTargetWidth = dimensions.width;
    plan.sceneTargetHeight = dimensions.height;
    if (!plan.validViewport)
    {
        markSkipped(plan, kPostSkippedPassReasonInvalidViewport);
    }

    if (input.forceSceneTarget)
    {
        plan.sceneTargetReasonMask |= kPostSceneTargetReasonForced;
    }
    if (input.ssaoEnabled)
    {
        plan.sceneTargetReasonMask |= kPostSceneTargetReasonSsao;
    }
    if (input.decalsEnabled && input.activeDecals)
    {
        plan.sceneTargetReasonMask |= kPostSceneTargetReasonDecals;
    }
    if (input.particlesEnabled && input.acceptedParticles && input.softParticlesEnabled)
    {
        plan.sceneTargetReasonMask |= kPostSceneTargetReasonSoftParticles;
    }
    if (input.colorGradingEnabled)
    {
        plan.sceneTargetReasonMask |= kPostSceneTargetReasonColorGrading;
    }

    plan.sceneTargetRequired = plan.sceneTargetReasonMask != kPostSceneTargetReasonNone;
    plan.sceneTargetAvailable = plan.sceneTargetRequired && input.sceneTargetAvailable;
    plan.readableSceneDepthRequired =
        (input.decalsEnabled && input.activeDecals) ||
        (input.particlesEnabled && input.acceptedParticles && input.softParticlesEnabled);
    plan.sceneDepthAvailable = plan.readableSceneDepthRequired && input.sceneDepthAvailable;

    if (input.colorGradingEnabled)
    {
        plan.presentMode = PostPresentMode::ColorGradedScenePresent;
    }
    else if (plan.sceneTargetRequired)
    {
        plan.presentMode = PostPresentMode::ScenePresent;
    }

    if (plan.sceneTargetRequired && !plan.sceneTargetAvailable)
    {
        ++plan.invalidResourceCount;
        markSkipped(plan, kPostSkippedPassReasonSceneTargetUnavailable);
    }
    if (plan.readableSceneDepthRequired && !plan.sceneDepthAvailable)
    {
        ++plan.invalidResourceCount;
        markSkipped(plan, kPostSkippedPassReasonReadableDepthUnavailable);
    }

    if (input.ssaoEnabled)
    {
        appendPass(plan, PostPassKind::SsaoDepthCapture, false);
        appendPass(plan, PostPassKind::SsaoEvaluate, true);
        if (input.ssaoBlurEnabled)
        {
            appendPass(plan, PostPassKind::SsaoBlurHorizontal, true);
            appendPass(plan, PostPassKind::SsaoBlurVertical, true);
        }
        appendPass(plan, PostPassKind::SsaoComposite, true);
    }

    if (input.decalsEnabled && input.activeDecals)
    {
        appendPass(plan, PostPassKind::DecalDepthCapture, false);
        appendPass(plan, PostPassKind::ProjectedDecals, true);
    }
    else if (input.decalsEnabled)
    {
        markSkipped(plan, kPostSkippedPassReasonNoActiveDecals);
    }

    if (input.particlesEnabled && input.acceptedParticles)
    {
        appendPass(plan, PostPassKind::ParticleBillboards, false);
    }
    else if (input.particlesEnabled)
    {
        markSkipped(plan, kPostSkippedPassReasonNoAcceptedParticles);
    }

    if (plan.sceneTargetRequired)
    {
        appendPass(
            plan,
            input.colorGradingEnabled ? PostPassKind::ColorGradingPresent : PostPassKind::ScenePresent,
            true);
    }

    if (input.selectionOutlineEnabled && input.hasSelectedObjects)
    {
        appendPass(plan, PostPassKind::SelectionMask, false);
        appendPass(plan, PostPassKind::SelectionOutline, true);
    }
    else if (input.selectionOutlineEnabled)
    {
        markSkipped(plan, kPostSkippedPassReasonNoSelectedObjects);
    }

    plan.finalPresentSubmitted =
        plan.validViewport &&
        (!plan.sceneTargetRequired || plan.sceneTargetAvailable);
    return plan;
}
} // namespace full_renderer::scene
