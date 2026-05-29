#pragma once

#include "full_renderer/Renderer.hpp"

namespace full_renderer::debug
{
/** @brief Debug-only budget thresholds used to classify frame diagnostics. */
struct FrameBudgetThresholds
{
    /** @brief CPU planning budget in microseconds; zero disables this warning. */
    std::uint64_t totalCpuMicroseconds = 8000;

    /** @brief Estimated staged byte budget; zero disables this warning. */
    std::uint64_t totalStagedBytes = 4ULL * 1024ULL * 1024ULL;

    /** @brief Terrain chunk create/destroy/reuse budget; zero disables this warning. */
    std::uint32_t terrainChunkChurn = 128;

    /** @brief Coarse draw/pass submission budget; zero disables this warning. */
    std::uint32_t drawSubmissionCount = 2000;

    /** @brief Submitted particle descriptor budget; zero disables this warning. */
    std::uint32_t particleCount = 4096;

    /** @brief Submitted decal descriptor budget; zero disables this warning. */
    std::uint32_t decalCount = kMaxFrameDecals;

    /** @brief Submitted skinned draw budget; zero disables this warning. */
    std::uint32_t skinnedDrawCount = 128;
};

/** @brief Warning bit for `FrameBudgetEvaluation::warningMask`. */
constexpr std::uint32_t kFrameBudgetWarningCpuTime = 1U << 0U;
/** @brief Warning bit for `FrameBudgetEvaluation::warningMask`. */
constexpr std::uint32_t kFrameBudgetWarningStagedBytes = 1U << 1U;
/** @brief Warning bit for `FrameBudgetEvaluation::warningMask`. */
constexpr std::uint32_t kFrameBudgetWarningTerrainChurn = 1U << 2U;
/** @brief Warning bit for `FrameBudgetEvaluation::warningMask`. */
constexpr std::uint32_t kFrameBudgetWarningDrawSubmissions = 1U << 3U;
/** @brief Warning bit for `FrameBudgetEvaluation::warningMask`. */
constexpr std::uint32_t kFrameBudgetWarningParticles = 1U << 4U;
/** @brief Warning bit for `FrameBudgetEvaluation::warningMask`. */
constexpr std::uint32_t kFrameBudgetWarningDecals = 1U << 5U;
/** @brief Warning bit for `FrameBudgetEvaluation::warningMask`. */
constexpr std::uint32_t kFrameBudgetWarningSkinnedDraws = 1U << 6U;

/** @brief Result of comparing a frame budget snapshot to debug thresholds. */
struct FrameBudgetEvaluation
{
    /** @brief OR-ed `kFrameBudgetWarning*` bits for thresholds exceeded by this frame. */
    std::uint32_t warningMask = 0;

    /** @brief Number of warning bits set in `warningMask`. */
    std::uint32_t warningCount = 0;
};

/** @brief Returns the debug label for a CPU frame budget stage. */
const char* frameBudgetStageName(FrameBudgetStage stage) noexcept;

/** @brief Returns default validation-oriented budget thresholds. */
FrameBudgetThresholds makeDefaultFrameBudgetThresholds() noexcept;

/** @brief Records a CPU stage duration into a budget snapshot. */
void recordFrameBudgetStage(
    FrameBudgetStats& stats,
    FrameBudgetStage stage,
    std::uint64_t microseconds) noexcept;

/**
 * @brief Estimates frame-local staging bytes and submitted counts from a render packet.
 *
 * The returned values are CPU-side estimates based on public descriptors and
 * pointer counts. They do not inspect backend allocations or retain any caller
 * pointers beyond the call.
 */
FrameBudgetStats estimateFrameBudgetSubmission(const RenderPacket& packet) noexcept;

/**
 * @brief Merges renderer and terrain stats into a frame budget snapshot.
 *
 * This fills accepted/culled/draw/churn counters from already-collected
 * backend-neutral diagnostics. Timing and staged-byte fields already present in
 * `budget` are preserved.
 */
void mergeFrameBudgetRuntimeStats(
    FrameBudgetStats& budget,
    const RendererStats& rendererStats,
    const TerrainStats& terrainStats) noexcept;

/** @brief Evaluates debug thresholds against a completed frame budget snapshot. */
FrameBudgetEvaluation evaluateFrameBudget(
    const RendererStats& rendererStats,
    const TerrainStats& terrainStats,
    const FrameBudgetThresholds& thresholds) noexcept;
} // namespace full_renderer::debug
