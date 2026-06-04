#include "engine/assets/LoadedAnimationSampler.hpp"

#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* const message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

bool near(const float a, const float b, const float epsilon = 0.001f) noexcept
{
    return std::fabs(a - b) <= epsilon;
}

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

void identity(float out[16]) noexcept
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

void translationMatrix(const float x, const float y, const float z, float out[16]) noexcept
{
    identity(out);
    out[12] = x;
    out[13] = y;
    out[14] = z;
}

full_engine::LoadedSkeletonJoint joint(const std::int32_t parentIndex, const float referenceX)
{
    full_engine::LoadedSkeletonJoint result;
    result.parentIndex = parentIndex;
    identity(result.inverseBindPose);
    translationMatrix(referenceX, 0.0f, 0.0f, result.referenceTransform);
    return result;
}

full_engine::LoadedSkeletonAsset skeleton()
{
    full_engine::LoadedSkeletonAsset result;
    result.id = asset(100);
    result.joints = {
        joint(-1, 0.0f),
        joint(0, 2.0f)};
    return result;
}

full_engine::LoadedAnimationJointTrack track(
    const std::uint16_t jointIndex,
    const float translationY0,
    const float translationY1)
{
    full_engine::LoadedAnimationJointTrack result;
    result.jointIndex = jointIndex;

    full_engine::LoadedAnimationTranslationKey translation0;
    translation0.timeSeconds = 0.0f;
    translation0.value[1] = translationY0;
    full_engine::LoadedAnimationTranslationKey translation1;
    translation1.timeSeconds = 2.0f;
    translation1.value[1] = translationY1;
    result.translations = {translation0, translation1};

    full_engine::LoadedAnimationRotationKey rotation0;
    rotation0.timeSeconds = 0.0f;
    rotation0.value[0] = 0.0f;
    rotation0.value[1] = 0.0f;
    rotation0.value[2] = 0.0f;
    rotation0.value[3] = 1.0f;
    full_engine::LoadedAnimationRotationKey rotation1 = rotation0;
    rotation1.timeSeconds = 2.0f;
    result.rotations = {rotation0, rotation1};

    full_engine::LoadedAnimationScaleKey scale0;
    scale0.timeSeconds = 0.0f;
    scale0.value[0] = 1.0f;
    scale0.value[1] = 1.0f;
    scale0.value[2] = 1.0f;
    full_engine::LoadedAnimationScaleKey scale1 = scale0;
    scale1.timeSeconds = 2.0f;
    result.scales = {scale0, scale1};
    return result;
}

full_engine::LoadedAnimationJointTrack rotationTrack(const std::uint16_t jointIndex)
{
    full_engine::LoadedAnimationJointTrack result = track(jointIndex, 0.0f, 0.0f);
    result.rotations[1].value[2] = -0.70710677f;
    result.rotations[1].value[3] = -0.70710677f;
    return result;
}

full_engine::LoadedAnimationJointTrack scaleTrack(const std::uint16_t jointIndex)
{
    full_engine::LoadedAnimationJointTrack result = track(jointIndex, 0.0f, 0.0f);
    result.scales[1].value[0] = 3.0f;
    result.scales[1].value[1] = 5.0f;
    result.scales[1].value[2] = 7.0f;
    return result;
}

full_engine::LoadedAnimationClipAsset clip()
{
    full_engine::LoadedAnimationClipAsset result;
    result.id = asset(200);
    result.skeletonAssetId = asset(100);
    result.durationSeconds = 2.0f;
    result.ticksPerSecond = 30.0f;
    result.tracks = {
        track(0, 0.0f, 4.0f),
        track(1, 1.0f, 3.0f)};
    return result;
}

const float* matrixAt(const std::vector<float>& matrices, const std::uint32_t index) noexcept
{
    return matrices.data() + static_cast<std::size_t>(index) * 16U;
}

