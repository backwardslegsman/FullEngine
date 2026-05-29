#include "renderer/debug/CsmDebug.hpp"

#include <algorithm>
#include <cmath>

namespace full_renderer::debug
{
DirectionalShadowDesc makeDefaultCsmValidationShadowDesc() noexcept
{
    DirectionalShadowDesc shadow;
    shadow.enabled = true;
    shadow.mapResolution = 1024;
    shadow.extentMeters = 42.0f;
    shadow.depthBias = 0.0045f;
    shadow.slopeBias = 0.0060f;
    shadow.strength = 0.48f;
    shadow.cascadeCount = 4;
    shadow.cascadeSplitMode = ShadowCascadeSplitMode::Practical;
    shadow.cascadeSplitLambda = 0.58f;
    shadow.stableCascadeProjection = true;
    shadow.cascadeBlendEnabled = true;
    shadow.cascadeBlendFraction = 0.12f;
    shadow.filterMode = ShadowFilterMode::Pcf2x2;
    shadow.cascadeShadowDistanceMeters = 132.0f;
    shadow.cascadeCameraNearMeters = 0.1f;
    shadow.cascadeCameraFarMeters = 140.0f;
    return shadow;
}

CsmValidationSceneConfig makeDefaultCsmValidationSceneConfig() noexcept
{
    return {};
}

ShadowMapPreviewSettings makeDefaultShadowMapPreviewSettings() noexcept
{
    return {};
}

std::uint32_t clampShadowMapPreviewSize(const std::uint32_t previewSize) noexcept
{
    return std::max(128U, std::min(previewSize, 512U));
}

void normalizeShadowMapPreviewRemap(float& blackDepth, float& whiteDepth) noexcept
{
    if (!std::isfinite(blackDepth))
    {
        blackDepth = 0.0f;
    }
    if (!std::isfinite(whiteDepth))
    {
        whiteDepth = 1.0f;
    }

    blackDepth = std::max(0.0f, std::min(blackDepth, 1.0f));
    whiteDepth = std::max(0.0f, std::min(whiteDepth, 1.0f));
    if (whiteDepth <= blackDepth + 0.0001f)
    {
        whiteDepth = std::min(1.0f, blackDepth + 0.0001f);
        if (whiteDepth <= blackDepth)
        {
            blackDepth = std::max(0.0f, whiteDepth - 0.0001f);
        }
    }
}

ShadowMapPreviewRequest makeShadowMapPreviewRequest(
    const ShadowMapPreviewSettings& settings,
    const std::uint32_t activeCascadeCount,
    const bool selectedCascadeValid) noexcept
{
    ShadowMapPreviewRequest request;
    request.enabled = settings.enabled;
    request.previewSize = clampShadowMapPreviewSize(settings.previewSize);
    request.blackDepth = settings.blackDepth;
    request.whiteDepth = settings.whiteDepth;
    normalizeShadowMapPreviewRemap(request.blackDepth, request.whiteDepth);
    request.invert = settings.invert;
    if (!settings.enabled || activeCascadeCount == 0)
    {
        return request;
    }

    request.cascadeIndex = std::min(settings.cascadeIndex, activeCascadeCount - 1U);
    request.available = selectedCascadeValid && request.cascadeIndex < activeCascadeCount;
    return request;
}

void applyCsmValidationDebugOverlayPreset(
    TerrainDebugOptions& terrainDebug,
    DirectionalShadowDesc& shadow) noexcept
{
    terrainDebug.drawChunkBounds = false;
    terrainDebug.drawLodOverlay = false;
    terrainDebug.drawMaterialOverlay = false;
    terrainDebug.drawSplatFallbackOverlay = false;
    terrainDebug.drawCombinedOverlay = false;
    shadow.debugDrawLightBounds = false;
    shadow.debugDrawShadowCasters = false;
    shadow.debugDrawCascadeFrusta = true;
    shadow.debugDrawCascadeCasters = true;
}
} // namespace full_renderer::debug
