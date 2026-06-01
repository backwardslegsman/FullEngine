#include "engine/renderer_integration/TerrainManifestAssetLoadJobCoordinator.hpp"

#include "engine/renderer_integration/TerrainManifestLoadState.hpp"

#include <algorithm>
#include <vector>

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

int priorityRank(const EngineJobPriority priority) noexcept
{
    switch (priority)
    {
    case EngineJobPriority::High:
        return 2;
    case EngineJobPriority::Normal:
        return 1;
    case EngineJobPriority::Low:
        return 0;
    }

    return 1;
}

TerrainManifestAssetLoadRequest requestFromJob(const EngineJobRequest& job) noexcept
{
    TerrainManifestAssetLoadRequest request;
    request.id = {job.payload0};
    request.kind = static_cast<AssetKind>(job.payload1);
    return request;
}

bool sameId(const EngineJobId lhs, const EngineJobId rhs) noexcept
{
    return lhs == rhs;
}

bool containsJobId(const std::vector<EngineJobId>& ids, const EngineJobId id) noexcept
{
    for (const EngineJobId candidate : ids)
    {
        if (sameId(candidate, id))
        {
            return true;
        }
    }
    return false;
}

bool hasDestinationHandle(
    const TerrainManifestAssetLoadRequest& request,
    const RendererAssetHandleCatalog& handles)
{
    switch (request.kind)
    {
    case AssetKind::Mesh:
        return handles.findMeshHandle(request.id) != nullptr;
    case AssetKind::Material:
        return handles.findMaterialHandle(request.id) != nullptr;
    case AssetKind::Texture:
        return handles.findTextureHandle(request.id) != nullptr;
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        return false;
    }

    return false;
}

EngineJobStatus executeLoadJob(
    const EngineJobRequest& job,
    const RendererAssetHandleCatalog& destinationHandles,
    TerrainManifestAssetLoadCallback callback,
    void* const userData)
{
    if (job.kind != EngineJobKind::ManifestAssetLoad)
    {
        return EngineJobStatus::Blocked;
    }

    const TerrainManifestAssetLoadRequest request = requestFromJob(job);
    if (!isValid(request.id) || !isSupportedLoadKind(request.kind))
    {
        return EngineJobStatus::Failed;
    }

    if (hasDestinationHandle(request, destinationHandles))
    {
        return EngineJobStatus::Completed;
    }

    if (callback == nullptr)
    {
        return EngineJobStatus::Blocked;
    }

    const TerrainManifestAssetLoadCallbackResult callbackResult = callback(request, userData);
    switch (callbackResult.status)
    {
    case TerrainManifestAssetLoadCallbackStatus::Loaded:
        return EngineJobStatus::Completed;
    case TerrainManifestAssetLoadCallbackStatus::Missing:
        return EngineJobStatus::Blocked;
    case TerrainManifestAssetLoadCallbackStatus::Failed:
        return EngineJobStatus::Failed;
    }

    return EngineJobStatus::Failed;
}

EngineJobExecutionResult runManifestLoadJobs(
    const std::vector<EngineJobRecord>& jobs,
    const std::vector<EngineJobId>& relevantIds,
    const RendererAssetHandleCatalog& destinationHandles,
    TerrainManifestAssetLoadCallback callback,
    void* const userData,
    const std::size_t maxJobs)
{
    EngineJobExecutionResult result;
    if (maxJobs == 0)
    {
        return result;
    }

    std::vector<std::size_t> indices;
    indices.reserve(jobs.size());
    for (std::size_t index = 0; index < jobs.size(); ++index)
    {
        const EngineJobRecord& record = jobs[index];
        if (record.request.kind == EngineJobKind::ManifestAssetLoad &&
            containsJobId(relevantIds, record.request.id))
        {
            indices.push_back(index);
        }
    }

    std::stable_sort(indices.begin(), indices.end(), [&jobs](const std::size_t lhs, const std::size_t rhs) {
        return priorityRank(jobs[lhs].request.priority) > priorityRank(jobs[rhs].request.priority);
    });

    for (const std::size_t index : indices)
    {
        if (result.summary.attemptedCount >= maxJobs)
        {
            break;
        }

        EngineJobRecord record = jobs[index];
        record.status = executeLoadJob(record.request, destinationHandles, callback, userData);
        result.records.push_back(record);
        ++result.summary.attemptedCount;

        switch (record.status)
        {
        case EngineJobStatus::Completed:
            ++result.summary.completedCount;
            break;
        case EngineJobStatus::Failed:
            ++result.summary.failedCount;
            break;
        case EngineJobStatus::Blocked:
            ++result.summary.blockedCount;
            break;
        case EngineJobStatus::Pending:
            break;
        }
    }

    return result;
}

