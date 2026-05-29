#pragma once

#include <cstdint>

namespace full_renderer::scene
{
enum class PostPassKind
{
    SsaoDepthCapture,
    SsaoEvaluate,
    SsaoBlurHorizontal,
    SsaoBlurVertical,
    SsaoComposite,
    DecalDepthCapture,
    ProjectedDecals,
    ParticleBillboards,
    ScenePresent,
    ColorGradingPresent,
    SelectionMask,
    SelectionOutline
};

enum class PostPresentMode
{
    DirectToSwapchain,
    ScenePresent,
    ColorGradedScenePresent
};

enum PostSceneTargetReason : std::uint32_t
{
    kPostSceneTargetReasonNone = 0U,
    kPostSceneTargetReasonSsao = 1U << 0U,
    kPostSceneTargetReasonDecals = 1U << 1U,
    kPostSceneTargetReasonSoftParticles = 1U << 2U,
    kPostSceneTargetReasonSelectionOutline = 1U << 3U,
    kPostSceneTargetReasonColorGrading = 1U << 4U,
    kPostSceneTargetReasonForced = 1U << 5U
};

enum PostSkippedPassReason : std::uint32_t
{
    kPostSkippedPassReasonNone = 0U,
    kPostSkippedPassReasonInvalidViewport = 1U << 0U,
    kPostSkippedPassReasonNoActiveDecals = 1U << 1U,
    kPostSkippedPassReasonNoAcceptedParticles = 1U << 2U,
    kPostSkippedPassReasonNoSelectedObjects = 1U << 3U,
    kPostSkippedPassReasonSceneTargetUnavailable = 1U << 4U,
    kPostSkippedPassReasonReadableDepthUnavailable = 1U << 5U,
    kPostSkippedPassReasonPassLimit = 1U << 6U
};

struct PostSceneTargetDimensions
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool valid = false;
};

enum class PostSceneTargetReconfigureReason
{
    NotRequired,
    InvalidViewport,
    MissingResource,
    DimensionChanged,
    Unchanged
};

struct PostSceneTargetReconfigurePlan
{
    PostSceneTargetDimensions dimensions;
    PostSceneTargetReconfigureReason reason = PostSceneTargetReconfigureReason::NotRequired;
    bool targetRequired = false;
    bool reconfigureRequired = false;
};

struct PostPassPlanInput
{
    std::uint32_t viewportWidth = 0;
    std::uint32_t viewportHeight = 0;
    bool forceSceneTarget = false;
    bool ssaoEnabled = false;
    bool ssaoBlurEnabled = false;
    bool decalsEnabled = false;
    bool activeDecals = false;
    bool particlesEnabled = false;
    bool acceptedParticles = false;
    bool softParticlesEnabled = false;
    bool selectionOutlineEnabled = false;
    bool hasSelectedObjects = false;
    bool colorGradingEnabled = false;
    bool sceneTargetAvailable = false;
    bool sceneDepthAvailable = false;
};

struct PostPassPlan
{
    static constexpr std::uint32_t kMaxPasses = 16;

    PostPassKind passes[kMaxPasses] = {};
    std::uint32_t passCount = 0;
    std::uint32_t fullscreenPassCount = 0;
    std::uint32_t skippedPassCount = 0;
    std::uint32_t invalidResourceCount = 0;
    std::uint32_t viewportWidth = 0;
    std::uint32_t viewportHeight = 0;
    std::uint32_t sceneTargetWidth = 0;
    std::uint32_t sceneTargetHeight = 0;
    std::uint32_t sceneTargetReasonMask = kPostSceneTargetReasonNone;
    std::uint32_t skippedPassReasonMask = kPostSkippedPassReasonNone;
    bool validViewport = false;
    bool sceneTargetRequired = false;
    bool sceneTargetAvailable = false;
    bool readableSceneDepthRequired = false;
    bool sceneDepthAvailable = false;
    bool finalPresentSubmitted = false;
    PostPresentMode presentMode = PostPresentMode::DirectToSwapchain;
};

bool isPostSceneTargetReasonSet(std::uint32_t reasonMask, PostSceneTargetReason reason) noexcept;
bool isPostSkippedPassReasonSet(std::uint32_t reasonMask, PostSkippedPassReason reason) noexcept;
const char* postPassKindName(PostPassKind pass) noexcept;
const char* postPresentModeName(PostPresentMode mode) noexcept;
PostSceneTargetDimensions makePostSceneTargetDimensions(
    std::uint32_t viewportWidth,
    std::uint32_t viewportHeight) noexcept;
PostSceneTargetReconfigurePlan makePostSceneTargetReconfigurePlan(
    std::uint32_t viewportWidth,
    std::uint32_t viewportHeight,
    bool targetRequired,
    std::uint32_t currentWidth,
    std::uint32_t currentHeight,
    bool currentResourceValid) noexcept;
bool isValidPostPassOrdering(const PostPassPlan& plan) noexcept;
PostPassPlan makePostPassPlan(const PostPassPlanInput& input) noexcept;
} // namespace full_renderer::scene
