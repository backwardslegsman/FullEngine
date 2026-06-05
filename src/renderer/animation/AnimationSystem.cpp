#include "renderer/animation/AnimationSystem.hpp"

#include "renderer/scene/Fade.hpp"

#include <algorithm>
#include <cmath>

namespace full_renderer::animation
{
namespace
{
constexpr float kWeightSumTolerance = 0.01f;
constexpr float kMinimumNormalLengthSquared = 0.000001f;

bool isFinite(const float value) noexcept
{
    return std::isfinite(value);
}

bool isFiniteArray(const float* values, const std::uint32_t count) noexcept
{
    if (values == nullptr)
    {
        return false;
    }
    for (std::uint32_t index = 0; index < count; ++index)
    {
        if (!isFinite(values[index]))
        {
            return false;
        }
    }
    return true;
}

bool isValidAabb(const Aabb& bounds) noexcept
{
    return isFiniteArray(bounds.min, 3) &&
        isFiniteArray(bounds.max, 3) &&
        bounds.min[0] <= bounds.max[0] &&
        bounds.min[1] <= bounds.max[1] &&
        bounds.min[2] <= bounds.max[2];
}

bool isUnitRangeColor(const float* values, const std::uint32_t count) noexcept
{
    if (values == nullptr)
    {
        return false;
    }
    for (std::uint32_t index = 0; index < count; ++index)
    {
        if (!isFinite(values[index]) || values[index] < 0.0f || values[index] > 1.0f)
        {
            return false;
        }
    }
    return true;
}

bool hasUsableNormal(const float normal[3]) noexcept
{
    if (!isFiniteArray(normal, 3))
    {
        return false;
    }
    const float lengthSquared =
        normal[0] * normal[0] +
        normal[1] * normal[1] +
        normal[2] * normal[2];
    return std::isfinite(lengthSquared) && lengthSquared >= kMinimumNormalLengthSquared;
}

bool hasUsableTangent(const float tangent[4]) noexcept
{
    constexpr float kHandednessTolerance = 0.001f;
    if (!isFiniteArray(tangent, 4))
    {
        return false;
    }
    const float lengthSquared =
        tangent[0] * tangent[0] +
        tangent[1] * tangent[1] +
        tangent[2] * tangent[2];
    if (!std::isfinite(lengthSquared) || lengthSquared < kMinimumNormalLengthSquared)
    {
        return false;
    }
    return std::fabs(std::fabs(tangent[3]) - 1.0f) <= kHandednessTolerance;
}

bool isValidSkinnedMeshSection(
    const SkinnedMeshSectionDesc& section,
    const std::uint32_t indexCount) noexcept
{
    return section.indexCount > 0 &&
        (section.firstIndex % 3U) == 0U &&
        (section.indexCount % 3U) == 0U &&
        section.firstIndex <= indexCount &&
        section.indexCount <= indexCount - section.firstIndex;
}

void transformPoint(const float matrix[16], const float point[3], float out[3]) noexcept
{
    out[0] = matrix[0] * point[0] + matrix[4] * point[1] + matrix[8] * point[2] + matrix[12];
    out[1] = matrix[1] * point[0] + matrix[5] * point[1] + matrix[9] * point[2] + matrix[13];
    out[2] = matrix[2] * point[0] + matrix[6] * point[1] + matrix[10] * point[2] + matrix[14];
}

void appendLine(
    std::vector<debug::DebugLineVertex>& outLines,
    const float a[3],
    const float b[3],
    const float color[4])
{
    debug::DebugLineVertex first{};
    debug::DebugLineVertex second{};
    for (std::uint32_t axis = 0; axis < 3; ++axis)
    {
        first.position[axis] = a[axis];
        second.position[axis] = b[axis];
    }
    for (std::uint32_t channel = 0; channel < 4; ++channel)
    {
        first.colorLinear[channel] = color[channel];
        second.colorLinear[channel] = color[channel];
    }
    outLines.push_back(first);
    outLines.push_back(second);
}
} // namespace

bool AnimationSystem::validateSkeletonDesc(const SkeletonDesc& desc) noexcept
{
    if (desc.joints == nullptr || desc.jointCount == 0 || desc.jointCount > kMaxSkinningJoints)
    {
        return false;
    }

    std::uint32_t rootCount = 0;
    for (std::uint32_t jointIndex = 0; jointIndex < desc.jointCount; ++jointIndex)
    {
        const SkeletonJointDesc& joint = desc.joints[jointIndex];
        if (!isFiniteArray(joint.inverseBindPose, 16))
        {
            return false;
        }
        if (joint.parentIndex < 0)
        {
            ++rootCount;
        }
        else if (static_cast<std::uint32_t>(joint.parentIndex) >= jointIndex)
        {
            return false;
        }
    }

    return rootCount == 1;
}

bool AnimationSystem::validateSkinningPalette(const SkinningPaletteDesc& palette, const std::uint32_t jointCount) noexcept
{
    if (jointCount == 0 || jointCount > kMaxSkinningJoints)
    {
        return false;
    }
    if (palette.skinningMatrices == nullptr || palette.matrixCount != jointCount)
    {
        return false;
    }
    if (!isFiniteArray(palette.skinningMatrices, jointCount * 16))
    {
        return false;
    }
    if (palette.debugJointModelMatrices != nullptr)
    {
        if (palette.debugJointModelMatrixCount != jointCount ||
            !isFiniteArray(palette.debugJointModelMatrices, jointCount * 16))
        {
            return false;
        }
    }
    else if (palette.debugJointModelMatrixCount != 0)
    {
        return false;
    }
    return true;
}

SkeletonHandle AnimationSystem::createSkeleton(const SkeletonDesc& desc)
{
    if (!validateSkeletonDesc(desc))
    {
        return {};
    }

    SkeletonRecord record{};
    record.handle.id = nextSkeletonId_++;
    if (nextSkeletonId_ == 0)
    {
        nextSkeletonId_ = 1;
    }
    record.joints.assign(desc.joints, desc.joints + desc.jointCount);
    record.active = true;
    skeletons_.push_back(record);
    return record.handle;
}

void AnimationSystem::destroySkeleton(const SkeletonHandle handle) noexcept
{
    if (!isValid(handle))
    {
        return;
    }
    for (SkeletonRecord& record : skeletons_)
    {
        if (record.active && record.handle.id == handle.id)
        {
            record.active = false;
            record.joints.clear();
            return;
        }
    }
}

bool AnimationSystem::validateSkinnedMeshDesc(const SkinnedMeshDesc& desc) const noexcept
{
    const SkeletonRecord* skeleton = findSkeleton(desc.skeleton);
    if (skeleton == nullptr || desc.vertices == nullptr || desc.vertexCount == 0 ||
        desc.indices == nullptr || desc.indexCount == 0 || (desc.indexCount % 3U) != 0U ||
        (desc.sectionCount > 0 && desc.sections == nullptr))
    {
        return false;
    }

    for (std::uint32_t vertexIndex = 0; vertexIndex < desc.vertexCount; ++vertexIndex)
    {
        const SkinnedMeshVertex& vertex = desc.vertices[vertexIndex];
        if (!isFiniteArray(vertex.position, 3) ||
            !hasUsableNormal(vertex.normal) ||
            !hasUsableTangent(vertex.tangent) ||
            !isUnitRangeColor(vertex.colorLinear, 4) ||
            !isFiniteArray(vertex.uv0, 2) ||
            !isFiniteArray(vertex.jointIndices, kMaxSkinningInfluences) ||
            !isFiniteArray(vertex.jointWeights, kMaxSkinningInfluences))
        {
            return false;
        }

        float weightSum = 0.0f;
        for (std::uint32_t influence = 0; influence < kMaxSkinningInfluences; ++influence)
        {
            const float jointValue = vertex.jointIndices[influence];
            const float roundedJoint = std::round(jointValue);
            if (std::fabs(jointValue - roundedJoint) > 0.001f ||
                roundedJoint < 0.0f ||
                static_cast<std::uint32_t>(roundedJoint) >= skeleton->joints.size() ||
                vertex.jointWeights[influence] < 0.0f)
            {
                return false;
            }
            weightSum += vertex.jointWeights[influence];
        }
        if (std::fabs(weightSum - 1.0f) > kWeightSumTolerance)
        {
            return false;
        }
    }

    for (std::uint32_t index = 0; index < desc.indexCount; ++index)
    {
        if (desc.indices[index] >= desc.vertexCount)
        {
            return false;
        }
    }

    for (std::uint32_t sectionIndex = 0; sectionIndex < desc.sectionCount; ++sectionIndex)
    {
        if (!isValidSkinnedMeshSection(desc.sections[sectionIndex], desc.indexCount))
        {
            return false;
        }
    }
    return true;
}

void AnimationSystem::registerSkinnedMesh(const SkinnedMeshHandle handle, const SkinnedMeshDesc& desc)
{
    const SkeletonRecord* skeleton = findSkeleton(desc.skeleton);
    if (!isValid(handle) || skeleton == nullptr)
    {
        return;
    }
    SkinnedMeshMetadata metadata{};
    metadata.handle = handle;
    metadata.skeleton = desc.skeleton;
    metadata.jointCount = static_cast<std::uint32_t>(skeleton->joints.size());
    metadata.sectionCount = desc.sectionCount > 0 ? desc.sectionCount : 1U;
    metadata.active = true;
    skinnedMeshes_.push_back(metadata);
}

void AnimationSystem::unregisterSkinnedMesh(const SkinnedMeshHandle handle) noexcept
{
    if (!isValid(handle))
    {
        return;
    }
    for (SkinnedMeshMetadata& metadata : skinnedMeshes_)
    {
        if (metadata.active && metadata.handle.id == handle.id)
        {
            metadata.active = false;
            return;
        }
    }
}

bool AnimationSystem::validateAnimatedDraws(const AnimatedDrawItem* draws, const std::uint32_t drawCount) const noexcept
{
    if (drawCount == 0)
    {
        return true;
    }
    if (draws == nullptr)
    {
        return false;
    }
    for (std::uint32_t drawIndex = 0; drawIndex < drawCount; ++drawIndex)
    {
        const AnimatedDrawItem& draw = draws[drawIndex];
        const SkinnedMeshMetadata* mesh = findSkinnedMesh(draw.mesh);
        if (mesh == nullptr ||
            findSkeleton(mesh->skeleton) == nullptr ||
            !isValid(draw.material) ||
            draw.sectionIndex >= mesh->sectionCount ||
            !isFiniteArray(draw.model, 16) ||
            !isValidAabb(draw.bounds) ||
            !scene::isValidFadeDesc(draw.fade) ||
            !validateSkinningPalette(draw.palette, mesh->jointCount))
        {
            return false;
        }
    }
    return true;
}

std::uint32_t AnimationSystem::appendDebugLines(
    const AnimatedDrawItem* draws,
    const std::uint32_t drawCount,
    const AnimationDebugOptions& options,
    std::vector<debug::DebugLineVertex>& outLines) const
{
    if (draws == nullptr || drawCount == 0 || (!options.drawBounds && !options.drawSkeletons))
    {
        return 0;
    }

    const std::uint32_t startCount = static_cast<std::uint32_t>(outLines.size());
    constexpr float kBoundsColor[4] = {0.95f, 0.85f, 0.25f, 1.0f};
    constexpr float kBoneColor[4] = {0.2f, 0.95f, 1.0f, 1.0f};
    const float origin[3] = {};

    for (std::uint32_t drawIndex = 0; drawIndex < drawCount; ++drawIndex)
    {
        const AnimatedDrawItem& draw = draws[drawIndex];
        const SkinnedMeshMetadata* mesh = findSkinnedMesh(draw.mesh);
        if (mesh == nullptr)
        {
            continue;
        }
        const SkeletonRecord* skeleton = findSkeleton(mesh->skeleton);
        if (skeleton == nullptr)
        {
            continue;
        }
        if (options.drawBounds && isValidAabb(draw.bounds))
        {
            debug::appendAabbDebugLines(draw.bounds, kBoundsColor, outLines);
        }
        if (!options.drawSkeletons ||
            draw.palette.debugJointModelMatrices == nullptr ||
            draw.palette.debugJointModelMatrixCount != mesh->jointCount)
        {
            continue;
        }

        for (std::uint32_t jointIndex = 0; jointIndex < mesh->jointCount; ++jointIndex)
        {
            const SkeletonJointDesc& joint = skeleton->joints[jointIndex];
            if (joint.parentIndex < 0)
            {
                continue;
            }
            const float* childLocal = draw.palette.debugJointModelMatrices + jointIndex * 16;
            const float* parentLocal = draw.palette.debugJointModelMatrices + static_cast<std::uint32_t>(joint.parentIndex) * 16;
            float childWorld[3] = {};
            float parentWorld[3] = {};
            float childPose[3] = {};
            float parentPose[3] = {};
            transformPoint(childLocal, origin, childPose);
            transformPoint(parentLocal, origin, parentPose);
            transformPoint(draw.model, childPose, childWorld);
            transformPoint(draw.model, parentPose, parentWorld);
            appendLine(outLines, parentWorld, childWorld, kBoneColor);
        }
    }

    return static_cast<std::uint32_t>(outLines.size()) - startCount;
}

void AnimationSystem::clear() noexcept
{
    skeletons_.clear();
    skinnedMeshes_.clear();
    nextSkeletonId_ = 1;
}

std::uint32_t AnimationSystem::liveSkeletonCount() const noexcept
{
    return static_cast<std::uint32_t>(std::count_if(
        skeletons_.begin(),
        skeletons_.end(),
        [](const SkeletonRecord& record) { return record.active; }));
}

std::uint32_t AnimationSystem::liveSkinnedMeshCount() const noexcept
{
    return static_cast<std::uint32_t>(std::count_if(
        skinnedMeshes_.begin(),
        skinnedMeshes_.end(),
        [](const SkinnedMeshMetadata& metadata) { return metadata.active; }));
}

const AnimationSystem::SkeletonRecord* AnimationSystem::findSkeleton(const SkeletonHandle handle) const noexcept
{
    if (!isValid(handle))
    {
        return nullptr;
    }
    for (const SkeletonRecord& record : skeletons_)
    {
        if (record.active && record.handle.id == handle.id)
        {
            return &record;
        }
    }
    return nullptr;
}

const SkinnedMeshMetadata* AnimationSystem::findSkinnedMesh(const SkinnedMeshHandle handle) const noexcept
{
    if (!isValid(handle))
    {
        return nullptr;
    }
    for (const SkinnedMeshMetadata& metadata : skinnedMeshes_)
    {
        if (metadata.active && metadata.handle.id == handle.id)
        {
            return &metadata;
        }
    }
    return nullptr;
}

bool validateSkeletonDescriptorForTests(const SkeletonDesc& desc) noexcept
{
    return AnimationSystem::validateSkeletonDesc(desc);
}

bool validateSkinningPaletteForTests(const SkinningPaletteDesc& palette, const std::uint32_t jointCount) noexcept
{
    return AnimationSystem::validateSkinningPalette(palette, jointCount);
}
} // namespace full_renderer::animation
