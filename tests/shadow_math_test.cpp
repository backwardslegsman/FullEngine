#include "renderer/scene/Shadow.hpp"

#include <cmath>
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

full_renderer::DirectionalLightDesc validLight()
{
    full_renderer::DirectionalLightDesc light;
    light.directionWorld[0] = -0.4f;
    light.directionWorld[1] = 0.8f;
    light.directionWorld[2] = -0.5f;
    light.intensity = 1.0f;
    return light;
}

full_renderer::DirectionalShadowDesc validShadow()
{
    full_renderer::DirectionalShadowDesc shadow;
    shadow.enabled = true;
    shadow.mapResolution = 1024;
    shadow.centerWorld[0] = 4.0f;
    shadow.centerWorld[1] = 0.0f;
    shadow.centerWorld[2] = -2.0f;
    shadow.extentMeters = 32.0f;
    shadow.depthBias = 0.003f;
    shadow.normalBias = 0.0f;
    shadow.strength = 0.5f;
    return shadow;
}

bool allFinite(const float values[16])
{
    for (int index = 0; index < 16; ++index)
    {
        if (!std::isfinite(values[index]))
        {
            return false;
        }
    }
    return true;
}

bool allCornersFinite(const float corners[8][3])
{
    for (int corner = 0; corner < 8; ++corner)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            if (!std::isfinite(corners[corner][axis]))
            {
                return false;
            }
        }
    }
    return true;
}

void identity(float out[16])
{
    for (int index = 0; index < 16; ++index)
    {
        out[index] = 0.0f;
    }
    out[0] = 1.0f;
    out[5] = 1.0f;
    out[10] = 1.0f;
    out[15] = 1.0f;
}

full_renderer::RenderViewDesc identityView()
{
    full_renderer::RenderViewDesc view;
    identity(view.view);
    identity(view.projection);
    return view;
}

void disabledShadowIsValid(int& failures)
{
    full_renderer::DirectionalShadowDesc shadow;
    shadow.enabled = false;
    shadow.mapResolution = 0;
    shadow.extentMeters = -1.0f;
    expect(full_renderer::scene::isValidDirectionalShadowDesc(shadow),
        "disabled shadow ignores inactive settings",
        failures);
}

void defaultShadowSettingsAreConservative(int& failures)
{
    const full_renderer::DirectionalShadowDesc shadow;
    expect(!shadow.enabled,
        "public shadow descriptor defaults to disabled",
        failures);
    expect(shadow.mapResolution == 1024,
        "public shadow descriptor uses a practical default resolution",
        failures);
    expect(shadow.cascadeCount == 1,
        "public shadow descriptor defaults to one cascade until enabled by caller",
        failures);
    expect(shadow.stableCascadeProjection,
        "stable cascade projection is enabled by default",
        failures);
    expect(shadow.cascadeBlendEnabled,
        "cascade blending is enabled by default",
        failures);
    expect(shadow.filterMode == full_renderer::ShadowFilterMode::Pcf2x2,
        "default shadow filter is practical 2x2 PCF",
        failures);
    expect(full_renderer::scene::isValidDirectionalShadowDesc(shadow),
        "disabled default shadow descriptor validates",
        failures);
}

