#pragma once

#include "engine/jobs/JobQueue.hpp"
#include "engine/renderer_integration/TerrainIntegrationDiagnostics.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadExecutor.hpp"
#include "engine/renderer_integration/TerrainManifestFileLoad.hpp"
#include "engine/streaming/TerrainStreamingBudgetTypes.hpp"
#include "engine/streaming/TerrainStreamingManifestCoordinator.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace full_engine
{
inline constexpr std::size_t kTerrainStreamingTickHistoryCapacity = 32;

/** @brief Compact value event retained after one synchronous terrain streaming tick. */
struct TerrainStreamingTickEvent
{
    std::uint64_t sequence = 0;
    TerrainStreamingManifestUpdateStatus streamingStatus =
        TerrainStreamingManifestUpdateStatus::Success;
    TerrainRuntimeUpdateStatus runtimeStatus = TerrainRuntimeUpdateStatus::Success;
    TerrainStreamingBudgetProfile budgetProfile = TerrainStreamingBudgetProfile::Balanced;
    bool runtimeUpdateRan = false;
    std::size_t setupRequestsBeforeRuntime = 0;
    std::size_t residencyRequestsBeforeRuntime = 0;
    std::size_t setupRequestsAfterRuntime = 0;
    std::size_t residencyRequestsAfterRuntime = 0;
    TerrainStreamingManifestUpdateSummary streaming = {};
    TerrainStreamingQueueSummary streamingQueue = {};
    TerrainLifecyclePlanSummary runtimeLifecycle = {};
    TerrainSubmissionSummary runtimeSubmission = {};
};

/**
 * @brief Fixed-capacity in-memory history of synchronous terrain streaming ticks.
 *
 * The log stores copied counters only. It does not retain manifests, job
 * records, world descriptors, renderer handles, renderer resources, runtime
 * queues, registries, catalogs, or sample UI state. It is CPU-only and not
 * thread-safe.
 */
class TerrainStreamingTickHistory
{
public:
    /** @brief Appends one copied tick event, assigning a monotonic sequence. */
    void append(const TerrainStreamingTickEvent& event);

    /** @brief Returns retained events in chronological order. */
    std::vector<TerrainStreamingTickEvent> events() const;

    /** @brief Returns the number of retained events. */
    std::size_t eventCount() const noexcept;

    /** @brief Returns the newest retained event, or null when empty. */
    const TerrainStreamingTickEvent* latestEvent() const noexcept;

    /** @brief Clears retained tick history and restarts event sequencing. */
    void clear() noexcept;

private:
    std::array<TerrainStreamingTickEvent, kTerrainStreamingTickHistoryCapacity> events_ = {};
    std::size_t nextIndex_ = 0;
    std::size_t count_ = 0;
    std::uint64_t nextSequence_ = 1;
};

/**
 * @brief Compact diagnostics for retained terrain streaming loop state.
 *
 * The snapshot copies counters and statuses only. It does not retain job
 * records, load records, manifests, world descriptors, renderer handles,
 * renderer resources, terrain runtime queues, registries, catalogs, or sample
 * UI state.
 */
struct TerrainStreamingLoopDiagnostics
{
    /** @brief True when the loop state currently retains a cooked manifest. */
    bool hasManifest = false;

    /** @brief Pending renderer-free manifest asset load requests. */
    std::size_t pendingLoadRequestCount = 0;

    /** @brief Pending generic engine jobs owned by the loop state. */
    std::size_t pendingJobCount = 0;

    /** @brief Retained synchronous streaming tick history event count. */
    std::size_t tickHistoryCount = 0;

    /** @brief Latest adaptive budget profile selection derived from tick history. */
    TerrainStreamingAdaptiveBudgetResult adaptiveBudget = {};

    /** @brief Latest synchronous manifest file-load status. */
    TerrainManifestFileLoadStatus latestFileLoadStatus = TerrainManifestFileLoadStatus::Success;

    /** @brief Latest manifest asset record count reported by file load. */
    std::size_t latestFileAssetCount = 0;

    /** @brief Latest manifest terrain chunk record count reported by file load. */
    std::size_t latestFileTerrainChunkCount = 0;

    /** @brief Latest manifest asset-load job diagnostics. */
    TerrainManifestAssetLoadJobDiagnostics loadJobs = {};

    /** @brief Latest manifest-aware streaming update status. */
    TerrainStreamingManifestUpdateStatus latestStreamingStatus =
        TerrainStreamingManifestUpdateStatus::Success;

    /** @brief Latest manifest-aware streaming update summary counters. */
    TerrainStreamingManifestUpdateSummary latestStreamingSummary = {};
};

/**
 * @brief Retained synchronous state for terrain manifest streaming coordination.
 *
 * The state groups a retained `TerrainManifestLoadState`, retained
 * `TerrainStreamingRuntimeState`, a generic manifest asset-load job queue, and
 * the latest value diagnostics produced by file reload, load-job coordination,
 * and manifest-aware streaming updates.
 *
 * It owns orchestration state only. Callers continue to own file paths,
 * renderer handle catalogs, loader callbacks, world descriptors, terrain
 * registries/catalogs/resources, terrain runtime state, renderer resources,
 * and UI mirrors. Methods are synchronous and not thread-safe; callers must
 * serialize access.
 */
class TerrainStreamingLoopState
{
public:
    /** @brief Returns the retained manifest/load state. */
    TerrainManifestLoadState& manifestLoad() noexcept;

    /** @brief Returns the retained manifest/load state. */
    const TerrainManifestLoadState& manifestLoad() const noexcept;

    /** @brief Returns the retained terrain streaming runtime state. */
    TerrainStreamingRuntimeState& streamingRuntime() noexcept;

    /** @brief Returns the retained terrain streaming runtime state. */
    const TerrainStreamingRuntimeState& streamingRuntime() const noexcept;

    /** @brief Returns the retained manifest asset-load job queue. */
    EngineJobQueue& manifestAssetLoadJobs() noexcept;

    /** @brief Returns the retained manifest asset-load job queue. */
    const EngineJobQueue& manifestAssetLoadJobs() const noexcept;

    /** @brief Returns latest file reload diagnostics. */
    const TerrainManifestFileReloadPlanResult& latestManifestReload() const noexcept;

    /** @brief Returns latest load-job coordinator diagnostics. */
    const TerrainManifestAssetLoadJobCoordinatorResult& latestLoadJobResult() const noexcept;

    /** @brief Returns latest manifest-aware streaming update diagnostics. */
    const TerrainStreamingManifestUpdateResult& latestStreamingUpdate() const noexcept;

    /** @brief Returns compact copied loop diagnostics. */
    const TerrainStreamingLoopDiagnostics& latestDiagnostics() const noexcept;

    /** @brief Returns retained synchronous streaming tick history. */
    const TerrainStreamingTickHistory& tickHistory() const noexcept;

    /** @brief Returns the retained synchronous streaming tick event count. */
    std::size_t tickHistoryCount() const noexcept;

    /** @brief Returns retained synchronous streaming tick events in chronological order. */
    std::vector<TerrainStreamingTickEvent> tickEvents() const;

    /** @brief Returns the newest retained synchronous streaming tick event, or null when empty. */
    const TerrainStreamingTickEvent* latestTickEvent() const noexcept;

    /**
     * @brief Appends one copied synchronous streaming tick event.
     *
     * This is used by the tick-shaped loop update helper after it has run
     * manifest-aware streaming coordination and optional terrain runtime
     * application. The event is copied by value and does not retain caller
     * objects.
     */
    void appendTickEvent(const TerrainStreamingTickEvent& event);

    /** @brief Clears retained synchronous streaming tick history only. */
    void clearTickHistory() noexcept;

    /**
     * @brief Reloads a manifest file and queues missing renderer-handle load intent.
     *
     * The call delegates to `reloadTerrainManifestFileAndQueueMissingAssetLoads`.
     * It does not mutate `handles`, consume load requests, run jobs, create
     * renderer resources, apply terrain runtime queues, or call renderer APIs.
     * Reloading clears stale retained jobs, load-job diagnostics, streaming
     * runtime state, and latest streaming update diagnostics.
     *
     * @param path Null-terminated JSON Lines manifest path. Read during the
     * call and not retained.
     * @param handles Caller-owned renderer handle catalog used for readiness
     * checks. It is read only and not retained.
     * @return Retained value result, valid until the next reload or clear.
     */
    const TerrainManifestFileReloadPlanResult& reloadManifestAndQueueMissingAssetLoads(
        const char* path,
        const RendererAssetHandleCatalog& handles);

    /**
     * @brief Runs retained manifest asset-load jobs through a caller callback.
     *
     * The call mirrors pending manifest load requests into the owned job queue,
     * runs relevant jobs through `callback`, consumes the retained load queue
     * when safe, and replans readiness through the lower-level coordinator. It
     * does not create renderer resources itself; the callback owns any external
     * loading or renderer-resource creation policy.
     *
     * @param destinationHandles Caller-owned runtime handle catalog to receive
     * loaded mappings when the whole batch succeeds.
     * @param callback Caller-owned synchronous handle provider.
     * @param userData Opaque caller data forwarded to `callback`.
     * @param maxJobs Maximum relevant load jobs to execute this pass.
     * @param priority Priority assigned to newly mirrored load jobs.
     * @return Retained value result, valid until the next load-job run, job
     * clear, manifest reload, or clear.
     */
    const TerrainManifestAssetLoadJobCoordinatorResult& runAssetLoadJobs(
        RendererAssetHandleCatalog& destinationHandles,
        TerrainManifestAssetLoadCallback callback,
        void* userData,
        std::size_t maxJobs,
        EngineJobPriority priority = EngineJobPriority::Normal);

    /**
     * @brief Runs one manifest-aware terrain streaming update.
     *
     * The call delegates to `updateTerrainStreamingFromManifest`, storing the
     * value result. It may queue missing asset-load intent in the retained
     * manifest load state and safe setup/residency intent into `runtime`
     * according to `options`, but it never applies terrain runtime queues,
     * mutates renderer handles, creates renderer resources, performs IO, or
     * calls renderer APIs.
     *
     * Caller-owned arrays and catalogs are read only for the duration of the
     * call. The returned reference is valid until the next streaming update,
     * manifest reload, or clear.
     *
     * @param handles Caller-owned renderer handle catalog used for readiness
     * and resource resolution. The catalog is not retained or mutated.
     * @param registry Current world chunk residency state.
     * @param worldCatalog Current world chunk metadata catalog.
     * @param resources Current terrain resource catalog.
     * @param worldDescs Caller-owned world descriptors matched by chunk ID.
     * May be null only when `worldDescCount` is zero.
     * @param worldDescCount Number of entries in `worldDescs`.
     * @param runtime Caller-owned terrain runtime state that receives queued
     * setup/residency intent when requested and safe.
     * @param streamingConfig Terrain grid size and inclusive load/resident
     * radii, in chunk units.
     * @param cameraWorld Absolute camera position in engine world meters.
     * @param currentSnapshot Caller-owned value snapshot of current terrain
     * runtime state.
     * @param options Queueing controls for missing asset loads and runtime
     * setup/residency intent.
     */
    const TerrainStreamingManifestUpdateResult& updateStreamingFromManifest(
        const RendererAssetHandleCatalog& handles,
        const WorldChunkRegistry& registry,
        const WorldChunkCatalog& worldCatalog,
        const TerrainResourceCatalog& resources,
        const WorldChunkDesc* worldDescs,
        std::size_t worldDescCount,
        TerrainRuntimeState& runtime,
        const TerrainStreamingPlannerConfig& streamingConfig,
        const WorldPosition& cameraWorld,
        const TerrainRuntimeStateSnapshot& currentSnapshot,
        const TerrainStreamingManifestUpdateOptions& options = {});

    /** @brief Clears manifest, streaming, jobs, latest results, and diagnostics. */
    void clear() noexcept;

    /** @brief Clears the retained manifest and all manifest-derived stale state. */
    void clearManifest();

    /** @brief Clears retained jobs and load-job diagnostics only. */
    void clearJobs() noexcept;

private:
    void refreshDiagnostics() noexcept;
    void resetLoadJobDiagnostics() noexcept;
    void resetStreamingUpdate() noexcept;

    TerrainManifestLoadState manifestLoad_ = {};
    TerrainStreamingRuntimeState streamingRuntime_ = {};
    EngineJobQueue manifestAssetLoadJobs_ = {};
    TerrainManifestFileReloadPlanResult latestManifestReload_ = {};
    TerrainManifestAssetLoadJobCoordinatorResult latestLoadJobResult_ = {};
    TerrainManifestAssetLoadJobDiagnostics latestLoadJobDiagnostics_ = {};
    TerrainStreamingManifestUpdateResult latestStreamingUpdate_ = {};
    TerrainStreamingTickHistory tickHistory_ = {};
    TerrainStreamingLoopDiagnostics latestDiagnostics_ = {};
};
} // namespace full_engine
