#pragma once

#include "engine/renderer_integration/TerrainAssetResolver.hpp"
#include "engine/renderer_integration/TerrainChunkRequests.hpp"
#include "engine/renderer_integration/TerrainDescriptorBuilder.hpp"
#include "engine/renderer_integration/TerrainRenderPrep.hpp"
#include "engine/renderer_integration/TerrainRendererCommands.hpp"
#include "engine/renderer_integration/TerrainSubmissionAdapter.hpp"
#include "engine/renderer_integration/WorldRenderSnapshot.hpp"
#include "engine/world/WorldResidencyRequests.hpp"

#include <cstddef>

namespace full_engine
{
/** @brief Value snapshot of the most recent terrain asset batch resolution. */
struct TerrainAssetBatchResolveDiagnostics
{
    std::size_t requestCount = 0;
    TerrainAssetBatchResolveSummary summary = {};
};

/** @brief Value snapshot of the most recently applied terrain setup request batch. */
struct TerrainSetupRequestDiagnostics
{
    std::size_t requestCount = 0;
    std::size_t addCount = 0;
    std::size_t removeCount = 0;
    TerrainChunkRequestApplySummary summary = {};
};

/** @brief Value snapshot of the most recently applied terrain residency request batch. */
struct TerrainResidencyRequestDiagnostics
{
    std::size_t requestCount = 0;
    std::size_t makeResidentCount = 0;
    std::size_t makeUnloadedCount = 0;
    WorldChunkResidencyApplySummary summary = {};
};

/** @brief Value snapshot of terrain integration counters produced by one pipeline run. */
struct TerrainPipelineDiagnostics
{
    std::size_t handleCount = 0;
    std::size_t snapshotReadyCount = 0;
    std::size_t snapshotNotResidentCount = 0;
    std::size_t snapshotMissingChunkCount = 0;
    std::size_t snapshotInvalidBoundsCount = 0;
    std::size_t snapshotOutOfRangeCount = 0;
    std::size_t snapshotInvalidInputCount = 0;
    TerrainRenderPrepSummary prep = {};
    TerrainLifecyclePlanSummary lifecycle = {};
    TerrainRendererCommandSummary commands = {};
    TerrainDescriptorBuildSummary descriptors = {};
    TerrainSubmissionSummary submission = {};
};

/** @brief Aggregated terrain integration diagnostics suitable for debug UI display. */
struct TerrainIntegrationDiagnostics
{
    TerrainSetupRequestDiagnostics setupRequests = {};
    TerrainResidencyRequestDiagnostics residencyRequests = {};
    TerrainPipelineDiagnostics pipeline = {};
};

/**
 * @brief Builds reusable diagnostics from a terrain asset batch resolve result.
 *
 * The returned value copies counters only. It does not retain references to
 * per-chunk records, resolved resource descriptors, renderer handles, or
 * renderer resource ownership.
 */
TerrainAssetBatchResolveDiagnostics makeTerrainAssetBatchResolveDiagnostics(
    const TerrainAssetBatchResolveResult& result);

/**
 * @brief Builds reusable diagnostics from an applied terrain setup request batch.
 *
 * The returned value copies counters only. It does not retain references to the
 * request records and does not mutate setup queues, catalogs, or renderer state.
 */
TerrainSetupRequestDiagnostics makeTerrainSetupRequestDiagnostics(
    const TerrainChunkRequestApplyResult& result);

/**
 * @brief Builds reusable diagnostics from an applied residency request batch.
 *
 * The returned value copies counters only. It does not retain references to the
 * request records and does not mutate residency queues or world registries.
 */
TerrainResidencyRequestDiagnostics makeTerrainResidencyRequestDiagnostics(
    const WorldChunkResidencyApplyResult& result);

/**
 * @brief Builds reusable diagnostics from one terrain integration pipeline run.
 *
 * The returned value copies summary counters and the supplied handle-map count.
 * It does not retain references to chunk records, descriptor intents, command
 * records, renderer handles, or submission results.
 */
TerrainPipelineDiagnostics makeTerrainPipelineDiagnostics(
    const WorldRenderSnapshot& snapshot,
    const TerrainRenderPrep& prep,
    const TerrainLifecyclePlan& lifecycle,
    const TerrainRendererCommandList& commands,
    const TerrainDescriptorBuildResult& descriptors,
    const TerrainSubmissionResult& submission,
    std::size_t handleCount);
} // namespace full_engine
