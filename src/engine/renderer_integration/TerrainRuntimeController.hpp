#pragma once

#include "engine/renderer_integration/TerrainChunkRequests.hpp"
#include "engine/renderer_integration/TerrainIntegrationDiagnostics.hpp"
#include "engine/renderer_integration/TerrainPipeline.hpp"
#include "engine/world/WorldResidencyRequests.hpp"

#include <cstddef>

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

/**
 * @brief Caller-facing terrain runtime state for queued intent and latest results.
 *
 * The state owns only terrain setup/residency intent queues and the latest
 * update result. It does not own world registries, terrain resources, renderer
 * handles, renderer resources, UI state, or sample chunk mirrors.
 */
class TerrainRuntimeState
{
public:
    /** @brief Queues terrain setup creation intent for a chunk. */
    void queueSetupAdd(const WorldChunkDesc& worldDesc, const TerrainChunkResourceDesc& resourceDesc);

    /** @brief Queues terrain setup removal intent for a chunk. */
    void queueSetupRemove(const ChunkId& id);

    /** @brief Queues a request to drive a registered chunk to resident. */
    void queueMakeResident(const ChunkId& id);

    /** @brief Queues a request to drive a registered chunk to unloaded. */
    void queueMakeUnloaded(const ChunkId& id);

    /** @brief Returns the number of queued setup requests. */
    std::size_t setupRequestCount() const noexcept;

    /** @brief Returns the number of queued residency requests. */
    std::size_t residencyRequestCount() const noexcept;

    /** @brief Returns whether any setup or residency requests are pending. */
    bool hasPendingRequests() const noexcept;

    /** @brief Returns the latest stored runtime update result. */
    const TerrainRuntimeUpdateResult& latestUpdate() const noexcept;

    /** @brief Returns diagnostics from the latest stored runtime update result. */
    const TerrainIntegrationDiagnostics& latestDiagnostics() const noexcept;

    /** @brief Clears pending setup and residency requests without erasing the latest result. */
    void clearRequests() noexcept;

    /** @brief Applies owned queues and stores the latest terrain runtime update result. */
    const TerrainRuntimeUpdateResult& update(
        full_renderer::IRenderer& renderer,
        WorldChunkRegistry& registry,
        WorldChunkCatalog& worldCatalog,
        TerrainResourceCatalog& resources,
        ChunkTerrainHandleMap& handles,
        const TerrainRuntimeUpdateOptions& options = {});

private:
    TerrainChunkRequestQueue setupRequests_;
    WorldChunkResidencyRequestQueue residencyRequests_;
    TerrainRuntimeUpdateResult latestUpdate_ = {};
};
} // namespace full_engine
