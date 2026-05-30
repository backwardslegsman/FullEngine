#pragma once

#include "engine/renderer_integration/ChunkTerrainHandleMap.hpp"
#include "engine/renderer_integration/TerrainRenderPrep.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Intended terrain renderer lifecycle action selected by the CPU-side planner. */
enum class TerrainLifecycleAction
{
    Create,
    Keep,
    Update,
    Release,
};

/** @brief Options controlling CPU-side terrain lifecycle diff behavior. */
struct TerrainLifecyclePlanOptions
{
    bool updateMappedReadyChunks = false;
};

/** @brief One intended terrain lifecycle operation for a chunk. */
struct TerrainLifecycleOp
{
    ChunkId id = {};
    TerrainLifecycleAction action = TerrainLifecycleAction::Create;
    RenderBounds bounds = {};
    full_renderer::TerrainChunkHandle handle = {};
};

/** @brief Counters for intended terrain lifecycle operations. */
struct TerrainLifecyclePlanSummary
{
    std::size_t createCount = 0;
    std::size_t keepCount = 0;
    std::size_t updateCount = 0;
    std::size_t releaseCount = 0;
};

/** @brief CPU-only terrain lifecycle operation plan. */
struct TerrainLifecyclePlan
{
    std::vector<TerrainLifecycleOp> operations;
    TerrainLifecyclePlanSummary summary;
};

/**
 * @brief Diffs ready terrain prep records against mapped renderer terrain handles.
 *
 * The returned operations describe intended renderer lifecycle work only. This
 * function does not create, update, release, validate liveness, or submit
 * renderer terrain resources.
 */
TerrainLifecyclePlan planTerrainLifecycle(
    const TerrainRenderPrep& prep,
    const ChunkTerrainHandleMap& handles,
    const TerrainLifecyclePlanOptions& options = {});
} // namespace full_engine
