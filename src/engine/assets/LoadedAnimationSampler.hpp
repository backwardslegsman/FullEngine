#pragma once

#include "engine/assets/LoadedAssetPayload.hpp"

#include <cstdint>
#include <vector>

namespace full_engine
{
/**
 * @brief Playback policy used when sampling outside an animation clip duration.
 *
 * Sampling is CPU-only and deterministic. `Clamp` holds the first or last pose
 * when time is outside the clip. `Loop` wraps finite time values into the clip
 * duration before sampling.
 */
enum class LoadedAnimationPlaybackMode
{
    Clamp,
    Loop,
};

/** @brief Result status for CPU animation clip sampling. */
enum class LoadedAnimationSampleStatus
{
    Success,
    InvalidArgument,
    SkeletonValidationFailed,
    ClipValidationFailed,
    SkeletonMismatch,
    TrackOutOfSkeletonRange,
};

/**
 * @brief CPU-evaluated pose for one skeleton and one clip sample.
 *
 * Matrices are column-major floats in skeleton-local space. Each vector owns
 * `jointCount * 16` values. `skinningMatrices` stores final palette matrices
 * as `jointModel * inverseBindPose`, matching the renderer-facing
 * `SkinningPaletteDesc` matrix convention without including renderer headers.
 * The value owns copied data only and does not reference the source skeleton,
 * clip, renderer state, or runtime animation state.
 */
struct LoadedAnimationPose
{
    /** @brief Skeleton asset identity used for this pose. */
    AssetId skeletonAssetId = {};

    /** @brief Time after clamp/wrap policy was applied, in seconds. */
    float sampledTimeSeconds = 0.0f;

    /** @brief Number of joints represented by each matrix vector. */
    std::uint32_t jointCount = 0;

    /** @brief Column-major local joint matrices, one 4x4 matrix per joint. */
    std::vector<float> localMatrices;

    /** @brief Column-major skeleton-space model matrices, one 4x4 matrix per joint. */
    std::vector<float> modelMatrices;

    /** @brief Column-major final skinning palette matrices, one 4x4 matrix per joint. */
    std::vector<float> skinningMatrices;
};

/**
 * @brief CPU animation sampling result.
 *
 * On `Success`, `pose` contains complete owned local/model/skinning matrices.
 * On failure, `pose` remains default-initialized. Sampling does not mutate the
 * input skeleton or clip and performs no renderer calls, handle lookup,
 * blending, compression, or runtime animation state updates.
 */
struct LoadedAnimationSampleResult
{
    /** @brief Sampling status. */
    LoadedAnimationSampleStatus status = LoadedAnimationSampleStatus::InvalidArgument;

    /** @brief Evaluated pose when `status == Success`. */
    LoadedAnimationPose pose = {};
};

/** @brief Returns a stable diagnostic name for a CPU animation sample status. */
const char* loadedAnimationSampleStatusName(
    LoadedAnimationSampleStatus status) noexcept;

/**
 * @brief Samples one loaded animation clip against one loaded skeleton.
 *
 * The skeleton and clip are first validated through the loaded payload
 * validators. Track joint indices must fit within the concrete skeleton joint
 * count. Translation and scale keys use linear interpolation. Rotation keys
 * use normalized linear interpolation with quaternion hemisphere correction.
 * Joints without a track use the skeleton joint `referenceTransform` as their
 * local pose. Parent model matrices are composed before children using the
 * existing parent-before-child skeleton contract.
 *
 * @param skeleton Loaded skeleton payload that owns hierarchy and bind data.
 * @param clip Loaded animation clip payload targeting `skeleton.id`.
 * @param timeSeconds Input sample time in seconds. Must be finite.
 * @param playback Playback policy for times outside the clip duration.
 * @return Sampling status and an owned pose on success.
 */
LoadedAnimationSampleResult sampleLoadedAnimationClip(
    const LoadedSkeletonAsset& skeleton,
    const LoadedAnimationClipAsset& clip,
    float timeSeconds,
    LoadedAnimationPlaybackMode playback = LoadedAnimationPlaybackMode::Clamp);
} // namespace full_engine
