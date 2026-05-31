#pragma once

#include "engine/renderer_integration/TerrainIntegrationDiagnostics.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadExecutor.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoader.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadPlan.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadRequests.hpp"
#include "engine/renderer_integration/TerrainManifestAssetReadiness.hpp"

#include <cstddef>

namespace full_engine
{
/**
 * @brief State-level result for staging a retained cooked terrain manifest.
 *
 * `NoManifest` means no manifest value has been supplied to the load state.
 * Runtime queues, catalogs, renderer handles, and renderer resources are not
 * mutated when this status is returned.
 */
enum class TerrainManifestLoadStageStatus
{
    /** @brief A manifest was retained and the staging coordinator ran. */
    Success,

    /** @brief No retained manifest was available, so no staging work was attempted. */
    NoManifest,
};

/** @brief Returns a stable diagnostic name for manifest load-state staging. */
const char* terrainManifestLoadStageStatusName(TerrainManifestLoadStageStatus status) noexcept;

/**
 * @brief Result of staging or queueing the retained cooked manifest value.
 *
 * The nested stage result is copied from the manifest runtime staging
 * coordinator when a manifest is available, or default-initialized when no
 * manifest has been loaded.
 */
struct TerrainManifestLoadStageResult
{
    /** @brief State-level staging status. */
    TerrainManifestLoadStageStatus status = TerrainManifestLoadStageStatus::Success;

    /** @brief Detailed coordinator result, or default values when no manifest was retained. */
    TerrainManifestRuntimeStageResult stage = {};
};

/**
 * @brief In-memory holder for a cooked manifest and latest loading diagnostics.
 *
 * The state owns a renderer-free manifest value, latest readiness/load/staging
 * value results, and a pending renderer-free load request queue. It does not
 * own file paths, renderer handles, renderer resources, world registries,
 * terrain resource catalogs, terrain runtime queues, or UI state.
 *
 * @note Thread safety: Not thread-safe. Callers must serialize access.
 */
class TerrainManifestLoadState
{
public:
    /** @brief Stores a manifest value by move and clears stale diagnostics and pending load intent. */
    void setManifest(CookedAssetManifest manifest);

    /** @brief Clears the retained manifest and resets latest diagnostics and pending load intent. */
    void clearManifest();

    /** @brief Returns whether a manifest value is currently retained. */
    bool hasManifest() const noexcept;

    /**
     * @brief Returns the retained manifest, or an empty default value when none is set.
     *
     * The returned reference is invalidated by `setManifest` or `clearManifest`.
     */
    const CookedAssetManifest& manifest() const noexcept;

    /** @brief Returns the latest manifest runtime staging result. */
    const TerrainManifestRuntimeStageResult& latestStage() const noexcept;

    /** @brief Returns copied diagnostics from the latest staging result. */
    const TerrainManifestRuntimeStageDiagnostics& latestDiagnostics() const noexcept;

    /** @brief Returns the latest retained manifest asset handle readiness plan. */
    const TerrainManifestAssetReadinessPlan& latestReadiness() const noexcept;

    /** @brief Returns the latest renderer-free asset load request plan. */
    const TerrainManifestAssetLoadRequestPlan& latestLoadRequests() const noexcept;

    /** @brief Returns the retained pending asset load request queue. */
    const TerrainManifestAssetLoadRequestQueue& loadRequestQueue() const noexcept;

    /** @brief Returns the latest result from queueing load requests into the retained queue. */
    const TerrainManifestAssetLoadQueuePushResult& latestLoadRequestQueueResult() const noexcept;

    /** @brief Returns the latest result from consuming retained load requests. */
    const TerrainManifestAssetLoadResult& latestLoadConsumeResult() const noexcept;

    /** @brief Returns the latest callback-executor result for retained load requests. */
    const TerrainManifestAssetLoadExecutorResult& latestLoadExecutorResult() const noexcept;

    /** @brief Returns the number of pending retained asset load requests. */
    std::size_t pendingLoadRequestCount() const noexcept;

    /**
     * @brief Plans renderer handle readiness for the retained manifest value.
     *
     * This compares manifest-declared mesh/material/texture references against
     * externally supplied handle mappings. It never loads files, creates
     * renderer resources, mutates handle catalogs, or changes runtime state.
     * When no manifest is retained, it stores and returns an empty plan.
     *
     * @param handles Caller-owned renderer handle catalog to inspect. The
     * catalog is not retained or mutated.
     * @return Retained value plan owned by this state until the next readiness
     * or manifest mutation call.
     */
    const TerrainManifestAssetReadinessPlan& planAssetReadiness(
        const RendererAssetHandleCatalog& handles);

    /**
     * @brief Converts latest missing readiness records into renderer-free load intent.
     *
     * This uses the latest readiness plan already stored on the state. It does
     * not inspect files, create renderer resources, mutate handle catalogs, or
     * enqueue async jobs.
     *
     * @return Retained value plan owned by this state until the next load-plan
     * or manifest mutation call.
     */
    const TerrainManifestAssetLoadRequestPlan& planAssetLoadRequests();

    /**
     * @brief Queues latest load intent records into the retained pending queue.
     *
     * Repeated calls deduplicate requests already pending in the queue. The
     * queue stores intent only and does not perform IO, start jobs, create
     * renderer resources, or mutate handle catalogs.
     *
     * @return Retained queueing diagnostics owned by this state until the next
     * queue or manifest mutation call.
     */
    const TerrainManifestAssetLoadQueuePushResult& queueLatestAssetLoadRequests();

