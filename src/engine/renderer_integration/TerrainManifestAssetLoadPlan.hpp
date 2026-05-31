#pragma once

#include "engine/renderer_integration/TerrainManifestAssetReadiness.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/**
 * @brief Renderer-free load intent for one missing manifest asset handle.
 *
 * Requests identify the asset ID and expected asset kind only. They do not
 * contain file paths, importer state, renderer handles, renderer resources, or
 * async job ownership.
 *
 * @note Thread safety: Plain value type. Synchronization is the caller's
 * responsibility if instances are shared across threads.
 */
struct TerrainManifestAssetLoadRequest
{
    /** @brief Engine asset ID that needs a renderer handle. A default ID is invalid. */
    AssetId id = {};

    /** @brief Expected renderer-facing asset kind. Only Mesh, Material, and Texture are loadable here. */
    AssetKind kind = AssetKind::Unknown;
};

/** @brief Aggregate counters for renderer-free manifest asset load intent. */
struct TerrainManifestAssetLoadRequestSummary
{
    /** @brief Total number of load requests represented by the plan or queue. */
    std::size_t requestCount = 0;

    /** @brief Number of requests for mesh renderer handles. */
    std::size_t meshRequestCount = 0;

    /** @brief Number of requests for material renderer handles. */
    std::size_t materialRequestCount = 0;

    /** @brief Number of requests for texture renderer handles. */
    std::size_t textureRequestCount = 0;
};

/**
 * @brief Ordered load intent records derived from missing readiness records.
 *
 * Request order follows the source readiness plan order. The plan is value-only
 * and does not mutate manifests, handle catalogs, runtime state, renderer
 * resources, or IO state.
 */
struct TerrainManifestAssetLoadRequestPlan
{
    /** @brief Ordered renderer-free load requests. */
    std::vector<TerrainManifestAssetLoadRequest> requests;

    /** @brief Counters matching the request vector contents. */
    TerrainManifestAssetLoadRequestSummary summary = {};
};

/**
 * @brief Converts missing manifest asset readiness records into load intent.
 *
 * Only `MissingHandle` readiness records produce load requests. `Ready` records
 * are skipped. The returned requests remain renderer-free and do not decide how
 * missing assets should be loaded.
 *
 * @param readiness Manifest asset readiness records to inspect. The data is
 * read only for the duration of the call.
 * @return Ordered value plan containing one load request for each missing
 * renderer handle.
 *
 * @note Thread safety: Reentrant for independent input values. The function
 * performs no IO and touches no renderer state.
 */
TerrainManifestAssetLoadRequestPlan buildTerrainManifestAssetLoadRequestPlan(
    const TerrainManifestAssetReadinessPlan& readiness);
} // namespace full_engine
