#pragma once

#include "engine/renderer_integration/TerrainRendererCommands.hpp"
#include "engine/renderer_integration/TerrainResourceCatalog.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Result status for one terrain descriptor-intent build record. */
enum class TerrainDescriptorBuildStatus
{
    Ready,
    IgnoredCommand,
    MissingResources,
    InvalidResources,
};

/** @brief Engine-owned renderer-shaped terrain descriptor intent. */
struct TerrainDescriptorIntent
{
    TerrainDescriptorIntent();
    TerrainDescriptorIntent(const TerrainDescriptorIntent& other);
    TerrainDescriptorIntent& operator=(const TerrainDescriptorIntent& other);
    TerrainDescriptorIntent(TerrainDescriptorIntent&& other) noexcept;
    TerrainDescriptorIntent& operator=(TerrainDescriptorIntent&& other) noexcept;

    ChunkId id = {};
    TerrainRendererCommandType sourceCommand = TerrainRendererCommandType::CreateChunk;
    TerrainDescriptorBuildStatus status = TerrainDescriptorBuildStatus::IgnoredCommand;
    full_renderer::TerrainChunkHandle handle = {};
    std::array<full_renderer::TerrainLodDesc, full_renderer::kMaxTerrainLodLevels> lods = {};
    full_renderer::TerrainChunkDesc desc = {};

    /** @brief Repoints `desc.lods` at owned LOD storage when the intent is ready. */
    void refreshDescriptorPointers() noexcept;
};

/** @brief Counters for terrain descriptor-intent build output. */
struct TerrainDescriptorBuildSummary
{
    std::size_t readyCount = 0;
    std::size_t ignoredCount = 0;
    std::size_t missingResourcesCount = 0;
    std::size_t invalidResourcesCount = 0;
};

/** @brief CPU-only terrain descriptor-intent build result. */
struct TerrainDescriptorBuildResult
{
    std::vector<TerrainDescriptorIntent> intents;
    TerrainDescriptorBuildSummary summary;
};

/**
 * @brief Builds renderer-shaped descriptor intent from terrain commands and resources.
 *
 * This function does not call renderer APIs, allocate renderer resources,
 * mutate handle maps, or submit terrain. Ready intents own their LOD storage;
 * `TerrainChunkDesc::lods` is valid while the containing intent is alive.
 */
TerrainDescriptorBuildResult buildTerrainDescriptors(
    const TerrainRendererCommandList& commands,
    const TerrainResourceCatalog& resources);
} // namespace full_engine
