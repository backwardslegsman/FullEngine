#pragma once

#include "engine/renderer_integration/TerrainIntegrationDiagnostics.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadPlan.hpp"
#include "engine/renderer_integration/TerrainManifestAssetReadiness.hpp"

#include <cstddef>

namespace full_engine
{
/**
 * @brief State-level result for staging a retained cooked terrain manifest.
 *
 * `NoManifest` means no manifest value has been supplied to the load state.
 * Runtime queues, catalogs, renderer handles, and renderer resources are not
 * mutated when this status is returned.
 */
enum class TerrainManifestLoadStageStatus
{
    Success,
    NoManifest,
};

/** @brief Returns a stable diagnostic name for manifest load-state staging. */
const char* terrainManifestLoadStageStatusName(TerrainManifestLoadStageStatus status) noexcept;

/**
 * @brief Result of staging or queueing the retained cooked manifest value.
 *
 * The nested stage result is copied from the manifest runtime staging
 * coordinator when a manifest is available, or default-initialized when no
 * manifest has been loaded.
 */
struct TerrainManifestLoadStageResult
{
    TerrainManifestLoadStageStatus status = TerrainManifestLoadStageStatus::Success;
    TerrainManifestRuntimeStageResult stage = {};
};

/**
 * @brief In-memory holder for a cooked manifest and latest staging diagnostics.
 *
 * The state owns only a renderer-free manifest value, the latest staging result,
 * and copied diagnostics. It does not own file paths, renderer handles,
 * renderer resources, world registries, terrain resource catalogs, runtime
 * queues, or UI state. The class is CPU-only and not thread-safe.
 */
class TerrainManifestLoadState
{
public:
    /** @brief Stores a manifest value by move and clears stale staging results. */
    void setManifest(CookedAssetManifest manifest);

    /** @brief Clears the retained manifest and resets latest staging results. */
    void clearManifest();

    /** @brief Returns whether a manifest value is currently retained. */
    bool hasManifest() const noexcept;

    /** @brief Returns the retained manifest, or an empty default value when none is set. */
    const CookedAssetManifest& manifest() const noexcept;

    /** @brief Returns the latest manifest runtime staging result. */
    const TerrainManifestRuntimeStageResult& latestStage() const noexcept;

    /** @brief Returns copied diagnostics from the latest staging result. */
    const TerrainManifestRuntimeStageDiagnostics& latestDiagnostics() const noexcept;

    /** @brief Returns the latest retained manifest asset handle readiness plan. */
    const TerrainManifestAssetReadinessPlan& latestReadiness() const noexcept;

    /** @brief Returns the latest renderer-free asset load request plan. */
    const TerrainManifestAssetLoadRequestPlan& latestLoadRequests() const noexcept;

    /**
     * @brief Plans renderer handle readiness for the retained manifest value.
     *
     * This compares manifest-declared mesh/material/texture references against
     * externally supplied handle mappings. It never loads files, creates
     * renderer resources, mutates handle catalogs, or changes runtime state.
     * When no manifest is retained, it stores and returns an empty plan.
     */
    const TerrainManifestAssetReadinessPlan& planAssetReadiness(
        const RendererAssetHandleCatalog& handles);

    /**
     * @brief Converts latest missing readiness records into renderer-free load intent.
     *
     * This uses the latest readiness plan already stored on the state. It does
     * not inspect files, create renderer resources, mutate handle catalogs, or
     * enqueue async jobs.
     */
    const TerrainManifestAssetLoadRequestPlan& planAssetLoadRequests();

    /**
     * @brief Dry-runs the retained manifest against current terrain setup state.
     *
     * This never queues runtime requests, applies queues, mutates catalogs,
     * calls renderer APIs, creates renderer resources, or loads assets. The
     * supplied world descriptor array is read only for the duration of the call.
     */
    TerrainManifestLoadStageResult stage(
        const RendererAssetHandleCatalog& handles,
        const WorldChunkRegistry& registry,
        const WorldChunkCatalog& worldCatalog,
        const TerrainResourceCatalog& resources,
        const WorldChunkDesc* worldDescs,
        std::size_t worldDescCount);

    /**
     * @brief Queues safe setup intent from the retained manifest into runtime state.
     *
     * This appends setup and optional make-resident requests through the
     * manifest staging coordinator when the plan is safe. It does not apply the
     * queued requests or mutate registries, catalogs, renderer handles, or
     * renderer resources.
     */
    TerrainManifestLoadStageResult queueStage(
        TerrainRuntimeState& runtime,
        const RendererAssetHandleCatalog& handles,
        const WorldChunkRegistry& registry,
        const WorldChunkCatalog& worldCatalog,
        const TerrainResourceCatalog& resources,
        const WorldChunkDesc* worldDescs,
        std::size_t worldDescCount,
        bool makeAddedChunksResident = true);

private:
    void storeStageResult(const TerrainManifestRuntimeStageResult& stage);
    TerrainManifestLoadStageResult noManifestResult();

    CookedAssetManifest manifest_ = {};
    bool hasManifest_ = false;
    TerrainManifestRuntimeStageResult latestStage_ = {};
    TerrainManifestRuntimeStageDiagnostics latestDiagnostics_ = {};
    TerrainManifestAssetReadinessPlan latestReadiness_ = {};
    TerrainManifestAssetLoadRequestPlan latestLoadRequests_ = {};
};
} // namespace full_engine