void validationRejectsBadEnabledSettings(int& failures)
{
    full_renderer::DirectionalShadowDesc shadow = validShadow();

    shadow.mapResolution = 64;
    expect(!full_renderer::scene::isValidDirectionalShadowDesc(shadow),
        "too-small shadow map is rejected",
        failures);

    shadow = validShadow();
    shadow.mapResolution = 8192;
    expect(!full_renderer::scene::isValidDirectionalShadowDesc(shadow),
        "too-large shadow map is rejected",
        failures);

    shadow = validShadow();
    shadow.extentMeters = 0.0f;
    expect(!full_renderer::scene::isValidDirectionalShadowDesc(shadow),
        "zero extent is rejected",
        failures);

    shadow = validShadow();
    shadow.depthBias = -0.1f;
    expect(!full_renderer::scene::isValidDirectionalShadowDesc(shadow),
        "negative depth bias is rejected",
        failures);

    shadow = validShadow();
    shadow.slopeBias = -0.1f;
    expect(!full_renderer::scene::isValidDirectionalShadowDesc(shadow),
        "negative slope bias is rejected",
        failures);

    shadow = validShadow();
    shadow.slopeBias = 0.2f;
    expect(!full_renderer::scene::isValidDirectionalShadowDesc(shadow),
        "large slope bias is rejected",
        failures);

    shadow = validShadow();
    shadow.strength = 1.2f;
    expect(!full_renderer::scene::isValidDirectionalShadowDesc(shadow),
        "shadow strength above one is rejected",
        failures);

    shadow = validShadow();
    shadow.centerWorld[0] = std::numeric_limits<float>::quiet_NaN();
    expect(!full_renderer::scene::isValidDirectionalShadowDesc(shadow),
        "non-finite center is rejected",
        failures);

    shadow = validShadow();
    shadow.cascadeSplitLambda = 1.5f;
    expect(!full_renderer::scene::isValidDirectionalShadowDesc(shadow),
        "cascade split lambda above one is rejected",
        failures);

    shadow = validShadow();
    shadow.cascadeCameraNearMeters = 0.0f;
    expect(!full_renderer::scene::isValidDirectionalShadowDesc(shadow),
        "zero cascade camera near is rejected",
        failures);

    shadow = validShadow();
    shadow.cascadeCameraFarMeters = shadow.cascadeCameraNearMeters;
    expect(!full_renderer::scene::isValidDirectionalShadowDesc(shadow),
        "cascade far must exceed near",
        failures);

    shadow = validShadow();
    shadow.cascadeShadowDistanceMeters = shadow.cascadeCameraNearMeters;
    expect(!full_renderer::scene::isValidDirectionalShadowDesc(shadow),
        "cascade shadow distance must exceed near",
        failures);
}

void resolutionClampsToSupportedRange(int& failures)
{
    expect(full_renderer::scene::clampShadowMapResolution(1) == 128,
        "resolution clamps to minimum",
        failures);
    expect(full_renderer::scene::clampShadowMapResolution(2048) == 2048,
        "supported resolution is unchanged",
        failures);
    expect(full_renderer::scene::clampShadowMapResolution(99999) == 4096,
        "resolution clamps to maximum",
        failures);
}

void biasAndFilterHelpersAreDeterministic(int& failures)
{
    expect(full_renderer::scene::clampShadowDepthBias(-1.0f) == 0.0f,
        "negative depth bias clamps to zero",
        failures);
    expect(full_renderer::scene::clampShadowDepthBias(0.02f) == 0.02f,
        "valid depth bias is preserved",
        failures);
    expect(full_renderer::scene::clampShadowDepthBias(1.0f) == 0.1f,
        "large depth bias clamps to maximum",
        failures);
    expect(full_renderer::scene::clampShadowDepthBias(std::numeric_limits<float>::quiet_NaN()) == 0.0f,
        "non-finite depth bias clamps to zero",
        failures);

    expect(full_renderer::scene::clampShadowSlopeBias(-1.0f) == 0.0f,
        "negative slope bias clamps to zero",
        failures);
    expect(full_renderer::scene::clampShadowSlopeBias(0.02f) == 0.02f,
        "valid slope bias is preserved",
        failures);
    expect(full_renderer::scene::clampShadowSlopeBias(1.0f) == 0.1f,
        "large slope bias clamps to maximum",
        failures);

    expect(full_renderer::scene::shadowFilterModeToTapCount(full_renderer::ShadowFilterMode::Nearest) == 1,
        "nearest filter maps to one tap",
        failures);
    expect(full_renderer::scene::shadowFilterModeToTapCount(full_renderer::ShadowFilterMode::Pcf2x2) == 4,
        "2x2 PCF maps to four taps",
        failures);
    expect(full_renderer::scene::shadowFilterModeToTapCount(full_renderer::ShadowFilterMode::Pcf3x3) == 9,
        "3x3 PCF maps to nine taps",
        failures);
    expect(full_renderer::scene::shadowFilterModeToTapCount(static_cast<full_renderer::ShadowFilterMode>(999)) == 1,
        "invalid filter mode falls back to nearest",
        failures);
}

