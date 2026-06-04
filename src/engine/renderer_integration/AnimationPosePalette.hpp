#pragma once

#include "engine/assets/LoadedAnimationSampler.hpp"

#include "full_renderer/Animation.hpp"

#include <cstdint>

namespace full_engine
{
/** @brief Result status for building a renderer-facing skinning palette view. */
enum class AnimationPosePaletteStatus
{
    Success,
    InvalidPose,
    JointCountOutOfRange,
    InvalidMatrixCounts,
    InvalidMatrixData,
};

/** @brief Returns a stable diagnostic name for an animation pose palette status. */
const char* animationPosePaletteStatusName(
    AnimationPosePaletteStatus status) noexcept;

/**
 * @brief Non-owning renderer-facing palette view over a loaded animation pose.
 *
 * The descriptor pointers borrow from the source `LoadedAnimationPose` vectors.
 * The pose must outlive renderer submission and must not be moved or mutated
 * while the view is in use. This value does not own renderer resources, create
 * handles, evaluate animation, or build complete `AnimatedDrawItem` records.
 */
struct AnimationPosePaletteView
{
    /** @brief Status copied from the build result for compact diagnostics. */
    AnimationPosePaletteStatus status = AnimationPosePaletteStatus::InvalidPose;

    /** @brief Number of joints represented by the borrowed palette. */
    std::uint32_t jointCount = 0;

    /** @brief Borrowed renderer-facing skinning palette descriptor. */
    full_renderer::SkinningPaletteDesc palette = {};
};

/** @brief Result from building a non-owning animation pose palette view. */
struct AnimationPosePaletteResult
{
    /** @brief Build status. */
    AnimationPosePaletteStatus status = AnimationPosePaletteStatus::InvalidPose;

    /** @brief Non-owning palette view when `status == Success`. */
    AnimationPosePaletteView view = {};
};

/**
 * @brief Builds a renderer-facing skinning palette view from a CPU sampled pose.
 *
 * `pose.skinningMatrices` must contain one finite column-major 4x4 matrix per
 * joint. When `includeDebugJointModels` is true, `pose.modelMatrices` must also
 * contain one finite column-major 4x4 matrix per joint and is exposed through
 * `debugJointModelMatrices` for renderer bone debug drawing. All pointers in
 * the returned descriptor borrow from `pose` and are frame-local submission
 * inputs only.
 *
 * @param pose Caller-owned sampled animation pose.
 * @param includeDebugJointModels Whether to expose model matrices for debug bones.
 * @return Palette view status and borrowed descriptor pointers.
 */
AnimationPosePaletteResult makeAnimationPosePaletteView(
    const LoadedAnimationPose& pose,
    bool includeDebugJointModels = true);
} // namespace full_engine
