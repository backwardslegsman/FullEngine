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

/**
 * @brief One deterministic terrain runtime state change record.
 *
 * The record is a copied value snapshot. It does not reference the source
 * snapshots, registries, catalogs, renderer handles, or renderer resources.
 * Residency values use engine chunk residency states, and readiness values use
 * the terrain runtime snapshot readiness policy.
 */
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

/**
 * @brief Ordered terrain runtime state snapshot diff.
 *
 * Changes are value-owned and sorted by `ChunkId`. Summary counters mirror the
 * records in `changes`; callers may persist or inspect the diff without keeping
 * the original snapshots alive.
 */
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
 * changed chunk. For chunks present in both snapshots, readiness changes take
 * precedence over residency changes, which take precedence over terrain-handle
 * presence changes. The function is CPU-only, copies all output, and does not
 * mutate snapshots or runtime owners.
 */
TerrainRuntimeStateDiff diffTerrainRuntimeStateSnapshots(
    const TerrainRuntimeStateSnapshot& previous,
    const TerrainRuntimeStateSnapshot& current);
} // namespace full_engine
