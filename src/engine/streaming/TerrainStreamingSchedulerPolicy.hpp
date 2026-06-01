#pragma once

#include "engine/streaming/TerrainStreamingBudgetPolicy.hpp"
#include "engine/streaming/TerrainStreamingTickHistorySummary.hpp"

#include <cstddef>

namespace full_engine
{
/** @brief High-level scheduler action selected from streaming diagnostics. */
enum class TerrainStreamingSchedulerStatus
{
    Idle,
    RunStreaming,
    RunAssetLoadJobs,
    RunStreamingAndAssetLoadJobs,
};

/** @brief Primary pressure source that caused a scheduler decision. */
enum class TerrainStreamingSchedulerReason
{
    NoWork,
    PendingAssetLoads,
    PendingJobs,
    DeferredWorkPressure,
    StreamingBacklog,
    CatchUp,
};

/**
 * @brief Thresholds for deterministic single-threaded streaming scheduler decisions.
 *
 * The policy uses these values only to choose what a caller should try this
 * tick. It never runs jobs, applies streaming updates, mutates runtime queues,
 * reads files, creates renderer resources, or calls renderer APIs.
 */
struct TerrainStreamingSchedulerOptions
{
    /** @brief Pending load requests at or above this count request asset-load jobs. */
    std::size_t pendingLoadRequestThreshold = 1;

    /** @brief Pending manifest asset-load jobs at or above this count request job execution. */
    std::size_t pendingJobThreshold = 1;

    /** @brief Deferred/backlog pressure at or above this count selects `CatchUp`. */
    std::size_t catchUpPressureThreshold = 8;

    /** @brief Asset-load job cap suggested when not in catch-up mode. */
    std::size_t normalMaxAssetLoadJobs = 2;

    /** @brief Asset-load job cap suggested when catch-up pressure is selected. */
    std::size_t catchUpMaxAssetLoadJobs = 8;
};

/**
 * @brief Value decision produced from current streaming diagnostics.
 *
 * The decision copies counters and selected options only. It does not retain
 * references to tick history, loop state, job queues, runtime queues, renderer
 * handles, renderer resources, manifests, registries, catalogs, or sample UI.
 */
struct TerrainStreamingSchedulerDecision
{
    TerrainStreamingSchedulerStatus status = TerrainStreamingSchedulerStatus::Idle;
    TerrainStreamingSchedulerReason reason = TerrainStreamingSchedulerReason::NoWork;
    TerrainStreamingBudgetProfile budgetProfile = TerrainStreamingBudgetProfile::Conservative;
    TerrainStreamingLoopBudgetOptions budgets =
        selectTerrainStreamingLoopBudgets(TerrainStreamingBudgetProfile::Conservative);
    std::size_t maxAssetLoadJobs = 0;
    std::size_t pendingLoadRequestCount = 0;
    std::size_t pendingJobCount = 0;
    std::size_t deferredWorkCount = 0;
    std::size_t peakDeferredWorkCount = 0;
    std::size_t runtimeBacklogCount = 0;
    std::size_t pressureCount = 0;
};

/** @brief Returns a stable diagnostic name for a scheduler status. */
const char* terrainStreamingSchedulerStatusName(
    TerrainStreamingSchedulerStatus status) noexcept;

/** @brief Returns a stable diagnostic name for a scheduler reason. */
const char* terrainStreamingSchedulerReasonName(
    TerrainStreamingSchedulerReason reason) noexcept;

/**
 * @brief Chooses single-threaded streaming work from trace and loop diagnostics.
 *
 * The helper is CPU-only and deterministic. It reads the supplied summaries,
 * selects whether the caller should run streaming, asset-load jobs, both, or
 * neither, then returns copied budget and pressure diagnostics. It does not
 * mutate the summaries, retained loop state, job queues, terrain runtime state,
 * handle catalogs, manifests, renderer resources, or sample/editor state.
 *
 * @param summary Caller-owned retained/imported tick trace summary. Read only
 * for deferred-work and runtime-backlog pressure.
 * @param loopDiagnostics Caller-owned latest loop diagnostics. Read only for
 * pending load request and pending job counts.
 * @param options Deterministic thresholds and suggested load-job caps.
 * @return Copied scheduler decision with selected phase status, reason,
 * concrete budget caps, and pressure counters.
 */
TerrainStreamingSchedulerDecision chooseTerrainStreamingSchedulerPolicy(
    const TerrainStreamingTickHistorySummary& summary,
    const TerrainStreamingLoopDiagnostics& loopDiagnostics,
    const TerrainStreamingSchedulerOptions& options = {}) noexcept;
} // namespace full_engine
