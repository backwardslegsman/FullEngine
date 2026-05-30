#pragma once

#include "engine/renderer_integration/TerrainLifecyclePlan.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief CPU-side renderer command intent derived from terrain lifecycle operations. */
enum class TerrainRendererCommandType
{
    CreateChunk,
    KeepChunk,
    UpdateChunk,
    DestroyChunk,
};

/** @brief One engine-owned terrain renderer command record. */
struct TerrainRendererCommand
{
    ChunkId id = {};
    TerrainRendererCommandType type = TerrainRendererCommandType::CreateChunk;
    RenderBounds bounds = {};
    full_renderer::TerrainChunkHandle handle = {};
};

/** @brief Counters for terrain renderer command intent. */
struct TerrainRendererCommandSummary
{
    std::size_t createCount = 0;
    std::size_t keepCount = 0;
    std::size_t updateCount = 0;
    std::size_t destroyCount = 0;
};

/** @brief CPU-only terrain renderer command list. */
struct TerrainRendererCommandList
{
    std::vector<TerrainRendererCommand> commands;
    TerrainRendererCommandSummary summary;
};

/**
 * @brief Converts terrain lifecycle operations into renderer command intent.
 *
 * This function preserves lifecycle operation order and copies existing fields
 * only. It does not create renderer terrain descriptors, mutate handle maps,
 * allocate resources, validate renderer liveness, or call renderer APIs.
 */
TerrainRendererCommandList buildTerrainRendererCommands(const TerrainLifecyclePlan& plan);
} // namespace full_engine
