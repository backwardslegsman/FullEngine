#include "engine/assets/LoadedAnimationSampler.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace full_engine
{
namespace
{
constexpr float kSmallTimeEpsilon = 0.000001f;

void copyMatrix(const float source[16], float* const destination) noexcept
{
    for (int index = 0; index < 16; ++index)
    {
        destination[index] = source[index];
    }
}

void multiplyMatrix(
    const float left[16],
    const float right[16],
    float out[16]) noexcept
{
    float result[16] = {};
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            result[column * 4 + row] =
                left[0 * 4 + row] * right[column * 4 + 0] +
                left[1 * 4 + row] * right[column * 4 + 1] +
                left[2 * 4 + row] * right[column * 4 + 2] +
                left[3 * 4 + row] * right[column * 4 + 3];
        }
    }
    copyMatrix(result, out);
}

float applyPlayback(
    const float timeSeconds,
    const float durationSeconds,
    const LoadedAnimationPlaybackMode playback) noexcept
{
    if (playback == LoadedAnimationPlaybackMode::Loop)
    {
        float wrapped = std::fmod(timeSeconds, durationSeconds);
        if (wrapped < 0.0f)
        {
            wrapped += durationSeconds;
        }
        return wrapped;
    }

    return std::max(0.0f, std::min(timeSeconds, durationSeconds));
}

void lerp3(
    const float a[3],
    const float b[3],
    const float t,
    float out[3]) noexcept
{
    for (int axis = 0; axis < 3; ++axis)
    {
        out[axis] = a[axis] + (b[axis] - a[axis]) * t;
    }
}

void normalizeQuaternion(float value[4]) noexcept
{
    const float lengthSquared =
        value[0] * value[0] +
        value[1] * value[1] +
        value[2] * value[2] +
        value[3] * value[3];
    if (lengthSquared <= 0.0f || !std::isfinite(lengthSquared))
    {
        value[0] = 0.0f;
        value[1] = 0.0f;
        value[2] = 0.0f;
        value[3] = 1.0f;
        return;
    }

    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    for (int channel = 0; channel < 4; ++channel)
    {
        value[channel] *= inverseLength;
    }
}

void sampleTranslation(
    const std::vector<LoadedAnimationTranslationKey>& keys,
    const float timeSeconds,
    float out[3]) noexcept
{
    if (timeSeconds <= keys.front().timeSeconds)
    {
        lerp3(keys.front().value, keys.front().value, 0.0f, out);
        return;
    }
    if (timeSeconds >= keys.back().timeSeconds)
    {
        lerp3(keys.back().value, keys.back().value, 0.0f, out);
        return;
    }

    for (std::size_t index = 1; index < keys.size(); ++index)
    {
        const LoadedAnimationTranslationKey& right = keys[index];
        if (timeSeconds <= right.timeSeconds)
        {
            const LoadedAnimationTranslationKey& left = keys[index - 1];
            const float span = right.timeSeconds - left.timeSeconds;
            const float t = span <= kSmallTimeEpsilon ? 1.0f : (timeSeconds - left.timeSeconds) / span;
            lerp3(left.value, right.value, t, out);
            return;
        }
    }

    lerp3(keys.back().value, keys.back().value, 0.0f, out);
}

void sampleScale(
    const std::vector<LoadedAnimationScaleKey>& keys,
    const float timeSeconds,
    float out[3]) noexcept
{
    if (timeSeconds <= keys.front().timeSeconds)
    {
        lerp3(keys.front().value, keys.front().value, 0.0f, out);
        return;
    }
    if (timeSeconds >= keys.back().timeSeconds)
    {
        lerp3(keys.back().value, keys.back().value, 0.0f, out);
        return;
    }

    for (std::size_t index = 1; index < keys.size(); ++index)
    {
        const LoadedAnimationScaleKey& right = keys[index];
        if (timeSeconds <= right.timeSeconds)
        {
            const LoadedAnimationScaleKey& left = keys[index - 1];
            const float span = right.timeSeconds - left.timeSeconds;
            const float t = span <= kSmallTimeEpsilon ? 1.0f : (timeSeconds - left.timeSeconds) / span;
            lerp3(left.value, right.value, t, out);
            return;
        }
    }

    lerp3(keys.back().value, keys.back().value, 0.0f, out);
}