std::vector<EngineJobId> jobIdsForRequests(const TerrainManifestAssetLoadRequestQueue& queue)
{
    std::vector<EngineJobId> ids;
    ids.reserve(queue.requests().size());
    for (const TerrainManifestAssetLoadRequest& request : queue.requests())
    {
        ids.push_back(engineJobIdForTerrainManifestAssetLoadRequest(request));
    }
    return ids;
}

std::size_t removeRelevantLoadJobs(EngineJobQueue& jobs, const std::vector<EngineJobId>& ids)
{
    const std::vector<EngineJobRecord> before = jobs.jobs();
    jobs.clear();
    std::size_t removedCount = 0;
    for (const EngineJobRecord& record : before)
    {
        if (record.request.kind == EngineJobKind::ManifestAssetLoad &&
            containsJobId(ids, record.request.id))
        {
            ++removedCount;
            continue;
        }
        (void)jobs.push(record.request);
    }
    return removedCount;
}
} // namespace

const char* terrainManifestAssetLoadJobCoordinatorStatusName(
    const TerrainManifestAssetLoadJobCoordinatorStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadJobCoordinatorStatus::Success:
        return "Success";
    case TerrainManifestAssetLoadJobCoordinatorStatus::NoPendingLoads:
        return "NoPendingLoads";
    case TerrainManifestAssetLoadJobCoordinatorStatus::JobsBlocked:
        return "JobsBlocked";
    case TerrainManifestAssetLoadJobCoordinatorStatus::LoadConsumeBlocked:
        return "LoadConsumeBlocked";
    }

    return "Unknown";
}

const char* terrainManifestAssetLoadJobScheduleStatusName(
    const TerrainManifestAssetLoadJobScheduleStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadJobScheduleStatus::Scheduled:
        return "Scheduled";
    case TerrainManifestAssetLoadJobScheduleStatus::NoPendingLoads:
        return "NoPendingLoads";
    case TerrainManifestAssetLoadJobScheduleStatus::Blocked:
        return "Blocked";
    }

    return "Unknown";
}

const char* terrainManifestAssetLoadJobReconcileStatusName(
    const TerrainManifestAssetLoadJobReconcileStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadJobReconcileStatus::Success:
        return "Success";
    case TerrainManifestAssetLoadJobReconcileStatus::NoPendingLoads:
        return "NoPendingLoads";
    case TerrainManifestAssetLoadJobReconcileStatus::CompletionPending:
        return "CompletionPending";
    case TerrainManifestAssetLoadJobReconcileStatus::LoadConsumeBlocked:
        return "LoadConsumeBlocked";
    }

    return "Unknown";
}

TerrainManifestAssetLoadJobScheduleResult scheduleTerrainManifestAssetLoadJobs(
    const TerrainManifestLoadState& manifestLoad,
    EngineJobQueue& jobs,
    const EngineJobPriority priority)
{
    TerrainManifestAssetLoadJobScheduleResult result;
    result.initialPendingLoadRequestCount = manifestLoad.pendingLoadRequestCount();
    if (result.initialPendingLoadRequestCount == 0)
    {
        result.status = TerrainManifestAssetLoadJobScheduleStatus::NoPendingLoads;
        result.finalPendingLoadRequestCount = manifestLoad.pendingLoadRequestCount();
        result.pendingJobCount = jobs.jobCount();
        return result;
    }

    result.mirror = mirrorTerrainManifestAssetLoadRequestsToJobs(
        manifestLoad.loadRequestQueue(),
        jobs,
        priority);
    result.finalPendingLoadRequestCount = manifestLoad.pendingLoadRequestCount();
    result.pendingJobCount = jobs.jobCount();
    result.status =
        result.mirror.summary.invalidArgumentCount == 0 ?
            TerrainManifestAssetLoadJobScheduleStatus::Scheduled :
            TerrainManifestAssetLoadJobScheduleStatus::Blocked;
    return result;
}

