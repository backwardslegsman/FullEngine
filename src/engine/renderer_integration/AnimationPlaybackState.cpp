#include "engine/renderer_integration/AnimationPlaybackState.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace full_engine
{
namespace
{
AnimationPlaybackTickStatus mapSampleFailure(
    const LoadedAnimationSampleStatus status) noexcept
{
    return status == LoadedAnimationSampleStatus::Success ?
        AnimationPlaybackTickStatus::Success :
        AnimationPlaybackTickStatus::SampleFailed;
}

AnimationPlaybackTickStatus mapPaletteFailure(
    const AnimationPosePaletteStatus status) noexcept
{
    return status == AnimationPosePaletteStatus::Success ?
        AnimationPlaybackTickStatus::Success :
        AnimationPlaybackTickStatus::PaletteFailed;
}
} // namespace

bool isValid(const AnimationPlaybackInstanceId id) noexcept
{
    return id.value != 0;
}

bool operator==(const AnimationPlaybackInstanceId lhs, const AnimationPlaybackInstanceId rhs) noexcept
{
    return lhs.value == rhs.value;
}

const char* animationPlaybackAddStatusName(const AnimationPlaybackAddStatus status) noexcept
{
    switch (status)
    {
    case AnimationPlaybackAddStatus::Added:
        return "Added";
    case AnimationPlaybackAddStatus::AlreadyExists:
        return "AlreadyExists";
    case AnimationPlaybackAddStatus::InvalidArgument:
        return "InvalidArgument";
    }

    return "Unknown";
}

const char* animationPlaybackRemoveStatusName(const AnimationPlaybackRemoveStatus status) noexcept
{
    switch (status)
    {
    case AnimationPlaybackRemoveStatus::Removed:
        return "Removed";
    case AnimationPlaybackRemoveStatus::NotFound:
        return "NotFound";
    case AnimationPlaybackRemoveStatus::InvalidArgument:
        return "InvalidArgument";
    }

    return "Unknown";
}

const char* animationPlaybackTickStatusName(const AnimationPlaybackTickStatus status) noexcept
{
    switch (status)
    {
    case AnimationPlaybackTickStatus::Success:
        return "Success";
    case AnimationPlaybackTickStatus::NotFound:
        return "NotFound";
    case AnimationPlaybackTickStatus::InvalidArgument:
        return "InvalidArgument";
    case AnimationPlaybackTickStatus::AssetMismatch:
        return "AssetMismatch";
    case AnimationPlaybackTickStatus::SampleFailed:
        return "SampleFailed";
    case AnimationPlaybackTickStatus::PaletteFailed:
        return "PaletteFailed";
    }

    return "Unknown";
}

AnimationPlaybackAddResult AnimationPlaybackState::addInstance(
    const AnimationPlaybackInstanceDesc& desc)
{
    AnimationPlaybackAddResult result;
    result.id = desc.id;

    if (!validateDesc(desc))
    {
        result.status = AnimationPlaybackAddStatus::InvalidArgument;
        result.summary = summary();
        return result;
    }

    if (findInstance(desc.id) != nullptr)
    {
        result.status = AnimationPlaybackAddStatus::AlreadyExists;
        result.summary = summary();
        return result;
    }

    AnimationPlaybackInstanceRecord record;
    record.desc = desc;
    record.currentTimeSeconds = desc.startTimeSeconds;
    record.latestTickStatus = AnimationPlaybackTickStatus::NotFound;
    instances_.push_back(record);

    result.status = AnimationPlaybackAddStatus::Added;
    result.summary = summary();
    return result;
}

AnimationPlaybackRemoveResult AnimationPlaybackState::removeInstance(
    const AnimationPlaybackInstanceId id)
{
    AnimationPlaybackRemoveResult result;
    result.id = id;

    if (!isValid(id))
    {
        result.status = AnimationPlaybackRemoveStatus::InvalidArgument;
        result.summary = summary();
        return result;
    }

    const auto found = std::find_if(
        instances_.begin(),
        instances_.end(),
        [id](const AnimationPlaybackInstanceRecord& record)
        {
            return record.desc.id == id;
        });

    if (found == instances_.end())
    {
        result.status = AnimationPlaybackRemoveStatus::NotFound;
        result.summary = summary();
        return result;
    }

    instances_.erase(found);
    result.status = AnimationPlaybackRemoveStatus::Removed;
    result.summary = summary();
    return result;
}

const AnimationPlaybackInstanceRecord* AnimationPlaybackState::findInstance(
    const AnimationPlaybackInstanceId id) const noexcept
{
    for (const AnimationPlaybackInstanceRecord& record : instances_)
    {
        if (record.desc.id == id)
        {
            return &record;
        }
    }
    return nullptr;
}

AnimationPlaybackInstanceRecord* AnimationPlaybackState::findInstance(
    const AnimationPlaybackInstanceId id) noexcept
{
    for (AnimationPlaybackInstanceRecord& record : instances_)
    {
        if (record.desc.id == id)
        {
            return &record;
        }
    }
    return nullptr;
}

std::size_t AnimationPlaybackState::instanceCount() const noexcept
{
    return instances_.size();
}

AnimationPlaybackSummary AnimationPlaybackState::summary() const noexcept
{
    AnimationPlaybackSummary result;
    result.instanceCount = instances_.size();
    for (const AnimationPlaybackInstanceRecord& record : instances_)
    {
        if (record.desc.playing)
        {
            ++result.playingCount;
        }
        else
        {
            ++result.pausedCount;
        }

        if (record.latestTickStatus == AnimationPlaybackTickStatus::Success)
        {
            ++result.sampledCount;
        }
        else if (record.latestTickStatus != AnimationPlaybackTickStatus::NotFound)
        {
            ++result.failedCount;
        }
    }
    return result;
}

AnimationPlaybackTickResult AnimationPlaybackState::tickInstance(
    const AnimationPlaybackInstanceId id,
    const LoadedSkeletonAsset& skeleton,
    const LoadedAnimationClipAsset& clip,
    const float deltaSeconds)
{
    AnimationPlaybackTickResult result;
    result.id = id;

    if (!isValid(id) || !std::isfinite(deltaSeconds))
    {
        result.status = AnimationPlaybackTickStatus::InvalidArgument;
        result.summary = summary();
        return result;
    }

    AnimationPlaybackInstanceRecord* const record = findInstance(id);
    if (record == nullptr)
    {
        result.status = AnimationPlaybackTickStatus::NotFound;
        result.summary = summary();
        return result;
    }

    result.currentTimeSeconds = record->currentTimeSeconds;

    if (skeleton.id.value != record->desc.skeletonAssetId.value ||
        clip.id.value != record->desc.clipAssetId.value ||
        clip.skeletonAssetId.value != record->desc.skeletonAssetId.value)
    {
        record->latestTickStatus = AnimationPlaybackTickStatus::AssetMismatch;
        result.status = record->latestTickStatus;
        result.summary = summary();
        return result;
    }

    const float candidateTime = record->desc.playing ?
        record->currentTimeSeconds + deltaSeconds * record->desc.speed :
        record->currentTimeSeconds;

    LoadedAnimationSampleResult sample =
        sampleLoadedAnimationClip(skeleton, clip, candidateTime, record->desc.playback);
    result.sampleStatus = sample.status;
    if (sample.status != LoadedAnimationSampleStatus::Success)
    {
        record->latestTickStatus = mapSampleFailure(sample.status);
        result.status = record->latestTickStatus;
        result.summary = summary();
        return result;
    }

    AnimationPosePaletteResult palette =
        makeAnimationPosePaletteView(sample.pose, record->desc.debugPalette);
    result.paletteStatus = palette.status;
    if (palette.status != AnimationPosePaletteStatus::Success)
    {
        record->latestTickStatus = mapPaletteFailure(palette.status);
        result.status = record->latestTickStatus;
        result.summary = summary();
        return result;
    }

    record->currentTimeSeconds = sample.pose.sampledTimeSeconds;
    record->latestPose = std::move(sample.pose);
    record->latestPalette =
        makeAnimationPosePaletteView(record->latestPose, record->desc.debugPalette);
    record->latestTickStatus = record->latestPalette.status == AnimationPosePaletteStatus::Success ?
        AnimationPlaybackTickStatus::Success :
        AnimationPlaybackTickStatus::PaletteFailed;

    result.status = record->latestTickStatus;
    result.currentTimeSeconds = record->currentTimeSeconds;
    result.paletteStatus = record->latestPalette.status;
    result.summary = summary();
    return result;
}

const LoadedAnimationPose* AnimationPlaybackState::latestPose(
    const AnimationPlaybackInstanceId id) const noexcept
{
    const AnimationPlaybackInstanceRecord* const record = findInstance(id);
    if (record == nullptr || record->latestTickStatus != AnimationPlaybackTickStatus::Success)
    {
        return nullptr;
    }
    return &record->latestPose;
}

const AnimationPosePaletteView* AnimationPlaybackState::latestPaletteView(
    const AnimationPlaybackInstanceId id) const noexcept
{
    const AnimationPlaybackInstanceRecord* const record = findInstance(id);
    if (record == nullptr ||
        record->latestTickStatus != AnimationPlaybackTickStatus::Success ||
        record->latestPalette.status != AnimationPosePaletteStatus::Success)
    {
        return nullptr;
    }
    return &record->latestPalette.view;
}

void AnimationPlaybackState::clear() noexcept
{
    instances_.clear();
}

bool AnimationPlaybackState::validateDesc(const AnimationPlaybackInstanceDesc& desc) noexcept
{
    return isValid(desc.id) &&
        isValid(desc.skeletonAssetId) &&
        isValid(desc.clipAssetId) &&
        std::isfinite(desc.startTimeSeconds) &&
        std::isfinite(desc.speed) &&
        (desc.playback == LoadedAnimationPlaybackMode::Clamp ||
            desc.playback == LoadedAnimationPlaybackMode::Loop);
}
} // namespace full_engine