void testValidSingleAndTwoJointSampling(std::vector<std::string>& failures)
{
    full_engine::LoadedAnimationSampleResult result =
        full_engine::sampleLoadedAnimationClip(
            skeleton(),
            clip(),
            1.0f,
            full_engine::LoadedAnimationPlaybackMode::Clamp);

    expect(result.status == full_engine::LoadedAnimationSampleStatus::Success, "valid sample succeeds", failures);
    expect(result.pose.jointCount == 2, "sample pose has two joints", failures);
    expect(result.pose.localMatrices.size() == 32, "local matrix vector has two matrices", failures);
    expect(result.pose.modelMatrices.size() == 32, "model matrix vector has two matrices", failures);
    expect(result.pose.skinningMatrices.size() == 32, "skinning matrix vector has two matrices", failures);
    expect(near(result.pose.sampledTimeSeconds, 1.0f), "sample time is copied", failures);

    const float* const rootLocal = matrixAt(result.pose.localMatrices, 0);
    const float* const childLocal = matrixAt(result.pose.localMatrices, 1);
    const float* const childModel = matrixAt(result.pose.modelMatrices, 1);
    expect(near(rootLocal[13], 2.0f), "root translation is interpolated", failures);
    expect(near(childLocal[13], 2.0f), "child local translation is interpolated", failures);
    expect(near(childModel[13], 4.0f), "child model composes with parent", failures);
}

void testExactKeyClampAndLoopSampling(std::vector<std::string>& failures)
{
    full_engine::LoadedAnimationSampleResult exact =
        full_engine::sampleLoadedAnimationClip(skeleton(), clip(), 2.0f);
    expect(exact.status == full_engine::LoadedAnimationSampleStatus::Success, "exact end key sample succeeds", failures);
    expect(near(matrixAt(exact.pose.localMatrices, 0)[13], 4.0f), "exact key uses end translation", failures);

    full_engine::LoadedAnimationSampleResult clamped =
        full_engine::sampleLoadedAnimationClip(
            skeleton(),
            clip(),
            4.5f,
            full_engine::LoadedAnimationPlaybackMode::Clamp);
    expect(near(clamped.pose.sampledTimeSeconds, 2.0f), "clamp caps time at duration", failures);
    expect(near(matrixAt(clamped.pose.localMatrices, 0)[13], 4.0f), "clamped sample uses last key", failures);

    full_engine::LoadedAnimationSampleResult looped =
        full_engine::sampleLoadedAnimationClip(
            skeleton(),
            clip(),
            2.5f,
            full_engine::LoadedAnimationPlaybackMode::Loop);
    expect(near(looped.pose.sampledTimeSeconds, 0.5f), "loop wraps time", failures);
    expect(near(matrixAt(looped.pose.localMatrices, 0)[13], 1.0f), "looped sample uses wrapped keys", failures);

    full_engine::LoadedAnimationSampleResult negativeLoop =
        full_engine::sampleLoadedAnimationClip(
            skeleton(),
            clip(),
            -0.5f,
            full_engine::LoadedAnimationPlaybackMode::Loop);
    expect(near(negativeLoop.pose.sampledTimeSeconds, 1.5f), "loop wraps negative time", failures);
}

void testRotationScaleFallbackAndSkinning(std::vector<std::string>& failures)
{
    full_engine::LoadedAnimationClipAsset rotationClip = clip();
    rotationClip.tracks = {rotationTrack(0)};
    full_engine::LoadedAnimationSampleResult rotation =
        full_engine::sampleLoadedAnimationClip(skeleton(), rotationClip, 1.0f);
    expect(rotation.status == full_engine::LoadedAnimationSampleStatus::Success, "rotation sample succeeds", failures);
    const float* const rootLocal = matrixAt(rotation.pose.localMatrices, 0);
    expect(near(rootLocal[0], 0.70710677f), "rotation nlerp applies hemisphere correction", failures);
    expect(near(rootLocal[1], 0.70710677f), "rotation nlerp produces expected sine term", failures);
    expect(near(matrixAt(rotation.pose.localMatrices, 1)[12], 2.0f), "missing child track uses reference transform", failures);

    full_engine::LoadedAnimationClipAsset scaledClip = clip();
    scaledClip.tracks = {scaleTrack(0)};
    full_engine::LoadedSkeletonAsset skel = skeleton();
    translationMatrix(-1.0f, 0.0f, 0.0f, skel.joints[0].inverseBindPose);
    full_engine::LoadedAnimationSampleResult scaled =
        full_engine::sampleLoadedAnimationClip(skel, scaledClip, 1.0f);
    expect(scaled.status == full_engine::LoadedAnimationSampleStatus::Success, "scale sample succeeds", failures);
    const float* const scaledLocal = matrixAt(scaled.pose.localMatrices, 0);
    expect(near(scaledLocal[0], 2.0f), "scale x interpolates", failures);
    expect(near(scaledLocal[5], 3.0f), "scale y interpolates", failures);
    expect(near(scaledLocal[10], 4.0f), "scale z interpolates", failures);
    expect(near(matrixAt(scaled.pose.skinningMatrices, 0)[12], -2.0f), "skinning matrix applies model * inverse bind", failures);
}

