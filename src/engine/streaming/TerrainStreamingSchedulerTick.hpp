#pragma once

#include "engine/streaming/TerrainStreamingLoopUpdate.hpp"
#include "engine/streaming/TerrainStreamingSchedulerPolicy.hpp"

namespace full_engine
{
/** @brief How a scheduler tick handles manifest asset-load work. */
enum class TerrainStreamingSchedulerLoadJobMode
{
    /** @brief Run the existing synchronous load-job coordinator. */
    ExecuteSynchronously,

    /** @brief Mirror load jobs into the retained queue and leave execution external. */
    ScheduleOnly,

    /** @brief Schedule jobs, advance retained service work, and reconcile completions synchronously. */
    RetainedService,

    /** @brief Schedule jobs now and reconcile caller-owned completions when supplied. */
    ExternalCompletions,
};

/**
 * @brief Options for one policy-driven synchronous streaming scheduler tick.
 *
 * These options select policy thresholds and lower-level loop defaults only.
 * The scheduler tick does not create threads, sleep, perform file IO, own
 * renderer resources, or hide renderer handle ownership. Future async
 * schedulers can replace individual phases while preserving the explicit
 * caller-owned state and callback boundaries.
 */
struct TerrainStreamingSchedulerTickOptions
{
    /** @brief Policy thresholds used to decide which phases to attempt. */
    TerrainStreamingSchedulerOptions scheduler = {};

    /**
     * @brief Whether load-job phases execute synchronously or are only scheduled.
     *
     * In `ScheduleOnly` mode, a tick that selects asset-load work stops after
     * mirroring pending requests into jobs. In `RetainedService` mode, the tick
     * also packetizes those jobs into the retained load service, advances
     * caller-owned callbacks synchronously, and reconciles emitted completions
     * before the streaming phase. In `ExternalCompletions` mode, the tick
     * schedules jobs, then reconciles caller-owned completion records only when
     * they are provided through `externalCompletions`.
     */
    TerrainStreamingSchedulerLoadJobMode loadJobMode =
        TerrainStreamingSchedulerLoadJobMode::ExecuteSynchronously;

    /**
     * @brief Caller-owned completed load-job records for `ExternalCompletions` mode.
     *
     * The array is read only during `runTerrainStreamingSchedulerTick` and is
     * never retained. The caller owns any worker state and renderer handles
     * represented by these records.
     */
    const TerrainManifestAssetLoadJobCompletion* externalCompletions = nullptr;

    /** @brief Number of records in `externalCompletions`; zero means no completions yet. */
    std::size_t externalCompletionCount = 0;

    /** @brief Default streaming/runtime options; policy-selected budgets overwrite `budgets`. */
    TerrainStreamingLoopUpdateOptions loopUpdate = {};

    /** @brief Priority assigned when pending manifest load requests are mirrored to jobs. */
    EngineJobPriority assetLoadJobPriority = EngineJobPriority::Normal;

    /** @brief When true, `maxAssetLoadJobs` overrides the scheduler decision cap. */
    bool overrideMaxAssetLoadJobs = false;

