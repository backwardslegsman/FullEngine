#pragma once

#include "engine/renderer_integration/LoadedAssetUploadPlan.hpp"
#include "engine/renderer_integration/TerrainAssetResolver.hpp"

#include "full_renderer/Renderer.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Execution status for one loaded asset upload-plan record. */
enum class LoadedAssetUploadExecuteStatus
{
    /** @brief Renderer upload succeeded and the produced handle was added to the catalog. */
    Uploaded,

    /** @brief A handle for this asset ID and kind was already present, so no upload ran. */
    AlreadyMapped,

    /** @brief A material texture asset reference did not have a renderer texture handle yet. */
    MissingTextureHandle,

    /** @brief A skinned mesh skeleton asset reference did not have a renderer skeleton handle yet. */
    MissingSkeletonHandle,

    /** @brief The source upload-plan record was not planned and no upload was attempted. */
    SkippedUnplanned,

    /** @brief The renderer returned an invalid handle for an attempted upload. */
    RendererFailed,

    /** @brief The renderer returned a handle, but catalog insertion rejected it. */
    CatalogRejected,

    /** @brief The planned record kind is not supported by this executor. */
    UnsupportedKind,
};

/** @brief Returns a stable diagnostic name for an upload execution status. */
const char* loadedAssetUploadExecuteStatusName(
    LoadedAssetUploadExecuteStatus status) noexcept;

/**
 * @brief Ordered diagnostic record for one loaded asset upload execution.
 *
 * Mesh and texture handles are copied from successful renderer uploads. The
 * record does not own renderer resources; caller-owned renderer lifetime and
 * eventual destruction remain external to this executor.
 */
struct LoadedAssetUploadExecuteRecord
{
    /** @brief Asset ID copied from the source upload-plan record. */
    AssetId id = {};

    /** @brief Asset kind copied from the source upload-plan record. */
    AssetKind kind = AssetKind::Unknown;

    /** @brief Source upload planning status. */
    LoadedAssetUploadStatus sourceStatus = LoadedAssetUploadStatus::InvalidPayload;

    /** @brief Upload execution outcome. */
    LoadedAssetUploadExecuteStatus status = LoadedAssetUploadExecuteStatus::SkippedUnplanned;

    /** @brief Mesh handle produced or found when `kind` is Mesh. */
    full_renderer::MeshHandle mesh = {};

    /** @brief Texture handle produced or found when `kind` is Texture. */
    full_renderer::TextureHandle texture = {};

    /** @brief Material handle produced or found when `kind` is Material. */
    full_renderer::MaterialHandle material = {};

    /** @brief Skeleton handle produced or found when `kind` is Skeleton. */
    full_renderer::SkeletonHandle skeleton = {};

    /** @brief Skinned mesh handle produced or found when `kind` is SkinnedMesh. */
    full_renderer::SkinnedMeshHandle skinnedMesh = {};

    /** @brief Result from inserting a produced handle into the renderer asset catalog. */
    RendererAssetHandleCatalogResult catalogResult = RendererAssetHandleCatalogResult::NotFound;
};

/** @brief Aggregate counters for loaded asset upload execution. */
struct LoadedAssetUploadExecuteSummary
{
    /** @brief Number of mesh uploads that produced and cataloged renderer handles. */
    std::size_t uploadedMeshCount = 0;

    /** @brief Number of texture uploads that produced and cataloged renderer handles. */
    std::size_t uploadedTextureCount = 0;

    /** @brief Number of material uploads that produced and cataloged renderer handles. */
    std::size_t uploadedMaterialCount = 0;

    /** @brief Number of skeleton uploads that produced and cataloged renderer handles. */
    std::size_t uploadedSkeletonCount = 0;

    /** @brief Number of skinned mesh uploads that produced and cataloged renderer handles. */
    std::size_t uploadedSkinnedMeshCount = 0;

    /** @brief Number of records skipped because the catalog already had a mapping. */
    std::size_t alreadyMappedCount = 0;

    /** @brief Number of material records waiting for texture handles. */
    std::size_t missingTextureHandleCount = 0;

    /** @brief Number of skinned mesh records waiting for skeleton handles. */
    std::size_t missingSkeletonHandleCount = 0;

    /** @brief Number of records skipped because the source upload plan did not plan them. */
    std::size_t skippedUnplannedCount = 0;

    /** @brief Number of attempted uploads that returned invalid renderer handles. */
    std::size_t rendererFailedCount = 0;

    /** @brief Number of produced handles rejected by the handle catalog. */
    std::size_t catalogRejectedCount = 0;

    /** @brief Number of planned records with unsupported active kinds. */
    std::size_t unsupportedKindCount = 0;
};

/** @brief Ordered result from executing a loaded asset upload plan. */
struct LoadedAssetUploadExecuteResult
{
    /** @brief One execution record per upload-plan record, in source order. */
    std::vector<LoadedAssetUploadExecuteRecord> records;

    /** @brief Aggregate execution counters. */
    LoadedAssetUploadExecuteSummary summary = {};
};

/**
 * @brief Executes planned loaded mesh, texture, material, skeleton, and skinned mesh uploads through public renderer APIs.
 *
 * The function is synchronous and must be called on the renderer owner thread
 * at a point where `IRenderer::createMesh`, `IRenderer::createTexture`, and
 * `IRenderer::createMaterial`, `IRenderer::createSkeleton`, and
 * `IRenderer::createSkinnedMesh` are valid. Successful handles are copied into
 * `handles`. Existing mappings are preserved and are not replaced. Material
 * texture asset IDs and skinned mesh skeleton asset IDs are resolved against
 * `handles`; missing mappings are reported as retryable diagnostics and do not
 * call the dependent renderer create function.
 *
 * This executor does not mutate load state, job queues, manifests, source
 * catalogs, or terrain runtime state. It also does not roll back renderer
 * resources if catalog insertion fails, because renderer resource destruction
 * remains caller-owned at this layer.
 *
 * @param renderer Caller-owned initialized renderer.
 * @param plan Caller-owned upload plan. Descriptor views must still be valid.
 * @param handles Caller-owned renderer asset handle catalog to receive new mappings.
 * @return Ordered execution diagnostics and summary counters.
 */
LoadedAssetUploadExecuteResult executeLoadedAssetUploadPlan(
    full_renderer::IRenderer& renderer,
    const LoadedAssetUploadPlan& plan,
    RendererAssetHandleCatalog& handles);
} // namespace full_engine
