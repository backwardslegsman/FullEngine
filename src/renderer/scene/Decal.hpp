#pragma once

#include "full_renderer/Renderer.hpp"

#include <cstdint>

namespace full_renderer::scene
{
struct Frustum;

struct DecalRenderItem
{
    DecalDesc desc;
    Aabb bounds;
    float worldToDecal[16] = {};
    std::uint32_t sourceIndex = 0;
    bool usesFallbackColor = false;
};

struct DecalRenderPlan
{
    DecalRenderItem items[kMaxFrameDecals];
    std::uint32_t submittedCount = 0;
    std::uint32_t activeCount = 0;
    std::uint32_t culledCount = 0;
    std::uint32_t rejectedCount = 0;
    std::uint32_t invalidDescriptorRejectCount = 0;
    std::uint32_t maxCountRejectedCount = 0;
    std::uint32_t fallbackColorCount = 0;
    std::uint32_t debugVolumeCount = 0;
    bool enabled = false;
    bool debugDrawVolumes = false;
    bool frustumCullingEnabled = false;
    bool activeProjectionSupported = false;
    bool projectionDeferred = false;
    float maxProjectionDepthMeters = 0.0f;
    float projectionEdgeFadeMeters = 0.0f;
};

struct DecalSceneTargetPlan
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool requiresSceneTarget = false;
};

bool isValidDecalDesc(const DecalDesc& desc) noexcept;
bool isValidDecalSubmitDesc(const DecalSubmitDesc& desc) noexcept;
float clampDecalOpacity(float opacity) noexcept;
float clampDecalProjectionDepthMeters(float depthMeters) noexcept;
float clampDecalProjectionEdgeFadeMeters(float fadeMeters) noexcept;
std::uint32_t clampDecalCount(std::uint32_t decalCount) noexcept;
Aabb buildDecalBounds(const DecalDesc& desc) noexcept;
bool buildWorldToDecalMatrix(const DecalDesc& desc, float outWorldToDecal[16]) noexcept;
DecalRenderPlan buildDecalRenderPlan(const DecalSubmitDesc* desc) noexcept;
DecalRenderPlan buildDecalRenderPlan(const DecalSubmitDesc* desc, const Frustum* cameraFrustum) noexcept;
DecalSceneTargetPlan makeDecalSceneTargetPlan(
    std::uint32_t viewportWidth,
    std::uint32_t viewportHeight,
    const DecalRenderPlan& plan) noexcept;
} // namespace full_renderer::scene
