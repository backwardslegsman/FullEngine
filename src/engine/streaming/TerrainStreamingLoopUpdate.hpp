#pragma once

#include "engine/streaming/TerrainStreamingLoopState.hpp"

namespace full_engine
{
/** @brief High-level status for one synchronous terrain streaming loop update. */
enum class TerrainStreamingLoopUpdateStatus
{
    /** @brief Streaming coordination and any needed terrain runtime update succeeded. */
    Success,

    /** @brief Manifest-aware streaming coordination could not queue safe runtime intent. */
    StreamingBlocked,

    /** @brief Terrain setup request application failed after streaming queued intent. */
    RuntimeSetupFailed,

    /** @brief Terrain residency request application failed after streaming queued intent. */
    RuntimeResidencyFailed,

    /** @brief Terrain renderer pipeline update failed after streaming queued intent. */
    RuntimePipelineFailed,
};

/** @brief Returns a stable diagnostic name for a streaming loop update status. */
const char* terrainStreamingLoopUpdateStatusName(TerrainStreamingLoopUpdateStatus status) noexcept;

/** @brief Per-tick budgets applied by the synchronous streaming loop helper. */
struct TerrainStreamingLoopBudgetOptions
{
    /** @brief Limits for queueing setup/residency intent from the streaming plan. */
    TerrainStreamingQueueOptions queue = {};

    /** @brief Maximum renderer terrain creates planned by the runtime pipeline this tick. */
    std::size_t maxPipelineCreates = kUnlimitedTerrainLifecycleBudget;

    /** @brief Maximum renderer terrain updates planned by the runtime pipeline this tick. */
    std::size_t maxPipelineUpdates = kUnlimitedTerrainLifecycleBudget;

    /** @brief Maximum renderer terrain releases planned by the runtime pipeline this tick. */
    std::size_t maxPipelineReleases = kUnlimitedTerrainLifecycleBudget;
};

/** @brief Grouped options for one synchronous terrain streaming loop update. */
struct TerrainStreamingLoopUpdateOptions
{
    /** @brief Manifest-aware streaming coordination options. */
    TerrainStreamingManifestUpdateOptions streaming = {};

    /** @brief Terrain runtime update and pipeline options. */
    TerrainRuntimeUpdateOptions runtime = {};

    /** @brief Per-tick queueing and renderer lifecycle budgets. */
    TerrainStreamingLoopBudgetOptions budgets = {};
};

/**
 * @brief Value result for one synchronous terrain streaming loop update.
 *
 * The result copies coordination and runtime update values only. It does not
 * own renderer handles, renderer resources, registries, catalogs, manifests,
 * tracked ID arrays, world descriptor arrays, snapshots, or sample UI state.
 */
struct TerrainStreamingLoopUpdateResult
{
    /** @brief High-level update outcome. */
    TerrainStreamingLoopUpdateStatus status = TerrainStreamingLoopUpdateStatus::Success;

    /** @brief Manifest-aware streaming coordination result. */
    TerrainStreamingManifestUpdateResult streaming = {};

    /** @brief Terrain runtime update result when runtime queues were applied. */
    TerrainRuntimeUpdateResult runtime = {};

    /** @brief True when `TerrainRuntimeState::updateWithSnapshot` ran. */
    bool runtimeUpdateRan = false;

    /** @brief Setup request count after streaming coordination and before runtime application. */
    std::size_t setupRequestsBeforeRuntime = 0;

    /** @brief Residency request count after streaming coordination and before runtime application. */
    std::size_t residencyRequestsBeforeRuntime = 0;

    /** @brief Setup request count after optional runtime application. */
    std::size_t setupRequestsAfterRuntime = 0;

