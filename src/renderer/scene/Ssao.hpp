#pragma once

#include "full_renderer/Renderer.hpp"

namespace full_renderer::scene
{
struct SsaoUniformPlan
{
    bool enabled = false;
    bool halfResolution = false;
    bool blurEnabled = false;
    bool debugVisualize = false;
    float radiusPixels = 0.0f;
    float blurRadiusPixels = 0.0f;
    float intensity = 0.0f;
    float biasNormalized = 0.0f;
    float power = 1.0f;
    float maxDistanceMeters = 1.0f;
    float inverseMaxDistanceMeters = 1.0f;
    std::uint32_t sampleCount = 0;
};

struct SsaoTargetDimensions
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

bool isValidSsaoDesc(const SsaoDesc& desc) noexcept;
float clampSsaoRadiusMeters(float radiusMeters) noexcept;
float clampSsaoBlurRadiusPixels(float blurRadiusPixels) noexcept;
float clampSsaoIntensity(float intensity) noexcept;
float clampSsaoBiasMeters(float biasMeters) noexcept;
float clampSsaoPower(float power) noexcept;
float clampSsaoMaxDistanceMeters(float maxDistanceMeters) noexcept;
std::uint32_t clampSsaoSampleCount(std::uint32_t sampleCount) noexcept;
SsaoTargetDimensions makeSsaoTargetDimensions(
    std::uint32_t viewportWidth,
    std::uint32_t viewportHeight,
    bool halfResolution) noexcept;
SsaoUniformPlan makeSsaoUniformPlan(const SsaoDesc& desc) noexcept;
SsaoDesc makeDefaultSsaoDesc() noexcept;
} // namespace full_renderer::scene
