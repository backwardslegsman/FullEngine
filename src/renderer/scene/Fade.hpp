#pragma once

#include "full_renderer/Renderer.hpp"

#include <cstdint>

namespace full_renderer::scene
{
struct FadeRenderState
{
    float params[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    FadeMode mode = FadeMode::None;
    float visibility = 1.0f;
    bool enabled = false;
    bool active = false;
    bool fullyVisible = true;
    bool fullyHidden = false;
    bool alphaMode = false;
    bool ditherMode = false;
};

struct FadeRenderPlan
{
    std::uint32_t submittedCount = 0;
    std::uint32_t activeCount = 0;
    std::uint32_t fullyVisibleCount = 0;
    std::uint32_t partiallyFadedCount = 0;
    std::uint32_t fullyHiddenCount = 0;
    std::uint32_t invalidCount = 0;
    std::uint32_t alphaDrawCount = 0;
    std::uint32_t ditherDrawCount = 0;
    std::uint32_t unsupportedTargetCount = 0;
};

bool isValidFadeMode(FadeMode mode) noexcept;
bool isValidFadeDesc(const FadeDesc& desc) noexcept;
float clampFadeVisibility(float visibility) noexcept;
FadeDesc makeDefaultFadeDesc() noexcept;
FadeRenderState makeFadeRenderState(const FadeDesc& desc) noexcept;
void accumulateFadeRenderState(const FadeRenderState& state, FadeRenderPlan& plan) noexcept;
} // namespace full_renderer::scene