void sampleRotation(
    const std::vector<LoadedAnimationRotationKey>& keys,
    const float timeSeconds,
    float out[4]) noexcept
{
    const auto copyRotation = [](const float source[4], float destination[4]) noexcept
    {
        for (int channel = 0; channel < 4; ++channel)
        {
            destination[channel] = source[channel];
        }
    };

    if (timeSeconds <= keys.front().timeSeconds)
    {
        copyRotation(keys.front().value, out);
        return;
    }
    if (timeSeconds >= keys.back().timeSeconds)
    {
        copyRotation(keys.back().value, out);
        return;
    }

    for (std::size_t index = 1; index < keys.size(); ++index)
    {
        const LoadedAnimationRotationKey& right = keys[index];
        if (timeSeconds <= right.timeSeconds)
        {
            const LoadedAnimationRotationKey& left = keys[index - 1];
            const float span = right.timeSeconds - left.timeSeconds;
            const float t = span <= kSmallTimeEpsilon ? 1.0f : (timeSeconds - left.timeSeconds) / span;

            float adjustedRight[4] = {};
            copyRotation(right.value, adjustedRight);
            const float dot =
                left.value[0] * right.value[0] +
                left.value[1] * right.value[1] +
                left.value[2] * right.value[2] +
                left.value[3] * right.value[3];
            if (dot < 0.0f)
            {
                for (float& channel : adjustedRight)
                {
                    channel = -channel;
                }
            }

            for (int channel = 0; channel < 4; ++channel)
            {
                out[channel] = left.value[channel] + (adjustedRight[channel] - left.value[channel]) * t;
            }
            normalizeQuaternion(out);
            return;
        }
    }

    copyRotation(keys.back().value, out);
}

void makeTrsMatrix(
    const float translation[3],
    const float rotation[4],
    const float scale[3],
    float out[16]) noexcept
{
    const float x = rotation[0];
    const float y = rotation[1];
    const float z = rotation[2];
    const float w = rotation[3];

    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    out[0] = (1.0f - 2.0f * (yy + zz)) * scale[0];
    out[1] = (2.0f * (xy + wz)) * scale[0];
    out[2] = (2.0f * (xz - wy)) * scale[0];
    out[3] = 0.0f;

    out[4] = (2.0f * (xy - wz)) * scale[1];
    out[5] = (1.0f - 2.0f * (xx + zz)) * scale[1];
    out[6] = (2.0f * (yz + wx)) * scale[1];
    out[7] = 0.0f;

    out[8] = (2.0f * (xz + wy)) * scale[2];
    out[9] = (2.0f * (yz - wx)) * scale[2];
    out[10] = (1.0f - 2.0f * (xx + yy)) * scale[2];
    out[11] = 0.0f;

    out[12] = translation[0];
    out[13] = translation[1];
    out[14] = translation[2];
    out[15] = 1.0f;
}

void sampleTrackToMatrix(
    const LoadedAnimationJointTrack& track,
    const float timeSeconds,
    float out[16]) noexcept
{
    float translation[3] = {};
    float rotation[4] = {};
    float scale[3] = {};
    sampleTranslation(track.translations, timeSeconds, translation);
    sampleRotation(track.rotations, timeSeconds, rotation);
    sampleScale(track.scales, timeSeconds, scale);
    makeTrsMatrix(translation, rotation, scale, out);
}
} // namespace

const char* loadedAnimationSampleStatusName(
    const LoadedAnimationSampleStatus status) noexcept
{
    switch (status)
    {
    case LoadedAnimationSampleStatus::Success:
        return "Success";
    case LoadedAnimationSampleStatus::InvalidArgument:
        return "InvalidArgument";
    case LoadedAnimationSampleStatus::SkeletonValidationFailed:
        return "SkeletonValidationFailed";
    case LoadedAnimationSampleStatus::ClipValidationFailed:
        return "ClipValidationFailed";
    case LoadedAnimationSampleStatus::SkeletonMismatch:
        return "SkeletonMismatch";
    case LoadedAnimationSampleStatus::TrackOutOfSkeletonRange:
        return "TrackOutOfSkeletonRange";
    }

    return "Unknown";
}