void testFailures(std::vector<std::string>& failures)
{
    full_engine::LoadedSkeletonAsset invalidSkeleton = skeleton();
    invalidSkeleton.joints.clear();
    expect(
        full_engine::sampleLoadedAnimationClip(invalidSkeleton, clip(), 0.0f).status ==
            full_engine::LoadedAnimationSampleStatus::SkeletonValidationFailed,
        "invalid skeleton fails sampling",
        failures);

    full_engine::LoadedAnimationClipAsset invalidClip = clip();
    invalidClip.tracks.clear();
    expect(
        full_engine::sampleLoadedAnimationClip(skeleton(), invalidClip, 0.0f).status ==
            full_engine::LoadedAnimationSampleStatus::ClipValidationFailed,
        "invalid clip fails sampling",
        failures);

    full_engine::LoadedAnimationClipAsset mismatched = clip();
    mismatched.skeletonAssetId = asset(999);
    expect(
        full_engine::sampleLoadedAnimationClip(skeleton(), mismatched, 0.0f).status ==
            full_engine::LoadedAnimationSampleStatus::SkeletonMismatch,
        "skeleton mismatch fails sampling",
        failures);

    full_engine::LoadedAnimationClipAsset outOfSkeleton = clip();
    outOfSkeleton.tracks = {track(2, 0.0f, 1.0f)};
    expect(
        full_engine::sampleLoadedAnimationClip(skeleton(), outOfSkeleton, 0.0f).status ==
            full_engine::LoadedAnimationSampleStatus::TrackOutOfSkeletonRange,
        "track beyond concrete skeleton fails sampling",
        failures);

    expect(
        full_engine::sampleLoadedAnimationClip(skeleton(), clip(), std::nanf("")).status ==
            full_engine::LoadedAnimationSampleStatus::InvalidArgument,
        "non-finite sample time fails",
        failures);
}

void testResultNames(std::vector<std::string>& failures)
{
    const full_engine::LoadedAnimationSampleStatus statuses[] = {
        full_engine::LoadedAnimationSampleStatus::Success,
        full_engine::LoadedAnimationSampleStatus::InvalidArgument,
        full_engine::LoadedAnimationSampleStatus::SkeletonValidationFailed,
        full_engine::LoadedAnimationSampleStatus::ClipValidationFailed,
        full_engine::LoadedAnimationSampleStatus::SkeletonMismatch,
        full_engine::LoadedAnimationSampleStatus::TrackOutOfSkeletonRange};

    for (const full_engine::LoadedAnimationSampleStatus status : statuses)
    {
        expect(
            std::string(full_engine::loadedAnimationSampleStatusName(status)) != "Unknown",
            "animation sample status has stable name",
            failures);
    }
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testValidSingleAndTwoJointSampling(failures);
    testExactKeyClampAndLoopSampling(failures);
    testRotationScaleFallbackAndSkinning(failures);
    testFailures(failures);
    testResultNames(failures);

    if (!failures.empty())
    {
        for (const std::string& failure : failures)
        {
            std::cerr << failure << '\n';
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
