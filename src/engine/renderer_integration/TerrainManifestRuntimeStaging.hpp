#pragma once

#include "engine/assets/CookedAssetManifest.hpp"
#include "engine/renderer_integration/TerrainAssetResolver.hpp"
#include "engine/renderer_integration/TerrainSetupStaging.hpp"

#include <cstddef>

namespace full_engine
{
/**
 * @brief Options for dry-running or queueing cooked-manifest terrain setup.
 *
 * These options affect only whether safe setup intent is appended to the
 * supplied runtime state. They do not apply queued requests, create renderer
 * resources, or change registry/catalog ownership.
 */
struct TerrainManifestRuntimeStageOptions
{
    bool queueWhenSafe = false;
    bool makeAddedChunksResident = true;
};

/**
 * @brief High-level status for manifest-to-runtime staging.
 *
 * Non-success statuses are diagnostic outcomes. The coordinator still returns
 * any partial manifest build, resource resolve, and staging-plan data that was
 * produced before the blocking condition was found.
 */
enum class TerrainManifestRuntimeStageStatus
{
    Success,
    InvalidManifest,
    MissingWorldDesc,
    AssetResolveFailed,
    QueueBlocked,
};

/**
 * @brief Counter summary for one manifest-to-runtime staging pass.
 *
 * Counts are copied from value-owned inputs and intermediate results. They do
 * not imply that runtime queues were applied or that renderer resources exist.
 */
struct TerrainManifestRuntimeStageSummary
{
    std::size_t manifestAssetCount = 0;
    std::size_t manifestTerrainChunkCount = 0;
    std::size_t resolvedResourceCount = 0;
    std::size_t missingWorldDescCount = 0;
    std::size_t desiredSetupCount = 0;
    std::size_t queuedSetupCount = 0;
    std::size_t queuedMakeResidentCount = 0;
};

/**
 * @brief Value result for dry-running or queueing manifest-driven terrain setup.
 *
 * The result stores diagnostics and value plans only. It does not own or mutate
 * renderer resources, live catalogs, runtime handles, or sample UI state.
 */
struct TerrainManifestRuntimeStageResult
{
    TerrainManifestRuntimeStageStatus status = TerrainManifestRuntimeStageStatus::Success;
    CookedAssetManifestBuildResult manifestBuild = {};
    TerrainAssetBatchResolveResult assetResolve = {};
    TerrainSetupStagePlan stagePlan = {};
    TerrainSetupStageQueueApplyResult queue = {};
    TerrainManifestRuntimeStageSummary summary = {};
};

/** @brief Returns a stable diagnostic name for a manifest runtime staging status. */
const char* terrainManifestRuntimeStageStatusName(TerrainManifestRuntimeStageStatus status) noexcept;

/**
 * @brief Builds a terrain setup staging plan from a cooked manifest and runtime state.
 *
 * The manifest supplies renderer-free asset identity, `handles` supplies
 * externally created renderer handles, and `worldDescs` supplies caller-owned
 * chunk bounds in engine world coordinates. The helper may queue safe
 * add/remove intent into `runtime` when requested, but it never applies queues,
 * mutates registries or catalogs, calls renderer APIs, creates renderer
 * resources, or loads assets. `worldDescs` may be null only when
 * `worldDescCount` is zero; missing descriptors are reported in the returned
 * status and counters.
 */
TerrainManifestRuntimeStageResult stageTerrainManifestRuntime(
    const CookedAssetManifest& manifest,
    const RendererAssetHandleCatalog& handles,
    const WorldChunkRegistry& registry,
    const WorldChunkCatalog& worldCatalog,
    const TerrainResourceCatalog& resources,
    const WorldChunkDesc* worldDescs,
    std::size_t worldDescCount,
    TerrainRuntimeState* runtime = nullptr,
    const TerrainManifestRuntimeStageOptions& options = {});
} // namespace full_engine
