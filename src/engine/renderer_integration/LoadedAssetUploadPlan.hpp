#pragma once

#include "engine/assets/LoadedAssetPayload.hpp"
#include "full_renderer/Renderer.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace full_engine
{
/** @brief Planning status for one loaded asset payload upload work record. */
enum class LoadedAssetUploadStatus
{
    /** @brief The payload was translated into renderer-facing upload work. */
    Planned,

    /** @brief The payload failed renderer-free loaded payload validation. */
    InvalidPayload,

    /** @brief The payload kind is not supported by this upload planner. */
    UnsupportedKind,

    /** @brief The payload is valid but outside the current renderer upload contract. */
    UnsupportedRendererContract,
};

/** @brief Returns a stable diagnostic name for a loaded asset upload status. */
const char* loadedAssetUploadStatusName(LoadedAssetUploadStatus status) noexcept;

/**
 * @brief Owned mesh upload work plus a renderer descriptor view.
 *
 * The vectors own the CPU data referenced by `desc`. The descriptor pointers
 * remain valid while this work item is alive and unmoved. This value does not
 * create renderer resources and does not call renderer APIs.
 */
struct LoadedMeshUploadWork
{
    /** @brief Asset ID copied from the loaded mesh payload. */
    AssetId id = {};

    /** @brief Owned renderer-facing fixed-format vertex data. */
    std::vector<full_renderer::MeshVertex> vertices;

    /** @brief Owned renderer-facing 16-bit triangle indices. */
    std::vector<std::uint16_t> indices;

    /** @brief Descriptor view pointing into `vertices` and `indices`. */
    full_renderer::MeshDesc desc = {};
};

/**
 * @brief Owned texture upload work plus a renderer descriptor view.
 *
 * The byte vector owns the CPU data referenced by `desc.data`. The descriptor
 * pointer remains valid while this work item is alive and unmoved.
 */
struct LoadedTextureUploadWork
{
    /** @brief Asset ID copied from the loaded texture payload. */
    AssetId id = {};

    /** @brief Owned renderer-facing texture bytes. */
    std::vector<std::uint8_t> bytes;

    /** @brief Descriptor view pointing into `bytes`. */
    full_renderer::TextureDesc desc = {};
};

/**
 * @brief Renderer-facing material upload expectation without renderer handles.
 *
 * Materials preserve named texture asset references because a later resolver
 * must map those IDs to renderer texture handles before a `MaterialDesc` can
 * be submitted to the renderer.
 */
struct LoadedMaterialUploadWork
{
    /** @brief Asset ID copied from the loaded material payload. */
    AssetId id = {};

    /** @brief Renderer material kind mapped from the loaded material model. */
    full_renderer::MaterialKind kind = full_renderer::MaterialKind::Basic;

    /** @brief Renderer alpha policy mapped from the loaded material payload. */
    full_renderer::MaterialAlphaMode alphaMode = full_renderer::MaterialAlphaMode::Opaque;

    /** @brief Named texture asset references that still need renderer handle resolution. */
    std::vector<AssetSourceMaterialTextureRef> textureRefs;
};

/**
 * @brief Owned skeleton upload work plus a renderer descriptor view.
 *
 * The joint vector owns the CPU data referenced by `desc.joints`. The
 * descriptor pointer remains valid while this work item is alive and unmoved.
 */
struct LoadedSkeletonUploadWork
{
    /** @brief Asset ID copied from the loaded skeleton payload. */
    AssetId id = {};

    /** @brief Owned renderer-facing skeleton joint descriptors. */
    std::vector<full_renderer::SkeletonJointDesc> joints;

    /** @brief Descriptor view pointing into `joints`. */
    full_renderer::SkeletonDesc desc = {};
};

/**
 * @brief Owned skinned mesh upload work plus a renderer descriptor view.
 *
 * The vertex/index vectors own the CPU data referenced by `desc`. The
 * descriptor pointer fields remain valid while this work item is alive and
 * unmoved. `desc.skeleton` is filled by the executor after resolving
 * `skeletonAssetId` in a renderer handle catalog.
 */
struct LoadedSkinnedMeshUploadWork
{
    /** @brief Asset ID copied from the loaded skinned mesh payload. */
    AssetId id = {};

    /** @brief Skeleton asset ID that must resolve to a renderer skeleton handle. */
    AssetId skeletonAssetId = {};

    /** @brief Owned renderer-facing skinned vertex data. */
    std::vector<full_renderer::SkinnedMeshVertex> vertices;

    /** @brief Owned renderer-facing 16-bit triangle indices. */
    std::vector<std::uint16_t> indices;

    /** @brief Owned renderer-facing skinned mesh draw sections. */
    std::vector<full_renderer::SkinnedMeshSectionDesc> sections;

    /** @brief Descriptor view pointing into `vertices` and `indices`. */
    full_renderer::SkinnedMeshDesc desc = {};
};

/** @brief Ordered upload planning record for one loaded asset payload. */
struct LoadedAssetUploadRecord
{
    /** @brief Asset ID copied from the active payload when available. */
    AssetId id = {};

    /** @brief Active payload kind. */
    AssetKind kind = AssetKind::Unknown;

    /** @brief Upload work planning outcome. */
    LoadedAssetUploadStatus status = LoadedAssetUploadStatus::InvalidPayload;

    /** @brief Mesh upload work when `kind` is Mesh and `status` is Planned. */
    LoadedMeshUploadWork mesh = {};

    /** @brief Texture upload work when `kind` is Texture and `status` is Planned. */
    LoadedTextureUploadWork texture = {};

    /** @brief Material upload expectation when `kind` is Material and `status` is Planned. */
    LoadedMaterialUploadWork material = {};

    /** @brief Skeleton upload work when `kind` is Skeleton and `status` is Planned. */
    LoadedSkeletonUploadWork skeleton = {};

    /** @brief Skinned mesh upload work when `kind` is SkinnedMesh and `status` is Planned. */
    LoadedSkinnedMeshUploadWork skinnedMesh = {};
};

/** @brief Aggregate counters for loaded asset upload planning. */
struct LoadedAssetUploadSummary
{
    /** @brief Number of payloads translated successfully. */
    std::size_t plannedCount = 0;

    /** @brief Number of payloads rejected by loaded payload validation. */
    std::size_t invalidPayloadCount = 0;

    /** @brief Number of payloads with unsupported active kinds. */
    std::size_t unsupportedKindCount = 0;

    /** @brief Number of valid payloads outside the current renderer upload contract. */
    std::size_t unsupportedRendererContractCount = 0;
};

/** @brief Ordered loaded asset upload planning result. */
struct LoadedAssetUploadPlan
{
    /** @brief One upload planning record per supplied payload, in source order. */
    std::vector<LoadedAssetUploadRecord> records;

    /** @brief Aggregate upload planning counters. */
    LoadedAssetUploadSummary summary = {};
};

/**
 * @brief Converts validated loaded asset payloads into renderer upload work.
 *
 * The returned plan owns copied CPU data needed by renderer descriptor views.
 * Descriptor pointers remain valid while the containing work item is alive and
 * unmoved. The function performs no file IO, renderer calls, renderer handle
 * lookup, load-state mutation, queue mutation, or renderer-resource creation.
 *
 * @param payloads Caller-owned loaded payload array. May be null only when
 * `payloadCount` is zero.
 * @param payloadCount Number of payloads to inspect.
 * @return Ordered upload work records and summary counters.
 */
LoadedAssetUploadPlan buildLoadedAssetUploadPlan(
    const LoadedAssetPayload* payloads,
    std::size_t payloadCount);
} // namespace full_engine