void matrixBuildIsDeterministicAndFinite(int& failures)
{
    full_renderer::scene::DirectionalShadowMatrices first;
    full_renderer::scene::DirectionalShadowMatrices second;
    const full_renderer::DirectionalLightDesc light = validLight();
    const full_renderer::DirectionalShadowDesc shadow = validShadow();

    expect(full_renderer::scene::buildDirectionalShadowMatrices(light, shadow, first),
        "valid shadow matrices build",
        failures);
    expect(full_renderer::scene::buildDirectionalShadowMatrices(light, shadow, second),
        "valid shadow matrices build repeatedly",
        failures);
    expect(allFinite(first.view) && allFinite(first.projection) && allFinite(first.viewProjection),
        "shadow matrices are finite",
        failures);
    for (int index = 0; index < 16; ++index)
    {
        expect(first.viewProjection[index] == second.viewProjection[index],
            "shadow matrix build is deterministic",
            failures);
    }
}

void frustumCornersComeFromInvertibleViewProjection(int& failures)
{
    full_renderer::scene::DirectionalShadowMatrices matrices;
    expect(full_renderer::scene::buildDirectionalShadowMatrices(validLight(), validShadow(), matrices),
        "valid matrices build before corner extraction",
        failures);

    float inverse[16] = {};
    expect(full_renderer::scene::invertColumnMajor4x4(matrices.viewProjection, inverse),
        "view-projection matrix is invertible",
        failures);
    expect(allFinite(inverse), "inverse view-projection is finite", failures);

    float corners[8][3] = {};
    expect(full_renderer::scene::extractWorldFrustumCorners(matrices.viewProjection, corners),
        "world frustum corners extract",
        failures);
    expect(allCornersFinite(corners), "world frustum corners are finite", failures);

    bool foundDifferentCorner = false;
    for (int axis = 0; axis < 3; ++axis)
    {
        if (corners[0][axis] != corners[6][axis])
        {
            foundDifferentCorner = true;
        }
    }
    expect(foundDifferentCorner, "corner extraction produces distinct corners", failures);
}

void nonInvertibleMatrixIsRejected(int& failures)
{
    float singular[16] = {};
    float inverse[16] = {};
    float corners[8][3] = {};
    expect(!full_renderer::scene::invertColumnMajor4x4(singular, inverse),
        "singular matrix inversion is rejected",
        failures);
    expect(!full_renderer::scene::extractWorldFrustumCorners(singular, corners),
        "singular frustum corner extraction is rejected",
        failures);
}

void shadowSplitBuildsOneDebugSplit(int& failures)
{
    full_renderer::scene::DirectionalShadowSplit split;
    const full_renderer::DirectionalShadowDesc shadow = validShadow();
    expect(full_renderer::scene::buildDirectionalShadowSplit(validLight(), shadow, split),
        "single directional shadow split builds",
        failures);
    expect(allFinite(split.matrices.viewProjection), "split stores finite view-projection", failures);
    expect(allFinite(split.inverseViewProjection), "split stores finite inverse view-projection", failures);
    expect(allCornersFinite(split.worldCorners), "split stores finite world corners", failures);
    expect(split.extentMeters == shadow.extentMeters, "split stores shadow extent", failures);
    expect(split.nearDistanceMeters == 0.0f, "split stores current near distance", failures);
    expect(split.farDistanceMeters == shadow.extentMeters * 2.0f, "split stores current far distance", failures);
}

