#include "renderer/scene/Shadow.hpp"

#include <algorithm>
#include <cmath>

namespace full_renderer::scene
{
namespace
{
constexpr std::uint32_t kMinimumShadowMapResolution = 128;
constexpr std::uint32_t kMaximumShadowMapResolution = 4096;
constexpr float kMinimumCascadeDistance = 0.0001f;

Vec3 add(const Vec3 lhs, const Vec3 rhs) noexcept
{
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 subtract(const Vec3 lhs, const Vec3 rhs) noexcept
{
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vec3 multiply(const Vec3 value, const float scalar) noexcept
{
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

float dot(const Vec3 lhs, const Vec3 rhs) noexcept
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(const Vec3 lhs, const Vec3 rhs) noexcept
{
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x};
}

Vec3 normalize(const Vec3 value) noexcept
{
    const float lengthSquared = dot(value, value);
    if (lengthSquared <= 0.000001f)
    {
        return {};
    }

    const float invLength = 1.0f / std::sqrt(lengthSquared);
    return multiply(value, invLength);
}

void setIdentity(float out[16]) noexcept
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

void makeLookAt(float out[16], const Vec3 eye, const Vec3 target, const Vec3 up) noexcept
{
    const Vec3 zAxis = normalize(subtract(eye, target));
    Vec3 xAxis = normalize(cross(up, zAxis));
    if (dot(xAxis, xAxis) <= 0.000001f)
    {
        xAxis = normalize(cross({1.0f, 0.0f, 0.0f}, zAxis));
    }
    const Vec3 yAxis = cross(zAxis, xAxis);

    out[0] = xAxis.x;
    out[1] = yAxis.x;
    out[2] = zAxis.x;
    out[3] = 0.0f;
    out[4] = xAxis.y;
    out[5] = yAxis.y;
    out[6] = zAxis.y;
    out[7] = 0.0f;
    out[8] = xAxis.z;
    out[9] = yAxis.z;
    out[10] = zAxis.z;
    out[11] = 0.0f;
    out[12] = -dot(xAxis, eye);
    out[13] = -dot(yAxis, eye);
    out[14] = -dot(zAxis, eye);
    out[15] = 1.0f;
}

void makeOrtho(float out[16], const float extent, const float nearZ, const float farZ) noexcept
{
    setIdentity(out);
    out[0] = 1.0f / extent;
    out[5] = 1.0f / extent;
    out[10] = 1.0f / (nearZ - farZ);
    out[14] = nearZ / (nearZ - farZ);
}

bool hasProjectionSpan(const float minValue, const float maxValue) noexcept
{
    return std::isfinite(minValue) &&
        std::isfinite(maxValue) &&
        std::fabs(maxValue - minValue) > 0.000001f;
}

bool makeOrthoOffCenter(
    float out[16],
    const float left,
    const float right,
    const float bottom,
    const float top,
    const float nearZ,
    const float farZ) noexcept
{
    if (!hasProjectionSpan(left, right) ||
        !hasProjectionSpan(bottom, top) ||
        !hasProjectionSpan(nearZ, farZ))
    {
        return false;
    }

    setIdentity(out);
    out[0] = 2.0f / (right - left);
    out[5] = 2.0f / (top - bottom);
    out[10] = 1.0f / (nearZ - farZ);
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[14] = nearZ / (nearZ - farZ);
    return isFinite16(out);
}

bool transformPoint(const float matrix[16], const float in[3], float out[3]) noexcept
{
    out[0] = matrix[0] * in[0] + matrix[4] * in[1] + matrix[8] * in[2] + matrix[12];
    out[1] = matrix[1] * in[0] + matrix[5] * in[1] + matrix[9] * in[2] + matrix[13];
    out[2] = matrix[2] * in[0] + matrix[6] * in[1] + matrix[10] * in[2] + matrix[14];
    return isFinite3(out);
}

void lerp3(const float a[3], const float b[3], const float t, float out[3]) noexcept
{
    out[0] = a[0] + (b[0] - a[0]) * t;
    out[1] = a[1] + (b[1] - a[1]) * t;
    out[2] = a[2] + (b[2] - a[2]) * t;
}

Vec3 averageCorners(const float corners[8][3]) noexcept
{
    Vec3 result;
    for (int corner = 0; corner < 8; ++corner)
    {
        result.x += corners[corner][0];
        result.y += corners[corner][1];
        result.z += corners[corner][2];
    }
    return multiply(result, 1.0f / 8.0f);
}

bool extractLightBoxWorldCorners(
    const float lightView[16],
    const float lightMin[3],
    const float lightMax[3],
    float outCorners[8][3]) noexcept
{
    if (!isFinite16(lightView) ||
        !isFinite3(lightMin) ||
        !isFinite3(lightMax) ||
        outCorners == nullptr)
    {
        return false;
    }

    float inverseView[16] = {};
    if (!invertColumnMajor4x4(lightView, inverseView))
    {
        return false;
    }

    const float lightCorners[8][3] = {
        {lightMin[0], lightMin[1], lightMin[2]},
        {lightMax[0], lightMin[1], lightMin[2]},
        {lightMax[0], lightMax[1], lightMin[2]},
        {lightMin[0], lightMax[1], lightMin[2]},
        {lightMin[0], lightMin[1], lightMax[2]},
        {lightMax[0], lightMin[1], lightMax[2]},
        {lightMax[0], lightMax[1], lightMax[2]},
        {lightMin[0], lightMax[1], lightMax[2]},
    };

    for (int corner = 0; corner < 8; ++corner)
    {
        if (!transformPoint(inverseView, lightCorners[corner], outCorners[corner]))
        {
            return false;
        }
    }

    return true;
}

void copyCorners(const float source[8][3], float destination[8][3]) noexcept
{
    for (int corner = 0; corner < 8; ++corner)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            destination[corner][axis] = source[corner][axis];
        }
    }
}

bool transformClipToWorld(
    const float inverseViewProjection[16],
    const float x,
    const float y,
    const float z,
    float out[3]) noexcept
{
    const float worldX =
        inverseViewProjection[0] * x +
        inverseViewProjection[4] * y +
        inverseViewProjection[8] * z +
        inverseViewProjection[12];
    const float worldY =
        inverseViewProjection[1] * x +
        inverseViewProjection[5] * y +
        inverseViewProjection[9] * z +
        inverseViewProjection[13];
    const float worldZ =
        inverseViewProjection[2] * x +
        inverseViewProjection[6] * y +
        inverseViewProjection[10] * z +
        inverseViewProjection[14];
    const float worldW =
        inverseViewProjection[3] * x +
        inverseViewProjection[7] * y +
        inverseViewProjection[11] * z +
        inverseViewProjection[15];

    if (!std::isfinite(worldW) || std::fabs(worldW) <= 0.000001f)
    {
        return false;
    }

    const float invW = 1.0f / worldW;
    out[0] = worldX * invW;
    out[1] = worldY * invW;
    out[2] = worldZ * invW;
    return isFinite3(out);
}
} // namespace

bool isValidDirectionalShadowDesc(const DirectionalShadowDesc& desc) noexcept
{
    if (!desc.enabled)
    {
        return true;
    }

    return desc.mapResolution >= kMinimumShadowMapResolution &&
        desc.mapResolution <= kMaximumShadowMapResolution &&
        isFinite3(desc.centerWorld) &&
        std::isfinite(desc.extentMeters) &&
        desc.extentMeters > 0.0f &&
        desc.extentMeters <= 10000.0f &&
        std::isfinite(desc.depthBias) &&
        clampShadowDepthBias(desc.depthBias) == desc.depthBias &&
        std::isfinite(desc.slopeBias) &&
        clampShadowSlopeBias(desc.slopeBias) == desc.slopeBias &&
        std::isfinite(desc.normalBias) &&
        desc.normalBias >= 0.0f &&
        desc.normalBias <= 100.0f &&
        std::isfinite(desc.strength) &&
        desc.strength >= 0.0f &&
        desc.strength <= 1.0f &&
        std::isfinite(desc.cascadeBlendFraction) &&
        std::isfinite(desc.cascadeSplitLambda) &&
        desc.cascadeSplitLambda >= 0.0f &&
        desc.cascadeSplitLambda <= 1.0f &&
        std::isfinite(desc.cascadeShadowDistanceMeters) &&
        std::isfinite(desc.cascadeCameraNearMeters) &&
        std::isfinite(desc.cascadeCameraFarMeters) &&
        desc.cascadeCameraNearMeters > 0.0f &&
        desc.cascadeCameraFarMeters > desc.cascadeCameraNearMeters &&
        desc.cascadeShadowDistanceMeters > desc.cascadeCameraNearMeters;
}

std::uint32_t clampShadowMapResolution(const std::uint32_t resolution) noexcept
{
    return std::max(kMinimumShadowMapResolution, std::min(kMaximumShadowMapResolution, resolution));
}

std::uint32_t clampShadowCascadeCount(const std::uint32_t cascadeCount) noexcept
{
    return std::max(1U, std::min(kMaxDirectionalShadowCascades, cascadeCount));
}

float clampCascadeBlendFraction(const float blendFraction) noexcept
{
    if (!std::isfinite(blendFraction))
    {
        return 0.0f;
    }
    return std::max(0.0f, std::min(0.5f, blendFraction));
}

float clampShadowDepthBias(const float depthBias) noexcept
{
    if (!std::isfinite(depthBias))
    {
        return 0.0f;
    }
    return std::max(0.0f, std::min(0.1f, depthBias));
}

float clampShadowSlopeBias(const float slopeBias) noexcept
{
    if (!std::isfinite(slopeBias))
    {
        return 0.0f;
    }
    return std::max(0.0f, std::min(0.1f, slopeBias));
}

std::uint32_t shadowFilterModeToTapCount(const ShadowFilterMode filterMode) noexcept
{
    switch (filterMode)
    {
    case ShadowFilterMode::Pcf2x2:
        return 4;
    case ShadowFilterMode::Pcf3x3:
        return 9;
    case ShadowFilterMode::Nearest:
    default:
        return 1;
    }
}

ShadowResourceConfig shadowResourceConfigFromDesc(const DirectionalShadowDesc& desc) noexcept
{
    ShadowResourceConfig config;
    config.enabled = desc.enabled;
    if (!desc.enabled)
    {
        return config;
    }

    config.mapResolution = clampShadowMapResolution(desc.mapResolution);
    config.cascadeCount = clampShadowCascadeCount(desc.cascadeCount);
    return config;
}

ShadowResourceReconfigurePlan planShadowResourceReconfiguration(
    const DirectionalShadowDesc& requested,
    const std::uint32_t activeResolution,
    const std::uint32_t activeCascadeCount,
    const bool activeResourcesValid) noexcept
{
    ShadowResourceReconfigurePlan plan;
    plan.requested = shadowResourceConfigFromDesc(requested);
    plan.valid = isValidDirectionalShadowDesc(requested);
    if (!plan.valid)
    {
        plan.action = ShadowResourceReconfigureAction::Release;
        return plan;
    }

    if (!plan.requested.enabled)
    {
        plan.action = activeCascadeCount > 0 ? ShadowResourceReconfigureAction::Release :
            ShadowResourceReconfigureAction::None;
        return plan;
    }

    if (!activeResourcesValid ||
        activeResolution != plan.requested.mapResolution ||
        activeCascadeCount != plan.requested.cascadeCount)
    {
        plan.action = ShadowResourceReconfigureAction::Recreate;
    }

    return plan;
}

bool computeCascadeBlendBand(
    const float previousSplitFarMeters,
    const float currentSplitFarMeters,
    const float blendFraction,
    float& outBandStartMeters,
    float& outBandSizeMeters) noexcept
{
    outBandStartMeters = currentSplitFarMeters;
    outBandSizeMeters = 0.0f;
    if (!std::isfinite(previousSplitFarMeters) ||
        !std::isfinite(currentSplitFarMeters) ||
        currentSplitFarMeters <= previousSplitFarMeters)
    {
        return false;
    }

    const float clampedFraction = clampCascadeBlendFraction(blendFraction);
    if (clampedFraction <= 0.0f)
    {
        return true;
    }

    const float span = currentSplitFarMeters - previousSplitFarMeters;
    outBandSizeMeters = span * clampedFraction;
    outBandStartMeters = currentSplitFarMeters - outBandSizeMeters;
    return std::isfinite(outBandStartMeters) &&
        std::isfinite(outBandSizeMeters) &&
        outBandStartMeters >= previousSplitFarMeters &&
        outBandStartMeters <= currentSplitFarMeters;
}

bool invertColumnMajor4x4(const float matrix[16], float outInverse[16]) noexcept
{
    if (matrix == nullptr || outInverse == nullptr || !isFinite16(matrix))
    {
        return false;
    }

    float inv[16] = {};
    inv[0] = matrix[5] * matrix[10] * matrix[15] -
        matrix[5] * matrix[11] * matrix[14] -
        matrix[9] * matrix[6] * matrix[15] +
        matrix[9] * matrix[7] * matrix[14] +
        matrix[13] * matrix[6] * matrix[11] -
        matrix[13] * matrix[7] * matrix[10];
    inv[4] = -matrix[4] * matrix[10] * matrix[15] +
        matrix[4] * matrix[11] * matrix[14] +
        matrix[8] * matrix[6] * matrix[15] -
        matrix[8] * matrix[7] * matrix[14] -
        matrix[12] * matrix[6] * matrix[11] +
        matrix[12] * matrix[7] * matrix[10];
    inv[8] = matrix[4] * matrix[9] * matrix[15] -
        matrix[4] * matrix[11] * matrix[13] -
        matrix[8] * matrix[5] * matrix[15] +
        matrix[8] * matrix[7] * matrix[13] +
        matrix[12] * matrix[5] * matrix[11] -
        matrix[12] * matrix[7] * matrix[9];
    inv[12] = -matrix[4] * matrix[9] * matrix[14] +
        matrix[4] * matrix[10] * matrix[13] +
        matrix[8] * matrix[5] * matrix[14] -
        matrix[8] * matrix[6] * matrix[13] -
        matrix[12] * matrix[5] * matrix[10] +
        matrix[12] * matrix[6] * matrix[9];
    inv[1] = -matrix[1] * matrix[10] * matrix[15] +
        matrix[1] * matrix[11] * matrix[14] +
        matrix[9] * matrix[2] * matrix[15] -
        matrix[9] * matrix[3] * matrix[14] -
        matrix[13] * matrix[2] * matrix[11] +
        matrix[13] * matrix[3] * matrix[10];
    inv[5] = matrix[0] * matrix[10] * matrix[15] -
        matrix[0] * matrix[11] * matrix[14] -
        matrix[8] * matrix[2] * matrix[15] +
        matrix[8] * matrix[3] * matrix[14] +
        matrix[12] * matrix[2] * matrix[11] -
        matrix[12] * matrix[3] * matrix[10];
    inv[9] = -matrix[0] * matrix[9] * matrix[15] +
        matrix[0] * matrix[11] * matrix[13] +
        matrix[8] * matrix[1] * matrix[15] -
        matrix[8] * matrix[3] * matrix[13] -
        matrix[12] * matrix[1] * matrix[11] +
        matrix[12] * matrix[3] * matrix[9];
    inv[13] = matrix[0] * matrix[9] * matrix[14] -
        matrix[0] * matrix[10] * matrix[13] -
        matrix[8] * matrix[1] * matrix[14] +
        matrix[8] * matrix[2] * matrix[13] +
        matrix[12] * matrix[1] * matrix[10] -
        matrix[12] * matrix[2] * matrix[9];
    inv[2] = matrix[1] * matrix[6] * matrix[15] -
        matrix[1] * matrix[7] * matrix[14] -
        matrix[5] * matrix[2] * matrix[15] +
        matrix[5] * matrix[3] * matrix[14] +
        matrix[13] * matrix[2] * matrix[7] -
        matrix[13] * matrix[3] * matrix[6];
    inv[6] = -matrix[0] * matrix[6] * matrix[15] +
        matrix[0] * matrix[7] * matrix[14] +
        matrix[4] * matrix[2] * matrix[15] -
        matrix[4] * matrix[3] * matrix[14] -
        matrix[12] * matrix[2] * matrix[7] +
        matrix[12] * matrix[3] * matrix[6];
    inv[10] = matrix[0] * matrix[5] * matrix[15] -
        matrix[0] * matrix[7] * matrix[13] -
        matrix[4] * matrix[1] * matrix[15] +
        matrix[4] * matrix[3] * matrix[13] +
        matrix[12] * matrix[1] * matrix[7] -
        matrix[12] * matrix[3] * matrix[5];
    inv[14] = -matrix[0] * matrix[5] * matrix[14] +
        matrix[0] * matrix[6] * matrix[13] +
        matrix[4] * matrix[1] * matrix[14] -
        matrix[4] * matrix[2] * matrix[13] -
        matrix[12] * matrix[1] * matrix[6] +
        matrix[12] * matrix[2] * matrix[5];
    inv[3] = -matrix[1] * matrix[6] * matrix[11] +
        matrix[1] * matrix[7] * matrix[10] +
        matrix[5] * matrix[2] * matrix[11] -
        matrix[5] * matrix[3] * matrix[10] -
        matrix[9] * matrix[2] * matrix[7] +
        matrix[9] * matrix[3] * matrix[6];
    inv[7] = matrix[0] * matrix[6] * matrix[11] -
        matrix[0] * matrix[7] * matrix[10] -
        matrix[4] * matrix[2] * matrix[11] +
        matrix[4] * matrix[3] * matrix[10] +
        matrix[8] * matrix[2] * matrix[7] -
        matrix[8] * matrix[3] * matrix[6];
    inv[11] = -matrix[0] * matrix[5] * matrix[11] +
        matrix[0] * matrix[7] * matrix[9] +
        matrix[4] * matrix[1] * matrix[11] -
        matrix[4] * matrix[3] * matrix[9] -
        matrix[8] * matrix[1] * matrix[7] +
        matrix[8] * matrix[3] * matrix[5];
    inv[15] = matrix[0] * matrix[5] * matrix[10] -
        matrix[0] * matrix[6] * matrix[9] -
        matrix[4] * matrix[1] * matrix[10] +
        matrix[4] * matrix[2] * matrix[9] +
        matrix[8] * matrix[1] * matrix[6] -
        matrix[8] * matrix[2] * matrix[5];

    const float determinant =
        matrix[0] * inv[0] +
        matrix[1] * inv[4] +
        matrix[2] * inv[8] +
        matrix[3] * inv[12];
    if (!std::isfinite(determinant) || std::fabs(determinant) <= 0.000001f)
    {
        return false;
    }

    const float invDeterminant = 1.0f / determinant;
    for (int index = 0; index < 16; ++index)
    {
        outInverse[index] = inv[index] * invDeterminant;
    }
    return isFinite16(outInverse);
}

bool extractWorldFrustumCorners(const float viewProjection[16], float outCorners[8][3]) noexcept
{
    if (viewProjection == nullptr || outCorners == nullptr)
    {
        return false;
    }

    float inverse[16] = {};
    if (!invertColumnMajor4x4(viewProjection, inverse))
    {
        return false;
    }

    constexpr float kClipCorners[8][3] = {
        {-1.0f, -1.0f, 0.0f},
        {1.0f, -1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f},
        {-1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f},
    };

    for (int index = 0; index < 8; ++index)
    {
        if (!transformClipToWorld(
                inverse,
                kClipCorners[index][0],
                kClipCorners[index][1],
                kClipCorners[index][2],
                outCorners[index]))
        {
            return false;
        }
    }

    return true;
}

bool computeDirectionalShadowCascadeRanges(
    const DirectionalShadowDesc& shadow,
    DirectionalShadowCascadeRange outRanges[kMaxDirectionalShadowCascades],
    std::uint32_t& outCascadeCount) noexcept
{
    outCascadeCount = 0;
    if (outRanges == nullptr || !isValidDirectionalShadowDesc(shadow))
    {
        return false;
    }

    const float nearDistance = shadow.cascadeCameraNearMeters;
    const float effectiveFar = std::min(shadow.cascadeCameraFarMeters, shadow.cascadeShadowDistanceMeters);
    if (nearDistance <= 0.0f || effectiveFar <= nearDistance + kMinimumCascadeDistance)
    {
        return false;
    }

    const std::uint32_t cascadeCount = clampShadowCascadeCount(shadow.cascadeCount);
    const float lambda = std::max(0.0f, std::min(1.0f, shadow.cascadeSplitLambda));
    float previousFar = nearDistance;
    for (std::uint32_t index = 0; index < cascadeCount; ++index)
    {
        const float fraction = static_cast<float>(index + 1U) / static_cast<float>(cascadeCount);
        const float uniformFar = nearDistance + (effectiveFar - nearDistance) * fraction;
        const float logFar = nearDistance * std::pow(effectiveFar / nearDistance, fraction);
        float splitFar = uniformFar;
        if (shadow.cascadeSplitMode == ShadowCascadeSplitMode::Logarithmic)
        {
            splitFar = logFar;
        }
        else if (shadow.cascadeSplitMode == ShadowCascadeSplitMode::Practical)
        {
            splitFar = uniformFar * (1.0f - lambda) + logFar * lambda;
        }

        if (index + 1U == cascadeCount)
        {
            splitFar = effectiveFar;
        }
        splitFar = std::max(splitFar, previousFar + kMinimumCascadeDistance);
        splitFar = std::min(splitFar, effectiveFar);
        if (!std::isfinite(splitFar) || splitFar <= previousFar)
        {
            return false;
        }

        DirectionalShadowCascadeRange& range = outRanges[index];
        range.splitIndex = index;
        range.nearDistanceMeters = previousFar;
        range.farDistanceMeters = splitFar;
        range.normalizedSplitDepth = (splitFar - nearDistance) / (effectiveFar - nearDistance);
        previousFar = splitFar;
    }

    outCascadeCount = cascadeCount;
    return true;
}

bool computeCameraFrustumSliceCorners(
    const RenderViewDesc& cameraView,
    const DirectionalShadowCascadeRange& range,
    const DirectionalShadowDesc& shadow,
    float outCorners[8][3]) noexcept
{
    if (outCorners == nullptr ||
        !isFinite16(cameraView.view) ||
        !isFinite16(cameraView.projection) ||
        !std::isfinite(range.nearDistanceMeters) ||
        !std::isfinite(range.farDistanceMeters) ||
        range.farDistanceMeters <= range.nearDistanceMeters ||
        shadow.cascadeCameraFarMeters <= shadow.cascadeCameraNearMeters)
    {
        return false;
    }

    float viewProjection[16] = {};
    multiplyColumnMajor4x4(cameraView.projection, cameraView.view, viewProjection);

    float fullCorners[8][3] = {};
    if (!extractWorldFrustumCorners(viewProjection, fullCorners))
    {
        return false;
    }

    const float invDepth = 1.0f / (shadow.cascadeCameraFarMeters - shadow.cascadeCameraNearMeters);
    const float nearT = std::max(
        0.0f,
        std::min(1.0f, (range.nearDistanceMeters - shadow.cascadeCameraNearMeters) * invDepth));
    const float farT = std::max(
        0.0f,
        std::min(1.0f, (range.farDistanceMeters - shadow.cascadeCameraNearMeters) * invDepth));

    for (int corner = 0; corner < 4; ++corner)
    {
        lerp3(fullCorners[corner], fullCorners[corner + 4], nearT, outCorners[corner]);
        lerp3(fullCorners[corner], fullCorners[corner + 4], farT, outCorners[corner + 4]);
        if (!isFinite3(outCorners[corner]) || !isFinite3(outCorners[corner + 4]))
        {
            return false;
        }
    }

    return true;
}

bool computeDirectionalShadowTexelSize(
    const float minValue,
    const float maxValue,
    const std::uint32_t shadowMapResolution,
    float& outTexelSize) noexcept
{
    outTexelSize = 0.0f;
    if (!hasProjectionSpan(minValue, maxValue))
    {
        return false;
    }

    const std::uint32_t resolution = clampShadowMapResolution(shadowMapResolution);
    outTexelSize = std::fabs(maxValue - minValue) / static_cast<float>(resolution);
    return std::isfinite(outTexelSize) && outTexelSize > 0.0f;
}

bool snapDirectionalShadowProjectionBounds(
    const float inMin[3],
    const float inMax[3],
    const std::uint32_t shadowMapResolution,
    const bool stable,
    float outMin[3],
    float outMax[3],
    float outTexelSize[2],
    float outUnsnappedCenter[2],
    float outSnappedCenter[2],
    float outSnapOffset[2]) noexcept
{
    if (inMin == nullptr ||
        inMax == nullptr ||
        outMin == nullptr ||
        outMax == nullptr ||
        outTexelSize == nullptr ||
        outUnsnappedCenter == nullptr ||
        outSnappedCenter == nullptr ||
        outSnapOffset == nullptr ||
        !isFinite3(inMin) ||
        !isFinite3(inMax))
    {
        return false;
    }

    for (int axis = 0; axis < 3; ++axis)
    {
        outMin[axis] = inMin[axis];
        outMax[axis] = inMax[axis];
    }

    for (int axis = 0; axis < 2; ++axis)
    {
        float texelSize = 0.0f;
        if (!computeDirectionalShadowTexelSize(inMin[axis], inMax[axis], shadowMapResolution, texelSize))
        {
            return false;
        }

        const float center = (inMin[axis] + inMax[axis]) * 0.5f;
        const float halfExtent = (inMax[axis] - inMin[axis]) * 0.5f;
        float snappedCenter = center;
        float snappedHalfExtent = halfExtent;
        if (stable)
        {
            snappedCenter = std::floor(center / texelSize + 0.5f) * texelSize;
            snappedHalfExtent = halfExtent + texelSize;
        }

        if (!std::isfinite(snappedCenter) || !std::isfinite(snappedHalfExtent) || snappedHalfExtent <= 0.0f)
        {
            return false;
        }

        outMin[axis] = snappedCenter - snappedHalfExtent;
        outMax[axis] = snappedCenter + snappedHalfExtent;
        outTexelSize[axis] = texelSize;
        outUnsnappedCenter[axis] = center;
        outSnappedCenter[axis] = snappedCenter;
        outSnapOffset[axis] = snappedCenter - center;
    }

    return hasProjectionSpan(outMin[0], outMax[0]) &&
        hasProjectionSpan(outMin[1], outMax[1]) &&
        hasProjectionSpan(outMin[2], outMax[2]);
}

bool buildDirectionalShadowMatrices(
    const DirectionalLightDesc& light,
    const DirectionalShadowDesc& shadow,
    DirectionalShadowMatrices& outMatrices) noexcept
{
    if (!isValidDirectionalShadowDesc(shadow) || !isFinite3(light.directionWorld))
    {
        return false;
    }

    const Vec3 lightDirection = normalize(fromArray(light.directionWorld));
    if (dot(lightDirection, lightDirection) <= 0.000001f)
    {
        return false;
    }

    const Vec3 center = fromArray(shadow.centerWorld);
    const Vec3 eye = add(center, multiply(lightDirection, shadow.extentMeters));
    const Vec3 up = std::fabs(lightDirection.y) > 0.95f ? Vec3{0.0f, 0.0f, 1.0f} : Vec3{0.0f, 1.0f, 0.0f};

    makeLookAt(outMatrices.view, eye, center, up);
    makeOrtho(outMatrices.projection, shadow.extentMeters, 0.0f, shadow.extentMeters * 2.0f);
    multiplyColumnMajor4x4(outMatrices.projection, outMatrices.view, outMatrices.viewProjection);
    return true;
}

bool buildDirectionalShadowSplit(
    const DirectionalLightDesc& light,
    const DirectionalShadowDesc& shadow,
    DirectionalShadowSplit& outSplit) noexcept
{
    DirectionalShadowMatrices matrices;
    if (!buildDirectionalShadowMatrices(light, shadow, matrices))
    {
        return false;
    }

    float inverse[16] = {};
    float corners[8][3] = {};
    if (!invertColumnMajor4x4(matrices.viewProjection, inverse) ||
        !extractWorldFrustumCorners(matrices.viewProjection, corners))
    {
        return false;
    }

    outSplit.matrices = matrices;
    for (int index = 0; index < 16; ++index)
    {
        outSplit.inverseViewProjection[index] = inverse[index];
    }
    for (int corner = 0; corner < 8; ++corner)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            outSplit.worldCorners[corner][axis] = corners[corner][axis];
        }
    }
    outSplit.extentMeters = shadow.extentMeters;
    outSplit.nearDistanceMeters = 0.0f;
    outSplit.farDistanceMeters = shadow.extentMeters * 2.0f;
    return true;
}

