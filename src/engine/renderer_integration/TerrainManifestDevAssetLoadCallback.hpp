#pragma once

#include "engine/assets/AssetSourceCatalog.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadCompletionAdapter.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadExecutor.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadJobWorkPackets.hpp"

#include "full_renderer/Renderer.hpp"

namespace full_engine
{
/**
 * @brief Caller-owned context for the dev manifest asset-load callback.
 *
 * The context bridges retained manifest load intent to the tiny dev asset
 * importer path. `sources` maps asset IDs to direct filesystem dev source
 * records. `renderer` owns the public renderer upload thread/validity
 * contract. `completedHandles` receives mesh/texture handles created by this
 * callback so existing completion-inbox and reconcile helpers can later copy
 * them into the runtime handle catalog. `alreadyLoadedHandles` is optional and
 * is checked before importing so callers can satisfy requests from an existing
 * runtime catalog.
 *
 * The context is read only except for renderer uploads and
 * `completedHandles` mutation. It is not retained by the callback or worker
 * helper and is not thread-safe for shared mutable objects.
 */
struct TerrainManifestDevAssetLoadContext
{
    /** @brief Source metadata catalog. Required for mesh/texture imports. */
    const AssetSourceCatalog* sources = nullptr;

    /** @brief Caller-owned renderer used only for mesh/texture upload execution. */
    full_renderer::IRenderer* renderer = nullptr;

    /** @brief Caller-owned completed-handle catalog populated by successful dev uploads. */
    RendererAssetHandleCatalog* completedHandles = nullptr;

    /** @brief Optional caller-owned catalog checked before importing or uploading. */
    const RendererAssetHandleCatalog* alreadyLoadedHandles = nullptr;
};

/**
 * @brief Result from running the dev loader over scheduled manifest load jobs.
 *
 * The result copies work-packet diagnostics and completion publish diagnostics.
 * It does not retain source records, payload bytes, renderer descriptors,
 * renderer resources, handle catalogs, workers, or job queue storage.
 */
struct TerrainManifestDevAssetLoadWorkerResult
{
    /** @brief Scheduled-job packet conversion diagnostics. */
    TerrainManifestAssetLoadJobWorkPacketResult packets = {};

    /** @brief Completion publish diagnostics after invoking the dev callback. */
    TerrainManifestAssetLoadWorkerCompletionPublishResult publish = {};
};

/**
 * @brief Dev-only manifest asset-load callback that imports and uploads tiny source files.
 *
 * Mesh and texture requests are resolved through `TerrainManifestDevAssetLoadContext::sources`,
 * imported with the dev loaded-asset importer, planned as loaded upload work,
 * uploaded through the caller-owned renderer, and stored in
 * `completedHandles`. Material requests import dev material payloads and
 * resolve texture asset references through the existing or completed handle
 * catalogs before calling the renderer material upload path.
 *
 * Missing source records return `Missing` so callers can retry after source
 * metadata is supplied. Import, descriptor, payload, upload-plan, renderer, or
 * catalog failures return `Failed`. The function does not consume load
 * requests, remove jobs, apply runtime queues, perform production package IO,
 * start async work, or own renderer resource lifetime.
 */
TerrainManifestAssetLoadCallbackResult terrainManifestDevAssetLoadCallback(
    const TerrainManifestAssetLoadRequest& request,
    void* userData);

/**
 * @brief Runs the dev callback for scheduled manifest load jobs and publishes completions.
 *
 * The helper packetizes valid `ManifestAssetLoad` jobs, invokes
 * `terrainManifestDevAssetLoadCallback` for each packet in queue order, and
 * publishes the resulting completion records into `destination`. It does not
 * mutate the job queue, consume manifest load requests, reconcile completions,
 * apply terrain runtime queues, or own renderer resources.
 */
TerrainManifestDevAssetLoadWorkerResult runTerrainManifestDevAssetLoadWorker(
    const EngineJobQueue& scheduledJobs,
    TerrainManifestAssetLoadCompletionInbox& destination,
    TerrainManifestDevAssetLoadContext& context);
} // namespace full_engine