    /** @brief Explicit load-job cap used only when `overrideMaxAssetLoadJobs` is true. */
    std::size_t maxAssetLoadJobs = 0;
};

/**
 * @brief Copied diagnostics from one scheduler-driven terrain streaming tick.
 *
 * The result owns value diagnostics only. It does not retain caller arrays,
 * renderer handles, renderer resources, registries, catalogs, manifests,
 * callbacks, snapshots, or sample/editor UI state.
 */
struct TerrainStreamingSchedulerTickResult
{
    TerrainStreamingSchedulerTickStatus status = TerrainStreamingSchedulerTickStatus::Idle;
    TerrainStreamingSchedulerDecision decision = {};
    TerrainStreamingTickHistorySummary historySummary = {};
    TerrainManifestAssetLoadJobCoordinatorResult loadJobs = {};
    TerrainManifestAssetLoadJobScheduleResult scheduledLoadJobs = {};
    TerrainManifestAssetLoadJobWorkPacketResult loadServiceWorkPackets = {};
    TerrainManifestAssetLoadServiceEnqueueResult loadServiceEnqueue = {};
    TerrainManifestAssetLoadServiceTickResult loadServiceTick = {};
    TerrainManifestAssetLoadJobCompletionReconcileResult loadServiceReconcile = {};
    TerrainManifestAssetLoadServiceDiagnostics loadService = {};
    TerrainManifestAssetLoadJobCompletionReconcileResult externalCompletionReconcile = {};
    TerrainStreamingLoopUpdateResult streaming = {};
    bool loadJobsRan = false;
    bool loadJobsScheduled = false;
    bool loadServiceRan = false;
    bool externalCompletionsReconciled = false;
    bool streamingRan = false;
};

/** @brief Returns a stable diagnostic name for a scheduler tick status. */
const char* terrainStreamingSchedulerTickStatusName(
    TerrainStreamingSchedulerTickStatus status) noexcept;

/**
 * @brief Runs one synchronous policy-driven terrain streaming scheduler tick.
 *
 * The helper summarizes retained tick history, chooses a scheduler decision,
 * optionally runs manifest asset-load jobs, and optionally runs the synchronous
 * streaming loop update with the selected budget caps. Load jobs run before
 * streaming when both are requested so newly available renderer handles can
 * satisfy the same tick's streaming phase.
 *
 * All state remains caller-owned. The helper may mutate only through the
 * documented lower-level phases: `loop.runAssetLoadJobs` and
 * `updateTerrainStreamingLoop`. It does not create threads, perform file IO,
 * create renderer resources itself, own handles, or call renderer APIs except
 * through the terrain runtime update path.
 *
 * @note Thread safety: Not thread-safe for shared objects. Callers must
 * serialize access to loop state, runtime state, catalogs, handle maps, job
 * queues, renderer handles, and renderer instances.
 *
 * @param loop Retained streaming loop state that owns manifest load state,
 * streaming runtime state, asset-load jobs, and latest diagnostics.
 * @param terrainRuntime Caller-owned terrain runtime state that may receive
 * and apply setup/residency intent during the streaming phase.
 * @param renderer Renderer interface used only by the terrain runtime pipeline
 * when the streaming phase applies runtime queues.
 * @param registry Mutable world chunk residency registry.
 * @param worldCatalog Mutable world chunk descriptor catalog.
 * @param resources Mutable terrain resource catalog.
 * @param terrainHandles Mutable chunk-to-renderer terrain handle map.
 * @param assetHandles Mutable renderer asset handle catalog. The load-job
 * phase may add externally supplied handles; the streaming phase reads it for
 * manifest readiness and resource resolution.
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
 * @param assetLoadCallback Caller-owned synchronous asset handle provider. It
 * may perform external loading; the scheduler helper itself performs none.
 * @param assetLoadUserData Opaque caller data forwarded to `assetLoadCallback`.
 * @param options Policy thresholds, lower-level loop defaults, and load-job
 * execution caps for this tick.
 * @return Copied decision, phase results, and booleans describing which phases
 * ran. The result owns no references to caller-owned state.
 */
TerrainStreamingSchedulerTickResult runTerrainStreamingSchedulerTick(
    TerrainStreamingLoopState& loop,
    TerrainRuntimeState& terrainRuntime,
    full_renderer::IRenderer& renderer,
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resources,
    ChunkTerrainHandleMap& terrainHandles,
    RendererAssetHandleCatalog& assetHandles,
    const WorldChunkDesc* worldDescs,
    std::size_t worldDescCount,
    const ChunkId* trackedIds,
    std::size_t trackedIdCount,
    const TerrainStreamingPlannerConfig& streamingConfig,
    const WorldPosition& cameraWorld,
    const TerrainRuntimeStateSnapshot& currentSnapshot,
    TerrainManifestAssetLoadCallback assetLoadCallback,
    void* assetLoadUserData = nullptr,
    const TerrainStreamingSchedulerTickOptions& options = {});
} // namespace full_engine
