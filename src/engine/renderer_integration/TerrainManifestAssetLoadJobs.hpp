#pragma once

#include "engine/jobs/JobQueue.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadRequests.hpp"

namespace full_engine
{
/** @brief Summary counters for mirroring manifest asset load requests into jobs. */
struct TerrainManifestAssetLoadJobMirrorSummary
{
    /** @brief Number of jobs appended to the destination job queue. */
    std::size_t queuedCount = 0;

    /** @brief Number of source requests skipped because the job ID was already pending. */
    std::size_t alreadyQueuedCount = 0;

    /** @brief Number of source requests rejected by the destination job queue. */
    std::size_t invalidArgumentCount = 0;
};

/** @brief Ordered result for mirroring pending manifest asset load requests into jobs. */
struct TerrainManifestAssetLoadJobMirrorResult
{
    /** @brief One diagnostic record per source request in source order. */
    std::vector<EngineJobRecord> records;

    /** @brief Aggregate counters for this mirror operation. */
    TerrainManifestAssetLoadJobMirrorSummary summary = {};
};

/**
 * @brief Builds a deterministic job ID for one manifest asset load request.
 *
 * The ID encodes the asset ID and asset kind exactly into the two numeric job
 * ID fields. It is stable across calls and does not inspect renderer handles,
 * manifests, queues, or external loader state.
 */
EngineJobId engineJobIdForTerrainManifestAssetLoadRequest(
    const TerrainManifestAssetLoadRequest& request) noexcept;

/**
 * @brief Copies pending manifest asset load intent into a generic job queue.
 *
 * Each source request becomes a `ManifestAssetLoad` job with the asset ID in
 * `payload0` and the asset kind numeric value in `payload1`. The source queue
 * is read only and not consumed. The destination job queue owns only copied job
 * intent and does not perform IO, start async work, create renderer resources,
 * or mutate renderer handle catalogs.
 *
 * @param requests Pending renderer-free asset load intent to mirror.
 * @param jobs Destination generic job queue.
 * @param priority Priority assigned to each mirrored job.
 * @return Ordered per-request queue diagnostics in source request order.
 */
TerrainManifestAssetLoadJobMirrorResult mirrorTerrainManifestAssetLoadRequestsToJobs(
    const TerrainManifestAssetLoadRequestQueue& requests,
    EngineJobQueue& jobs,
    EngineJobPriority priority = EngineJobPriority::Normal);
} // namespace full_engine
