#pragma once

#include "engine/assets/AssimpRigidNodeSceneImporter.hpp"
#include "engine/assets/LoadedAnimationSampler.hpp"
#include "engine/renderer_integration/TerrainAssetResolver.hpp"

#include "full_renderer/Renderer.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Status for building one rigid node-attached draw item. */
enum class RigidNodeAttachmentDrawStatus
{
    Built,
    InvalidArgument,
    InvalidPose,
    MissingMeshHandle,
    InvalidMaterialHandle,
    JointOutOfRange,
};

/** @brief Ordered diagnostic record for one rigid attachment draw build. */
struct RigidNodeAttachmentDrawRecord
{
    /** @brief Source mesh asset ID. */
    AssetId meshAssetId = {};

    /** @brief Attachment joint index. */
    std::uint16_t jointIndex = 0;

    /** @brief Draw build status. */
    RigidNodeAttachmentDrawStatus status = RigidNodeAttachmentDrawStatus::InvalidArgument;
};

/** @brief Aggregate counters for rigid attachment draw building. */
struct RigidNodeAttachmentDrawSummary
{
    std::size_t builtCount = 0;
    std::size_t invalidArgumentCount = 0;
    std::size_t invalidPoseCount = 0;
    std::size_t missingMeshHandleCount = 0;
    std::size_t invalidMaterialHandleCount = 0;
    std::size_t jointOutOfRangeCount = 0;
};

/** @brief Value result for rigid node-attached draw building. */
struct RigidNodeAttachmentDrawResult
{
    /** @brief One diagnostic record per supplied attachment, in source order. */
    std::vector<RigidNodeAttachmentDrawRecord> records;

    /** @brief Built static draw items for successfully resolved attachments. */
    std::vector<full_renderer::DrawItem> draws;

    /** @brief Aggregate draw build counters. */
    RigidNodeAttachmentDrawSummary summary = {};
};

/** @brief Returns a stable diagnostic name for a rigid attachment draw status. */
const char* rigidNodeAttachmentDrawStatusName(
    RigidNodeAttachmentDrawStatus status) noexcept;

/**
 * @brief Builds static draw items for rigid meshes attached to sampled animation joints.
 *
 * Each attachment mesh is resolved through `handles` and driven by
 * `pose.modelMatrices[attachment.jointIndex]`. `material` is applied to every
 * emitted draw. `worldTransformColumnMajor` may be null for identity, or point
 * to a caller-owned column-major 4x4 matrix multiplied before the sampled node
 * model. The result owns copied draw descriptors only. It performs no renderer
 * calls, resource creation, resource destruction, animation sampling, file IO,
 * or handle catalog mutation.
 */
RigidNodeAttachmentDrawResult buildRigidNodeAttachmentDraws(
    const AssimpRigidNodeMeshAttachment* attachments,
    std::size_t attachmentCount,
    const RendererAssetHandleCatalog& handles,
    const LoadedAnimationPose& pose,
    full_renderer::MaterialHandle material,
    const float* worldTransformColumnMajor = nullptr);
} // namespace full_engine
