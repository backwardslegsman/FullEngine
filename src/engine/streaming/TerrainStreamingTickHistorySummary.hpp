#pragma once

#include "engine/streaming/TerrainStreamingLoopState.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Counts manifest-aware streaming update statuses in a tick trace. */
struct TerrainStreamingStatusSummary
{
    std::size_t successCount = 0;
    std::size_t noManifestCount = 0;
    std::size_t assetLoadsPendingCount = 0;
    std::size_t manifestStageFailedCount = 0;
    std::size_t unsupportedStageChangesCount = 0;
    std::size_t streamingQueueBlockedCount = 0;
};

/** @brief Counts terrain runtime update statuses in a tick trace. */
struct TerrainStreamingRuntimeStatusSummary
{
    std::size_t successCount = 0;
    std::size_t setupFailedCount = 0;
    std::size_t residencyFailedCount = 0;
    std::size_t pipelineFailedCount = 0;
};

/** @brief Counts selected streaming budget profiles in a tick trace. */
struct TerrainStreamingBudgetProfileSummary
{
    std::size_t unlimitedCount = 0;
    std::size_t conservativeCount = 0;
    std::size_t balancedCount = 0;
    std::size_t catchUpCount = 0;
};

/** @brief Total and peak deferred streaming work observed in a tick trace. */
struct TerrainStreamingDeferredWorkSummary
{
    std::size_t totalSetupAddDeferredCount = 0;
    std::size_t peakSetupAddDeferredCount = 0;
    std::size_t totalSetupRemoveDeferredCount = 0;
    std::size_t peakSetupRemoveDeferredCount = 0;
    std::size_t totalMakeResidentDeferredCount = 0;
    std::size_t peakMakeResidentDeferredCount = 0;
    std::size_t totalMakeUnloadedDeferredCount = 0;
    std::size_t peakMakeUnloadedDeferredCount = 0;
    std::size_t totalLifecycleCreateDeferredCount = 0;
    std::size_t peakLifecycleCreateDeferredCount = 0;
    std::size_t totalLifecycleUpdateDeferredCount = 0;
    std::size_t peakLifecycleUpdateDeferredCount = 0;
    std::size_t totalLifecycleReleaseDeferredCount = 0;
    std::size_t peakLifecycleReleaseDeferredCount = 0;
    std::size_t totalDeferredWorkCount = 0;
    std::size_t peakDeferredWorkCount = 0;
};

/**
 * @brief Offline summary of retained or imported terrain streaming tick events.
 *
 * The summary copies counters only. It does not validate, sort, repair,
 * export, import, or mutate source events, retained loop state, runtime queues,
 * registries, catalogs, renderer handles, renderer resources, jobs, manifests,
 * or sample/editor UI state.
 */
struct TerrainStreamingTickHistorySummary
{
    std::size_t tickCount = 0;
    std::size_t runtimeUpdateRanCount = 0;
    TerrainStreamingStatusSummary streamingStatuses = {};
    TerrainStreamingRuntimeStatusSummary runtimeStatuses = {};
    TerrainStreamingBudgetProfileSummary budgetProfiles = {};
    TerrainStreamingDeferredWorkSummary deferredWork = {};
    std::size_t maxSetupRequestsBeforeRuntime = 0;
    std::size_t maxResidencyRequestsBeforeRuntime = 0;
    std::size_t maxSetupRequestsAfterRuntime = 0;
    std::size_t maxResidencyRequestsAfterRuntime = 0;
    double averageDeferredWork = 0.0;
};

/**
 * @brief Summarizes retained tick history in chronological event order.
 *
 * @param history Retained loop history to snapshot. The history is read only
 * and not consumed or mutated.
 * @return Copied status, profile, request-backlog, and deferred-work counters.
 */
TerrainStreamingTickHistorySummary summarizeTerrainStreamingTickHistory(
    const TerrainStreamingTickHistory& history);

/**
 * @brief Summarizes a caller-owned tick event snapshot in existing vector order.
 *
 * @param events Caller-owned events, commonly imported from JSON Lines traces.
 * The vector is read only and not sorted or repaired.
 * @return Copied status, profile, request-backlog, and deferred-work counters.
 */
TerrainStreamingTickHistorySummary summarizeTerrainStreamingTickHistory(
    const std::vector<TerrainStreamingTickEvent>& events);
} // namespace full_engine