LoadedAnimationSampleResult sampleLoadedAnimationClip(
    const LoadedSkeletonAsset& skeleton,
    const LoadedAnimationClipAsset& clip,
    const float timeSeconds,
    const LoadedAnimationPlaybackMode playback)
{
    LoadedAnimationSampleResult result;

    if (!std::isfinite(timeSeconds) ||
        (playback != LoadedAnimationPlaybackMode::Clamp &&
            playback != LoadedAnimationPlaybackMode::Loop))
    {
        result.status = LoadedAnimationSampleStatus::InvalidArgument;
        return result;
    }

    if (validateLoadedSkeletonAsset(skeleton) != LoadedAssetPayloadValidationResult::Success)
    {
        result.status = LoadedAnimationSampleStatus::SkeletonValidationFailed;
        return result;
    }

    if (validateLoadedAnimationClipAsset(clip) != LoadedAssetPayloadValidationResult::Success)
    {
        result.status = LoadedAnimationSampleStatus::ClipValidationFailed;
        return result;
    }

    if (clip.skeletonAssetId.value != skeleton.id.value)
    {
        result.status = LoadedAnimationSampleStatus::SkeletonMismatch;
        return result;
    }

    const std::uint32_t jointCount = static_cast<std::uint32_t>(skeleton.joints.size());
    std::vector<const LoadedAnimationJointTrack*> tracksByJoint(jointCount, nullptr);
    for (const LoadedAnimationJointTrack& track : clip.tracks)
    {
        if (track.jointIndex >= jointCount)
        {
            result.status = LoadedAnimationSampleStatus::TrackOutOfSkeletonRange;
            return result;
        }
        tracksByJoint[track.jointIndex] = &track;
    }

    const float sampledTime = applyPlayback(timeSeconds, clip.durationSeconds, playback);

    LoadedAnimationPose pose;
    pose.skeletonAssetId = skeleton.id;
    pose.sampledTimeSeconds = sampledTime;
    pose.jointCount = jointCount;
    pose.localMatrices.assign(static_cast<std::size_t>(jointCount) * 16U, 0.0f);
    pose.modelMatrices.assign(static_cast<std::size_t>(jointCount) * 16U, 0.0f);
    pose.skinningMatrices.assign(static_cast<std::size_t>(jointCount) * 16U, 0.0f);

    for (std::uint32_t jointIndex = 0; jointIndex < jointCount; ++jointIndex)
    {
        float* const local = pose.localMatrices.data() + static_cast<std::size_t>(jointIndex) * 16U;
        const LoadedAnimationJointTrack* const track = tracksByJoint[jointIndex];
        if (track != nullptr)
        {
            sampleTrackToMatrix(*track, sampledTime, local);
        }
        else
        {
            copyMatrix(skeleton.joints[jointIndex].referenceTransform, local);
        }

        float* const model = pose.modelMatrices.data() + static_cast<std::size_t>(jointIndex) * 16U;
        if (skeleton.joints[jointIndex].parentIndex < 0)
        {
            copyMatrix(local, model);
        }
        else
        {
            const std::uint32_t parentIndex = static_cast<std::uint32_t>(skeleton.joints[jointIndex].parentIndex);
            const float* const parentModel =
                pose.modelMatrices.data() + static_cast<std::size_t>(parentIndex) * 16U;
            multiplyMatrix(parentModel, local, model);
        }

        float* const skinning =
            pose.skinningMatrices.data() + static_cast<std::size_t>(jointIndex) * 16U;
        multiplyMatrix(model, skeleton.joints[jointIndex].inverseBindPose, skinning);
    }

    result.status = LoadedAnimationSampleStatus::Success;
    result.pose = std::move(pose);
    return result;
}
} // namespace full_engine
