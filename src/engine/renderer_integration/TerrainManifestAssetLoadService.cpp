#include "engine/renderer_integration/TerrainManifestAssetLoadService.hpp"

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

bool hasValidActiveHandle(
    const TerrainManifestAssetLoadRequest& request,
    const TerrainManifestAssetLoadCallbackResult& callback) noexcept
{
    switch (request.kind)
    {
    case AssetKind::Mesh:
        return full_renderer::isValid(callback.mesh);
    case AssetKind::Material:
        return full_renderer::isValid(callback.material);
    case AssetKind::Texture:
        return full_renderer::isValid(callback.texture);
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        return false;
    }

    return false;
}

bool isValidPacket(const TerrainManifestAssetLoadJobWorkPacket& packet) noexcept
{
    return isValid(packet.request.id) &&
        isSupportedLoadKind(packet.request.kind) &&
        packet.jobId == engineJobIdForTerrainManifestAssetLoadRequest(packet.request);
}

void incrementEnqueueSummary(
    TerrainManifestAssetLoadServiceEnqueueSummary& summary,
    const TerrainManifestAssetLoadServiceEnqueueStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadServiceEnqueueStatus::Queued:
        ++summary.queuedCount;
        break;
    case TerrainManifestAssetLoadServiceEnqueueStatus::AlreadyQueued:
        ++summary.alreadyQueuedCount;
        break;
    case TerrainManifestAssetLoadServiceEnqueueStatus::InvalidPacket:
        ++summary.invalidPacketCount;
        break;
    }
}

void appendCompletion(
    const TerrainManifestAssetLoadRequest& request,
    const TerrainManifestAssetLoadCallbackResult& callback,
    std::vector<TerrainManifestAssetLoadJobCompletion>& completions)
{
    TerrainManifestAssetLoadJobCompletion completion;
    completion.request = request;
    completion.output = callback;
    completions.push_back(completion);
}

void fillRetainedCounts(
    const TerrainManifestAssetLoadService& service,
    TerrainManifestAssetLoadServiceTickSummary& summary) noexcept
{
    summary.completedCount = service.completedCount();
    summary.pendingCount = service.pendingCount();
    summary.retainedFailedCount = service.failedCount();
}
} // namespace

const char* terrainManifestAssetLoadServiceRequestStatusName(
    const TerrainManifestAssetLoadServiceRequestStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadServiceRequestStatus::Pending:
        return "Pending";
    case TerrainManifestAssetLoadServiceRequestStatus::Completed:
        return "Completed";
    case TerrainManifestAssetLoadServiceRequestStatus::Failed:
        return "Failed";
    }

    return "Unknown";
}

const char* terrainManifestAssetLoadServiceEnqueueStatusName(
    const TerrainManifestAssetLoadServiceEnqueueStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadServiceEnqueueStatus::Queued:
        return "Queued";
    case TerrainManifestAssetLoadServiceEnqueueStatus::AlreadyQueued:
        return "AlreadyQueued";
    case TerrainManifestAssetLoadServiceEnqueueStatus::InvalidPacket:
        return "InvalidPacket";
    }

    return "Unknown";
}

const char* terrainManifestAssetLoadServiceTickStatusName(
    const TerrainManifestAssetLoadServiceTickStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadServiceTickStatus::Idle:
        return "Idle";
    case TerrainManifestAssetLoadServiceTickStatus::Progressed:
        return "Progressed";
    case TerrainManifestAssetLoadServiceTickStatus::Blocked:
        return "Blocked";
    }

    return "Unknown";
}

TerrainManifestAssetLoadServiceEnqueueResult TerrainManifestAssetLoadService::enqueueWorkPackets(
    const TerrainManifestAssetLoadJobWorkPacket* const packets,
    const std::size_t packetCount)
{
    TerrainManifestAssetLoadServiceEnqueueResult result;
    if (packets == nullptr && packetCount > 0)
    {
        result.summary.invalidPacketCount = packetCount;
        return result;
    }

    result.records.reserve(packetCount);
    for (std::size_t index = 0; index < packetCount; ++index)
    {
        TerrainManifestAssetLoadServiceEnqueueRecord record;
        record.packet = packets[index];

        if (!isValidPacket(record.packet))
        {
            record.status = TerrainManifestAssetLoadServiceEnqueueStatus::InvalidPacket;
        }
        else if (contains(record.packet.request.id, record.packet.request.kind))
        {
            record.status = TerrainManifestAssetLoadServiceEnqueueStatus::AlreadyQueued;
        }
        else
        {
            TerrainManifestAssetLoadServiceRecord retained;
            retained.packet = record.packet;
            retained.status = TerrainManifestAssetLoadServiceRequestStatus::Pending;
            records_.push_back(retained);
            record.status = TerrainManifestAssetLoadServiceEnqueueStatus::Queued;
        }

        incrementEnqueueSummary(result.summary, record.status);
        result.records.push_back(record);
    }

    return result;
}

