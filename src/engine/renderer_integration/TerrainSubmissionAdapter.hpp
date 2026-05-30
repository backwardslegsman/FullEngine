#pragma once

#include "engine/renderer_integration/ChunkTerrainHandleMap.hpp"
#include "engine/renderer_integration/TerrainDescriptorBuilder.hpp"

#include "full_renderer/Renderer.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Result status for one terrain renderer submission operation. */
enum class TerrainSubmissionOpStatus
{
    Created,
    Updated,
    Destroyed,
    Kept,
    Skipped,
    RendererFailed,
    HandleMapFailed,
};

/** @brief One terrain renderer submission operation result. */
struct TerrainSubmissionOp
{
    ChunkId id = {};
    TerrainRendererCommandType sourceCommand = TerrainRendererCommandType::CreateChunk;
    TerrainSubmissionOpStatus status = TerrainSubmissionOpStatus::Skipped;
    full_renderer::RendererResult rendererResult = full_renderer::RendererResult::Success;
    full_renderer::TerrainChunkHandle handle = {};
};

/** @brief Counters for terrain renderer submission adapter output. */
struct TerrainSubmissionSummary
{
    std::size_t createdCount = 0;
    std::size_t updatedCount = 0;
    std::size_t destroyedCount = 0;
    std::size_t keptCount = 0;
    std::size_t skippedCount = 0;
    std::size_t rendererFailedCount = 0;
    std::size_t handleMapFailedCount = 0;
};

/** @brief Ordered result of applying terrain command intent to a renderer. */
struct TerrainSubmissionResult
{
    std::vector<TerrainSubmissionOp> operations;
    TerrainSubmissionSummary summary;
};

/**
 * @brief Applies terrain descriptor intent to the public renderer terrain APIs.
 *
 * This adapter calls only `IRenderer` terrain chunk creation, update, and
 * destruction APIs. It mutates `handles` only after successful create or
 * destroy operations and does not submit frames or manage mesh/material/texture
 * resources.
 */
TerrainSubmissionResult submitTerrainCommands(
    full_renderer::IRenderer& renderer,
    const TerrainDescriptorBuildResult& descriptors,
    const TerrainRendererCommandList& commands,
    ChunkTerrainHandleMap& handles);
} // namespace full_engine
