#pragma once

#include "engine/assets/AssetSourceCatalog.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadPlan.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Source lookup status for one manifest asset load request. */
enum class TerrainManifestAssetSourceRequestStatus
{
    /** @brief The request mapped to a source record with the same asset ID and kind. */
    Mapped,

    /** @brief No matching source record exists for the requested asset ID and kind. */
    MissingSource,

    /** @brief The request asset ID or kind is not valid for manifest asset loading. */
    InvalidRequest,
};

/** @brief Returns a stable diagnostic name for a source request status. */
const char* terrainManifestAssetSourceRequestStatusName(
    TerrainManifestAssetSourceRequestStatus status) noexcept;

/** @brief Ordered source lookup diagnostic for one manifest asset load request. */
struct TerrainManifestAssetSourceRequestRecord
{
    /** @brief Original renderer-free load request supplied by the caller. */
    TerrainManifestAssetLoadRequest request = {};

    /** @brief Copied source record when `status` is `Mapped`, otherwise default. */
    AssetSourceRecord source = {};

    /** @brief Source lookup outcome for this request. */
    TerrainManifestAssetSourceRequestStatus status =
        TerrainManifestAssetSourceRequestStatus::InvalidRequest;
};

/** @brief Aggregate counters for mapping manifest load requests to source records. */
struct TerrainManifestAssetSourceRequestSummary
{
    /** @brief Number of requests mapped to source records. */
    std::size_t mappedCount = 0;

    /** @brief Number of valid requests without a matching source record. */
    std::size_t missingSourceCount = 0;

    /** @brief Number of invalid requests rejected before source lookup. */
    std::size_t invalidRequestCount = 0;
};

/** @brief Ordered result of mapping manifest load requests to source records. */
struct TerrainManifestAssetSourceRequestPlan
{
    /** @brief One source lookup record per supplied request, in request order. */
    std::vector<TerrainManifestAssetSourceRequestRecord> records;

    /** @brief Aggregate source lookup counters. */
    TerrainManifestAssetSourceRequestSummary summary = {};
};

/**
 * @brief Maps renderer-free manifest load requests to asset source records.
 *
 * The helper reads caller-owned requests and a caller-owned source catalog for
 * the duration of the call. It copies matching source records into the result
 * and mutates no load request queues, source catalogs, manifest load state, job
 * queues, renderer handle catalogs, runtime state, or renderer resources.
 *
 * @param requests Caller-owned load request array. May be null only when
 * `requestCount` is zero.
 * @param requestCount Number of requests to inspect.
 * @param sources Catalog of renderer-free source descriptors to query.
 * @return Ordered lookup diagnostics and summary counters.
 */
TerrainManifestAssetSourceRequestPlan buildTerrainManifestAssetSourceRequestPlan(
    const TerrainManifestAssetLoadRequest* requests,
    std::size_t requestCount,
    const AssetSourceCatalog& sources);

/**
 * @brief Maps every request in a manifest asset load request plan to source records.
 *
 * Request order follows `plan.requests`. Duplicate requests produce duplicate
 * diagnostics and do not mutate the source catalog.
 */
TerrainManifestAssetSourceRequestPlan buildTerrainManifestAssetSourceRequestPlan(
    const TerrainManifestAssetLoadRequestPlan& plan,
    const AssetSourceCatalog& sources);
} // namespace full_engine
