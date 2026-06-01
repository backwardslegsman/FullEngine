#pragma once

#include "engine/renderer_integration/TerrainManifestAssetLoadJobs.hpp"

#include <vector>

namespace full_engine
{
/**
 * @brief Caller-facing work packet decoded from one scheduled manifest asset-load job.
 *
 * The packet is a renderer-free input value for an external worker or loader.
 * It copies the scheduled job identity, request, and priority so callers can
 * perform work outside the engine and later publish
 * `TerrainManifestAssetLoadJobCompletion` values. It does not reference job
 * queue storage, renderer handles, files, workers, futures, or renderer
 * resources.
 */
struct TerrainManifestAssetLoadJobWorkPacket
{
    /** @brief Scheduled job identity that produced this packet. */
    EngineJobId jobId = {};

    /** @brief Asset load request decoded from the job payload. */
    TerrainManifestAssetLoadRequest request = {};

    /** @brief Priority copied from the scheduled job. */
    EngineJobPriority priority = EngineJobPriority::Normal;
};

/** @brief Per-job status when converting scheduled jobs into worker packets. */
enum class TerrainManifestAssetLoadJobWorkPacketStatus
{
    /** @brief A valid manifest asset-load job was converted into a work packet. */
    Packetized,

    /** @brief The job is not a manifest asset-load job and was ignored. */
    SkippedUnsupportedJob,

    /** @brief The manifest asset-load job has invalid or inconsistent payload fields. */
    InvalidPayload,
};

/** @brief Returns a stable diagnostic name for a work-packet conversion status. */
const char* terrainManifestAssetLoadJobWorkPacketStatusName(
    TerrainManifestAssetLoadJobWorkPacketStatus status) noexcept;

/** @brief Ordered diagnostic record for one inspected job. */
struct TerrainManifestAssetLoadJobWorkPacketRecord
{
    /** @brief Source job request copied from the queue. */
    EngineJobRequest job = {};

    /** @brief Conversion outcome for this job. */
    TerrainManifestAssetLoadJobWorkPacketStatus status =
        TerrainManifestAssetLoadJobWorkPacketStatus::SkippedUnsupportedJob;

    /** @brief Decoded packet when `status` is `Packetized`, otherwise default. */
    TerrainManifestAssetLoadJobWorkPacket packet = {};
};

/** @brief Aggregate counters for scheduled load-job work-packet conversion. */
struct TerrainManifestAssetLoadJobWorkPacketSummary
{
    /** @brief Number of valid packets emitted. */
    std::size_t packetizedCount = 0;

    /** @brief Number of non-manifest-load jobs skipped. */
    std::size_t skippedUnsupportedJobCount = 0;

    /** @brief Number of manifest-load jobs rejected for invalid payloads. */
    std::size_t invalidPayloadCount = 0;
};

/** @brief Ordered result of converting scheduled jobs into external worker packets. */
struct TerrainManifestAssetLoadJobWorkPacketResult
{
    /** @brief One diagnostic record per inspected pending job, in queue order. */
    std::vector<TerrainManifestAssetLoadJobWorkPacketRecord> records;

    /** @brief Valid worker packets in queue order. */
    std::vector<TerrainManifestAssetLoadJobWorkPacket> packets;

    /** @brief Aggregate conversion counters. */
    TerrainManifestAssetLoadJobWorkPacketSummary summary = {};
};

/**
 * @brief Converts scheduled manifest asset-load jobs into caller-owned work packets.
 *
 * The helper inspects the current pending job queue in insertion order and
 * emits packets only for valid `ManifestAssetLoad` jobs produced by the
 * manifest load mirroring convention. Unrelated jobs are reported as skipped.
 * Manifest-load jobs with invalid asset IDs, unsupported asset kinds, or job
 * IDs that do not match `engineJobIdForTerrainManifestAssetLoadRequest` are
 * reported as invalid payloads.
 *
 * The function performs no IO, starts no workers, executes no callbacks,
 * creates no renderer resources, mutates no job queues, consumes no retained
 * load requests, and touches no renderer handle catalogs.
 *
 * @param jobs Caller-owned pending job queue to inspect. It is not retained or
 * mutated.
 * @return Ordered packet conversion diagnostics and valid work packets.
 *
 * @note Thread safety: Not thread-safe for concurrently mutated job queues.
 * Callers must serialize access to `jobs`.
 */
TerrainManifestAssetLoadJobWorkPacketResult buildTerrainManifestAssetLoadJobWorkPackets(
    const EngineJobQueue& jobs);
} // namespace full_engine
