#pragma once

#include "full_renderer/Renderer.hpp"

namespace full_renderer::scene
{
struct EnvironmentUniformPlan
{
    float colors[4][4] = {};
    float fogParams[4] = {};
    bool skyEnabled = false;
    bool fogEnabled = false;
};

bool isValidEnvironmentDesc(const EnvironmentDesc& desc) noexcept;
float clampFogDistance(float value, float fallback) noexcept;
EnvironmentUniformPlan makeEnvironmentUniformPlan(const EnvironmentDesc& desc) noexcept;
EnvironmentDesc makeDefaultOpenWorldEnvironmentDesc() noexcept;
} // namespace full_renderer::scene