TerrainManifestAssetLoadServiceEnqueueResult TerrainManifestAssetLoadService::enqueueWorkPackets(
    const TerrainManifestAssetLoadJobWorkPacketResult& packets)
{
    return enqueueWorkPackets(packets.packets.data(), packets.packets.size());
}

TerrainManifestAssetLoadServiceTickResult TerrainManifestAssetLoadService::tick(
    const std::size_t maxLoads,
    TerrainManifestAssetLoadCallback callback,
    void* const userData)
{
    TerrainManifestAssetLoadServiceTickResult result;
    fillRetainedCounts(*this, result.summary);

    if (maxLoads == 0 || pendingCount() == 0)
    {
        result.status = TerrainManifestAssetLoadServiceTickStatus::Idle;
        return result;
    }

    if (callback == nullptr)
    {
        result.status = TerrainManifestAssetLoadServiceTickStatus::Blocked;
        return result;
    }

    result.status = TerrainManifestAssetLoadServiceTickStatus::Progressed;
    std::size_t attempted = 0;
    for (TerrainManifestAssetLoadServiceRecord& retained : records_)
    {
        if (attempted >= maxLoads)
        {
            break;
        }

        if (retained.status != TerrainManifestAssetLoadServiceRequestStatus::Pending)
        {
            continue;
        }

        ++attempted;
        ++retained.attemptCount;

        TerrainManifestAssetLoadServiceTickRecord record;
        record.request = retained.packet.request;
        record.callback = callback(retained.packet.request, userData);
        retained.lastCallback = record.callback;
        ++result.summary.attemptedCount;

        if (record.callback.status == TerrainManifestAssetLoadCallbackStatus::Loaded &&
            hasValidActiveHandle(record.request, record.callback))
        {
            retained.status = TerrainManifestAssetLoadServiceRequestStatus::Completed;
            record.status = retained.status;
            appendCompletion(record.request, record.callback, completions_);
            ++result.summary.loadedCount;
        }
        else if (record.callback.status == TerrainManifestAssetLoadCallbackStatus::Missing)
        {
            retained.status = TerrainManifestAssetLoadServiceRequestStatus::Pending;
            record.status = retained.status;
            ++result.summary.missingCount;
        }
        else
        {
            retained.status = TerrainManifestAssetLoadServiceRequestStatus::Failed;
            record.status = retained.status;
            appendCompletion(record.request, record.callback, completions_);
            ++result.summary.failedCount;
        }

        result.records.push_back(record);
    }

    fillRetainedCounts(*this, result.summary);
    return result;
}

std::size_t TerrainManifestAssetLoadService::retryFailed() noexcept
{
    std::size_t retried = 0;
    for (TerrainManifestAssetLoadServiceRecord& record : records_)
    {
        if (record.status == TerrainManifestAssetLoadServiceRequestStatus::Failed)
        {
            record.status = TerrainManifestAssetLoadServiceRequestStatus::Pending;
            ++retried;
        }
    }
    return retried;
}

void TerrainManifestAssetLoadService::clearCompletions() noexcept
{
    completions_.clear();
}

void TerrainManifestAssetLoadService::clear() noexcept
{
    records_.clear();
    completions_.clear();
}

const std::vector<TerrainManifestAssetLoadServiceRecord>& TerrainManifestAssetLoadService::records() const noexcept
{
    return records_;
}

const std::vector<TerrainManifestAssetLoadJobCompletion>& TerrainManifestAssetLoadService::completions() const noexcept
{
    return completions_;
}

std::size_t TerrainManifestAssetLoadService::requestCount() const noexcept
{
    return records_.size();
}

std::size_t TerrainManifestAssetLoadService::pendingCount() const noexcept
{
    std::size_t count = 0;
    for (const TerrainManifestAssetLoadServiceRecord& record : records_)
    {
        if (record.status == TerrainManifestAssetLoadServiceRequestStatus::Pending)
        {
            ++count;
        }
    }
    return count;
}

std::size_t TerrainManifestAssetLoadService::completedCount() const noexcept
{
    std::size_t count = 0;
    for (const TerrainManifestAssetLoadServiceRecord& record : records_)
    {
        if (record.status == TerrainManifestAssetLoadServiceRequestStatus::Completed)
        {
            ++count;
        }
    }
    return count;
}

std::size_t TerrainManifestAssetLoadService::failedCount() const noexcept
{
    std::size_t count = 0;
    for (const TerrainManifestAssetLoadServiceRecord& record : records_)
    {
        if (record.status == TerrainManifestAssetLoadServiceRequestStatus::Failed)
        {
            ++count;
        }
    }
    return count;
}

bool TerrainManifestAssetLoadService::contains(
    const AssetId id,
    const AssetKind kind) const noexcept
{
    for (const TerrainManifestAssetLoadServiceRecord& record : records_)
    {
        if (record.packet.request.id == id && record.packet.request.kind == kind)
        {
            return true;
        }
    }
    return false;
}
} // namespace full_engine
