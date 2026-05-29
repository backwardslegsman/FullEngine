#include "renderer/scene/Fade.hpp"

#include <algorithm>
#include <cmath>

namespace full_renderer::scene
{
namespace
{
float fadeModeToShaderValue(const FadeMode mode) noexcept
{
    switch (mode)
    {
    case FadeMode::Alpha:
        return 1.0f;
    case FadeMode::Dithered:
        return 2.0f;
    case FadeMode::None:
        return 0.0f;
    }
    return 0.0f;
}
} // namespace

bool isValidFadeMode(const FadeMode mode) noexcept
{
    switch (mode)
    {
    case FadeMode::None:
    case FadeMode::Alpha:
    case FadeMode::Dithered:
        return true;
    }
    return false;
}

bool isValidFadeDesc(const FadeDesc& desc) noexcept
{
    if (!desc.enabled)
    {
        return true;
    }

    return std::isfinite(desc.visibility) && isValidFadeMode(desc.mode);
}

float clampFadeVisibility(const float visibility) noexcept
{
    if (!std::isfinite(visibility))
    {
        return 1.0f;
    }
    return std::clamp(visibility, 0.0f, 1.0f);
}

FadeDesc makeDefaultFadeDesc() noexcept
{
    return {};
}

FadeRenderState makeFadeRenderState(const FadeDesc& desc) noexcept
{
    FadeRenderState state;
    if (!desc.enabled || !isValidFadeDesc(desc))
    {
        return state;
    }

    state.enabled = true;
    state.mode = desc.mode;
    state.visibility = clampFadeVisibility(desc.visibility);
    state.fullyVisible = state.visibility >= 1.0f || desc.mode == FadeMode::None;
    state.fullyHidden = state.visibility <= 0.0f && desc.mode != FadeMode::None;
    state.alphaMode = desc.mode == FadeMode::Alpha;
    state.ditherMode = desc.mode == FadeMode::Dithered;
    state.active = desc.mode != FadeMode::None && state.visibility < 1.0f;
    state.params[0] = state.visibility;
    state.params[1] = fadeModeToShaderValue(desc.mode);
    state.params[2] = state.ditherMode ? 1.0f : 0.0f;
    state.params[3] = state.active ? 1.0f : 0.0f;
    return state;
}

void accumulateFadeRenderState(const FadeRenderState& state, FadeRenderPlan& plan) noexcept
{
    if (!state.enabled)
    {
        return;
    }

    ++plan.submittedCount;
    if (state.fullyVisible)
    {
        ++plan.fullyVisibleCount;
    }
    else if (state.fullyHidden)
    {
        ++plan.fullyHiddenCount;
    }
    else
    {
        ++plan.partiallyFadedCount;
    }

    if (state.active)
    {
        ++plan.activeCount;
        if (state.alphaMode)
        {
            ++plan.alphaDrawCount;
        }
        else if (state.ditherMode)
        {
            ++plan.ditherDrawCount;
        }
    }
}
} // namespace full_renderer::scene
