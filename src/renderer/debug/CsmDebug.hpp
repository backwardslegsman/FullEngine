#pragma once

#include "full_renderer/Renderer.hpp"

namespace full_renderer::debug
{
struct CsmValidationSceneConfig
{
    std::uint32_t terrainGridRadius = 10;
    float chunkSizeMeters = 4.0f;
    float cameraStartX = 0.0f;
    float cameraStartY = 3.0f;
    float cameraStartZ = -18.0f;
    float cameraTargetX = 0.0f;
    float cameraTargetY = -0.25f;
    float cameraTargetZ = 12.0f;
    std::uint32_t staticCasterCount = 4;
    std::uint32_t instancedCasterCount = 8;
};

struct ShadowMapPreviewSettings
{
    bool enabled = false;
    std::uint32_t cascadeIndex = 0;
    std::uint32_t previewSize = 256;
    float blackDepth = 0.0f;
    float whiteDepth = 1.0f;
    bool invert = false;
};

struct ShadowMapPreviewRequest
{
    bool enabled = false;
    bool available = false;
    std::uint32_t cascadeIndex = 0;
    std::uint32_t previewSize = 256;
    float blackDepth = 0.0f;
    float whiteDepth = 1.0f;
    bool invert = false;
};

DirectionalShadowDesc makeDefaultCsmValidationShadowDesc() noexcept;
CsmValidationSceneConfig makeDefaultCsmValidationSceneConfig() noexcept;
ShadowMapPreviewSettings makeDefaultShadowMapPreviewSettings() noexcept;
std::uint32_t clampShadowMapPreviewSize(std::uint32_t previewSize) noexcept;
void normalizeShadowMapPreviewRemap(float& blackDepth, float& whiteDepth) noexcept;
ShadowMapPreviewRequest makeShadowMapPreviewRequest(
    const ShadowMapPreviewSettings& settings,
    std::uint32_t activeCascadeCount,
    bool selectedCascadeValid) noexcept;
void applyCsmValidationDebugOverlayPreset(
    TerrainDebugOptions& terrainDebug,
    DirectionalShadowDesc& shadow) noexcept;
} // namespace full_renderer::debug
