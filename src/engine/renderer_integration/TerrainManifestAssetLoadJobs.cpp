#include "engine/renderer_integration/TerrainManifestAssetLoadJobs.hpp"

namespace full_engine
{
namespace
{
std::uint64_t kindValue(const AssetKind kind) noexcept
{
    return static_cast<std::uint64_t>(kind);
}

void countMirrorResult(
    TerrainManifestAssetLoadJobMirrorSummary& summary,
    const EngineJobQueueResult result) noexcept
{
    switch (result)
    {
    case EngineJobQueueResult::Queued:
        ++summary.queuedCount;
        break;
    case EngineJobQueueResult::AlreadyQueued:
        ++summary.alreadyQueuedCount;
        break;
    case EngineJobQueueResult::InvalidArgument:
        ++summary.invalidArgumentCount;
        break;
    }
}

EngineJobStatus statusForQueueResult(const EngineJobQueueResult result) noexcept
{
    switch (result)
    {
    case EngineJobQueueResult::Queued:
        return EngineJobStatus::Pending;
    case EngineJobQueueResult::AlreadyQueued:
        return EngineJobStatus::Blocked;
    case EngineJobQueueResult::InvalidArgument:
        return EngineJobStatus::Failed;
    }

    return EngineJobStatus::Failed;
}
} // namespace

EngineJobId engineJobIdForTerrainManifestAssetLoadRequest(
    const TerrainManifestAssetLoadRequest& request) noexcept
{
    EngineJobId id;
    id.value = request.id.value;
    id.subValue = kindValue(request.kind);
    return id;
}

TerrainManifestAssetLoadJobMirrorResult mirrorTerrainManifestAssetLoadRequestsToJobs(
    const TerrainManifestAssetLoadRequestQueue& requests,
    EngineJobQueue& jobs,
    const EngineJobPriority priority)
{
    TerrainManifestAssetLoadJobMirrorResult result;
    result.records.reserve(requests.requests().size());

    for (const TerrainManifestAssetLoadRequest& request : requests.requests())
    {
        EngineJobRequest job;
        job.id = engineJobIdForTerrainManifestAssetLoadRequest(request);
        job.kind = EngineJobKind::ManifestAssetLoad;
        job.priority = priority;
        job.payload0 = request.id.value;
        job.payload1 = kindValue(request.kind);

        const EngineJobQueueResult queueResult = jobs.push(job);
        countMirrorResult(result.summary, queueResult);

        EngineJobRecord record;
        record.request = job;
        record.status = statusForQueueResult(queueResult);
        result.records.push_back(record);
    }

    return result;
}
} // namespace full_engine