void cascadeCountClampsAndUniformRangesAreOrdered(int& failures)
{
    full_renderer::DirectionalShadowDesc shadow = validShadow();
    shadow.cascadeCount = 99;
    shadow.cascadeSplitMode = full_renderer::ShadowCascadeSplitMode::Uniform;
    shadow.cascadeCameraNearMeters = 1.0f;
    shadow.cascadeCameraFarMeters = 101.0f;
    shadow.cascadeShadowDistanceMeters = 81.0f;

    full_renderer::scene::DirectionalShadowCascadeRange ranges[full_renderer::kMaxDirectionalShadowCascades] = {};
    std::uint32_t cascadeCount = 0;
    expect(full_renderer::scene::clampShadowCascadeCount(0) == 1, "zero cascade count clamps to one", failures);
    expect(full_renderer::scene::clampShadowCascadeCount(99) == full_renderer::kMaxDirectionalShadowCascades,
        "large cascade count clamps to max",
        failures);
    expect(full_renderer::scene::computeDirectionalShadowCascadeRanges(shadow, ranges, cascadeCount),
        "uniform cascade ranges compute",
        failures);
    expect(cascadeCount == full_renderer::kMaxDirectionalShadowCascades,
        "range computation returns clamped cascade count",
        failures);
    expect(ranges[0].nearDistanceMeters == 1.0f, "first cascade starts at camera near", failures);
    expect(ranges[cascadeCount - 1U].farDistanceMeters == 81.0f,
        "final cascade clamps to shadow distance",
        failures);
    for (std::uint32_t index = 1; index < cascadeCount; ++index)
    {
        expect(ranges[index].nearDistanceMeters == ranges[index - 1U].farDistanceMeters,
            "cascade ranges are contiguous",
            failures);
        expect(ranges[index].farDistanceMeters > ranges[index].nearDistanceMeters,
            "cascade ranges are strictly ordered",
            failures);
    }
}

void practicalRangesAreDeterministic(int& failures)
{
    full_renderer::DirectionalShadowDesc shadow = validShadow();
    shadow.cascadeCount = 4;
    shadow.cascadeSplitMode = full_renderer::ShadowCascadeSplitMode::Practical;
    shadow.cascadeSplitLambda = 0.65f;
    shadow.cascadeCameraNearMeters = 0.1f;
    shadow.cascadeCameraFarMeters = 100.0f;
    shadow.cascadeShadowDistanceMeters = 60.0f;

    full_renderer::scene::DirectionalShadowCascadeRange first[full_renderer::kMaxDirectionalShadowCascades] = {};
    full_renderer::scene::DirectionalShadowCascadeRange second[full_renderer::kMaxDirectionalShadowCascades] = {};
    std::uint32_t firstCount = 0;
    std::uint32_t secondCount = 0;
    expect(full_renderer::scene::computeDirectionalShadowCascadeRanges(shadow, first, firstCount),
        "practical cascade ranges compute",
        failures);
    expect(full_renderer::scene::computeDirectionalShadowCascadeRanges(shadow, second, secondCount),
        "practical cascade ranges compute repeatedly",
        failures);
    expect(firstCount == secondCount, "cascade range count is deterministic", failures);
    for (std::uint32_t index = 0; index < firstCount; ++index)
    {
        expect(first[index].nearDistanceMeters == second[index].nearDistanceMeters &&
                first[index].farDistanceMeters == second[index].farDistanceMeters,
            "cascade range values are deterministic",
            failures);
    }
}