bool buildDirectionalShadowCascadeSet(
    const DirectionalLightDesc& light,
    const DirectionalShadowDesc& shadow,
    const RenderViewDesc& cameraView,
    DirectionalShadowCascadeSet& outCascadeSet) noexcept
{
    outCascadeSet = {};
    if (!isValidDirectionalShadowDesc(shadow) ||
        !isFinite3(light.directionWorld) ||
        !isFinite16(cameraView.view) ||
        !isFinite16(cameraView.projection))
    {
        return false;
    }

    DirectionalShadowCascadeRange ranges[kMaxDirectionalShadowCascades] = {};
    std::uint32_t cascadeCount = 0;
    if (!computeDirectionalShadowCascadeRanges(shadow, ranges, cascadeCount))
    {
        return false;
    }

    const Vec3 lightDirection = normalize(fromArray(light.directionWorld));
    if (dot(lightDirection, lightDirection) <= 0.000001f)
    {
        return false;
    }
    const Vec3 up = std::fabs(lightDirection.y) > 0.95f ? Vec3{0.0f, 0.0f, 1.0f} : Vec3{0.0f, 1.0f, 0.0f};

    for (std::uint32_t index = 0; index < cascadeCount; ++index)
    {
        float sliceCorners[8][3] = {};
        if (!computeCameraFrustumSliceCorners(cameraView, ranges[index], shadow, sliceCorners))
        {
            return false;
        }

        DirectionalShadowSplit& split = outCascadeSet.splits[index];
        split.splitIndex = index;
        split.nearDistanceMeters = ranges[index].nearDistanceMeters;
        split.farDistanceMeters = ranges[index].farDistanceMeters;
        split.normalizedSplitDepth = ranges[index].normalizedSplitDepth;
        split.extentMeters = shadow.extentMeters;
        copyCorners(sliceCorners, split.cameraSliceCorners);

        const Vec3 center = averageCorners(sliceCorners);
        const float eyeDistance = std::max(shadow.extentMeters, split.farDistanceMeters - split.nearDistanceMeters);
        const Vec3 eye = add(center, multiply(lightDirection, eyeDistance));
        makeLookAt(split.matrices.view, eye, center, up);

        float lightMin[3] = {};
        float lightMax[3] = {};
        for (int corner = 0; corner < 8; ++corner)
        {
            float lightSpace[3] = {};
            if (!transformPoint(split.matrices.view, sliceCorners[corner], lightSpace))
            {
                return false;
            }

            if (corner == 0)
            {
                lightMin[0] = lightMax[0] = lightSpace[0];
                lightMin[1] = lightMax[1] = lightSpace[1];
                lightMin[2] = lightMax[2] = lightSpace[2];
            }
            else
            {
                for (int axis = 0; axis < 3; ++axis)
                {
                    lightMin[axis] = std::min(lightMin[axis], lightSpace[axis]);
                    lightMax[axis] = std::max(lightMax[axis], lightSpace[axis]);
                }
            }
        }

        constexpr float kPadding = 0.25f;
        float paddedMin[3] = {};
        float paddedMax[3] = {};
        for (int axis = 0; axis < 3; ++axis)
        {
            paddedMin[axis] = lightMin[axis] - kPadding;
            paddedMax[axis] = lightMax[axis] + kPadding;
        }

        if (!snapDirectionalShadowProjectionBounds(
                paddedMin,
                paddedMax,
                shadow.mapResolution,
                shadow.stableCascadeProjection,
                split.orthoMin,
                split.orthoMax,
                split.texelSize,
                split.unsnappedCenter,
                split.snappedCenter,
                split.snapOffset))
        {
            return false;
        }

        if (!makeOrthoOffCenter(
            split.matrices.projection,
            split.orthoMin[0],
            split.orthoMax[0],
            split.orthoMin[1],
            split.orthoMax[1],
            split.orthoMin[2],
            split.orthoMax[2]))
        {
            return false;
        }
        multiplyColumnMajor4x4(split.matrices.projection, split.matrices.view, split.matrices.viewProjection);
        if (!invertColumnMajor4x4(split.matrices.viewProjection, split.inverseViewProjection))
        {
            for (float& value : split.inverseViewProjection)
            {
                value = 0.0f;
            }
        }
        if (!extractLightBoxWorldCorners(
                split.matrices.view,
                split.orthoMin,
                split.orthoMax,
                split.worldCorners))
        {
            return false;
        }
    }

    outCascadeSet.cascadeCount = cascadeCount;
    return true;
}

std::uint32_t selectDirectionalShadowCascade(
    const float viewDepthMeters,
    const DirectionalShadowCascadeRange ranges[kMaxDirectionalShadowCascades],
    const std::uint32_t cascadeCount) noexcept
{
    if (!std::isfinite(viewDepthMeters) || viewDepthMeters < 0.0f || ranges == nullptr || cascadeCount == 0)
    {
        return UINT32_MAX;
    }

    const std::uint32_t clampedCount = std::min(cascadeCount, kMaxDirectionalShadowCascades);
    for (std::uint32_t index = 0; index < clampedCount; ++index)
    {
        if (!std::isfinite(ranges[index].farDistanceMeters))
        {
            return UINT32_MAX;
        }
        if (viewDepthMeters <= ranges[index].farDistanceMeters)
        {
            return index;
        }
    }

    return UINT32_MAX;
}
} // namespace full_renderer::scene
