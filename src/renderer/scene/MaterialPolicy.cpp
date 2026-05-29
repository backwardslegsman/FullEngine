#include "renderer/scene/MaterialPolicy.hpp"

#include "renderer/scene/Math.hpp"

#include <algorithm>
#include <cmath>

namespace full_renderer::scene
{
namespace
{
bool hasNormalMappedTerrainLayer(const MaterialDesc& desc) noexcept
{
    if (desc.kind != MaterialKind::TerrainSplat)
    {
        return false;
    }

    for (const TerrainMaterialLayerDesc& layer : desc.terrain.layers)
    {
        if (isValid(layer.normalTexture))
        {
            return true;
        }
    }
    return false;
}
} // namespace

bool isValidMaterialKind(const MaterialKind kind) noexcept
{
    switch (kind)
    {
    case MaterialKind::Basic:
    case MaterialKind::TerrainSplat:
        return true;
    }
    return false;
}

bool isValidMaterialAlphaMode(const MaterialAlphaMode mode) noexcept
{
    switch (mode)
    {
    case MaterialAlphaMode::Opaque:
    case MaterialAlphaMode::AlphaTest:
    case MaterialAlphaMode::AlphaBlend:
        return true;
    }
    return false;
}

float materialAlphaModeToShaderValue(const MaterialAlphaMode mode) noexcept
{
    switch (mode)
    {
    case MaterialAlphaMode::AlphaTest:
        return 1.0f;
    case MaterialAlphaMode::AlphaBlend:
        return 2.0f;
    case MaterialAlphaMode::Opaque:
        return 0.0f;
    }
    return 0.0f;
}

float clampMaterialAlphaCutoff(const float cutoff) noexcept
{
    if (!std::isfinite(cutoff))
    {
        return 0.5f;
    }
    return std::clamp(cutoff, 0.0f, 1.0f);
}

bool isValidMaterialPolicyDesc(const MaterialDesc& desc) noexcept
{
    if (!isValidMaterialKind(desc.kind) ||
        !isValidMaterialAlphaMode(desc.alphaMode) ||
        !std::isfinite(desc.alphaCutoff) ||
        desc.alphaCutoff < 0.0f ||
        desc.alphaCutoff > 1.0f)
    {
        return false;
    }

    if (desc.kind == MaterialKind::TerrainSplat && desc.alphaMode != MaterialAlphaMode::Opaque)
    {
        return false;
    }

    return true;
}

MaterialRenderBucket renderBucketForMaterial(const MaterialDesc& desc) noexcept
{
    if (!isValidMaterialPolicyDesc(desc))
    {
        return MaterialRenderBucket::Unsupported;
    }

    switch (desc.alphaMode)
    {
    case MaterialAlphaMode::AlphaTest:
        return MaterialRenderBucket::AlphaTest;
    case MaterialAlphaMode::AlphaBlend:
        return MaterialRenderBucket::AlphaBlend;
    case MaterialAlphaMode::Opaque:
        return MaterialRenderBucket::Opaque;
    }
    return MaterialRenderBucket::Unsupported;
}

MaterialRenderBucket renderBucketForMaterialAndFade(
    const MaterialDesc& desc,
    const FadeRenderState& fadeState) noexcept
{
    if (fadeState.active && fadeState.alphaMode)
    {
        return MaterialRenderBucket::AlphaBlend;
    }
    return renderBucketForMaterial(desc);
}

bool isTransparentRenderBucket(const MaterialRenderBucket bucket) noexcept
{
    return bucket == MaterialRenderBucket::AlphaBlend;
}

bool materialCastsDirectionalShadowByPolicy(const MaterialDesc& desc) noexcept
{
    if (!isValidMaterialPolicyDesc(desc))
    {
        return false;
    }

    return desc.alphaMode == MaterialAlphaMode::Opaque;
}

bool materialReceivesCsmByPolicy(const MaterialDesc& desc) noexcept
{
    return isValidMaterialPolicyDesc(desc) && desc.lit;
}

ShaderVariantKey makeForwardShaderVariantKey(
    const MaterialDesc& desc,
    const ShaderVariantPass pass,
    const bool fogEnabled,
    const bool receivesCsm,
    const FadeRenderState& fadeState) noexcept
{
    ShaderVariantKey key;
    key.pass = pass;

    if (desc.lit)
    {
        key.featureMask |= kShaderFeatureLit;
    }
    if (pass == ShaderVariantPass::ForwardSkinned)
    {
        key.featureMask |= kShaderFeatureSkinned;
    }
    if (pass == ShaderVariantPass::ForwardInstanced || pass == ShaderVariantPass::TerrainSplat)
    {
        key.featureMask |= kShaderFeatureInstanced;
    }
    if (desc.kind == MaterialKind::TerrainSplat)
    {
        key.featureMask |= kShaderFeatureTerrainSplat;
    }
    if (desc.alphaMode == MaterialAlphaMode::AlphaTest)
    {
        key.featureMask |= kShaderFeatureAlphaTest;
    }
    else if (desc.alphaMode == MaterialAlphaMode::AlphaBlend)
    {
        key.featureMask |= kShaderFeatureAlphaBlend;
    }
    if (receivesCsm)
    {
        key.featureMask |= kShaderFeatureReceivesCsm;
    }
    if (fogEnabled)
    {
        key.featureMask |= kShaderFeatureFog;
    }
    if (hasNormalMappedTerrainLayer(desc))
    {
        key.featureMask |= kShaderFeatureNormalMap;
    }
    if (fadeState.active && fadeState.ditherMode)
    {
        key.featureMask |= kShaderFeatureDitherFade;
    }
    if (fadeState.active && fadeState.alphaMode)
    {
        key.featureMask |= kShaderFeatureAlphaFade;
    }

    return key;
}

std::uint32_t shaderVariantStableIndex(const ShaderVariantKey& key) noexcept
{
    const std::uint32_t pass = static_cast<std::uint32_t>(key.pass);
    const std::uint32_t alpha =
        (key.featureMask & kShaderFeatureAlphaBlend) != 0U ? 2U :
        (key.featureMask & kShaderFeatureAlphaTest) != 0U ? 1U :
        0U;
    return (pass * 4U) + alpha;
}

bool extractCameraPositionFromViewMatrix(
    const float view[16],
    float outCameraPositionWorld[3]) noexcept
{
    if (view == nullptr || outCameraPositionWorld == nullptr || !isFinite16(view))
    {
        return false;
    }

    const float tx = view[12];
    const float ty = view[13];
    const float tz = view[14];
    outCameraPositionWorld[0] = -(view[0] * tx + view[1] * ty + view[2] * tz);
    outCameraPositionWorld[1] = -(view[4] * tx + view[5] * ty + view[6] * tz);
    outCameraPositionWorld[2] = -(view[8] * tx + view[9] * ty + view[10] * tz);
    return isFinite3(outCameraPositionWorld);
}

bool computeSortDistanceSquared(
    const float centerWorld[3],
    const float cameraPositionWorld[3],
    float& outDistanceSquared) noexcept
{
    if (centerWorld == nullptr ||
        cameraPositionWorld == nullptr ||
        !isFinite3(centerWorld) ||
        !isFinite3(cameraPositionWorld))
    {
        outDistanceSquared = 0.0f;
        return false;
    }

    const float x = centerWorld[0] - cameraPositionWorld[0];
    const float y = centerWorld[1] - cameraPositionWorld[1];
    const float z = centerWorld[2] - cameraPositionWorld[2];
    outDistanceSquared = x * x + y * y + z * z;
    if (!std::isfinite(outDistanceSquared))
    {
        outDistanceSquared = 0.0f;
        return false;
    }
    return true;
}

void stableSortTransparentBackToFront(
    TransparentSortItem* items,
    const std::uint32_t itemCount) noexcept
{
    if (items == nullptr || itemCount < 2U)
    {
        return;
    }

    for (std::uint32_t index = 1; index < itemCount; ++index)
    {
        const TransparentSortItem item = items[index];
        std::uint32_t insertAt = index;
        while (insertAt > 0U)
        {
            const TransparentSortItem& previous = items[insertAt - 1U];
            const bool itemBeforePrevious =
                (item.validDistance && !previous.validDistance) ||
                (item.validDistance == previous.validDistance &&
                    item.distanceSquared > previous.distanceSquared) ||
                (item.validDistance == previous.validDistance &&
                    item.distanceSquared == previous.distanceSquared &&
                    item.stableOrder < previous.stableOrder);
            if (!itemBeforePrevious)
            {
                break;
            }
            items[insertAt] = previous;
            --insertAt;
        }
        items[insertAt] = item;
    }
}
} // namespace full_renderer::scene
