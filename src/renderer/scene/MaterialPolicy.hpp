#pragma once

#include "full_renderer/Renderer.hpp"
#include "renderer/scene/Fade.hpp"

#include <cstdint>

namespace full_renderer::scene
{
enum class MaterialRenderBucket : std::uint8_t
{
    Opaque,
    AlphaTest,
    AlphaBlend,
    Particle,
    Decal,
    SelectionMask,
    ShadowDepth,
    Debug,
    Unsupported
};

enum class ShaderVariantPass : std::uint8_t
{
    ForwardStatic,
    ForwardInstanced,
    ForwardSkinned,
    TerrainSplat,
    ShadowDepth,
    SelectionMask,
    Decal,
    Particle,
    ColorGrading,
    Debug
};

enum ShaderVariantFeatureBits : std::uint32_t
{
    kShaderFeatureLit = 1U << 0U,
    kShaderFeatureSkinned = 1U << 1U,
    kShaderFeatureInstanced = 1U << 2U,
    kShaderFeatureTerrainSplat = 1U << 3U,
    kShaderFeatureAlphaTest = 1U << 4U,
    kShaderFeatureAlphaBlend = 1U << 5U,
    kShaderFeatureReceivesCsm = 1U << 6U,
    kShaderFeatureFog = 1U << 7U,
    kShaderFeatureNormalMap = 1U << 8U,
    kShaderFeatureDitherFade = 1U << 9U,
    kShaderFeatureAlphaFade = 1U << 10U
};

struct ShaderVariantKey
{
    ShaderVariantPass pass = ShaderVariantPass::ForwardStatic;
    std::uint32_t featureMask = 0;
};

struct TransparentSortItem
{
    std::uint32_t submissionIndex = 0;
    std::uint32_t stableOrder = 0;
    float distanceSquared = 0.0f;
    bool validDistance = false;
};

bool isValidMaterialKind(MaterialKind kind) noexcept;
bool isValidMaterialAlphaMode(MaterialAlphaMode mode) noexcept;
float materialAlphaModeToShaderValue(MaterialAlphaMode mode) noexcept;
float clampMaterialAlphaCutoff(float cutoff) noexcept;
bool isValidMaterialPolicyDesc(const MaterialDesc& desc) noexcept;

MaterialRenderBucket renderBucketForMaterial(const MaterialDesc& desc) noexcept;
MaterialRenderBucket renderBucketForMaterialAndFade(
    const MaterialDesc& desc,
    const FadeRenderState& fadeState) noexcept;
bool isTransparentRenderBucket(MaterialRenderBucket bucket) noexcept;
bool materialCastsDirectionalShadowByPolicy(const MaterialDesc& desc) noexcept;
bool materialReceivesCsmByPolicy(const MaterialDesc& desc) noexcept;

ShaderVariantKey makeForwardShaderVariantKey(
    const MaterialDesc& desc,
    ShaderVariantPass pass,
    bool fogEnabled,
    bool receivesCsm,
    const FadeRenderState& fadeState) noexcept;
std::uint32_t shaderVariantStableIndex(const ShaderVariantKey& key) noexcept;

bool extractCameraPositionFromViewMatrix(const float view[16], float outCameraPositionWorld[3]) noexcept;
bool computeSortDistanceSquared(
    const float centerWorld[3],
    const float cameraPositionWorld[3],
    float& outDistanceSquared) noexcept;
void stableSortTransparentBackToFront(
    TransparentSortItem* items,
    std::uint32_t itemCount) noexcept;
} // namespace full_renderer::scene