void logarithmicRangesAreOrderedAndClamped(int& failures)
{
    full_renderer::DirectionalShadowDesc shadow = validShadow();
    shadow.cascadeCount = 4;
    shadow.cascadeSplitMode = full_renderer::ShadowCascadeSplitMode::Logarithmic;
    shadow.cascadeCameraNearMeters = 0.5f;
    shadow.cascadeCameraFarMeters = 500.0f;
    shadow.cascadeShadowDistanceMeters = 128.0f;

    full_renderer::scene::DirectionalShadowCascadeRange ranges[full_renderer::kMaxDirectionalShadowCascades] = {};
    std::uint32_t cascadeCount = 0;
    expect(full_renderer::scene::computeDirectionalShadowCascadeRanges(shadow, ranges, cascadeCount),
        "logarithmic cascade ranges compute",
        failures);
    expect(cascadeCount == 4,
        "logarithmic range computation preserves requested cascade count",
        failures);
    expect(ranges[0].nearDistanceMeters == shadow.cascadeCameraNearMeters,
        "logarithmic first range starts at camera near",
        failures);
    expect(ranges[cascadeCount - 1U].farDistanceMeters == shadow.cascadeShadowDistanceMeters,
        "logarithmic final range clamps to shadow distance",
        failures);
    for (std::uint32_t index = 1; index < cascadeCount; ++index)
    {
        expect(ranges[index].nearDistanceMeters == ranges[index - 1U].farDistanceMeters,
            "logarithmic ranges are contiguous",
            failures);
        expect(ranges[index].farDistanceMeters > ranges[index].nearDistanceMeters,
            "logarithmic ranges are strictly increasing",
            failures);
        expect(ranges[index].normalizedSplitDepth >= ranges[index - 1U].normalizedSplitDepth,
            "logarithmic normalized split depths are ordered",
            failures);
    }
}

void cameraSliceAndCascadeSetBuild(int& failures)
{
    full_renderer::DirectionalShadowDesc shadow = validShadow();
    shadow.cascadeCount = 3;
    shadow.cascadeCameraNearMeters = 0.1f;
    shadow.cascadeCameraFarMeters = 100.0f;
    shadow.cascadeShadowDistanceMeters = 50.0f;

    full_renderer::scene::DirectionalShadowCascadeRange ranges[full_renderer::kMaxDirectionalShadowCascades] = {};
    std::uint32_t cascadeCount = 0;
    expect(full_renderer::scene::computeDirectionalShadowCascadeRanges(shadow, ranges, cascadeCount),
        "ranges compute before slice corners",
        failures);

    float sliceCorners[8][3] = {};
    expect(full_renderer::scene::computeCameraFrustumSliceCorners(identityView(), ranges[0], shadow, sliceCorners),
        "camera frustum slice corners compute",
        failures);
    expect(allCornersFinite(sliceCorners), "camera slice corners are finite", failures);

    full_renderer::scene::DirectionalShadowCascadeSet cascadeSet;
    expect(full_renderer::scene::buildDirectionalShadowCascadeSet(validLight(), shadow, identityView(), cascadeSet),
        "directional shadow cascade set builds",
        failures);
    expect(cascadeSet.cascadeCount == 3, "cascade set stores configured cascade count", failures);
    for (std::uint32_t index = 0; index < cascadeSet.cascadeCount; ++index)
    {
        expect(cascadeSet.splits[index].splitIndex == index, "cascade split stores index", failures);
        expect(allFinite(cascadeSet.splits[index].matrices.viewProjection),
            "cascade split stores finite light view-projection",
            failures);
        expect(allFinite(cascadeSet.splits[index].inverseViewProjection),
            "cascade split stores finite inverse light view-projection",
            failures);
        expect(allCornersFinite(cascadeSet.splits[index].worldCorners),
            "cascade split stores finite light debug corners",
            failures);
        expect(cascadeSet.splits[index].orthoMax[0] > cascadeSet.splits[index].orthoMin[0],
            "cascade split stores orthographic bounds",
            failures);
        expect(cascadeSet.splits[index].texelSize[0] > 0.0f && cascadeSet.splits[index].texelSize[1] > 0.0f,
            "cascade split stores positive stable texel sizes",
            failures);
        expect(std::fabs(cascadeSet.splits[index].snapOffset[0]) <= cascadeSet.splits[index].texelSize[0] &&
                std::fabs(cascadeSet.splits[index].snapOffset[1]) <= cascadeSet.splits[index].texelSize[1],
            "cascade split snap offsets stay within one texel",
            failures);
    }
}

