#pragma once

#include "engine/assets/CookedAssetManifestJson.hpp"
#include "engine/renderer_integration/TerrainManifestLoadState.hpp"

#include <cstddef>

namespace full_engine
{
/**
 * @brief Status for synchronously loading a cooked terrain manifest file into load state.
 *
 * This status describes the file-load coordinator result. Detailed parser,
 * validation, and IO status remains available through the nested
 * `CookedAssetManifestImport` value.
 */
enum class TerrainManifestFileLoadStatus
{
    /** @brief The manifest file imported successfully and was retained in the load state. */
    Success,

    /** @brief The supplied path was null or empty. The target load state was cleared. */
    InvalidArgument,

    /** @brief Import, parse, or manifest validation failed. The target load state was cleared. */
    ImportFailed,
};

/**
 * @brief Result of loading a cooked manifest JSON Lines file into `TerrainManifestLoadState`.
 *
 * `imported` contains the underlying asset-layer import result and validation
 * detail. Counts are nonzero only on success. The result owns copied diagnostic
 * values only; it does not own file handles, renderer handles, renderer
 * resources, runtime queues, or catalog state.
 */
struct TerrainManifestFileLoadResult
{
    /** @brief High-level file-load coordinator status. */
    TerrainManifestFileLoadStatus status = TerrainManifestFileLoadStatus::Success;

    /** @brief Underlying JSONL import result and manifest validation detail. */
    CookedAssetManifestImport imported = {};

    /** @brief Number of generic asset records in the retained manifest on success. */
    std::size_t assetCount = 0;

    /** @brief Number of terrain chunk asset descriptors in the retained manifest on success. */
    std::size_t terrainChunkCount = 0;
};

/**
 * @brief Result of loading a cooked manifest file and preparing missing-asset load intent.
 *
 * The nested `load` result describes the file import and state retention step.
 * Readiness, load-request, and queue diagnostics are copied from
 * `TerrainManifestLoadState` only when the load succeeds. The result owns
 * copied diagnostics only and does not own renderer handles, renderer
 * resources, queues, catalogs, or file handles.
 */
struct TerrainManifestFileReloadPlanResult
{
    /** @brief File-load and manifest-retention diagnostics. */
    TerrainManifestFileLoadResult load = {};

    /** @brief Copied handle-readiness plan produced after a successful load. */
    TerrainManifestAssetReadinessPlan readiness = {};

    /** @brief Copied renderer-free load-request plan produced after readiness planning. */
    TerrainManifestAssetLoadRequestPlan loadRequests = {};

    /** @brief Copied diagnostics from queueing the load-request plan into the state-owned queue. */
    TerrainManifestAssetLoadQueuePushResult queue = {};
};

/** @brief Returns a stable diagnostic name for a terrain manifest file-load status. */
const char* terrainManifestFileLoadStatusName(TerrainManifestFileLoadStatus status) noexcept;

/**
 * @brief Imports a cooked manifest JSON Lines file and stores it in load state.
 *
 * The helper is a synchronous CPU-side coordinator around
 * `importCookedAssetManifestJsonLines` and `TerrainManifestLoadState`. On
 * success it copies the imported manifest value into `state` with
 * `setManifest`, which clears stale readiness, load-request, staging, and
 * consume diagnostics. On invalid arguments or import failure it clears
 * `state` so callers cannot accidentally stage or queue against stale manifest
 * data.
 *
 * The call performs no renderer handle lookup, renderer calls, async work,
 * asset loading, resource creation, terrain runtime queueing, or catalog
 * mutation beyond the supplied load state.
 *
 * @param path Null-terminated path to the JSON Lines manifest file. The path is
 * read during the call and is not retained.
 * @param state Caller-owned load state to update or clear.
 * @return Value diagnostics describing the file-load operation.
 *
 * @note Thread safety: Not thread-safe for shared `state` instances. Callers
 * must serialize access to the supplied state object.
 */
TerrainManifestFileLoadResult loadTerrainManifestFileIntoState(
    const char* path,
    TerrainManifestLoadState& state);

/**
 * @brief Reloads a cooked manifest file and queues missing renderer-handle load intent.
 *
 * This helper chains `loadTerrainManifestFileIntoState`, manifest asset
 * readiness planning against `handles`, renderer-free load-request planning,
 * and queueing those load requests into the retained queue owned by `state`.
 * When file loading fails, `state` is cleared by the load step and no readiness
 * or queue work is attempted.
 *
 * The call does not consume load requests, create renderer resources, mutate
 * handle catalogs, apply terrain runtime queues, call renderer APIs, or start
 * async work. The supplied handle catalog is read only and not retained.
 *
 * @param path Null-terminated path to the JSON Lines manifest file. The path is
 * read during the call and is not retained.
 * @param state Caller-owned load state to update, plan, and queue into.
 * @param handles Caller-owned renderer handle catalog used for readiness
 * checks. The catalog is not retained or mutated.
 * @return Value diagnostics for the load, readiness, load-plan, and queueing
 * steps.
 *
 * @note Thread safety: Not thread-safe for shared `state` instances. Callers
 * must serialize access to the supplied state object.
 */
TerrainManifestFileReloadPlanResult reloadTerrainManifestFileAndQueueMissingAssetLoads(
    const char* path,
    TerrainManifestLoadState& state,
    const RendererAssetHandleCatalog& handles);
} // namespace full_engine
