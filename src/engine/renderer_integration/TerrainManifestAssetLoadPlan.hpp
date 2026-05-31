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
 */
struct TerrainManifestAssetLoadRequest
{
    AssetId id = {};
    AssetKind kind = AssetKind::Unknown;
};

/** @brief Aggregate counters for manifest asset load intent. */
struct TerrainManifestAssetLoadRequestSummary
{
    std::size_t requestCount = 0;
    std::size_t meshRequestCount = 0;
    std::size_t materialRequestCount = 0;
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
    std::vector<TerrainManifestAssetLoadRequest> requests;
    TerrainManifestAssetLoadRequestSummary summary = {};
};

/**
 * @brief Converts missing manifest asset readiness records into load intent.
 *
 * Only `MissingHandle` readiness records produce load requests. `Ready` records
 * are skipped. The returned requests remain renderer-free and do not decide how
 * missing assets should be loaded.
 */
TerrainManifestAssetLoadRequestPlan buildTerrainManifestAssetLoadRequestPlan(
    const TerrainManifestAssetReadinessPlan& readiness);
} // namespace full_engine
