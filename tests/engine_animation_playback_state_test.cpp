#include "engine/renderer_integration/AnimationPlaybackState.hpp"

#include "renderer/animation/AnimationSystem.hpp"

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

full_engine::AnimationPlaybackInstanceId instance(const std::uint64_t value) noexcept
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

full_engine::LoadedSkeletonJoint joint(const std::int32_t parentIndex)
{
    full_engine::LoadedSkeletonJoint result;
    result.parentIndex = parentIndex;
    identity(result.inverseBindPose);
    identity(result.referenceTransform);
    return result;
}

full_engine::LoadedSkeletonAsset skeleton()
{
    full_engine::LoadedSkeletonAsset result;
    result.id = asset(10);
    result.joints = {joint(-1), joint(0)};
    return result;
}

full_engine::LoadedAnimationJointTrack track(const std::uint16_t jointIndex)
{
    full_engine::LoadedAnimationJointTrack result;
    result.jointIndex = jointIndex;

    full_engine::LoadedAnimationTranslationKey translation0;
    translation0.timeSeconds = 0.0f;
    full_engine::LoadedAnimationTranslationKey translation1;
    translation1.timeSeconds = 2.0f;
    translation1.value[1] = 4.0f;
    result.translations = {translation0, translation1};

    full_engine::LoadedAnimationRotationKey rotation0;
    rotation0.timeSeconds = 0.0f;
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

full_engine::LoadedAnimationClipAsset clip()
{
    full_engine::LoadedAnimationClipAsset result;
    result.id = asset(20);
    result.skeletonAssetId = asset(10);
    result.durationSeconds = 2.0f;
    result.ticksPerSecond = 30.0f;
    result.tracks = {track(0), track(1)};
    return result;
}

full_engine::AnimationPlaybackInstanceDesc desc(
    const std::uint64_t id = 1,
    const full_engine::LoadedAnimationPlaybackMode playback = full_engine::LoadedAnimationPlaybackMode::Clamp)
{
    full_engine::AnimationPlaybackInstanceDesc result;
    result.id = instance(id);
    result.skeletonAssetId = asset(10);
    result.clipAssetId = asset(20);
    result.playback = playback;
    result.startTimeSeconds = 0.0f;
    result.speed = 1.0f;
    result.playing = true;
    result.debugPalette = true;
    return result;
}

const float* matrixAt(const std::vector<float>& matrices, const std::uint32_t index) noexcept
{
    return matrices.data() + static_cast<std::size_t>(index) * 16U;
}

void testDefaultAddRemoveAndClear(std::vector<std::string>& failures)
{
    full_engine::AnimationPlaybackState state;
    expect(state.instanceCount() == 0, "default playback state is empty", failures);
    expect(state.summary().instanceCount == 0, "default summary is empty", failures);

    full_engine::AnimationPlaybackAddResult add = state.addInstance(desc());
    expect(add.status == full_engine::AnimationPlaybackAddStatus::Added, "valid instance is added", failures);
    expect(state.instanceCount() == 1, "instance count increments", failures);
    expect(add.summary.playingCount == 1, "add summary counts playing instance", failures);
    expect(state.findInstance(instance(1)) != nullptr, "find returns added instance", failures);

    full_engine::AnimationPlaybackAddResult duplicate = state.addInstance(desc());
    expect(duplicate.status == full_engine::AnimationPlaybackAddStatus::AlreadyExists, "duplicate id is rejected", failures);

    full_engine::AnimationPlaybackInstanceDesc invalid = desc(2);
    invalid.id = {};
    expect(
        state.addInstance(invalid).status == full_engine::AnimationPlaybackAddStatus::InvalidArgument,
        "invalid id add is rejected",
        failures);

    full_engine::AnimationPlaybackRemoveResult remove = state.removeInstance(instance(1));
    expect(remove.status == full_engine::AnimationPlaybackRemoveStatus::Removed, "existing instance is removed", failures);
    expect(state.instanceCount() == 0, "instance count decrements", failures);
    expect(
        state.removeInstance(instance(1)).status == full_engine::AnimationPlaybackRemoveStatus::NotFound,
        "missing remove reports not found",
        failures);
    expect(
        state.removeInstance({}).status == full_engine::AnimationPlaybackRemoveStatus::InvalidArgument,
        "invalid remove id is rejected",
        failures);

    state.addInstance(desc(3));
    state.clear();
    expect(state.instanceCount() == 0, "clear removes retained instances", failures);
}

void testTickProducesPoseAndPalette(std::vector<std::string>& failures)
{
    full_engine::AnimationPlaybackState state;
    state.addInstance(desc());

    full_engine::AnimationPlaybackTickResult tick =
        state.tickInstance(instance(1), skeleton(), clip(), 1.0f);
    expect(tick.status == full_engine::AnimationPlaybackTickStatus::Success, "tick succeeds", failures);
    expect(near(tick.currentTimeSeconds, 1.0f), "tick advances retained clip time", failures);
    expect(tick.summary.sampledCount == 1, "tick summary counts sampled instance", failures);

    const full_engine::LoadedAnimationPose* const latestPose = state.latestPose(instance(1));
    const full_engine::AnimationPosePaletteView* const palette = state.latestPaletteView(instance(1));
    expect(latestPose != nullptr, "latest pose is retained", failures);
    expect(palette != nullptr, "latest palette view is retained", failures);
    if (latestPose != nullptr && palette != nullptr)
    {
        expect(near(latestPose->sampledTimeSeconds, 1.0f), "pose stores sampled time", failures);
        expect(near(matrixAt(latestPose->localMatrices, 0)[13], 2.0f), "sampled pose has interpolated translation", failures);
        expect(palette->palette.skinningMatrices == latestPose->skinningMatrices.data(), "palette borrows retained pose skinning matrices", failures);
        expect(palette->palette.debugJointModelMatrices == latestPose->modelMatrices.data(), "palette borrows retained pose model matrices", failures);
        expect(
            full_renderer::animation::validateSkinningPaletteForTests(palette->palette, 2),
            "renderer validates retained palette view",
            failures);
    }
}

void testLoopClampAndPausedPlayback(std::vector<std::string>& failures)
{
    full_engine::AnimationPlaybackState state;
    full_engine::AnimationPlaybackInstanceDesc loopDesc =
        desc(1, full_engine::LoadedAnimationPlaybackMode::Loop);
    loopDesc.startTimeSeconds = 1.5f;
    state.addInstance(loopDesc);
    full_engine::AnimationPlaybackTickResult loopTick =
        state.tickInstance(instance(1), skeleton(), clip(), 1.0f);
    expect(loopTick.status == full_engine::AnimationPlaybackTickStatus::Success, "loop tick succeeds", failures);
    expect(near(loopTick.currentTimeSeconds, 0.5f), "loop tick wraps retained time", failures);

    full_engine::AnimationPlaybackInstanceDesc clampDesc =
        desc(2, full_engine::LoadedAnimationPlaybackMode::Clamp);
    clampDesc.startTimeSeconds = 1.5f;
    state.addInstance(clampDesc);
    full_engine::AnimationPlaybackTickResult clampTick =
        state.tickInstance(instance(2), skeleton(), clip(), 1.0f);
    expect(clampTick.status == full_engine::AnimationPlaybackTickStatus::Success, "clamp tick succeeds", failures);
    expect(near(clampTick.currentTimeSeconds, 2.0f), "clamp tick caps retained time", failures);

    full_engine::AnimationPlaybackInstanceDesc pausedDesc = desc(3);
    pausedDesc.startTimeSeconds = 1.0f;
    pausedDesc.playing = false;
    state.addInstance(pausedDesc);
    full_engine::AnimationPlaybackTickResult pausedTick =
        state.tickInstance(instance(3), skeleton(), clip(), 1.0f);
    expect(pausedTick.status == full_engine::AnimationPlaybackTickStatus::Success, "paused tick succeeds", failures);
    expect(near(pausedTick.currentTimeSeconds, 1.0f), "paused tick does not advance retained time", failures);
    expect(state.summary().pausedCount == 1, "summary counts paused instances", failures);
}

void testFailuresDoNotCommitTime(std::vector<std::string>& failures)
{
    full_engine::AnimationPlaybackState state;
    state.addInstance(desc());
    state.tickInstance(instance(1), skeleton(), clip(), 0.5f);
    const float committedTime = state.findInstance(instance(1))->currentTimeSeconds;

    full_engine::LoadedSkeletonAsset wrongSkeleton = skeleton();
    wrongSkeleton.id = asset(99);
    full_engine::AnimationPlaybackTickResult mismatch =
        state.tickInstance(instance(1), wrongSkeleton, clip(), 0.5f);
    expect(mismatch.status == full_engine::AnimationPlaybackTickStatus::AssetMismatch, "asset mismatch fails tick", failures);
    expect(near(state.findInstance(instance(1))->currentTimeSeconds, committedTime), "asset mismatch does not commit time", failures);

    full_engine::LoadedAnimationClipAsset invalidClip = clip();
    invalidClip.tracks.clear();
    full_engine::AnimationPlaybackTickResult sampleFailed =
        state.tickInstance(instance(1), skeleton(), invalidClip, 0.5f);
    expect(sampleFailed.status == full_engine::AnimationPlaybackTickStatus::SampleFailed, "invalid clip maps to sample failed", failures);
    expect(sampleFailed.sampleStatus != full_engine::LoadedAnimationSampleStatus::Success, "sample failure status is copied", failures);
    expect(near(state.findInstance(instance(1))->currentTimeSeconds, committedTime), "sample failure does not commit time", failures);
    expect(state.summary().failedCount == 1, "summary counts failed latest tick", failures);

    expect(
        state.tickInstance(instance(42), skeleton(), clip(), 0.5f).status ==
            full_engine::AnimationPlaybackTickStatus::NotFound,
        "missing tick reports not found",
        failures);
    expect(
        state.tickInstance({}, skeleton(), clip(), 0.5f).status ==
            full_engine::AnimationPlaybackTickStatus::InvalidArgument,
        "invalid tick id is rejected",
        failures);
    expect(
        state.tickInstance(instance(1), skeleton(), clip(), std::nanf("")).status ==
            full_engine::AnimationPlaybackTickStatus::InvalidArgument,
        "non-finite delta is rejected",
        failures);
}

void testStatusNames(std::vector<std::string>& failures)
{
    const full_engine::AnimationPlaybackAddStatus addStatuses[] = {
        full_engine::AnimationPlaybackAddStatus::Added,
        full_engine::AnimationPlaybackAddStatus::AlreadyExists,
        full_engine::AnimationPlaybackAddStatus::InvalidArgument};
    for (const full_engine::AnimationPlaybackAddStatus status : addStatuses)
    {
        expect(
            std::string(full_engine::animationPlaybackAddStatusName(status)) != "Unknown",
            "add status has stable name",
            failures);
    }

    const full_engine::AnimationPlaybackRemoveStatus removeStatuses[] = {
        full_engine::AnimationPlaybackRemoveStatus::Removed,
        full_engine::AnimationPlaybackRemoveStatus::NotFound,
        full_engine::AnimationPlaybackRemoveStatus::InvalidArgument};
    for (const full_engine::AnimationPlaybackRemoveStatus status : removeStatuses)
    {
        expect(
            std::string(full_engine::animationPlaybackRemoveStatusName(status)) != "Unknown",
            "remove status has stable name",
            failures);
    }

    const full_engine::AnimationPlaybackTickStatus tickStatuses[] = {
        full_engine::AnimationPlaybackTickStatus::Success,
        full_engine::AnimationPlaybackTickStatus::NotFound,
        full_engine::AnimationPlaybackTickStatus::InvalidArgument,
        full_engine::AnimationPlaybackTickStatus::AssetMismatch,
        full_engine::AnimationPlaybackTickStatus::SampleFailed,
        full_engine::AnimationPlaybackTickStatus::PaletteFailed};
    for (const full_engine::AnimationPlaybackTickStatus status : tickStatuses)
    {
        expect(
            std::string(full_engine::animationPlaybackTickStatusName(status)) != "Unknown",
            "tick status has stable name",
            failures);
    }
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testDefaultAddRemoveAndClear(failures);
    testTickProducesPoseAndPalette(failures);
    testLoopClampAndPausedPlayback(failures);
    testFailuresDoNotCommitTime(failures);
    testStatusNames(failures);

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