void stableProjectionSnapsToTexelGrid(int& failures)
{
    float texelSize = 0.0f;
    expect(full_renderer::scene::computeDirectionalShadowTexelSize(-5.0f, 5.0f, 1024, texelSize),
        "shadow texel size computes",
        failures);
    expect(std::fabs(texelSize - (10.0f / 1024.0f)) < 0.00001f,
        "shadow texel size uses projection span and resolution",
        failures);
    expect(!full_renderer::scene::computeDirectionalShadowTexelSize(2.0f, 2.0f, 1024, texelSize),
        "zero projection span rejects texel size",
        failures);

    const float minA[3] = {-5.0f, -6.0f, -7.0f};
    const float maxA[3] = {5.0f, 6.0f, 9.0f};
    float snappedMin[3] = {};
    float snappedMax[3] = {};
    float texels[2] = {};
    float unsnappedCenter[2] = {};
    float snappedCenter[2] = {};
    float snapOffset[2] = {};
    expect(full_renderer::scene::snapDirectionalShadowProjectionBounds(
               minA,
               maxA,
               1024,
               true,
               snappedMin,
               snappedMax,
               texels,
               unsnappedCenter,
               snappedCenter,
               snapOffset),
        "stable projection bounds snap",
        failures);
    expect(snappedMin[0] <= minA[0] && snappedMax[0] >= maxA[0] &&
            snappedMin[1] <= minA[1] && snappedMax[1] >= maxA[1],
        "stable snapping does not shrink XY coverage",
        failures);
    expect(snappedMin[2] == minA[2] && snappedMax[2] == maxA[2],
        "stable snapping leaves depth bounds unchanged",
        failures);

    const float tinyMoveMin[3] = {-4.998f, -5.998f, -7.0f};
    const float tinyMoveMax[3] = {5.002f, 6.002f, 9.0f};
    float tinySnappedMin[3] = {};
    float tinySnappedMax[3] = {};
    float tinyTexels[2] = {};
    float tinyUnsnappedCenter[2] = {};
    float tinySnappedCenter[2] = {};
    float tinySnapOffset[2] = {};
    expect(full_renderer::scene::snapDirectionalShadowProjectionBounds(
               tinyMoveMin,
               tinyMoveMax,
               1024,
               true,
               tinySnappedMin,
               tinySnappedMax,
               tinyTexels,
               tinyUnsnappedCenter,
               tinySnappedCenter,
               tinySnapOffset),
        "tiny moved stable bounds snap",
        failures);
    expect(tinySnappedCenter[0] == snappedCenter[0] && tinySnappedCenter[1] == snappedCenter[1],
        "tiny light-space movement keeps snapped center stable",
        failures);

    const float largeMoveMin[3] = {-4.80f, -5.80f, -7.0f};
    const float largeMoveMax[3] = {5.20f, 6.20f, 9.0f};
    float largeSnappedMin[3] = {};
    float largeSnappedMax[3] = {};
    float largeTexels[2] = {};
    float largeUnsnappedCenter[2] = {};
    float largeSnappedCenter[2] = {};
    float largeSnapOffset[2] = {};
    expect(full_renderer::scene::snapDirectionalShadowProjectionBounds(
               largeMoveMin,
               largeMoveMax,
               1024,
               true,
               largeSnappedMin,
               largeSnappedMax,
               largeTexels,
               largeUnsnappedCenter,
               largeSnappedCenter,
               largeSnapOffset),
        "large moved stable bounds snap",
        failures);
    expect(largeSnappedCenter[0] != snappedCenter[0] || largeSnappedCenter[1] != snappedCenter[1],
        "large light-space movement advances snapped center",
        failures);
}

