#pragma once

#include "engine/renderer_integration/TerrainChunkRequests.hpp"
#include "engine/renderer_integration/TerrainIntegrationDiagnostics.hpp"
#include "engine/renderer_integration/TerrainPipeline.hpp"
#include "engine/world/WorldResidencyRequests.hpp"

namespace full_engine
{
/** @brief Options for one stateless terrain runtime update. */
struct TerrainRuntimeUpdateOptions
{
    TerrainPipelineRunOptions pipelineOptions = {};
};

/** @brief High-level result status for one terrain runtime update. */
enum class TerrainRuntimeUpdateStatus
{
    Success,
    SetupFailed,
    ResidencyFailed,
    PipelineFailed,
};

/**
 * @brief Result of applying terrain setup, residency, and renderer pipeline work.
 *
 * The result owns value snapshots of request application, pipeline output, and
 * diagnostics. It does not own registries, catalogs, queues, renderer handles,
 * mesh/material/texture resources, UI state, or sample state.
 */
struct TerrainRuntimeUpdateResult
{
    TerrainRuntimeUpdateStatus status = TerrainRuntimeUpdateStatus::Success;
    TerrainChunkRequestApplyResult setup = {};
    WorldChunkResidencyApplyResult residency = {};
    TerrainPipelineRunResult pipeline = {};
    TerrainIntegrationDiagnostics diagnostics = {};
};

/**
 * @brief Applies queued terrain runtime intent and runs the terrain pipeline.
 *
 * Setup requests are applied before residency requests. Residency requests are
 * applied only for chunks that remain registered in world and terrain resource
 * state. Both queues are cleared after their requests have been captured. The
 * renderer is touched only through `runTerrainPipeline`.
 */
TerrainRuntimeUpdateResult updateTerrainRuntime(
    full_renderer::IRenderer& renderer,
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resources,
    ChunkTerrainHandleMap& handles,
    TerrainChunkRequestQueue& setupRequests,
    WorldChunkResidencyRequestQueue& residencyRequests,
    const TerrainRuntimeUpdateOptions& options = {});
} // namespace full_engine
