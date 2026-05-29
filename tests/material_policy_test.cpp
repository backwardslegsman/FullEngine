#include "renderer/scene/Fade.hpp"
#include "renderer/scene/MaterialPolicy.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void expect(const bool condition, const char* message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

full_renderer::FadeDesc alphaFade()
{
    full_renderer::FadeDesc fade;
    fade.enabled = true;
    fade.mode = full_renderer::FadeMode::Alpha;
    fade.visibility = 0.5f;
    return fade;
}

void materialDescriptorValidationCoversAlphaPolicy(int& failures)
{
    full_renderer::MaterialDesc material;
    expect(full_renderer::scene::isValidMaterialPolicyDesc(material),
        "default material policy validates",
        failures);

    material.alphaMode = full_renderer::MaterialAlphaMode::AlphaTest;
    material.alphaCutoff = 0.25f;
    expect(full_renderer::scene::isValidMaterialPolicyDesc(material),
        "basic alpha-test material validates",
        failures);

    material.alphaCutoff = std::numeric_limits<float>::quiet_NaN();
    expect(!full_renderer::scene::isValidMaterialPolicyDesc(material),
        "alpha cutoff rejects NaN",
        failures);

    material = {};
    material.kind = full_renderer::MaterialKind::TerrainSplat;
    material.alphaMode = full_renderer::MaterialAlphaMode::AlphaBlend;
    expect(!full_renderer::scene::isValidMaterialPolicyDesc(material),
        "terrain splat alpha blend is rejected",
        failures);

    material = {};
    material.alphaMode = static_cast<full_renderer::MaterialAlphaMode>(99);
    expect(!full_renderer::scene::isValidMaterialPolicyDesc(material),
        "invalid alpha mode enum is rejected",
        failures);
}

void renderBucketMappingIsExplicit(int& failures)
{
    full_renderer::MaterialDesc material;
    expect(full_renderer::scene::renderBucketForMaterial(material) ==
            full_renderer::scene::MaterialRenderBucket::Opaque,
        "default material maps to opaque bucket",
        failures);

    material.alphaMode = full_renderer::MaterialAlphaMode::AlphaTest;
    expect(full_renderer::scene::renderBucketForMaterial(material) ==
            full_renderer::scene::MaterialRenderBucket::AlphaTest,
        "alpha-test material maps to alpha-test bucket",
        failures);

    material.alphaMode = full_renderer::MaterialAlphaMode::AlphaBlend;
    expect(full_renderer::scene::renderBucketForMaterial(material) ==
            full_renderer::scene::MaterialRenderBucket::AlphaBlend,
        "alpha-blend material maps to alpha-blend bucket",
        failures);

    material.alphaMode = full_renderer::MaterialAlphaMode::Opaque;
    const full_renderer::scene::FadeRenderState fadeState =
        full_renderer::scene::makeFadeRenderState(alphaFade());
    expect(full_renderer::scene::renderBucketForMaterialAndFade(material, fadeState) ==
            full_renderer::scene::MaterialRenderBucket::AlphaBlend,
        "alpha fade maps an opaque material to transparent bucket for the frame",
        failures);
}

void shadowCompatibilityPolicyIsConservative(int& failures)
{
    full_renderer::MaterialDesc material;
    expect(full_renderer::scene::materialCastsDirectionalShadowByPolicy(material),
        "opaque material casts shadows by policy",
        failures);

    material.alphaMode = full_renderer::MaterialAlphaMode::AlphaTest;
    expect(!full_renderer::scene::materialCastsDirectionalShadowByPolicy(material),
        "alpha-test shadow clipping is diagnosed as unsupported",
        failures);

    material.alphaMode = full_renderer::MaterialAlphaMode::AlphaBlend;
    expect(!full_renderer::scene::materialCastsDirectionalShadowByPolicy(material),
        "alpha-blend material does not cast shadows by default",
        failures);
}

