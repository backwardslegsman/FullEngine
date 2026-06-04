#pragma once

#include "engine/renderer_integration/AnimationPosePalette.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace full_engine
{
/**
 * @brief Caller-owned identity for one retained animation playback instance.
 *
 * The default value is invalid. The engine does not allocate entity IDs or
 * scene objects in this slice; callers choose stable nonzero IDs and own their
 * mapping to gameplay or renderable instances.
 */
struct AnimationPlaybackInstanceId
{
    std::uint64_t value = 0;
};

/** @brief Returns whether a playback instance ID is non-default. */
bool isValid(AnimationPlaybackInstanceId id) noexcept;

/** @brief Compares playback instance IDs for equality. */
bool operator==(AnimationPlaybackInstanceId lhs, AnimationPlaybackInstanceId rhs) noexcept;

/**
 * @brief Configuration for one single-clip playback instance.
 *
 * Times are seconds. `speed` scales positive or negative tick deltas before
 * sampling. `debugPalette` controls whether model matrices are exposed through
 * the resulting palette view for renderer bone debug drawing. The descriptor
 * stores asset IDs only; callers still own loaded skeleton and clip payloads.
 */
struct AnimationPlaybackInstanceDesc
{
    /** @brief Nonzero caller-owned playback instance ID. */
    AnimationPlaybackInstanceId id = {};

    /** @brief Loaded skeleton asset ID expected by this instance. */
    AssetId skeletonAssetId = {};

    /** @brief Loaded animation clip asset ID expected by this instance. */
    AssetId clipAssetId = {};

    /** @brief Clamp or loop policy forwarded to the CPU sampler. */
    LoadedAnimationPlaybackMode playback = LoadedAnimationPlaybackMode::Clamp;

    /** @brief Initial retained playback time in seconds. */
    float startTimeSeconds = 0.0f;

    /** @brief Multiplier applied to tick deltas while playing. */
    float speed = 1.0f;

    /** @brief Whether ticks advance retained time before sampling. */
    bool playing = true;

    /** @brief Whether palette views include debug joint model matrices. */
    bool debugPalette = true;
};

/** @brief Result for adding a playback instance. */
enum class AnimationPlaybackAddStatus
{
    Added,
    AlreadyExists,
    InvalidArgument,
};

/** @brief Result for removing a playback instance. */
enum class AnimationPlaybackRemoveStatus
{
    Removed,
    NotFound,
    InvalidArgument,
};

/** @brief Result for ticking a playback instance. */
enum class AnimationPlaybackTickStatus
{
    Success,
    NotFound,
    InvalidArgument,
    AssetMismatch,
    SampleFailed,
    PaletteFailed,
};

/** @brief Returns a stable diagnostic name for an animation playback add status. */
const char* animationPlaybackAddStatusName(AnimationPlaybackAddStatus status) noexcept;

/** @brief Returns a stable diagnostic name for an animation playback remove status. */
const char* animationPlaybackRemoveStatusName(AnimationPlaybackRemoveStatus status) noexcept;

/** @brief Returns a stable diagnostic name for an animation playback tick status. */
const char* animationPlaybackTickStatusName(AnimationPlaybackTickStatus status) noexcept;

/** @brief Aggregate counters for retained animation playback state. */
struct AnimationPlaybackSummary
{
    /** @brief Number of retained playback instances. */
    std::size_t instanceCount = 0;

    /** @brief Number of retained instances marked playing. */
    std::size_t playingCount = 0;

    /** @brief Number of retained instances marked paused. */
    std::size_t pausedCount = 0;

    /** @brief Number of retained instances with a successful latest sample. */
    std::size_t sampledCount = 0;

    /** @brief Number of retained instances whose latest tick failed. */
    std::size_t failedCount = 0;
};

/** @brief Result of adding one retained playback instance. */
struct AnimationPlaybackAddResult
{
    /** @brief Add operation status. */
    AnimationPlaybackAddStatus status = AnimationPlaybackAddStatus::InvalidArgument;

    /** @brief Playback instance ID requested by the caller. */
    AnimationPlaybackInstanceId id = {};

    /** @brief Summary after the add attempt. */
    AnimationPlaybackSummary summary = {};
};

/** @brief Result of removing one retained playback instance. */
struct AnimationPlaybackRemoveResult
{
    /** @brief Remove operation status. */
    AnimationPlaybackRemoveStatus status = AnimationPlaybackRemoveStatus::InvalidArgument;

    /** @brief Playback instance ID requested by the caller. */
    AnimationPlaybackInstanceId id = {};

    /** @brief Summary after the remove attempt. */
    AnimationPlaybackSummary summary = {};
};

/** @brief Result of ticking one retained playback instance. */
struct AnimationPlaybackTickResult
{
    /** @brief Tick operation status. */
    AnimationPlaybackTickStatus status = AnimationPlaybackTickStatus::InvalidArgument;

    /** @brief Playback instance ID requested by the caller. */
    AnimationPlaybackInstanceId id = {};

    /** @brief Retained time after the tick when successful, otherwise previous time. */
    float currentTimeSeconds = 0.0f;

    /** @brief CPU sampling status from the lower-level sampler. */
    LoadedAnimationSampleStatus sampleStatus = LoadedAnimationSampleStatus::InvalidArgument;

    /** @brief Palette adapter status from the lower-level renderer integration helper. */
    AnimationPosePaletteStatus paletteStatus = AnimationPosePaletteStatus::InvalidPose;

    /** @brief Summary after the tick attempt. */
    AnimationPlaybackSummary summary = {};
};

/**
 * @brief One retained single-clip animation playback record.
 *
 * The record owns the latest sampled pose so its palette view can safely borrow
 * matrix pointers until this instance is ticked, removed, or the state is
 * cleared. It owns no renderer resources and stores no mesh/material handles.
 */
struct AnimationPlaybackInstanceRecord
{
    /** @brief Caller-supplied instance configuration. */
    AnimationPlaybackInstanceDesc desc = {};

    /** @brief Current retained playback time in seconds. */
    float currentTimeSeconds = 0.0f;

    /** @brief Latest tick status for this instance. */
    AnimationPlaybackTickStatus latestTickStatus = AnimationPlaybackTickStatus::NotFound;

    /** @brief Latest sampled pose retained for borrowed palette views. */
    LoadedAnimationPose latestPose = {};

    /** @brief Latest palette adapter result borrowing from `latestPose`. */
    AnimationPosePaletteResult latestPalette = {};
};

/**
 * @brief Retained CPU playback state for single-clip animation instances.
 *
 * This state is synchronous and not thread-safe. It advances caller-defined
 * instances through caller-owned loaded skeleton/clip payloads, stores the
 * latest sampled pose, and exposes borrowed frame-local palette views for
 * renderer submission. It performs no renderer calls, no resource creation,
 * no draw-item construction, no blending, no root motion, no animation events,
 * no retargeting, and no sample UI wiring.
 */
class AnimationPlaybackState
{
public:
    /** @brief Adds a retained playback instance when the descriptor is valid and unmapped. */
    AnimationPlaybackAddResult addInstance(const AnimationPlaybackInstanceDesc& desc);

    /** @brief Removes one retained playback instance. */
    AnimationPlaybackRemoveResult removeInstance(AnimationPlaybackInstanceId id);

    /** @brief Returns one retained playback instance, or null when missing. */
    const AnimationPlaybackInstanceRecord* findInstance(AnimationPlaybackInstanceId id) const noexcept;

    /** @brief Returns one retained playback instance, or null when missing. */
    AnimationPlaybackInstanceRecord* findInstance(AnimationPlaybackInstanceId id) noexcept;

    /** @brief Returns the number of retained playback instances. */
    std::size_t instanceCount() const noexcept;

    /** @brief Returns compact counters for retained playback instances. */
    AnimationPlaybackSummary summary() const noexcept;

    /**
     * @brief Advances and samples one retained playback instance.
     *
     * `deltaSeconds` must be finite. When the instance is playing, retained time
     * advances by `deltaSeconds * speed`; paused instances resample their
     * current time. New time, pose, and palette are committed only after both
     * sampling and palette adaptation succeed. Returned palette pointers borrow
     * from retained state and are valid only until the next mutation of this
     * instance or `clear()`.
     */
    AnimationPlaybackTickResult tickInstance(
        AnimationPlaybackInstanceId id,
        const LoadedSkeletonAsset& skeleton,
        const LoadedAnimationClipAsset& clip,
        float deltaSeconds);

    /** @brief Returns the latest sampled pose for an instance, or null when unavailable. */
    const LoadedAnimationPose* latestPose(AnimationPlaybackInstanceId id) const noexcept;

    /** @brief Returns the latest borrowed palette view for an instance, or null when unavailable. */
    const AnimationPosePaletteView* latestPaletteView(AnimationPlaybackInstanceId id) const noexcept;

    /** @brief Removes all retained playback instances and borrowed palette views. */
    void clear() noexcept;

private:
    static bool validateDesc(const AnimationPlaybackInstanceDesc& desc) noexcept;

    std::vector<AnimationPlaybackInstanceRecord> instances_;
};
} // namespace full_engine