    /** @brief Residency request count after optional runtime application. */
    std::size_t residencyRequestsAfterRuntime = 0;
};

/**
 * @brief Runs retained manifest streaming coordination and applies queued terrain runtime work.
 *
 * The helper first delegates to `TerrainStreamingLoopState::updateStreamingFromManifest`.
 * Only a successful streaming result can be followed by
 * `TerrainRuntimeState::updateWithSnapshot`. Runtime queue application is
 * skipped when streaming reports blocked manifest, asset-load, staging, or
 * streaming-plan state.
 *
 * The helper is synchronous and not thread-safe for shared objects. Callers
 * keep ownership of the renderer, handle catalogs, registries, catalogs,
 * terrain runtime state, world descriptor arrays, tracked chunk ID arrays,
 * snapshots, and camera input. The helper does not run asset-load jobs, import
 * manifests, consume load requests, create renderer resources, mirror sample
 * UI state, or call renderer APIs except through the existing terrain runtime
 * pipeline update.
 *
 * @param loop Retained streaming loop state that stores manifest/streaming
 * coordination diagnostics.
 * @param terrainRuntime Terrain runtime state that receives and optionally
 * applies queued setup/residency intent.
 * @param renderer Renderer interface used only by the terrain runtime pipeline
 * when runtime queues are applied.
 * @param registry Mutable world residency registry.
 * @param worldCatalog Mutable world chunk descriptor catalog.
 * @param resources Mutable terrain resource catalog.
 * @param handles Mutable chunk-to-renderer terrain handle map.
 * @param assetHandles Caller-owned renderer asset handle catalog used by
 * manifest readiness and terrain resource resolution.
 * @param worldDescs Caller-owned world descriptors matched by chunk ID. May be
 * null only when `worldDescCount` is zero.
 * @param worldDescCount Number of descriptors in `worldDescs`.
 * @param trackedIds Caller-owned chunk IDs used for post-update runtime
 * snapshot tracking. May be null only when `trackedIdCount` is zero.
 * @param trackedIdCount Number of IDs in `trackedIds`.
 * @param streamingConfig Terrain chunk size and load/resident radii.
 * @param cameraWorld Absolute camera position in engine world meters.
 * @param currentSnapshot Caller-owned snapshot describing terrain state before
 * streaming coordination.
 * @param options Grouped streaming/runtime options and per-tick budgets.
 * @return Copied streaming/runtime results and request counters for this tick.
 */
TerrainStreamingLoopUpdateResult updateTerrainStreamingLoop(
    TerrainStreamingLoopState& loop,
    TerrainRuntimeState& terrainRuntime,
    full_renderer::IRenderer& renderer,
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resources,
    ChunkTerrainHandleMap& handles,
    const RendererAssetHandleCatalog& assetHandles,
    const WorldChunkDesc* worldDescs,
    std::size_t worldDescCount,
    const ChunkId* trackedIds,
    std::size_t trackedIdCount,
    const TerrainStreamingPlannerConfig& streamingConfig,
    const WorldPosition& cameraWorld,
    const TerrainRuntimeStateSnapshot& currentSnapshot,
    const TerrainStreamingLoopUpdateOptions& options);

/**
 * @brief Compatibility overload using existing streaming/runtime options.
 *
 * This overload preserves the original unlimited-budget behavior. Prefer the
 * grouped-options overload when per-tick queue or pipeline budgets are needed.
 */
TerrainStreamingLoopUpdateResult updateTerrainStreamingLoop(
    TerrainStreamingLoopState& loop,
    TerrainRuntimeState& terrainRuntime,
    full_renderer::IRenderer& renderer,
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resources,
    ChunkTerrainHandleMap& handles,
    const RendererAssetHandleCatalog& assetHandles,
    const WorldChunkDesc* worldDescs,
    std::size_t worldDescCount,
    const ChunkId* trackedIds,
    std::size_t trackedIdCount,
    const TerrainStreamingPlannerConfig& streamingConfig,
    const WorldPosition& cameraWorld,
    const TerrainRuntimeStateSnapshot& currentSnapshot,
    const TerrainStreamingManifestUpdateOptions& streamingOptions = {},
    const TerrainRuntimeUpdateOptions& runtimeOptions = {});
} // namespace full_engine
