#pragma once

#include "engine/assets/CookedAssetManifest.hpp"
#include "engine/renderer_integration/TerrainAssetResolver.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
/** @brief Renderer handle catalog kind required by a manifest asset reference. */
enum class TerrainManifestAssetHandleKind
{
    Mesh,
    Material,
    Texture,
};

/** @brief Readiness status for one manifest asset handle requirement. */
enum class TerrainManifestAssetReadinessStatus
{
    Ready,
    MissingHandle,
};

/**
 * @brief One deduplicated manifest asset handle requirement.
 *
 * Records are value diagnostics only. They do not own renderer resources and
 * do not prove renderer resource liveness beyond handle presence in the
 * externally supplied handle catalog.
 */
struct TerrainManifestAssetReadinessRecord
{
    AssetId id = {};
    TerrainManifestAssetHandleKind kind = TerrainManifestAssetHandleKind::Mesh;
    TerrainManifestAssetReadinessStatus status = TerrainManifestAssetReadinessStatus::MissingHandle;
};

/** @brief Aggregate counters for manifest asset handle readiness. */
struct TerrainManifestAssetReadinessSummary
{
    std::size_t requestedCount = 0;
    std::size_t readyCount = 0;
    std::size_t missingHandleCount = 0;
    std::size_t meshRequestedCount = 0;
    std::size_t meshReadyCount = 0;
    std::size_t meshMissingHandleCount = 0;
    std::size_t materialRequestedCount = 0;
    std::size_t materialReadyCount = 0;
    std::size_t materialMissingHandleCount = 0;
    std::size_t textureRequestedCount = 0;
    std::size_t textureReadyCount = 0;
    std::size_t textureMissingHandleCount = 0;
};

/**
 * @brief Ordered readiness plan for manifest asset references.
 *
 * Records are sorted by handle kind, then asset ID. The plan is CPU-only and
 * does not mutate manifests, handle catalogs, runtime state, or renderer
 * resources.
 */
struct TerrainManifestAssetReadinessPlan
{
    std::vector<TerrainManifestAssetReadinessRecord> records;
    TerrainManifestAssetReadinessSummary summary = {};
};

/**
 * @brief Compares manifest-declared renderer asset references with handle mappings.
 *
 * The planner gathers active terrain LOD mesh/material references, non-default
 * terrain splat map references, and active generic dependencies whose kind is
 * mesh, material, or texture. It deduplicates by asset ID and handle kind, then
 * reports whether each requirement has a matching renderer handle mapping.
 */
TerrainManifestAssetReadinessPlan planTerrainManifestAssetReadiness(
    const CookedAssetManifest& manifest,
    const RendererAssetHandleCatalog& handles);
} // namespace full_engine