    /**
     * @brief Consumes retained pending load requests using externally supplied handles.
     *
     * Handles are copied from `sourceHandles` into `destinationHandles` through
     * the manifest asset load adapter. The state stores only the ordered
     * diagnostics. It does not own either handle catalog, create renderer
     * resources, perform IO, start jobs, or mutate the retained manifest.
     * Pending requests are cleared only if the adapter consumes the whole
     * batch.
     *
     * @param sourceHandles Caller-owned catalog of externally supplied renderer
     * handles. Read only for the duration of the call.
     * @param destinationHandles Caller-owned runtime catalog to receive copied
     * handle mappings.
     * @return Retained consume diagnostics owned by this state until the next
     * consume or manifest mutation call.
     */
    const TerrainManifestAssetLoadResult& consumePendingAssetLoadRequests(
        const RendererAssetHandleCatalog& sourceHandles,
        RendererAssetHandleCatalog& destinationHandles);

    /**
     * @brief Executes retained pending load requests through a caller callback.
     *
     * This delegates to the manifest asset load executor, stores the ordered
     * callback diagnostics, and stores the nested consume diagnostics. The
     * state does not own the callback, callback data, destination handle
     * catalog, renderer resources, IO, or async work.
     *
     * @param destinationHandles Caller-owned runtime catalog to receive loaded
     * mappings when the whole batch succeeds.
     * @param callback Caller-owned synchronous handle provider.
     * @param userData Opaque caller data passed through to `callback`.
     * @return Retained executor diagnostics owned by this state until the next
     * execute or manifest mutation call.
     */
    const TerrainManifestAssetLoadExecutorResult& executePendingAssetLoadRequests(
        RendererAssetHandleCatalog& destinationHandles,
        TerrainManifestAssetLoadCallback callback,
        void* userData = nullptr);

    /**
     * @brief Dry-runs the retained manifest against current terrain setup state.
     *
     * This never queues runtime requests, applies queues, mutates catalogs,
     * calls renderer APIs, creates renderer resources, or loads assets. The
     * supplied world descriptor array is read only for the duration of the call.
     *
     * @param handles Caller-owned renderer handle catalog used for asset
     * readiness/resource resolution. The catalog is not retained or mutated.
     * @param registry Current world chunk residency state.
     * @param worldCatalog Current world chunk metadata catalog.
     * @param resources Current terrain resource catalog.
     * @param worldDescs Caller-owned array of world chunk descriptors that can
     * be matched with manifest terrain chunk IDs. May be null when
     * `worldDescCount` is zero.
     * @param worldDescCount Number of descriptors in `worldDescs`.
     * @return State-level result plus retained staging diagnostics available
     * through `latestStage` and `latestDiagnostics`.
     */
    TerrainManifestLoadStageResult stage(
        const RendererAssetHandleCatalog& handles,
        const WorldChunkRegistry& registry,
        const WorldChunkCatalog& worldCatalog,
        const TerrainResourceCatalog& resources,
        const WorldChunkDesc* worldDescs,
        std::size_t worldDescCount);

    /**
     * @brief Queues safe setup intent from the retained manifest into runtime state.
     *
     * This appends setup and optional make-resident requests through the
     * manifest staging coordinator when the plan is safe. It does not apply the
     * queued requests or mutate registries, catalogs, renderer handles, or
     * renderer resources.
     *
     * @param runtime Caller-owned terrain runtime state that receives queued
     * setup/residency intent when staging is safe.
     * @param handles Caller-owned renderer handle catalog used for asset
     * readiness/resource resolution. The catalog is not retained or mutated.
     * @param registry Current world chunk residency state.
     * @param worldCatalog Current world chunk metadata catalog.
     * @param resources Current terrain resource catalog.
     * @param worldDescs Caller-owned array of world chunk descriptors that can
     * be matched with manifest terrain chunk IDs. May be null when
     * `worldDescCount` is zero.
     * @param worldDescCount Number of descriptors in `worldDescs`.
     * @param makeAddedChunksResident When true, queued add operations also
     * queue make-resident intent so new chunks can become visible after the
     * runtime update applies requests.
     * @return State-level result plus retained staging diagnostics available
     * through `latestStage` and `latestDiagnostics`.
     */
    TerrainManifestLoadStageResult queueStage(
        TerrainRuntimeState& runtime,
        const RendererAssetHandleCatalog& handles,
        const WorldChunkRegistry& registry,
        const WorldChunkCatalog& worldCatalog,
        const TerrainResourceCatalog& resources,
        const WorldChunkDesc* worldDescs,
        std::size_t worldDescCount,
        bool makeAddedChunksResident = true);

private:
    void storeStageResult(const TerrainManifestRuntimeStageResult& stage);
    TerrainManifestLoadStageResult noManifestResult();

    CookedAssetManifest manifest_ = {};
    bool hasManifest_ = false;
    TerrainManifestRuntimeStageResult latestStage_ = {};
    TerrainManifestRuntimeStageDiagnostics latestDiagnostics_ = {};
    TerrainManifestAssetReadinessPlan latestReadiness_ = {};
    TerrainManifestAssetLoadRequestPlan latestLoadRequests_ = {};
    TerrainManifestAssetLoadRequestQueue loadRequestQueue_ = {};
    TerrainManifestAssetLoadQueuePushResult latestLoadRequestQueueResult_ = {};
    TerrainManifestAssetLoadResult latestLoadConsumeResult_ = {};
    TerrainManifestAssetLoadExecutorResult latestLoadExecutorResult_ = {};
};
} // namespace full_engine
