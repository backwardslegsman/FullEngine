#include "engine/renderer_integration/RigidNodeAttachmentDrawBuilder.hpp"

#include <algorithm>
#include <cmath>

namespace full_engine
{
namespace
{
void identity(float target[16]) noexcept
{
    for (int index = 0; index < 16; ++index)
    {
        target[index] = 0.0f;
    }
    target[0] = 1.0f;
    target[5] = 1.0f;
    target[10] = 1.0f;
    target[15] = 1.0f;
}

bool finiteMatrix(const float* matrix) noexcept
{
    if (matrix == nullptr)
    {
        return false;
    }
    for (int index = 0; index < 16; ++index)
    {
        if (!std::isfinite(matrix[index]))
        {
            return false;
        }
    }
    return true;
}

void multiplyColumnMajor(const float* lhs, const float* rhs, float* target) noexcept
{
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            target[column * 4 + row] =
                lhs[0 * 4 + row] * rhs[column * 4 + 0] +
                lhs[1 * 4 + row] * rhs[column * 4 + 1] +
                lhs[2 * 4 + row] * rhs[column * 4 + 2] +
                lhs[3 * 4 + row] * rhs[column * 4 + 3];
        }
    }
}

void transformPoint(const float* matrix, const float source[3], float target[3]) noexcept
{
    target[0] = matrix[0] * source[0] + matrix[4] * source[1] + matrix[8] * source[2] + matrix[12];
    target[1] = matrix[1] * source[0] + matrix[5] * source[1] + matrix[9] * source[2] + matrix[13];
    target[2] = matrix[2] * source[0] + matrix[6] * source[1] + matrix[10] * source[2] + matrix[14];
}

full_renderer::Aabb transformBounds(
    const AssetSourceBounds& source,
    const float* matrix) noexcept
{
    full_renderer::Aabb result;
    bool first = true;
    for (int x = 0; x < 2; ++x)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int z = 0; z < 2; ++z)
            {
                const float corner[3] = {
                    x == 0 ? source.min[0] : source.max[0],
                    y == 0 ? source.min[1] : source.max[1],
                    z == 0 ? source.min[2] : source.max[2]};
                float transformed[3] = {};
                transformPoint(matrix, corner, transformed);
                if (first)
                {
                    result.min[0] = transformed[0];
                    result.min[1] = transformed[1];
                    result.min[2] = transformed[2];
                    result.max[0] = transformed[0];
                    result.max[1] = transformed[1];
                    result.max[2] = transformed[2];
                    first = false;
                }
                else
                {
                    result.min[0] = (std::min)(result.min[0], transformed[0]);
                    result.min[1] = (std::min)(result.min[1], transformed[1]);
                    result.min[2] = (std::min)(result.min[2], transformed[2]);
                    result.max[0] = (std::max)(result.max[0], transformed[0]);
                    result.max[1] = (std::max)(result.max[1], transformed[1]);
                    result.max[2] = (std::max)(result.max[2], transformed[2]);
                }
            }
        }
    }
    return result;
}

bool poseMatricesValid(const LoadedAnimationPose& pose) noexcept
{
    if (!isValid(pose.skeletonAssetId) ||
        pose.jointCount == 0 ||
        pose.modelMatrices.size() != static_cast<std::size_t>(pose.jointCount) * 16U)
    {
        return false;
    }
    return finiteMatrix(pose.modelMatrices.data());
}

void incrementSummary(
    RigidNodeAttachmentDrawSummary& summary,
    const RigidNodeAttachmentDrawStatus status) noexcept
{
    switch (status)
    {
    case RigidNodeAttachmentDrawStatus::Built:
        ++summary.builtCount;
        break;
    case RigidNodeAttachmentDrawStatus::InvalidArgument:
        ++summary.invalidArgumentCount;
        break;
    case RigidNodeAttachmentDrawStatus::InvalidPose:
        ++summary.invalidPoseCount;
        break;
    case RigidNodeAttachmentDrawStatus::MissingMeshHandle:
        ++summary.missingMeshHandleCount;
        break;
    case RigidNodeAttachmentDrawStatus::InvalidMaterialHandle:
        ++summary.invalidMaterialHandleCount;
        break;
    case RigidNodeAttachmentDrawStatus::JointOutOfRange:
        ++summary.jointOutOfRangeCount;
        break;
    }
}
} // namespace

