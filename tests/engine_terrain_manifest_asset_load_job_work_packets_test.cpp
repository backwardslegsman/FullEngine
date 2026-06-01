#include "engine/renderer_integration/TerrainManifestAssetLoadJobWorkPackets.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

full_engine::TerrainManifestAssetLoadRequest request(
    const std::uint64_t id,
    const full_engine::AssetKind kind) noexcept
{
    full_engine::TerrainManifestAssetLoadRequest result;
    result.id = asset(id);
    result.kind = kind;
    return result;
}

full_engine::EngineJobRequest customJob()
{
    full_engine::EngineJobRequest result;
    result.id = {9000, 0};
    result.kind = full_engine::EngineJobKind::Custom;
    result.priority = full_engine::EngineJobPriority::High;
    result.payload0 = 9000;
    return result;
}

full_engine::EngineJobRequest malformedManifestJob(
    const std::uint64_t id,
    const full_engine::AssetKind kind,
    const bool mismatchedJobId = false)
{
    const full_engine::TerrainManifestAssetLoadRequest load = request(id, kind);
    full_engine::EngineJobRequest result;
    result.id = mismatchedJobId
        ? full_engine::EngineJobId{9999, 0}
        : full_engine::engineJobIdForTerrainManifestAssetLoadRequest(load);
    result.kind = full_engine::EngineJobKind::ManifestAssetLoad;
    result.priority = full_engine::EngineJobPriority::Normal;
    result.payload0 = id;
    result.payload1 = static_cast<std::uint64_t>(kind);
    return result;
}

full_engine::TerrainManifestAssetLoadRequestQueue loadQueue()
{
    full_engine::TerrainManifestAssetLoadRequestQueue queue;
    (void)queue.push(request(1, full_engine::AssetKind::Mesh));
    (void)queue.push(request(2, full_engine::AssetKind::Material));
    (void)queue.push(request(3, full_engine::AssetKind::Texture));
    return queue;
}

void testDefaultQueueProducesNoPackets(std::vector<std::string>& failures)
{
    const full_engine::EngineJobQueue jobs;
    const full_engine::TerrainManifestAssetLoadJobWorkPacketResult result =
        full_engine::buildTerrainManifestAssetLoadJobWorkPackets(jobs);

    expect(result.records.empty(), "empty job queue emits no records", failures);
    expect(result.packets.empty(), "empty job queue emits no packets", failures);
    expect(result.summary.packetizedCount == 0, "empty job queue packetized count is zero", failures);
}

void testScheduledJobsBecomeWorkPackets(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetLoadRequestQueue requests = loadQueue();
    full_engine::EngineJobQueue jobs;
    (void)full_engine::mirrorTerrainManifestAssetLoadRequestsToJobs(
        requests,
        jobs,
        full_engine::EngineJobPriority::High);
    (void)jobs.push(customJob());

    const full_engine::TerrainManifestAssetLoadJobWorkPacketResult result =
        full_engine::buildTerrainManifestAssetLoadJobWorkPackets(jobs);

    expect(result.records.size() == 4, "packet helper emits one record per pending job", failures);
    expect(result.packets.size() == 3, "packet helper emits one packet per load job", failures);
    expect(result.summary.packetizedCount == 3, "packet helper counts packetized jobs", failures);
    expect(result.summary.skippedUnsupportedJobCount == 1, "packet helper counts skipped custom job", failures);
    expect(result.summary.invalidPayloadCount == 0, "packet helper reports no invalid payloads", failures);
    expect(result.packets[0].request.kind == full_engine::AssetKind::Mesh, "packet order preserves mesh job", failures);
    expect(result.packets[1].request.kind == full_engine::AssetKind::Material, "packet order preserves material job", failures);
    expect(result.packets[2].request.kind == full_engine::AssetKind::Texture, "packet order preserves texture job", failures);
    expect(result.packets[0].priority == full_engine::EngineJobPriority::High, "packet priority is copied", failures);
    expect(result.packets[0].jobId == full_engine::engineJobIdForTerrainManifestAssetLoadRequest(result.packets[0].request), "packet job ID matches request", failures);
    expect(jobs.jobCount() == 4, "packet helper does not mutate job queue", failures);
    expect(std::string(full_engine::terrainManifestAssetLoadJobWorkPacketStatusName(result.records[0].status)) == "Packetized", "packet status name is stable", failures);
}

void testInvalidPayloadsAreReported(std::vector<std::string>& failures)
{
    full_engine::EngineJobQueue jobs;
    (void)jobs.push(malformedManifestJob(0, full_engine::AssetKind::Mesh));
    (void)jobs.push(malformedManifestJob(4, full_engine::AssetKind::Shader));
    (void)jobs.push(malformedManifestJob(5, full_engine::AssetKind::Texture, true));

    const full_engine::TerrainManifestAssetLoadJobWorkPacketResult result =
        full_engine::buildTerrainManifestAssetLoadJobWorkPackets(jobs);

    expect(result.records.size() == 3, "invalid payloads emit records", failures);
    expect(result.packets.empty(), "invalid payloads emit no packets", failures);
    expect(result.summary.invalidPayloadCount == 3, "invalid payloads are counted", failures);
    expect(result.records[0].status == full_engine::TerrainManifestAssetLoadJobWorkPacketStatus::InvalidPayload, "invalid asset id is rejected", failures);
    expect(result.records[1].status == full_engine::TerrainManifestAssetLoadJobWorkPacketStatus::InvalidPayload, "unsupported asset kind is rejected", failures);
    expect(result.records[2].status == full_engine::TerrainManifestAssetLoadJobWorkPacketStatus::InvalidPayload, "mismatched job id is rejected", failures);
    expect(jobs.jobCount() == 3, "invalid packet helper leaves jobs pending", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testDefaultQueueProducesNoPackets(failures);
    testScheduledJobsBecomeWorkPackets(failures);
    testInvalidPayloadsAreReported(failures);

    if (!failures.empty())
    {
        for (const std::string& failure : failures)
        {
            std::cerr << failure << '\n';
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
