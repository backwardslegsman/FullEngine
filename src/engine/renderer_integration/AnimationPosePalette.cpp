#include "engine/renderer_integration/AnimationPosePalette.hpp"

#include <cstddef>
#include <cmath>
#include <vector>

namespace full_engine
{
namespace
{
bool hasFiniteValues(const std::vector<float>& values) noexcept
{
    for (const float value : values)
    {
        if (!std::isfinite(value))
        {
            return false;
        }
    }
    return true;
}

std::size_t expectedMatrixValueCount(const std::uint32_t jointCount) noexcept
{
    return static_cast<std::size_t>(jointCount) * 16U;
}
} // namespace

const char* animationPosePaletteStatusName(
    const AnimationPosePaletteStatus status) noexcept
{
    switch (status)
    {
    case AnimationPosePaletteStatus::Success:
        return "Success";
    case AnimationPosePaletteStatus::InvalidPose:
        return "InvalidPose";
    case AnimationPosePaletteStatus::JointCountOutOfRange:
        return "JointCountOutOfRange";
    case AnimationPosePaletteStatus::InvalidMatrixCounts:
        return "InvalidMatrixCounts";
    case AnimationPosePaletteStatus::InvalidMatrixData:
        return "InvalidMatrixData";
    }

    return "Unknown";
}

AnimationPosePaletteResult makeAnimationPosePaletteView(
    const LoadedAnimationPose& pose,
    const bool includeDebugJointModels)
{
    AnimationPosePaletteResult result;

    if (!isValid(pose.skeletonAssetId))
    {
        result.status = AnimationPosePaletteStatus::InvalidPose;
        return result;
    }

    if (pose.jointCount == 0 ||
        pose.jointCount > full_renderer::kMaxSkinningJoints)
    {
        result.status = AnimationPosePaletteStatus::JointCountOutOfRange;
        return result;
    }

    const std::size_t expectedValueCount = expectedMatrixValueCount(pose.jointCount);
    if (pose.skinningMatrices.size() != expectedValueCount ||
        (includeDebugJointModels && pose.modelMatrices.size() != expectedValueCount))
    {
        result.status = AnimationPosePaletteStatus::InvalidMatrixCounts;
        return result;
    }

    if (!hasFiniteValues(pose.skinningMatrices) ||
        (includeDebugJointModels && !hasFiniteValues(pose.modelMatrices)))
    {
        result.status = AnimationPosePaletteStatus::InvalidMatrixData;
        return result;
    }

    AnimationPosePaletteView view;
    view.status = AnimationPosePaletteStatus::Success;
    view.jointCount = pose.jointCount;
    view.palette.skinningMatrices = pose.skinningMatrices.data();
    view.palette.matrixCount = pose.jointCount;
    if (includeDebugJointModels)
    {
        view.palette.debugJointModelMatrices = pose.modelMatrices.data();
        view.palette.debugJointModelMatrixCount = pose.jointCount;
    }

    result.status = AnimationPosePaletteStatus::Success;
    result.view = view;
    return result;
}
} // namespace full_engine
