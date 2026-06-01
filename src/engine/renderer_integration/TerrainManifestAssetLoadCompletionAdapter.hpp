#pragma once

#include "engine/renderer_integration/TerrainManifestAssetLoadCompletionInbox.hpp"

#include <cstddef>

namespace full_engine
{
/**
 * @brief Result of publishing worker-produced completions into an inbox.
 *
 * The result copies publish diagnostics and the inbox size after the publish.
 * It does not retain worker state, job records, renderer resources, load state,
 * handle catalogs, or streaming loop state.
 */
struct TerrainManifestAssetLoadWorkerCompletionPublishResult
{
    /** @brief Ordered diagnostics from retaining completion records. */
    TerrainManifestAssetLoadCompletionInboxPublishResult publish = {};

    /** @brief Number of completions retained in the destination inbox after publishing. */
    std::size_t pendingCompletionCount = 0;
};

/**
 * @brief Result of replacing or publishing one worker-produced completion.
 *
 * The result copies replacement diagnostics and the inbox size after the
 * operation. It does not retain worker state, job records, renderer resources,
 * load state, handle catalogs, or streaming loop state.
 */
struct TerrainManifestAssetLoadWorkerCompletionReplaceResult
{
    /** @brief Diagnostic result from replacing or appending the completion. */
    TerrainManifestAssetLoadCompletionInboxReplaceResult replace = {};

    /** @brief Number of completions retained in the destination inbox after replacement. */
    std::size_t pendingCompletionCount = 0;
};

/**
 * @brief Result of removing one worker-produced completion from an inbox.
 *
 * The result copies removal diagnostics and the inbox size after the operation.
 * It does not consume load requests, remove jobs, mutate handles, or apply
 * runtime queues.
 */
struct TerrainManifestAssetLoadWorkerCompletionRemoveResult
{
    /** @brief Diagnostic result from removing a retained completion. */
    TerrainManifestAssetLoadCompletionInboxRemoveResult remove = {};

    /** @brief Number of completions retained in the destination inbox after removal. */
    std::size_t pendingCompletionCount = 0;
};

/**
 * @brief Publishes one worker-produced completion into an external completion inbox.
 *
 * This is the worker-facing adapter for code that produces
 * `TerrainManifestAssetLoadJobCompletion` values but should not depend on
 * `TerrainStreamingLoopState`. The destination inbox owns copied completion
 * records. The adapter performs no IO, job execution, renderer calls, renderer
 * resource creation, load-request consumption, handle-catalog mutation, or
 * runtime queue application.
 *
 * @note Thread safety: Not thread-safe for shared inbox instances. Callers
 * must serialize access.
 */
TerrainManifestAssetLoadWorkerCompletionPublishResult publishTerrainManifestAssetLoadWorkerCompletion(
    TerrainManifestAssetLoadCompletionInbox& inbox,
    const TerrainManifestAssetLoadJobCompletion& completion);

/**
 * @brief Publishes a batch of worker-produced completions into an external completion inbox.
 *
 * The completion array is read only during the call and is never retained by
 * reference. Valid completion records are copied into `inbox` in source order
 * and deduplicated by the inbox's `(AssetId, AssetKind)` policy.
 *
 * @param inbox Destination retained completion inbox.
 * @param completions Caller-owned completion array. May be null only when
 * `completionCount` is zero.
 * @param completionCount Number of completion records to inspect.
 * @return Ordered publish diagnostics and final inbox size.
 */
TerrainManifestAssetLoadWorkerCompletionPublishResult publishTerrainManifestAssetLoadWorkerCompletions(
    TerrainManifestAssetLoadCompletionInbox& inbox,
    const TerrainManifestAssetLoadJobCompletion* completions,
    std::size_t completionCount);

/**
 * @brief Replaces or publishes one worker-produced completion in an inbox.
 *
 * This retry-friendly adapter validates the completion and overwrites an
 * existing retained completion for the same `(AssetId, AssetKind)` in place.
 * When no matching completion is retained, the valid completion is appended.
 */
TerrainManifestAssetLoadWorkerCompletionReplaceResult replaceTerrainManifestAssetLoadWorkerCompletion(
    TerrainManifestAssetLoadCompletionInbox& inbox,
    const TerrainManifestAssetLoadJobCompletion& completion);

/**
 * @brief Removes one retained worker-produced completion from an inbox.
 *
 * This lets external worker code discard stale completion output by request
 * identity without depending on `TerrainStreamingLoopState`.
 */
TerrainManifestAssetLoadWorkerCompletionRemoveResult removeTerrainManifestAssetLoadWorkerCompletion(
    TerrainManifestAssetLoadCompletionInbox& inbox,
    AssetId id,
    AssetKind kind);
} // namespace full_engine
