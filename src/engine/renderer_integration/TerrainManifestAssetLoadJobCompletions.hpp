#pragma once

#include "engine/renderer_integration/TerrainManifestAssetLoadExecutor.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadJobCoordinator.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/**
 * @brief Caller-owned output for one externally completed manifest asset-load job.
 *
 * Completion records carry the original renderer-free request plus the public
 * renderer handle produced by caller-owned work. The active handle slot is
 * selected from `request.kind`. The engine copies handles by value into handle
 * catalogs and does not own worker state, renderer resources, files, futures,
 * or cancellation policy.
 */
struct TerrainManifestAssetLoadJobCompletion
{
    /** @brief Original manifest asset load request that completed externally. */
    TerrainManifestAssetLoadRequest request = {};

    /** @brief Caller-owned completed handle value for the request. */
    TerrainManifestAssetLoadCallbackResult output = {};
};

/** @brief Per-completion publishing outcome. */
enum class TerrainManifestAssetLoadJobCompletionStatus
{
    /** @brief The completion supplied a valid handle and it was published. */
    Published,

    /** @brief A handle for the same request was already present in the destination catalog. */
    AlreadyPublished,

    /** @brief The request asset ID or asset kind is invalid for manifest asset loading. */
    InvalidRequest,

    /** @brief The completion did not contain a loaded, valid handle for the request kind. */
    MissingHandle,

    /** @brief The destination catalog rejected the otherwise valid completion. */
    CatalogRejected,
};

/** @brief Returns a stable diagnostic name for a completion publishing status. */
const char* terrainManifestAssetLoadJobCompletionStatusName(
    TerrainManifestAssetLoadJobCompletionStatus status) noexcept;

/** @brief Ordered diagnostic record for publishing one external completion. */
struct TerrainManifestAssetLoadJobCompletionRecord
{
    /** @brief Source completion supplied by the caller. */
    TerrainManifestAssetLoadJobCompletion completion = {};

    /** @brief Publishing outcome for this completion. */
    TerrainManifestAssetLoadJobCompletionStatus status =
        TerrainManifestAssetLoadJobCompletionStatus::MissingHandle;

    /** @brief Result from the destination handle catalog when insertion is attempted. */
    RendererAssetHandleCatalogResult catalogResult = RendererAssetHandleCatalogResult::NotFound;
};

/** @brief Aggregate counters for publishing external load-job completions. */
struct TerrainManifestAssetLoadJobCompletionSummary
{
    /** @brief Number of completions published into the destination catalog. */
    std::size_t publishedCount = 0;

    /** @brief Number of completions skipped because the handle was already present. */
    std::size_t alreadyPublishedCount = 0;

    /** @brief Number of completions rejected for invalid request identity or kind. */
    std::size_t invalidRequestCount = 0;

    /** @brief Number of completions missing a loaded, valid handle. */
    std::size_t missingHandleCount = 0;

    /** @brief Number of completions rejected by the destination handle catalog. */
    std::size_t catalogRejectedCount = 0;
};

/** @brief Ordered result of publishing external manifest asset-load completions. */
struct TerrainManifestAssetLoadJobCompletionPublishResult
{
    /** @brief One diagnostic record per supplied completion, in source order. */
    std::vector<TerrainManifestAssetLoadJobCompletionRecord> records;

    /** @brief Aggregate publishing counters. */
    TerrainManifestAssetLoadJobCompletionSummary summary = {};
};

/** @brief High-level outcome for publishing completions and reconciling retained load state. */
enum class TerrainManifestAssetLoadJobCompletionReconcileStatus
{
    /** @brief Completions published, retained load requests consumed, jobs cleaned up, and readiness replanned. */
    Success,

    /** @brief The retained manifest load state had no pending asset load requests. */
    NoPendingLoads,

    /** @brief One or more completion records were invalid or missing required handles. */
    CompletionPublishFailed,

    /** @brief Valid completions published, but one or more required handles are still pending. */
    CompletionPending,

