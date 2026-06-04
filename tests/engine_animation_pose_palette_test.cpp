#include "engine/assets/LoadedAnimationSampler.hpp"
#include "engine/renderer_integration/AnimationPosePalette.hpp"

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

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

void identity(float* const out) noexcept
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

full_engine::LoadedAnimationPose pose(const std::uint32_t jointCount)
{
    full_engine::LoadedAnimationPose result;
    result.skeletonAssetId = asset(10);
    result.sampledTimeSeconds = 1.0f;
    result.jointCount = jointCount;
    result.localMatrices.assign(static_cast<std::size_t>(jointCount) * 16U, 0.0f);
    result.modelMatrices.assign(static_cast<std::size_t>(jointCount) * 16U, 0.0f);
    result.skinningMatrices.assign(static_cast<std::size_t>(jointCount) * 16U, 0.0f);
    for (std::uint32_t index = 0; index < jointCount; ++index)
    {
        identity(result.localMatrices.data() + static_cast<std::size_t>(index) * 16U);
        identity(result.modelMatrices.data() + static_cast<std::size_t>(index) * 16U);
        identity(result.skinningMatrices.data() + static_cast<std::size_t>(index) * 16U);
    }
    return result;
}

void testValidPaletteView(std::vector<std::string>& failures)
{
    full_engine::LoadedAnimationPose source = pose(2);
    full_engine::AnimationPosePaletteResult result =
        full_engine::makeAnimationPosePaletteView(source);

    expect(result.status == full_engine::AnimationPosePaletteStatus::Success, "valid pose builds palette view", failures);
    expect(result.view.status == full_engine::AnimationPosePaletteStatus::Success, "view copies success status", failures);
    expect(result.view.jointCount == 2, "view copies joint count", failures);
    expect(result.view.palette.skinningMatrices == source.skinningMatrices.data(), "palette borrows skinning matrix storage", failures);
    expect(result.view.palette.matrixCount == 2, "palette matrix count matches joint count", failures);
    expect(result.view.palette.debugJointModelMatrices == source.modelMatrices.data(), "palette borrows debug model storage", failures);
    expect(result.view.palette.debugJointModelMatrixCount == 2, "debug matrix count matches joint count", failures);
    expect(
        full_renderer::animation::validateSkinningPaletteForTests(result.view.palette, 2),
        "renderer accepts produced palette",
        failures);
}

void testDebugMatricesCanBeOmitted(std::vector<std::string>& failures)
{
    full_engine::LoadedAnimationPose source = pose(2);
    source.modelMatrices.clear();

    full_engine::AnimationPosePaletteResult result =
        full_engine::makeAnimationPosePaletteView(source, false);
    expect(result.status == full_engine::AnimationPosePaletteStatus::Success, "debug matrices can be omitted", failures);
    expect(result.view.palette.debugJointModelMatrices == nullptr, "debug model pointer is null when omitted", failures);
    expect(result.view.palette.debugJointModelMatrixCount == 0, "debug model count is zero when omitted", failures);
    expect(
        full_renderer::animation::validateSkinningPaletteForTests(result.view.palette, 2),
        "renderer accepts palette without debug matrices",
        failures);
}

void testInvalidInputs(std::vector<std::string>& failures)
{
    full_engine::LoadedAnimationPose source = pose(1);
    source.skeletonAssetId = {};
    expect(
        full_engine::makeAnimationPosePaletteView(source).status ==
            full_engine::AnimationPosePaletteStatus::InvalidPose,
        "default skeleton id is invalid pose",
        failures);

    source = pose(0);
    expect(
        full_engine::makeAnimationPosePaletteView(source).status ==
            full_engine::AnimationPosePaletteStatus::JointCountOutOfRange,
        "zero joint count is out of range",
        failures);

    source = pose(full_renderer::kMaxSkinningJoints + 1U);
    expect(
        full_engine::makeAnimationPosePaletteView(source).status ==
            full_engine::AnimationPosePaletteStatus::JointCountOutOfRange,
        "over-limit joint count is out of range",
        failures);

    source = pose(2);
    source.skinningMatrices.pop_back();
    expect(
        full_engine::makeAnimationPosePaletteView(source).status ==
            full_engine::AnimationPosePaletteStatus::InvalidMatrixCounts,
        "wrong skinning matrix count is rejected",
        failures);

    source = pose(2);
    source.modelMatrices.pop_back();
    expect(
        full_engine::makeAnimationPosePaletteView(source).status ==
            full_engine::AnimationPosePaletteStatus::InvalidMatrixCounts,
        "wrong debug matrix count is rejected",
        failures);

    source = pose(2);
    source.skinningMatrices[3] = std::nanf("");
    expect(
        full_engine::makeAnimationPosePaletteView(source).status ==
            full_engine::AnimationPosePaletteStatus::InvalidMatrixData,
        "non-finite skinning matrix data is rejected",
        failures);

    source = pose(2);
    source.modelMatrices[7] = std::nanf("");
    expect(
        full_engine::makeAnimationPosePaletteView(source).status ==
            full_engine::AnimationPosePaletteStatus::InvalidMatrixData,
        "non-finite debug matrix data is rejected",
        failures);
}

void testResultNames(std::vector<std::string>& failures)
{
    const full_engine::AnimationPosePaletteStatus statuses[] = {
        full_engine::AnimationPosePaletteStatus::Success,
        full_engine::AnimationPosePaletteStatus::InvalidPose,
        full_engine::AnimationPosePaletteStatus::JointCountOutOfRange,
        full_engine::AnimationPosePaletteStatus::InvalidMatrixCounts,
        full_engine::AnimationPosePaletteStatus::InvalidMatrixData};

    for (const full_engine::AnimationPosePaletteStatus status : statuses)
    {
        expect(
            std::string(full_engine::animationPosePaletteStatusName(status)) != "Unknown",
            "animation pose palette status has stable name",
            failures);
    }
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testValidPaletteView(failures);
    testDebugMatricesCanBeOmitted(failures);
    testInvalidInputs(failures);
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