void transparentSortIsBackToFrontAndStable(int& failures)
{
    full_renderer::scene::TransparentSortItem items[4];
    items[0].submissionIndex = 0;
    items[0].stableOrder = 0;
    items[0].distanceSquared = 4.0f;
    items[0].validDistance = true;
    items[1].submissionIndex = 1;
    items[1].stableOrder = 1;
    items[1].distanceSquared = 25.0f;
    items[1].validDistance = true;
    items[2].submissionIndex = 2;
    items[2].stableOrder = 2;
    items[2].distanceSquared = 25.0f;
    items[2].validDistance = true;
    items[3].submissionIndex = 3;
    items[3].stableOrder = 3;
    items[3].validDistance = false;

    full_renderer::scene::stableSortTransparentBackToFront(items, 4);

    expect(items[0].submissionIndex == 1 && items[1].submissionIndex == 2,
        "equal transparent distances preserve submission order",
        failures);
    expect(items[2].submissionIndex == 0,
        "nearer transparent item is rendered after farther items",
        failures);
    expect(items[3].submissionIndex == 3,
        "invalid sort distances remain safely last",
        failures);
}

void cameraPositionAndSortDistanceAreDeterministic(int& failures)
{
    float view[16] = {};
    view[0] = 1.0f;
    view[5] = 1.0f;
    view[10] = 1.0f;
    view[15] = 1.0f;
    view[12] = -3.0f;
    view[13] = -4.0f;
    view[14] = -5.0f;

    float camera[3] = {};
    expect(full_renderer::scene::extractCameraPositionFromViewMatrix(view, camera),
        "camera position extracts from finite rigid view matrix",
        failures);
    expect(camera[0] == 3.0f && camera[1] == 4.0f && camera[2] == 5.0f,
        "identity-rotation view translation inverts to camera position",
        failures);

    const float center[3] = {3.0f, 4.0f, 9.0f};
    float distanceSquared = 0.0f;
    expect(full_renderer::scene::computeSortDistanceSquared(center, camera, distanceSquared),
        "finite sort distance computes",
        failures);
    expect(distanceSquared == 16.0f,
        "sort distance is squared camera-to-center distance",
        failures);
}

void shaderVariantKeysAreDeterministic(int& failures)
{
    full_renderer::MaterialDesc material;
    material.alphaMode = full_renderer::MaterialAlphaMode::AlphaBlend;
    const full_renderer::scene::FadeRenderState fadeState =
        full_renderer::scene::makeFadeRenderState(alphaFade());

    const full_renderer::scene::ShaderVariantKey first =
        full_renderer::scene::makeForwardShaderVariantKey(
            material,
            full_renderer::scene::ShaderVariantPass::ForwardSkinned,
            true,
            true,
            fadeState);
    const full_renderer::scene::ShaderVariantKey second =
        full_renderer::scene::makeForwardShaderVariantKey(
            material,
            full_renderer::scene::ShaderVariantPass::ForwardSkinned,
            true,
            true,
            fadeState);

    expect(first.featureMask == second.featureMask &&
            first.pass == second.pass &&
            full_renderer::scene::shaderVariantStableIndex(first) ==
                full_renderer::scene::shaderVariantStableIndex(second),
        "shader variant key construction is deterministic",
        failures);
    expect((first.featureMask & full_renderer::scene::kShaderFeatureAlphaBlend) != 0U,
        "alpha blend feature bit is present",
        failures);
    expect((first.featureMask & full_renderer::scene::kShaderFeatureSkinned) != 0U,
        "skinned feature bit is present",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    materialDescriptorValidationCoversAlphaPolicy(failures);
    renderBucketMappingIsExplicit(failures);
    shadowCompatibilityPolicyIsConservative(failures);
    transparentSortIsBackToFrontAndStable(failures);
    cameraPositionAndSortDistanceAreDeterministic(failures);
    shaderVariantKeysAreDeterministic(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