TerrainManifestAssetLoadJobReconcileResult reconcileTerrainManifestAssetLoadJobs(
    TerrainManifestLoadState& manifestLoad,
    EngineJobQueue& jobs,
    const RendererAssetHandleCatalog& completedHandles,
    RendererAssetHandleCatalog& destinationHandles)
{
    TerrainManifestAssetLoadJobReconcileResult result;
    result.summary.initialPendingLoadRequestCount = manifestLoad.pendingLoadRequestCount();
    result.summary.initialPendingJobCount = jobs.jobCount();

    if (result.summary.initialPendingLoadRequestCount == 0)
    {
        result.status = TerrainManifestAssetLoadJobReconcileStatus::NoPendingLoads;
        result.summary.finalPendingLoadRequestCount = manifestLoad.pendingLoadRequestCount();
        result.summary.finalPendingJobCount = jobs.jobCount();
        return result;
    }

    const std::vector<EngineJobId> relevantJobIds = jobIdsForRequests(manifestLoad.loadRequestQueue());
    result.load = manifestLoad.consumePendingAssetLoadRequests(
        completedHandles,
        destinationHandles);

    if (!result.load.consumed)
    {
        result.status =
            result.load.summary.missingHandleCount > 0 ?
                TerrainManifestAssetLoadJobReconcileStatus::CompletionPending :
                TerrainManifestAssetLoadJobReconcileStatus::LoadConsumeBlocked;
        result.summary.finalPendingLoadRequestCount = manifestLoad.pendingLoadRequestCount();
        result.summary.finalPendingJobCount = jobs.jobCount();
        return result;
    }

    result.summary.removedScheduledJobCount = removeRelevantLoadJobs(jobs, relevantJobIds);
    result.readiness = manifestLoad.planAssetReadiness(destinationHandles);
    result.summary.finalPendingLoadRequestCount = manifestLoad.pendingLoadRequestCount();
    result.summary.finalPendingJobCount = jobs.jobCount();
    result.summary.finalReadyHandleCount = result.readiness.summary.readyCount;
    result.summary.finalMissingHandleCount = result.readiness.summary.missingHandleCount;
    result.status = TerrainManifestAssetLoadJobReconcileStatus::Success;
    return result;
}

TerrainManifestAssetLoadJobCoordinatorResult runTerrainManifestAssetLoadJobs(
    TerrainManifestLoadState& manifestLoad,
    EngineJobQueue& jobs,
    RendererAssetHandleCatalog& destinationHandles,
    TerrainManifestAssetLoadCallback callback,
    void* const userData,
    const std::size_t maxJobs,
    const EngineJobPriority priority)
{
    TerrainManifestAssetLoadJobCoordinatorResult result;
    result.summary.initialPendingLoadRequestCount = manifestLoad.pendingLoadRequestCount();

    if (result.summary.initialPendingLoadRequestCount == 0)
    {
        result.status = TerrainManifestAssetLoadJobCoordinatorStatus::NoPendingLoads;
        result.summary.finalPendingLoadRequestCount = manifestLoad.pendingLoadRequestCount();
        return result;
    }

    const std::vector<EngineJobId> relevantJobIds = jobIdsForRequests(manifestLoad.loadRequestQueue());
    result.mirror = mirrorTerrainManifestAssetLoadRequestsToJobs(
        manifestLoad.loadRequestQueue(),
        jobs,
        priority);

    result.jobs = runManifestLoadJobs(
        jobs.jobs(),
        relevantJobIds,
        destinationHandles,
        callback,
        userData,
        maxJobs);

    const bool allJobsAttempted = result.jobs.summary.attemptedCount >= relevantJobIds.size();
    const bool jobsSucceeded = allJobsAttempted &&
        result.jobs.summary.completedCount == relevantJobIds.size() &&
        result.jobs.summary.failedCount == 0 &&
        result.jobs.summary.blockedCount == 0;
    if (!jobsSucceeded)
    {
        result.status = TerrainManifestAssetLoadJobCoordinatorStatus::JobsBlocked;
        result.summary.finalPendingLoadRequestCount = manifestLoad.pendingLoadRequestCount();
        return result;
    }

    result.load = manifestLoad.executePendingAssetLoadRequests(
        destinationHandles,
        callback,
        userData);

    if (result.load.status != TerrainManifestAssetLoadExecutorStatus::Consumed ||
        !result.load.consume.consumed)
    {
        result.status = TerrainManifestAssetLoadJobCoordinatorStatus::LoadConsumeBlocked;
        result.summary.finalPendingLoadRequestCount = manifestLoad.pendingLoadRequestCount();
        return result;
    }

    (void)removeRelevantLoadJobs(jobs, relevantJobIds);
    result.readiness = manifestLoad.planAssetReadiness(destinationHandles);
    result.summary.finalPendingLoadRequestCount = manifestLoad.pendingLoadRequestCount();
    result.summary.finalReadyHandleCount = result.readiness.summary.readyCount;
    result.summary.finalMissingHandleCount = result.readiness.summary.missingHandleCount;
    result.status = TerrainManifestAssetLoadJobCoordinatorStatus::Success;
    return result;
}
} // namespace full_engine
