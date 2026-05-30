#pragma once

#include "engine/renderer_integration/ChunkTerrainHandleMap.hpp"
#include "engine/renderer_integration/TerrainDescriptorBuilder.hpp"
#include "engine/renderer_integration/TerrainIntegrationDiagnostics.hpp"
#include "engine/renderer_integration/TerrainLifecyclePlan.hpp"
#include "engine/renderer_integration/TerrainRendererCommands.hpp"
#include "engine/renderer_integration/TerrainRenderPrep.hpp"
#include "engine/renderer_integration/TerrainResourceCatalog.hpp"
#include "engine/renderer_integration/TerrainSubmissionAdapter.hpp"
#include "engine/renderer_integration/WorldRenderSnapshot.hpp"
#include "engine/world/WorldChunkCatalog.hpp"
#include "engine/world/WorldChunkRegistry.hpp"

#include "full_renderer/Renderer.hpp"

namespace full_engine
{
/** @brief Options for one engine-owned terrain pipeline run. */
struct TerrainPipelineRunOptions
{
    WorldRenderSnapshotOptions snapshotOptions = {};
    TerrainLifecyclePlanOptions lifecycleOptions = {};
};

/**
 * @brief Ordered output of one terrain pipeline run.
 *
 * The result owns the CPU-side intermediate records produced while translating
 * engine world state into renderer terrain submission calls. It does not own
 * mesh, material, texture, or renderer terrain resources.
 */
struct TerrainPipelineRunResult
{
    WorldRenderSnapshot snapshot = {};
    TerrainRenderPrep prep = {};
    TerrainLifecyclePlan lifecycle = {};
    TerrainRendererCommandList commands = {};
    TerrainDescriptorBuildResult descriptors = {};
    TerrainSubmissionResult submission = {};
    TerrainPipelineDiagnostics diagnostics = {};
    bool succeeded = false;
};

/**
 * @brief Runs the engine terrain integration pipeline against a public renderer.
 *
 * This function gathers chunk descriptors from `worldCatalog`, prepares
 * render-space terrain data, builds renderer command and descriptor intent, and
 * submits through the public renderer terrain APIs. It mutates `handles` only
 * through `submitTerrainCommands` after renderer create/destroy operations.
 */
TerrainPipelineRunResult runTerrainPipeline(
    full_renderer::IRenderer& renderer,
    const WorldChunkRegistry& registry,
    const WorldChunkCatalog& worldCatalog,
    const TerrainResourceCatalog& resources,
    ChunkTerrainHandleMap& handles,
    const TerrainPipelineRunOptions& options = {});
} // namespace full_engine