void cascadeSelectionByViewDepthIsDeterministic(int& failures)
{
    full_renderer::DirectionalShadowDesc shadow = validShadow();
    shadow.cascadeCount = 3;
    shadow.cascadeSplitMode = full_renderer::ShadowCascadeSplitMode::Uniform;
    shadow.cascadeCameraNearMeters = 1.0f;
    shadow.cascadeCameraFarMeters = 61.0f;
    shadow.cascadeShadowDistanceMeters = 61.0f;

    full_renderer::scene::DirectionalShadowCascadeRange ranges[full_renderer::kMaxDirectionalShadowCascades] = {};
    std::uint32_t cascadeCount = 0;
    expect(full_renderer::scene::computeDirectionalShadowCascadeRanges(shadow, ranges, cascadeCount),
        "ranges compute before cascade selection",
        failures);
    expect(full_renderer::scene::selectDirectionalShadowCascade(1.0f, ranges, cascadeCount) == 0,
        "near depth selects cascade 0",
        failures);
    expect(full_renderer::scene::selectDirectionalShadowCascade(ranges[0].farDistanceMeters, ranges, cascadeCount) == 0,
        "exact first boundary stays in cascade 0",
        failures);
    expect(full_renderer::scene::selectDirectionalShadowCascade(ranges[0].farDistanceMeters + 0.001f, ranges, cascadeCount) == 1,
        "depth after first boundary selects cascade 1",
        failures);
    expect(full_renderer::scene::selectDirectionalShadowCascade(ranges[1].farDistanceMeters + 0.001f, ranges, cascadeCount) == 2,
        "depth after second boundary selects cascade 2",
        failures);
    expect(full_renderer::scene::selectDirectionalShadowCascade(ranges[2].farDistanceMeters + 0.001f, ranges, cascadeCount) ==
            UINT32_MAX,
        "depth beyond final cascade is unshadowed",
        failures);
    expect(full_renderer::scene::selectDirectionalShadowCascade(ranges[1].farDistanceMeters + 0.001f, ranges, cascadeCount) ==
            full_renderer::scene::selectDirectionalShadowCascade(ranges[1].farDistanceMeters + 0.001f, ranges, cascadeCount),
        "cascade selection is deterministic",
        failures);
}

void cascadeBlendBandPlanningIsDeterministic(int& failures)
{
    expect(full_renderer::scene::clampCascadeBlendFraction(-1.0f) == 0.0f,
        "negative blend fraction clamps to zero",
        failures);
    expect(full_renderer::scene::clampCascadeBlendFraction(0.25f) == 0.25f,
        "valid blend fraction is preserved",
        failures);
    expect(full_renderer::scene::clampCascadeBlendFraction(2.0f) == 0.5f,
        "large blend fraction clamps to maximum",
        failures);
    expect(full_renderer::scene::clampCascadeBlendFraction(std::numeric_limits<float>::quiet_NaN()) == 0.0f,
        "non-finite blend fraction clamps to zero",
        failures);

    float bandStart = 0.0f;
    float bandSize = 0.0f;
    expect(full_renderer::scene::computeCascadeBlendBand(10.0f, 30.0f, 0.25f, bandStart, bandSize),
        "cascade blend band computes",
        failures);
    expect(std::fabs(bandSize - 5.0f) < 0.0001f,
        "blend band size uses cascade span and fraction",
        failures);
    expect(std::fabs(bandStart - 25.0f) < 0.0001f,
        "blend band starts before current split far",
        failures);

    float disabledStart = 0.0f;
    float disabledSize = 1.0f;
    expect(full_renderer::scene::computeCascadeBlendBand(10.0f, 30.0f, 0.0f, disabledStart, disabledSize),
        "disabled cascade blend band is valid",
        failures);
    expect(disabledStart == 30.0f && disabledSize == 0.0f,
        "disabled cascade blend band has zero size at boundary",
        failures);

    float repeatedStart = 0.0f;
    float repeatedSize = 0.0f;
    expect(full_renderer::scene::computeCascadeBlendBand(10.0f, 30.0f, 0.25f, repeatedStart, repeatedSize),
        "cascade blend band computes repeatedly",
        failures);
    expect(repeatedStart == bandStart && repeatedSize == bandSize,
        "cascade blend band is deterministic",
        failures);
    expect(!full_renderer::scene::computeCascadeBlendBand(30.0f, 30.0f, 0.25f, repeatedStart, repeatedSize),
        "invalid cascade span rejects blend band",
        failures);
}

