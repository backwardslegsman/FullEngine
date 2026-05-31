#pragma once

#include "engine/renderer_integration/TerrainManifestLoadState.hpp"
#include "engine/streaming/TerrainStreamingRuntime.hpp"

#include <cstddef>

namespace full_engine
{
/**
 * @brief Options for one manifest-aware terrain streaming update.
 *
 * These options control queueing only. The coordinator never applies terrain
 * runtime queues, consumes asset load queues, creates renderer resources,
 * starts async jobs, performs file IO, or calls renderer APIs.
 */
struct TerrainStreamingManifestUpdateOptions
{
    /** @brief Queue missing renderer-handle load intent into the manifest load state. */
    bool queueMissingAssetLoads = true;

    /** @brief Queue safe setup/residency intent into the terrain runtime state. */
    bool queueRuntimeRequests = true;
};

/** @brief High-level status for manifest-aware terrain streaming coordination. */
enum class TerrainStreamingManifestUpdateStatus
{
    Success,
    NoManifest,
    AssetLoadsPending,
    ManifestStageFailed,
    UnsupportedStageChanges,
    StreamingQueueBlocked,
};

/** @brief Returns a stable diagnostic name for a manifest-aware streaming update status. */
const char* terrainStreamingManifestUpdateStatusName(TerrainStreamingManifestUpdateStatus status) noexcept;

/** @brief Compact counters copied from one manifest-aware streaming coordination pass. */
struct TerrainStreamingManifestUpdateSummary
{
    std::size_t manifestTerrainChunkCount = 0;
    std::size_t readinessMissingHandleCount = 0;
    std::size_t loadRequestCount = 0;
    std::size_t queuedLoadRequestCount = 0;
    std::size_t desiredSetupCount = 0;
    std::size_t streamingPlanOperationCount = 0;
    std::size_t queuedSetupAddCount = 0;
    std::size_t queuedSetupRemoveCount = 0;
    std::size_t queuedMakeResidentCount = 0;
    std::size_t queuedMakeUnloadedCount = 0;
};

/**
 * @brief Value result for manifest-aware terrain streaming coordination.
 *
 * The result owns copied diagnostics and value plans only. It does not own
 * manifests, registries, catalogs, renderer handles, renderer resources,
 * runtime queues, async jobs, or sample/editor UI state.
 */
struct TerrainStreamingManifestUpdateResult
{
    TerrainStreamingManifestUpdateStatus status = TerrainStreamingManifestUpdateStatus::Success;
    TerrainManifestAssetReadinessPlan readiness = {};
    TerrainManifestAssetLoadRequestPlan loadRequests = {};
    TerrainManifestAssetLoadQueuePushResult loadQueue = {};
    TerrainManifestLoadStageResult manifestStage = {};
    TerrainStreamingPlan streamingPlan = {};
    TerrainStreamingQueueResult streamingQueue = {};
    TerrainStreamingManifestUpdateSummary summary = {};
};

/**
 * @brief Coordinates retained manifest state with terrain streaming policy.
 *
 * The function plans handle readiness, optionally queues missing renderer-free
 * asset load intent, stages manifest terrain setup descriptors, runs the
 * terrain streaming planner, and optionally queues safe setup/residency intent
 * into `runtime`. It performs no IO, renderer calls, async work, renderer
 * resource creation, queue application, or direct registry/catalog mutation.
 *
 * Caller-owned arrays and catalogs are read only for the duration of the call.
 * `manifestLoad`, `streaming`, and `runtime` retain only their normal state and
 * queue diagnostics.
 *
 * @param manifestLoad Caller-owned retained manifest/load state. The function
 * may update its latest readiness, load-request, load-queue, and staging
 * diagnostics.
 * @param handles Caller-owned renderer handle catalog used for readiness and
 * resource resolution. It is not retained or mutated.
 * @param registry Current world chunk residency state.
 * @param worldCatalog Current world chunk metadata catalog.
 * @param resources Current terrain resource catalog.
 * @param worldDescs Caller-owned world descriptors matched by chunk ID for
 * staged setup. May be null only when `worldDescCount` is zero.
 * @param worldDescCount Number of entries in `worldDescs`.
 * @param streaming Caller-owned retained streaming state that stores the latest
 * plan and queue diagnostics.
 * @param runtime Caller-owned terrain runtime state that receives queued
 * setup/residency intent when requested and safe.
 * @param streamingConfig Terrain grid size and inclusive load/resident radii.
 * @param cameraWorld Absolute camera position in engine world meters.
 * @param currentSnapshot Caller-owned value snapshot of current terrain runtime
 * state.
 * @param options Queueing controls for missing asset loads and runtime
 * setup/residency intent.
 * @return Value diagnostics for the readiness, load-intent, staging, planning,
 * and queueing steps.
 */
TerrainStreamingManifestUpdateResult updateTerrainStreamingFromManifest(
    TerrainManifestLoadState& manifestLoad,
    const RendererAssetHandleCatalog& handles,
    const WorldChunkRegistry& registry,
    const WorldChunkCatalog& worldCatalog,
    const TerrainResourceCatalog& resources,
    const WorldChunkDesc* worldDescs,
    std::size_t worldDescCount,
    TerrainStreamingRuntimeState& streaming,
    TerrainRuntimeState& runtime,
    const TerrainStreamingPlannerConfig& streamingConfig,
    const WorldPosition& cameraWorld,
    const TerrainRuntimeStateSnapshot& currentSnapshot,
    const TerrainStreamingManifestUpdateOptions& options = {});
} // namespace full_engine
