#pragma once

#include "engine/renderer_integration/TerrainRuntimeController.hpp"
#include "engine/renderer_integration/TerrainSetupStaging.hpp"
#include "engine/streaming/TerrainStreamingPlanner.hpp"

#include <cstddef>
#include <limits>

namespace full_engine
{
/** @brief Sentinel value for unlimited per-tick streaming queue budgets. */
inline constexpr std::size_t kUnlimitedTerrainStreamingQueueBudget =
    (std::numeric_limits<std::size_t>::max)();

/** @brief Per-tick limits for queueing streaming plan intent into runtime requests. */
struct TerrainStreamingQueueOptions
{
    /** @brief Maximum setup add requests to queue this tick. */
    std::size_t maxSetupAdds = kUnlimitedTerrainStreamingQueueBudget;

    /** @brief Maximum setup remove requests to queue this tick. */
    std::size_t maxSetupRemoves = kUnlimitedTerrainStreamingQueueBudget;

    /** @brief Maximum make-resident requests to queue this tick. */
    std::size_t maxMakeResident = kUnlimitedTerrainStreamingQueueBudget;

    /** @brief Maximum make-unloaded requests to queue this tick. */
    std::size_t maxMakeUnloaded = kUnlimitedTerrainStreamingQueueBudget;
};

/** @brief Result status for queueing a retained terrain streaming plan. */
enum class TerrainStreamingQueueStatus
{
    Success,
    NoPlan,
    BlockedInvalidPlan,
    MissingSetupDesc,
};

/** @brief Returns a stable diagnostic name for a terrain streaming queue status. */
const char* terrainStreamingQueueStatusName(TerrainStreamingQueueStatus status) noexcept;

/** @brief Summary of streaming plan operations queued or skipped. */
struct TerrainStreamingQueueSummary
{
    std::size_t queuedSetupAddCount = 0;
    std::size_t queuedSetupRemoveCount = 0;
    std::size_t queuedMakeResidentCount = 0;
    std::size_t queuedMakeUnloadedCount = 0;
    std::size_t deferredSetupAddCount = 0;
    std::size_t deferredSetupRemoveCount = 0;
    std::size_t deferredMakeResidentCount = 0;
    std::size_t deferredMakeUnloadedCount = 0;
    std::size_t skippedKeepSetupCount = 0;
    std::size_t skippedKeepResidentCount = 0;
    std::size_t skippedKeepUnloadedCount = 0;
    std::size_t skippedPlanSkippedCount = 0;
    std::size_t missingSetupDescCount = 0;
};

/** @brief Result of queueing a retained terrain streaming plan into runtime intent. */
struct TerrainStreamingQueueResult
{
    TerrainStreamingQueueStatus status = TerrainStreamingQueueStatus::Success;
    TerrainStreamingQueueSummary summary = {};
};

/** @brief Compact retained terrain streaming diagnostics for UI or tooling. */
struct TerrainStreamingRuntimeDiagnostics
{
    bool hasLatestPlan = false;
    std::size_t latestPlanOperationCount = 0;
    TerrainStreamingPlanSummary latestPlanSummary = {};
    TerrainStreamingQueueStatus latestQueueStatus = TerrainStreamingQueueStatus::Success;
    TerrainStreamingQueueSummary latestQueueSummary = {};
};

/**
 * @brief Retained terrain streaming planner state and queue diagnostics.
 *
 * The state owns only copied planner output and the latest queueing result. It
 * does not own world registries, terrain resources, renderer handles, renderer
 * resources, asset load state, jobs, or sample/editor state. Methods are
 * CPU-only and not thread-safe.
 */
class TerrainStreamingRuntimeState
{
public:
    /**
     * @brief Runs and stores the terrain streaming planner.
     *
     * Caller-owned `knownIds` and `current` data are read only during the call.
     * Planning resets stale queue diagnostics because queued intent no longer
     * corresponds to the newly retained plan.
     *
     * @param config Terrain grid size and inclusive load/resident radii.
     * @param cameraWorld Absolute camera position in engine world meters.
     * @param knownIds Caller-owned terrain chunk IDs to consider. May be null
     * only when `knownIdCount` is zero.
     * @param knownIdCount Number of entries in `knownIds`.
     * @param current Caller-owned value snapshot of current terrain runtime
     * state.
     * @return Reference to the retained plan, valid until the next `plan` or
     * `clear` call on this object.
     */
    const TerrainStreamingPlan& plan(
        const TerrainStreamingPlannerConfig& config,
        const WorldPosition& cameraWorld,
        const ChunkId* knownIds,
        std::size_t knownIdCount,
        const TerrainRuntimeStateSnapshot& current);

    /**
     * @brief Queues retained streaming intent into a terrain runtime state.
     *
     * This method queues intent only; it never applies runtime updates. Add
     * setup operations require a matching caller-owned `TerrainSetupStageDesc`
     * by chunk ID when the add operation is within the supplied per-tick
     * budget. Queueing is all-or-nothing for invalid plans or missing required
     * setup descriptors, so failed queue attempts leave `runtime` unchanged.
     * Budget exhaustion is not a failure; over-budget operations are counted
     * as deferred and can be retried by planning again on a later tick.
     *
     * @param runtime Caller-owned terrain runtime state that receives queued
     * setup/residency intent when the retained plan is safe.
     * @param desiredSetup Caller-owned setup descriptors used by `AddSetup`
     * operations. May be null only when `desiredSetupCount` is zero.
     * @param desiredSetupCount Number of entries in `desiredSetup`.
     * @param options Per-action queue limits for this tick.
     * @return Reference to the retained queue result, valid until the next
     * queue, `plan`, or `clear` call on this object.
     */
    const TerrainStreamingQueueResult& queueLatestPlan(
        TerrainRuntimeState& runtime,
        const TerrainSetupStageDesc* desiredSetup,
        std::size_t desiredSetupCount,
        const TerrainStreamingQueueOptions& options = {});

    /** @brief Returns whether a streaming plan has been retained. */
    bool hasLatestPlan() const noexcept;

    /** @brief Returns the latest retained streaming plan. */
    const TerrainStreamingPlan& latestPlan() const noexcept;

    /** @brief Returns the latest queueing result. */
    const TerrainStreamingQueueResult& latestQueueResult() const noexcept;

    /** @brief Returns compact diagnostics for the retained plan and queueing result. */
    const TerrainStreamingRuntimeDiagnostics& latestDiagnostics() const noexcept;

    /** @brief Clears retained plan, queueing result, and diagnostics. */
    void clear() noexcept;

private:
    TerrainStreamingPlan latestPlan_ = {};
    TerrainStreamingQueueResult latestQueueResult_ = {};
    TerrainStreamingRuntimeDiagnostics latestDiagnostics_ = {};
    bool hasLatestPlan_ = false;
};
} // namespace full_engine
