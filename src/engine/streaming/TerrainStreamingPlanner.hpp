#pragma once

#include "engine/renderer_integration/TerrainRuntimeStateSnapshot.hpp"
#include "engine/world/WorldChunkRegistry.hpp"
#include "engine/world/WorldOrigin.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/**
 * @brief CPU-only terrain streaming window policy configuration.
 *
 * The planner treats terrain as a square X/Z grid of equally sized chunks.
 * `chunkSizeMeters` must be positive and finite. Radius values are inclusive
 * Chebyshev distances in chunk coordinates; the resident radius should not
 * exceed the load radius. The planner reads this value only and never retains
 * references to it.
 */
struct TerrainStreamingPlannerConfig
{
    double chunkSizeMeters = 0.0;
    int loadRadiusChunks = 0;
    int residentRadiusChunks = 0;
};

/** @brief Dry-run terrain setup or residency intent emitted by the streaming planner. */
enum class TerrainStreamingChunkAction
{
    AddSetup,
    KeepSetup,
    RemoveSetup,
    MakeResident,
    KeepResident,
    MakeUnloaded,
    KeepUnloaded,
    Skipped,
};

/**
 * @brief One terrain streaming dry-run operation.
 *
 * Operations are value snapshots. Current setup/resource/handle/residency
 * flags are copied from the supplied `TerrainRuntimeStateSnapshot`; desired
 * flags describe whether the chunk is inside the computed load or resident
 * window for this plan. The planner does not queue or apply these operations.
 */
struct TerrainStreamingPlanOp
{
    ChunkId id = {};
    TerrainStreamingChunkAction action = TerrainStreamingChunkAction::Skipped;
    bool desiredLoad = false;
    bool desiredResident = false;
    bool hasRegistry = false;
    bool hasWorldDesc = false;
    bool hasResources = false;
    bool hasTerrainHandle = false;
    ChunkResidencyState residency = ChunkResidencyState::Unloaded;
    TerrainRuntimeChunkReadiness readiness = TerrainRuntimeChunkReadiness::MissingRegistry;
};

/** @brief Summary counters for a terrain streaming dry-run plan. */
struct TerrainStreamingPlanSummary
{
    std::size_t addSetupCount = 0;
    std::size_t keepSetupCount = 0;
    std::size_t removeSetupCount = 0;
    std::size_t makeResidentCount = 0;
    std::size_t keepResidentCount = 0;
    std::size_t makeUnloadedCount = 0;
    std::size_t keepUnloadedCount = 0;
    std::size_t skippedCount = 0;
    std::size_t invalidInputCount = 0;
};

/** @brief Ordered dry-run terrain streaming operations plus summary counters. */
struct TerrainStreamingPlan
{
    std::vector<TerrainStreamingPlanOp> operations;
    TerrainStreamingPlanSummary summary;
};

/** @brief Returns a stable diagnostic name for a terrain streaming action. */
const char* terrainStreamingChunkActionName(TerrainStreamingChunkAction action) noexcept;

/**
 * @brief Plans terrain setup and residency intent for a camera-centered grid window.
 *
 * `knownIds` is caller-owned and read only for the duration of the call. Input
 * order is preserved for chunks inside the desired windows. Cleanup operations
 * for snapshot chunks outside the desired load window are appended in
 * deterministic `ChunkId` order. The planner is dry-run only: it does not
 * mutate registries, catalogs, queues, renderer handles, renderer resources,
 * jobs, or runtime diagnostics.
 *
 * Invalid configuration or a null `knownIds` pointer with non-zero
 * `knownIdCount` produces deterministic skipped/invalid counters and no
 * runtime mutation.
 *
 * @param config Terrain grid size and inclusive load/resident radii.
 * @param cameraWorld Absolute camera position in engine world meters.
 * @param knownIds Caller-owned terrain chunk IDs to consider. May be null only
 * when `knownIdCount` is zero.
 * @param knownIdCount Number of entries in `knownIds`.
 * @param current Caller-owned value snapshot of current terrain runtime state.
 * @return Value plan containing dry-run setup/residency intent and counters.
 */
TerrainStreamingPlan planTerrainStreaming(
    const TerrainStreamingPlannerConfig& config,
    const WorldPosition& cameraWorld,
    const ChunkId* knownIds,
    std::size_t knownIdCount,
    const TerrainRuntimeStateSnapshot& current);
} // namespace full_engine