void shadowResourceReconfigurationPlanningIsDeterministic(int& failures)
{
    using full_renderer::scene::ShadowResourceReconfigureAction;

    full_renderer::DirectionalShadowDesc shadow = validShadow();
    shadow.enabled = false;
    full_renderer::scene::ShadowResourceReconfigurePlan plan =
        full_renderer::scene::planShadowResourceReconfiguration(shadow, 0, 0, false);
    expect(plan.valid, "disabled resource plan is valid", failures);
    expect(plan.action == ShadowResourceReconfigureAction::None,
        "disabled shadows with no active resources are a no-op",
        failures);
    expect(!plan.requested.enabled && plan.requested.mapResolution == 0 && plan.requested.cascadeCount == 0,
        "disabled resource config requests no resources",
        failures);

    plan = full_renderer::scene::planShadowResourceReconfiguration(shadow, 1024, 2, true);
    expect(plan.action == ShadowResourceReconfigureAction::Release,
        "disabled shadows release active resources",
        failures);

    shadow = validShadow();
    shadow.mapResolution = 2048;
    shadow.cascadeCount = 3;
    plan = full_renderer::scene::planShadowResourceReconfiguration(shadow, 2048, 3, true);
    expect(plan.valid, "matching enabled resource plan is valid", failures);
    expect(plan.action == ShadowResourceReconfigureAction::None,
        "matching valid resources are preserved",
        failures);
    expect(plan.requested.enabled && plan.requested.mapResolution == 2048 && plan.requested.cascadeCount == 3,
        "enabled resource config stores clamped request",
        failures);

    plan = full_renderer::scene::planShadowResourceReconfiguration(shadow, 1024, 3, true);
    expect(plan.action == ShadowResourceReconfigureAction::Recreate,
        "resolution changes recreate resources",
        failures);

    plan = full_renderer::scene::planShadowResourceReconfiguration(shadow, 2048, 1, true);
    expect(plan.action == ShadowResourceReconfigureAction::Recreate,
        "cascade-count changes recreate resources",
        failures);

    plan = full_renderer::scene::planShadowResourceReconfiguration(shadow, 2048, 3, false);
    expect(plan.action == ShadowResourceReconfigureAction::Recreate,
        "invalid active resources are recreated",
        failures);

    shadow.mapResolution = 99999;
    shadow.cascadeCount = 99;
    const full_renderer::scene::ShadowResourceConfig clamped =
        full_renderer::scene::shadowResourceConfigFromDesc(shadow);
    expect(clamped.mapResolution == 4096,
        "resource config clamps requested resolution",
        failures);
    expect(clamped.cascadeCount == full_renderer::kMaxDirectionalShadowCascades,
        "resource config clamps requested cascade count",
        failures);

    shadow = validShadow();
    shadow.enabled = true;
    shadow.extentMeters = 0.0f;
    plan = full_renderer::scene::planShadowResourceReconfiguration(shadow, 1024, 2, true);
    expect(!plan.valid, "invalid shadow descriptor marks resource plan invalid", failures);
    expect(plan.action == ShadowResourceReconfigureAction::Release,
        "invalid shadow descriptor releases active resources",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    disabledShadowIsValid(failures);
    defaultShadowSettingsAreConservative(failures);
    validationRejectsBadEnabledSettings(failures);
    resolutionClampsToSupportedRange(failures);
    biasAndFilterHelpersAreDeterministic(failures);
    matrixBuildIsDeterministicAndFinite(failures);
    frustumCornersComeFromInvertibleViewProjection(failures);
    nonInvertibleMatrixIsRejected(failures);
    shadowSplitBuildsOneDebugSplit(failures);
    cascadeCountClampsAndUniformRangesAreOrdered(failures);
    practicalRangesAreDeterministic(failures);
    logarithmicRangesAreOrderedAndClamped(failures);
    cameraSliceAndCascadeSetBuild(failures);
    stableProjectionSnapsToTexelGrid(failures);
    cascadeSelectionByViewDepthIsDeterministic(failures);
    cascadeBlendBandPlanningIsDeterministic(failures);
    shadowResourceReconfigurationPlanningIsDeterministic(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
