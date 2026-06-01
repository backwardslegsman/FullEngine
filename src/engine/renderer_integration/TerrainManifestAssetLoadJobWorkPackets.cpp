#include "engine/renderer_integration/TerrainManifestAssetLoadJobWorkPackets.hpp"

namespace full_engine
{
namespace
{
bool isSupportedLoadKind(const AssetKind kind) noexcept
{
    return kind == AssetKind::Mesh ||
        kind == AssetKind::Material ||
        kind == AssetKind::Texture;
}

TerrainManifestAssetLoadRequest requestFromJob(const EngineJobRequest& job) noexcept
{
    TerrainManifestAssetLoadRequest request;
    request.id = {job.payload0};
    request.kind = static_cast<AssetKind>(job.payload1);
    return request;
}

void incrementSummary(
    TerrainManifestAssetLoadJobWorkPacketSummary& summary,
    const TerrainManifestAssetLoadJobWorkPacketStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadJobWorkPacketStatus::Packetized:
        ++summary.packetizedCount;
        break;
    case TerrainManifestAssetLoadJobWorkPacketStatus::SkippedUnsupportedJob:
        ++summary.skippedUnsupportedJobCount;
        break;
    case TerrainManifestAssetLoadJobWorkPacketStatus::InvalidPayload:
        ++summary.invalidPayloadCount;
        break;
    }
}

bool isValidManifestLoadPayload(const EngineJobRequest& job, const TerrainManifestAssetLoadRequest& request) noexcept
{
    return isValid(request.id) &&
        isSupportedLoadKind(request.kind) &&
        job.id == engineJobIdForTerrainManifestAssetLoadRequest(request);
}
} // namespace

const char* terrainManifestAssetLoadJobWorkPacketStatusName(
    const TerrainManifestAssetLoadJobWorkPacketStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadJobWorkPacketStatus::Packetized:
        return "Packetized";
    case TerrainManifestAssetLoadJobWorkPacketStatus::SkippedUnsupportedJob:
        return "SkippedUnsupportedJob";
    case TerrainManifestAssetLoadJobWorkPacketStatus::InvalidPayload:
        return "InvalidPayload";
    }

    return "Unknown";
}

TerrainManifestAssetLoadJobWorkPacketResult buildTerrainManifestAssetLoadJobWorkPackets(
    const EngineJobQueue& jobs)
{
    TerrainManifestAssetLoadJobWorkPacketResult result;
    result.records.reserve(jobs.jobs().size());

    for (const EngineJobRecord& source : jobs.jobs())
    {
        TerrainManifestAssetLoadJobWorkPacketRecord record;
        record.job = source.request;

        if (source.request.kind != EngineJobKind::ManifestAssetLoad)
        {
            record.status = TerrainManifestAssetLoadJobWorkPacketStatus::SkippedUnsupportedJob;
        }
        else
        {
            const TerrainManifestAssetLoadRequest request = requestFromJob(source.request);
            if (!isValidManifestLoadPayload(source.request, request))
            {
                record.status = TerrainManifestAssetLoadJobWorkPacketStatus::InvalidPayload;
            }
            else
            {
                record.status = TerrainManifestAssetLoadJobWorkPacketStatus::Packetized;
                record.packet.jobId = source.request.id;
                record.packet.request = request;
                record.packet.priority = source.request.priority;
                result.packets.push_back(record.packet);
            }
        }

        incrementSummary(result.summary, record.status);
        result.records.push_back(record);
    }

    return result;
}
} // namespace full_engine