    /** @brief Valid completions published, but retained load request consumption was blocked. */
    LoadConsumeBlocked,
};

/** @brief Returns a stable diagnostic name for completion reconcile status. */
const char* terrainManifestAssetLoadJobCompletionReconcileStatusName(
    TerrainManifestAssetLoadJobCompletionReconcileStatus status) noexcept;

/**
 * @brief Result of publishing external completions and reconciling retained load state.
 *
 * `publish` describes the caller-owned completion handoff. `reconcile` is
 * populated only when publishing succeeds and the existing schedule-only
 * reconcile path is attempted.
 */
struct TerrainManifestAssetLoadJobCompletionReconcileResult
{
    /** @brief High-level completion reconcile outcome. */
    TerrainManifestAssetLoadJobCompletionReconcileStatus status =
        TerrainManifestAssetLoadJobCompletionReconcileStatus::NoPendingLoads;

    /** @brief Ordered diagnostics from publishing completion records into a temporary catalog. */
    TerrainManifestAssetLoadJobCompletionPublishResult publish = {};

    /** @brief Nested diagnostics from the existing all-or-nothing reconcile path. */
    TerrainManifestAssetLoadJobReconcileResult reconcile = {};
};

/**
 * @brief Publishes caller-owned completed manifest asset-load outputs into a handle catalog.
 *
 * Each completion is validated against its request kind and copied into
 * `completedHandles` when valid. Duplicate completions report
 * `AlreadyPublished`. The function performs no file IO, job execution,
 * threading, renderer calls, renderer-resource creation, retained load-request
 * consumption, or runtime mutation.
 *
 * @param completions Caller-owned completion array. May be null only when
 * `completionCount` is zero.
 * @param completionCount Number of completion records to inspect.
 * @param completedHandles Destination catalog receiving valid completed
 * handles by value. Existing mappings are preserved.
 * @return Ordered publishing diagnostics in source order.
 *
 * @note Thread safety: Not thread-safe for shared destination catalogs.
 * Callers must serialize access.
 */
TerrainManifestAssetLoadJobCompletionPublishResult publishTerrainManifestAssetLoadJobCompletions(
    const TerrainManifestAssetLoadJobCompletion* completions,
    std::size_t completionCount,
    RendererAssetHandleCatalog& completedHandles);

/**
 * @brief Publishes external completions and reconciles retained manifest load jobs when safe.
 *
 * Completion records are first published into a temporary completed-handle
 * catalog. If any completion is invalid, missing a valid handle, or rejected by
 * that catalog, the retained manifest load state, scheduled jobs, and runtime
 * destination handles are left unchanged. When publishing succeeds, the helper
 * delegates to `reconcileTerrainManifestAssetLoadJobs` so the existing
 * all-or-nothing consume policy remains authoritative.
 *
 * The helper does not perform file IO, execute jobs, create threads, call
 * renderer APIs, create renderer resources, or apply terrain runtime queues.
 *
 * @param manifestLoad Retained manifest load state owning pending load intent.
 * @param jobs Caller-owned job queue containing scheduled manifest load jobs.
 * @param completions Caller-owned external completion records. May be null
 * only when `completionCount` is zero.
 * @param completionCount Number of completion records to inspect.
 * @param destinationHandles Runtime handle catalog to receive loaded handles
 * if reconcile succeeds.
 * @return Publishing diagnostics plus nested reconcile diagnostics when
 * attempted.
 *
 * @note Thread safety: Not thread-safe for shared state, queue, or catalog
 * instances. Callers must serialize access.
 */
TerrainManifestAssetLoadJobCompletionReconcileResult reconcileTerrainManifestAssetLoadJobCompletions(
    TerrainManifestLoadState& manifestLoad,
    EngineJobQueue& jobs,
    const TerrainManifestAssetLoadJobCompletion* completions,
    std::size_t completionCount,
    RendererAssetHandleCatalog& destinationHandles);
} // namespace full_engine
