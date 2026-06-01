#pragma once

#include <cstddef>

namespace full_engine
{
/**
 * @brief Named per-frame budget profiles for synchronous terrain streaming.
 *
 * Profiles describe deterministic request queueing and renderer lifecycle caps.
 * They are policy values only; they do not measure frame time, run jobs,
 * perform IO, create renderer resources, or mutate runtime state.
 */
enum class TerrainStreamingBudgetProfile
{
    /** @brief Leaves every queue and lifecycle cap unlimited. */
    Unlimited,

    /** @brief Small caps intended for smooth foreground frames. */
    Conservative,

    /** @brief Moderate caps intended as the default runtime pacing profile. */
    Balanced,

    /** @brief Larger caps intended to catch up after camera jumps or stalls. */
    CatchUp,
};

/** @brief Options controlling adaptive profile selection from recent tick history. */
struct TerrainStreamingAdaptiveBudgetOptions
{
    /**
     * @brief Number of newest tick events to inspect.
     *
     * A value of zero falls back to the default window of five events. The
     * selector never mutates or consumes tick history.
     */
    std::size_t recentTickCount = 5;

    /**
     * @brief Deferred-work count that promotes the profile to `CatchUp`.
     *
     * Any nonzero pressure below this threshold selects `Balanced`. Zero
     * pressure selects `Conservative`; empty history selects `Balanced`.
     */
    std::size_t catchUpDeferredWorkThreshold = 8;
};

/** @brief Value diagnostics for adaptive streaming budget profile selection. */
struct TerrainStreamingAdaptiveBudgetResult
{
    /** @brief Selected deterministic budget profile. */
    TerrainStreamingBudgetProfile profile = TerrainStreamingBudgetProfile::Balanced;

    /** @brief Number of recent retained tick events inspected. */
    std::size_t inspectedTickCount = 0;

    /** @brief Sum of deferred setup, residency, and lifecycle work inspected. */
    std::size_t deferredWorkCount = 0;
};
} // namespace full_engine