const char* rigidNodeAttachmentDrawStatusName(
    const RigidNodeAttachmentDrawStatus status) noexcept
{
    switch (status)
    {
    case RigidNodeAttachmentDrawStatus::Built:
        return "Built";
    case RigidNodeAttachmentDrawStatus::InvalidArgument:
        return "InvalidArgument";
    case RigidNodeAttachmentDrawStatus::InvalidPose:
        return "InvalidPose";
    case RigidNodeAttachmentDrawStatus::MissingMeshHandle:
        return "MissingMeshHandle";
    case RigidNodeAttachmentDrawStatus::InvalidMaterialHandle:
        return "InvalidMaterialHandle";
    case RigidNodeAttachmentDrawStatus::JointOutOfRange:
        return "JointOutOfRange";
    }
    return "Unknown";
}

RigidNodeAttachmentDrawResult buildRigidNodeAttachmentDraws(
    const AssimpRigidNodeMeshAttachment* const attachments,
    const std::size_t attachmentCount,
    const RendererAssetHandleCatalog& handles,
    const LoadedAnimationPose& pose,
    const full_renderer::MaterialHandle material,
    const float* const worldTransformColumnMajor)
{
    RigidNodeAttachmentDrawResult result;
    if (attachments == nullptr && attachmentCount > 0)
    {
        result.summary.invalidArgumentCount = attachmentCount;
        return result;
    }

    float identityWorld[16] = {};
    identity(identityWorld);
    const float* const world = worldTransformColumnMajor != nullptr ?
        worldTransformColumnMajor :
        identityWorld;

    result.records.reserve(attachmentCount);
    result.draws.reserve(attachmentCount);
    const bool poseValid = poseMatricesValid(pose);
    const bool materialValid = full_renderer::isValid(material);
    const bool worldValid = finiteMatrix(world);

    for (std::size_t index = 0; index < attachmentCount; ++index)
    {
        const AssimpRigidNodeMeshAttachment& attachment = attachments[index];
        RigidNodeAttachmentDrawRecord record;
        record.meshAssetId = attachment.meshAssetId;
        record.jointIndex = attachment.jointIndex;

        if (!isValid(attachment.meshAssetId) || !worldValid)
        {
            record.status = RigidNodeAttachmentDrawStatus::InvalidArgument;
        }
        else if (!poseValid)
        {
            record.status = RigidNodeAttachmentDrawStatus::InvalidPose;
        }
        else if (!materialValid)
        {
            record.status = RigidNodeAttachmentDrawStatus::InvalidMaterialHandle;
        }
        else if (attachment.jointIndex >= pose.jointCount)
        {
            record.status = RigidNodeAttachmentDrawStatus::JointOutOfRange;
        }
        else
        {
            const full_renderer::MeshHandle* const mesh =
                handles.findMeshHandle(attachment.meshAssetId);
            if (mesh == nullptr)
            {
                record.status = RigidNodeAttachmentDrawStatus::MissingMeshHandle;
            }
            else
            {
                const float* const jointModel =
                    pose.modelMatrices.data() + static_cast<std::size_t>(attachment.jointIndex) * 16U;
                full_renderer::DrawItem draw;
                draw.mesh = *mesh;
                draw.material = material;
                multiplyColumnMajor(world, jointModel, draw.model);
                draw.bounds = transformBounds(attachment.mesh.localBounds, draw.model);
                draw.castsShadow = true;
                result.draws.push_back(draw);
                record.status = RigidNodeAttachmentDrawStatus::Built;
            }
        }

        incrementSummary(result.summary, record.status);
        result.records.push_back(record);
    }

    return result;
}
} // namespace full_engine
