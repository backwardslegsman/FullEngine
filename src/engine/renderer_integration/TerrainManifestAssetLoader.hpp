#pragma once

#include "engine/renderer_integration/TerrainAssetResolver.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadRequests.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Per-request status produced while consuming pending manifest asset load intent. */
enum class TerrainManifestAssetLoadStatus
{
    /** @brief Matching supplied handle was copied into the destination catalog. */
    Loaded,

    /** @brief Destination catalog already had a handle for this request. */
    AlreadyLoaded,

    /** @brief Source catalog did not contain the requested handle. */
    MissingHandle,

    /** @brief Destination catalog rejected the handle copy. */
    CatalogRejected,
};

/**
 * @brief One ordered diagnostic record from manifest asset load consumption.
 *
 * `catalogResult` is set when the helper attempts to copy a supplied renderer
 * handle into the destination handle catalog. It remains `NotFound` for
 * already-loaded or missing-handle records.
 */
struct TerrainManifestAssetLoadRecord
{
    /** @brief Original pending request inspected by the consume operation. */
    TerrainManifestAssetLoadRequest request = {};

    /** @brief Consume outcome for this request. */
    TerrainManifestAssetLoadStatus status = TerrainManifestAssetLoadStatus::MissingHandle;

    /** @brief Destination catalog mutation result when a handle copy was attempted. */
    RendererAssetHandleCatalogResult catalogResult = RendererAssetHandleCatalogResult::NotFound;
};

/** @brief Aggregate counters for manifest asset load consumption. */
struct TerrainManifestAssetLoadSummary
{
    /** @brief Number of handles copied from source to destination. */
    std::size_t loadedCount = 0;

    /** @brief Number of requests already satisfied by the destination catalog. */
    std::size_t alreadyLoadedCount = 0;

    /** @brief Number of requests missing from the source catalog. */
    std::size_t missingHandleCount = 0;

    /** @brief Number of destination catalog copy failures. */
    std::size_t catalogRejectedCount = 0;
};

/**
 * @brief Result of attempting to consume pending manifest asset load requests.
 *
 * `consumed` is true only when every request was either copied into the
 * destination handle catalog or was already present there. Failed batches leave
 * the pending queue intact so callers can retry after supplying more handles.
 */
struct TerrainManifestAssetLoadResult
{
    /** @brief One diagnostic record per pending request in queue order. */
    std::vector<TerrainManifestAssetLoadRecord> records;

    /** @brief Aggregate counters for this consume attempt. */
    TerrainManifestAssetLoadSummary summary = {};

    /** @brief True when the pending queue was cleared after a successful consume attempt. */
    bool consumed = false;
};

/**
 * @brief Copies externally supplied handles into a runtime handle catalog.
 *
 * The helper consumes pending renderer-free load intent by looking up matching
 * mesh/material/texture handles in `sourceHandles` and copying them into
 * `destinationHandles`. It performs no file IO, starts no jobs, creates no
 * renderer resources, and calls no renderer APIs. The source catalog represents
 * handles created by a caller-owned loader or test seam.
 *
 * The operation is all-or-nothing for expected failures: if any requested
 * handle is missing from `sourceHandles`, the queue and destination catalog are
 * left unchanged. When all requests can be satisfied, copied handles are added
 * to `destinationHandles` and the queue is cleared. Handles already present in
 * the destination catalog are reported as `AlreadyLoaded` and preserved.
 *
 * @param queue Pending load intent to consume. Cleared only when the operation
 * succeeds for the whole batch.
 * @param sourceHandles Caller-owned catalog of externally created handles.
 * @param destinationHandles Caller-owned runtime catalog to receive copied
 * handle mappings.
 * @return Ordered consume diagnostics and summary counters.
 *
 * @note Thread safety: Not thread-safe for shared queue or catalog instances.
 * Callers must serialize access to the supplied objects.
 */
TerrainManifestAssetLoadResult consumeTerrainManifestAssetLoadRequests(
    TerrainManifestAssetLoadRequestQueue& queue,
    const RendererAssetHandleCatalog& sourceHandles,
    RendererAssetHandleCatalog& destinationHandles);
} // namespace full_engine
