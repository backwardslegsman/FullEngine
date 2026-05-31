#pragma once

#include "engine/renderer_integration/TerrainManifestAssetLoader.hpp"

#include <vector>

namespace full_engine
{
/** @brief Status returned by a caller-owned manifest asset load callback. */
enum class TerrainManifestAssetLoadCallbackStatus
{
    /** @brief The callback supplied a renderer handle for the requested asset. */
    Loaded,

    /** @brief The callback could not find the requested asset handle yet. */
    Missing,

    /** @brief The callback attempted loading and failed. */
    Failed,
};

/**
 * @brief Caller-supplied callback result for one manifest asset load request.
 *
 * The active handle field is selected from the request's `AssetKind`. Invalid
 * or wrong-kind handles block the whole executor batch. The value carries
 * renderer handles only; it does not transfer renderer resource ownership to
 * the executor.
 */
struct TerrainManifestAssetLoadCallbackResult
{
    /** @brief Callback outcome for this request. */
    TerrainManifestAssetLoadCallbackStatus status = TerrainManifestAssetLoadCallbackStatus::Missing;

    /** @brief Mesh handle used when the request kind is `AssetKind::Mesh`. */
    full_renderer::MeshHandle mesh = {};

    /** @brief Material handle used when the request kind is `AssetKind::Material`. */
    full_renderer::MaterialHandle material = {};

    /** @brief Texture handle used when the request kind is `AssetKind::Texture`. */
    full_renderer::TextureHandle texture = {};
};

/**
 * @brief Synchronous caller-owned asset load callback.
 *
 * The callback may perform external IO, importer work, renderer-resource
 * creation, or test-handle lookup. `full_engine` does not own or schedule that
 * work; it only consumes the returned public renderer handles.
 */
using TerrainManifestAssetLoadCallback = TerrainManifestAssetLoadCallbackResult (*)(
    const TerrainManifestAssetLoadRequest& request,
    void* userData);

/** @brief High-level status for callback-driven load execution. */
enum class TerrainManifestAssetLoadExecutorStatus
{
    /** @brief All pending requests were already loaded or were loaded and the queue was consumed. */
    Consumed,

    /** @brief At least one request could not be satisfied; queue and destination were left unchanged. */
    Blocked,
};

/**
 * @brief Ordered diagnostic record for callback-driven load execution.
 *
 * Records are emitted for every pending request in queue order. Already-loaded
 * destination handles skip the callback and report `callbackInvoked == false`.
 */
struct TerrainManifestAssetLoadExecutorRecord
{
    /** @brief Original pending request. */
    TerrainManifestAssetLoadRequest request = {};

    /** @brief True when the caller callback was invoked for this request. */
    bool callbackInvoked = false;

    /** @brief Callback result when invoked, otherwise a default value. */
    TerrainManifestAssetLoadCallbackResult callback = {};

    /** @brief Result of inserting the callback-supplied handle into the temporary source catalog. */
    RendererAssetHandleCatalogResult sourceCatalogResult = RendererAssetHandleCatalogResult::NotFound;
};

/**
 * @brief Result of executing pending manifest asset load intent through a callback.
 *
 * `callbackRecords` describe preflight callback activity. `consume` is the
 * lower-level all-or-nothing source-catalog consumption result and is populated
 * only when the callback preflight succeeds.
 */
struct TerrainManifestAssetLoadExecutorResult
{
    /** @brief High-level executor outcome. */
    TerrainManifestAssetLoadExecutorStatus status = TerrainManifestAssetLoadExecutorStatus::Blocked;

    /** @brief One preflight/callback diagnostic record per pending request. */
    std::vector<TerrainManifestAssetLoadExecutorRecord> callbackRecords;

    /** @brief Final queue-consumption result when preflight succeeds. */
    TerrainManifestAssetLoadResult consume = {};
};

/**
 * @brief Executes pending load requests through a synchronous caller callback.
 *
 * The helper preflights every pending request before mutating the destination
 * handle catalog. Requests already present in `destinationHandles` skip the
 * callback. Missing destination mappings call `callback`, collect valid
 * renderer handles into a temporary source catalog, then delegate to
 * `consumeTerrainManifestAssetLoadRequests` for all-or-nothing destination
 * mutation and queue clearing.
 *
 * The helper itself performs no file IO, importer work, async scheduling,
 * renderer calls, or renderer-resource creation. Those responsibilities remain
 * with the caller-owned callback.
 *
 * @param queue Pending renderer-free load intent. Cleared only when all
 * requests are satisfied.
 * @param destinationHandles Caller-owned runtime handle catalog to receive
 * loaded mappings.
 * @param callback Caller-owned synchronous handle provider. Required only for
 * requests not already present in `destinationHandles`.
 * @param userData Opaque caller data passed through to `callback`.
 * @return Ordered callback diagnostics plus final consume diagnostics.
 *
 * @note Thread safety: Not thread-safe for shared queue or catalog instances.
 * Callers must serialize access to the supplied objects.
 */
TerrainManifestAssetLoadExecutorResult executeTerrainManifestAssetLoadRequests(
    TerrainManifestAssetLoadRequestQueue& queue,
    RendererAssetHandleCatalog& destinationHandles,
    TerrainManifestAssetLoadCallback callback,
    void* userData = nullptr);
} // namespace full_engine
