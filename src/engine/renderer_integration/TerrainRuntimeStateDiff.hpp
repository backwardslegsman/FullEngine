#pragma once

#include "engine/renderer_integration/TerrainRuntimeStateSnapshot.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Type of change detected between two terrain runtime state snapshots. */
enum class TerrainRuntimeStateChangeType
{
    Added,
    Removed,
    ReadinessChanged,
    ResidencyChanged,
    HandlePresenceChanged,
};

/** @brief Returns a stable diagnostic name for a terrain runtime state change type. */
const char* terrainRuntimeStateChangeTypeName(TerrainRuntimeStateChangeType type) noexcept;

/** @brief One deterministic terrain runtime state change record. */
struct TerrainRuntimeStateChange
{
    ChunkId id = {};
    TerrainRuntimeStateChangeType type = TerrainRuntimeStateChangeType::ReadinessChanged;
    TerrainRuntimeChunkReadiness previousReadiness = TerrainRuntimeChunkReadiness::MissingRegistry;
    TerrainRuntimeChunkReadiness currentReadiness = TerrainRuntimeChunkReadiness::MissingRegistry;
    ChunkResidencyState previousResidency = ChunkResidencyState::Unloaded;
    ChunkResidencyState currentResidency = ChunkResidencyState::Unloaded;
    bool previousHasTerrainHandle = false;
    bool currentHasTerrainHandle = false;
};

/** @brief Summary counters for terrain runtime state snapshot changes. */
struct TerrainRuntimeStateDiffSummary
{
    std::size_t addedCount = 0;
    std::size_t removedCount = 0;
    std::size_t readinessChangedCount = 0;
    std::size_t residencyChangedCount = 0;
    std::size_t handlePresenceChangedCount = 0;
};

/** @brief Ordered terrain runtime state snapshot diff. */
struct TerrainRuntimeStateDiff
{
    std::vector<TerrainRuntimeStateChange> changes;
    TerrainRuntimeStateDiffSummary summary = {};
};

/**
 * @brief Computes deterministic chunk state changes between two snapshots.
 *
 * Chunks are compared by `ChunkId`, independent of snapshot vector order. The
 * returned changes are sorted by `ChunkId` and contain at most one record per
 * changed chunk.
 */
TerrainRuntimeStateDiff diffTerrainRuntimeStateSnapshots(
    const TerrainRuntimeStateSnapshot& previous,
    const TerrainRuntimeStateSnapshot& current);
} // namespace full_engine
