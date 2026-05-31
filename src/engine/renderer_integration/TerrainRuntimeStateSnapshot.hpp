#pragma once

#include "engine/renderer_integration/ChunkTerrainHandleMap.hpp"
#include "engine/renderer_integration/TerrainResourceCatalog.hpp"
#include "engine/world/WorldChunkCatalog.hpp"
#include "engine/world/WorldChunkRegistry.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Readiness classification for one terrain runtime chunk state snapshot. */
enum class TerrainRuntimeChunkReadiness
{
    Renderable,
    MissingRegistry,
    MissingWorldDesc,
    MissingResources,
    NotResident,
    MissingTerrainHandle,
};

/** @brief Returns a stable diagnostic name for a terrain runtime chunk readiness value. */
const char* terrainRuntimeChunkReadinessName(TerrainRuntimeChunkReadiness readiness) noexcept;

/** @brief Value snapshot of terrain setup, residency, resource, and handle state for one chunk. */
struct TerrainRuntimeChunkState
{
    ChunkId id = {};
    bool hasRegistry = false;
    bool hasWorldDesc = false;
    bool hasResources = false;
    bool hasTerrainHandle = false;
    ChunkResidencyState residency = ChunkResidencyState::Unloaded;
    TerrainRuntimeChunkReadiness readiness = TerrainRuntimeChunkReadiness::MissingRegistry;
};

/** @brief Ordered terrain runtime chunk state snapshot with summary counters. */
struct TerrainRuntimeStateSnapshot
{
    std::vector<TerrainRuntimeChunkState> chunks;
    std::size_t renderableCount = 0;
    std::size_t missingRegistryCount = 0;
    std::size_t missingWorldDescCount = 0;
    std::size_t missingResourcesCount = 0;
    std::size_t notResidentCount = 0;
    std::size_t missingTerrainHandleCount = 0;
    std::size_t invalidInputCount = 0;
};

/**
 * @brief Builds value snapshots for caller-supplied terrain chunk IDs.
 *
 * The input order is preserved exactly. The function only reads registry,
 * catalog, resource, and terrain handle state; it does not mutate queues,
 * catalogs, handle maps, renderer resources, or runtime diagnostics.
 */
TerrainRuntimeStateSnapshot buildTerrainRuntimeStateSnapshot(
    const WorldChunkRegistry& registry,
    const WorldChunkCatalog& worldCatalog,
    const TerrainResourceCatalog& resources,
    const ChunkTerrainHandleMap& handles,
    const ChunkId* ids,
    std::size_t idCount);
} // namespace full_engine
