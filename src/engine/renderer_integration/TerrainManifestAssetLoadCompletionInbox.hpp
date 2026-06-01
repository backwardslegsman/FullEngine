#pragma once

#include "engine/renderer_integration/TerrainManifestAssetLoadJobCompletions.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Result for retaining one externally produced manifest asset-load completion. */
enum class TerrainManifestAssetLoadCompletionInboxStatus
{
    /** @brief Completion was valid and appended to the inbox. */
    Published,

    /** @brief A completion for the same asset ID and kind is already retained. */
    AlreadyPublished,

    /** @brief Completion request ID or kind is not valid for manifest asset loading. */
    InvalidRequest,

    /** @brief Completion did not contain a loaded, valid handle for its request kind. */
    MissingHandle,
};

/** @brief Returns a stable diagnostic name for an external completion inbox status. */
const char* terrainManifestAssetLoadCompletionInboxStatusName(
    TerrainManifestAssetLoadCompletionInboxStatus status) noexcept;

/** @brief Ordered diagnostic record for publishing one completion into an inbox. */
struct TerrainManifestAssetLoadCompletionInboxRecord
{
    /** @brief Source completion supplied by the caller. */
    TerrainManifestAssetLoadJobCompletion completion = {};

    /** @brief Inbox publish outcome for this completion. */
    TerrainManifestAssetLoadCompletionInboxStatus status =
        TerrainManifestAssetLoadCompletionInboxStatus::MissingHandle;
};

/** @brief Aggregate counters for retaining externally produced completions. */
struct TerrainManifestAssetLoadCompletionInboxSummary
{
    /** @brief Number of completions appended to the inbox. */
    std::size_t publishedCount = 0;

    /** @brief Number of completions skipped because the request is already retained. */
    std::size_t alreadyPublishedCount = 0;

    /** @brief Number of completions rejected for invalid request identity or kind. */
    std::size_t invalidRequestCount = 0;

    /** @brief Number of completions rejected for missing or invalid completed handles. */
    std::size_t missingHandleCount = 0;
};

/** @brief Ordered result of publishing one or more completions into an inbox. */
struct TerrainManifestAssetLoadCompletionInboxPublishResult
{
    /** @brief One diagnostic record per supplied completion, in source order. */
    std::vector<TerrainManifestAssetLoadCompletionInboxRecord> records;

    /** @brief Aggregate publish counters. */
    TerrainManifestAssetLoadCompletionInboxSummary summary = {};
};

/** @brief Result for removing one retained external completion. */
enum class TerrainManifestAssetLoadCompletionInboxRemoveStatus
{
    /** @brief A retained completion was removed. */
    Removed,

    /** @brief No retained completion matched the requested asset ID and kind. */
    NotFound,

    /** @brief The requested asset ID or kind is not valid for manifest asset loading. */
    InvalidArgument,
};

/** @brief Returns a stable diagnostic name for a completion inbox removal status. */
const char* terrainManifestAssetLoadCompletionInboxRemoveStatusName(
    TerrainManifestAssetLoadCompletionInboxRemoveStatus status) noexcept;

/** @brief Diagnostic result for removing one retained completion. */
struct TerrainManifestAssetLoadCompletionInboxRemoveResult
{
    /** @brief Request identity that the caller asked to remove. */
    TerrainManifestAssetLoadRequest request = {};

    /** @brief Removal outcome. */
    TerrainManifestAssetLoadCompletionInboxRemoveStatus status =
        TerrainManifestAssetLoadCompletionInboxRemoveStatus::InvalidArgument;
};

/** @brief Result for replacing or publishing one retained external completion. */
enum class TerrainManifestAssetLoadCompletionInboxReplaceStatus
{
    /** @brief An existing retained completion was overwritten in place. */
    Replaced,

    /** @brief No existing completion matched, so the new completion was appended. */
    Published,

    /** @brief Completion request ID or kind is not valid for manifest asset loading. */
    InvalidRequest,

    /** @brief Completion did not contain a loaded, valid handle for its request kind. */
    MissingHandle,
};

/** @brief Returns a stable diagnostic name for a completion inbox replacement status. */
const char* terrainManifestAssetLoadCompletionInboxReplaceStatusName(
    TerrainManifestAssetLoadCompletionInboxReplaceStatus status) noexcept;

/** @brief Diagnostic result for replacing or publishing one retained completion. */
struct TerrainManifestAssetLoadCompletionInboxReplaceResult
{
    /** @brief Source completion supplied by the caller. */
    TerrainManifestAssetLoadJobCompletion completion = {};

    /** @brief Replacement outcome. */
    TerrainManifestAssetLoadCompletionInboxReplaceStatus status =
        TerrainManifestAssetLoadCompletionInboxReplaceStatus::MissingHandle;
};

/**
 * @brief CPU-only inbox for externally completed manifest asset-load outputs.
 *
 * The inbox owns copied completion records until a caller reconciles them with
 * retained manifest load state. It does not execute jobs, perform IO, create
 * renderer resources, mutate renderer handle catalogs, consume load requests,
 * or apply terrain runtime queues. Completions are deduplicated by
 * `(AssetId, AssetKind)`.
 *
 * @note Thread safety: Not thread-safe. Callers must serialize mutation and
 * access.
 */
class TerrainManifestAssetLoadCompletionInbox
{
public:
    /**
     * @brief Retains one completed load-job output when valid and not already pending.
     *
     * The completion is copied by value. Mesh, Material, and Texture requests
     * with non-default IDs, `Loaded` callback status, and matching non-default
     * handle slots are accepted.
     */
    TerrainManifestAssetLoadCompletionInboxRecord publish(
        const TerrainManifestAssetLoadJobCompletion& completion);

    /**
     * @brief Retains all valid completions in source order.
     *
     * @param completions Caller-owned completion array. May be null only when
     * `completionCount` is zero.
     * @param completionCount Number of completion records to inspect.
     * @return Per-completion diagnostics plus aggregate counters.
     */
    TerrainManifestAssetLoadCompletionInboxPublishResult publish(
        const TerrainManifestAssetLoadJobCompletion* completions,
        std::size_t completionCount);

    /**
     * @brief Removes one retained completion so stale worker output can be retried.
     *
     * The call mutates only this inbox. Invalid asset IDs or unsupported kinds
     * are rejected without mutation. Removing one completion preserves the
     * relative order of all remaining completions.
     */
    TerrainManifestAssetLoadCompletionInboxRemoveResult remove(AssetId id, AssetKind kind);

    /**
     * @brief Replaces an existing retained completion or appends a new valid one.
     *
     * This is the retry-friendly publish path. The completion is validated with
     * the same rules as `publish`. If a completion for the same `(AssetId,
     * AssetKind)` is already retained, it is overwritten in place so existing
     * source order is stable; otherwise the valid completion is appended.
     */
    TerrainManifestAssetLoadCompletionInboxReplaceResult replace(
        const TerrainManifestAssetLoadJobCompletion& completion);

    /** @brief Returns whether a completion for the same asset ID and kind is retained. */
    bool contains(AssetId id, AssetKind kind) const noexcept;

    /** @brief Returns the number of retained completions. */
    std::size_t completionCount() const noexcept;

    /**
     * @brief Returns retained completions in first-published order.
     *
     * The returned reference is invalidated by any later non-const inbox call.
     */
    const std::vector<TerrainManifestAssetLoadJobCompletion>& completions() const noexcept;

    /** @brief Removes all retained completions. */
    void clear() noexcept;

private:
    std::vector<TerrainManifestAssetLoadJobCompletion> completions_;
};
} // namespace full_engine
