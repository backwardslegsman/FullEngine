#pragma once

#include "full_renderer/Renderer.hpp"
#include "renderer/scene/Math.hpp"

#include <cstdint>

namespace full_renderer::scene
{
struct DirectionalShadowMatrices
{
    float view[16] = {};
    float projection[16] = {};
    float viewProjection[16] = {};
};

struct DirectionalShadowSplit
{
    DirectionalShadowMatrices matrices;
    float inverseViewProjection[16] = {};
    float worldCorners[8][3] = {};
    float cameraSliceCorners[8][3] = {};
    float orthoMin[3] = {};
    float orthoMax[3] = {};
    float texelSize[2] = {};
    float unsnappedCenter[2] = {};
    float snappedCenter[2] = {};
    float snapOffset[2] = {};
    float normalizedSplitDepth = 0.0f;
    std::uint32_t splitIndex = 0;
    float extentMeters = 0.0f;
    float nearDistanceMeters = 0.0f;
    float farDistanceMeters = 0.0f;
};

struct DirectionalShadowCascadeRange
{
    std::uint32_t splitIndex = 0;
    float nearDistanceMeters = 0.0f;
    float farDistanceMeters = 0.0f;
    float normalizedSplitDepth = 0.0f;
};

struct DirectionalShadowCascadeSet
{
    std::uint32_t cascadeCount = 0;
    DirectionalShadowSplit splits[kMaxDirectionalShadowCascades];
};

enum class ShadowResourceReconfigureAction
{
    None,
    Release,
    Recreate,
};

struct ShadowResourceConfig
{
    bool enabled = false;
    std::uint32_t mapResolution = 0;
    std::uint32_t cascadeCount = 0;
};

struct ShadowResourceReconfigurePlan
{
    ShadowResourceReconfigureAction action = ShadowResourceReconfigureAction::None;
    ShadowResourceConfig requested;
    bool valid = true;
};

bool isValidDirectionalShadowDesc(const DirectionalShadowDesc& desc) noexcept;
std::uint32_t clampShadowMapResolution(std::uint32_t resolution) noexcept;
std::uint32_t clampShadowCascadeCount(std::uint32_t cascadeCount) noexcept;
float clampCascadeBlendFraction(float blendFraction) noexcept;
float clampShadowDepthBias(float depthBias) noexcept;
float clampShadowSlopeBias(float slopeBias) noexcept;
std::uint32_t shadowFilterModeToTapCount(ShadowFilterMode filterMode) noexcept;
ShadowResourceConfig shadowResourceConfigFromDesc(const DirectionalShadowDesc& desc) noexcept;
ShadowResourceReconfigurePlan planShadowResourceReconfiguration(
    const DirectionalShadowDesc& requested,
    std::uint32_t activeResolution,
    std::uint32_t activeCascadeCount,
    bool activeResourcesValid) noexcept;
bool computeCascadeBlendBand(
    float previousSplitFarMeters,
    float currentSplitFarMeters,
    float blendFraction,
    float& outBandStartMeters,
    float& outBandSizeMeters) noexcept;
bool invertColumnMajor4x4(const float matrix[16], float outInverse[16]) noexcept;
bool extractWorldFrustumCorners(const float viewProjection[16], float outCorners[8][3]) noexcept;
bool computeDirectionalShadowCascadeRanges(
    const DirectionalShadowDesc& shadow,
    DirectionalShadowCascadeRange outRanges[kMaxDirectionalShadowCascades],
    std::uint32_t& outCascadeCount) noexcept;
bool computeCameraFrustumSliceCorners(
    const RenderViewDesc& cameraView,
    const DirectionalShadowCascadeRange& range,
    const DirectionalShadowDesc& shadow,
    float outCorners[8][3]) noexcept;
bool computeDirectionalShadowTexelSize(
    float minValue,
    float maxValue,
    std::uint32_t shadowMapResolution,
    float& outTexelSize) noexcept;
bool snapDirectionalShadowProjectionBounds(
    const float inMin[3],
    const float inMax[3],
    std::uint32_t shadowMapResolution,
    bool stable,
    float outMin[3],
    float outMax[3],
    float outTexelSize[2],
    float outUnsnappedCenter[2],
    float outSnappedCenter[2],
    float outSnapOffset[2]) noexcept;
bool buildDirectionalShadowMatrices(
    const DirectionalLightDesc& light,
    const DirectionalShadowDesc& shadow,
    DirectionalShadowMatrices& outMatrices) noexcept;
bool buildDirectionalShadowSplit(
    const DirectionalLightDesc& light,
    const DirectionalShadowDesc& shadow,
    DirectionalShadowSplit& outSplit) noexcept;
bool buildDirectionalShadowCascadeSet(
    const DirectionalLightDesc& light,
    const DirectionalShadowDesc& shadow,
    const RenderViewDesc& cameraView,
    DirectionalShadowCascadeSet& outCascadeSet) noexcept;
std::uint32_t selectDirectionalShadowCascade(
    float viewDepthMeters,
    const DirectionalShadowCascadeRange ranges[kMaxDirectionalShadowCascades],
    std::uint32_t cascadeCount) noexcept;
} // namespace full_renderer::scene
