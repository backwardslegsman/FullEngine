#pragma once

#include "engine/streaming/TerrainStreamingBudgetTypes.hpp"
#include "engine/streaming/TerrainStreamingLoopUpdate.hpp"

namespace full_engine
{
/** @brief Returns a stable diagnostic name for a terrain streaming budget profile. */
const char* terrainStreamingBudgetProfileName(TerrainStreamingBudgetProfile profile) noexcept;

/**
 * @brief Selects queue and lifecycle caps for a named streaming budget profile.
 *
 * The returned value is fully copied and has no references to caller-owned
 * state. Unknown enum values fall back to the balanced profile so persisted or
 * debug UI values remain deterministic after future enum growth.
 */
TerrainStreamingLoopBudgetOptions selectTerrainStreamingLoopBudgets(
    TerrainStreamingBudgetProfile profile) noexcept;

/**
 * @brief Selects a budget profile from retained streaming tick pressure.
 *
 * The selector reads recent `TerrainStreamingTickHistory` counters and returns
 * copied diagnostics only. It does not mutate the history, loop state, runtime
 * queues, renderer handles, renderer resources, jobs, or manifests.
 *
 * Empty history returns `Balanced` so a newly enabled streaming loop starts at
 * moderate pacing. Recent history with no deferred work returns
 * `Conservative`; light deferred work returns `Balanced`; deferred work at or
 * above `catchUpDeferredWorkThreshold` returns `CatchUp`.
 */
TerrainStreamingAdaptiveBudgetResult selectAdaptiveTerrainStreamingBudgetProfile(
    const TerrainStreamingTickHistory& history,
    const TerrainStreamingAdaptiveBudgetOptions& options = {}) noexcept;

/** @brief Selects concrete loop budget caps from adaptive tick-history pressure. */
TerrainStreamingLoopBudgetOptions selectAdaptiveTerrainStreamingLoopBudgets(
    const TerrainStreamingTickHistory& history,
    const TerrainStreamingAdaptiveBudgetOptions& options = {}) noexcept;
} // namespace full_engine
