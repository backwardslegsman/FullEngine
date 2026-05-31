#include "engine/jobs/JobQueue.hpp"
#include "engine/renderer_integration/TerrainManifestAssetLoadJobs.hpp"

#include <cassert>
#include <cstdlib>
#include <vector>

namespace
{
full_engine::EngineJobId jobId(const std::uint64_t value) noexcept
{
    return {value, 0};
}

full_engine::EngineJobRequest job(
    const std::uint64_t id,
    const full_engine::EngineJobKind kind,
    const full_engine::EngineJobPriority priority = full_engine::EngineJobPriority::Normal) noexcept
{
    full_engine::EngineJobRequest request;
    request.id = jobId(id);
    request.kind = kind;
    request.priority = priority;
    request.payload0 = id * 10;
    return request;
}

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

full_engine::TerrainManifestAssetLoadRequest loadRequest(
    const std::uint64_t id,
    const full_engine::AssetKind kind) noexcept
{
    return {asset(id), kind};
}

struct CallbackState
{
    std::vector<std::uint64_t> calledIds;
    full_engine::EngineJobStatus status = full_engine::EngineJobStatus::Completed;
};

full_engine::EngineJobStatus recordingCallback(
    const full_engine::EngineJobRequest& request,
    void* userData)
{
    CallbackState* state = static_cast<CallbackState*>(userData);
    state->calledIds.push_back(request.id.value);
    return state->status;
}

full_engine::EngineJobStatus mixedCallback(
    const full_engine::EngineJobRequest& request,
    void* userData)
{
    CallbackState* state = static_cast<CallbackState*>(userData);
    state->calledIds.push_back(request.id.value);
    if (request.id.value == 1)
    {
        return full_engine::EngineJobStatus::Completed;
    }
    if (request.id.value == 2)
    {
        return full_engine::EngineJobStatus::Failed;
    }
    return full_engine::EngineJobStatus::Blocked;
}
} // namespace

int main()
{
    {
        const full_engine::EngineJobQueue queue;
        assert(queue.jobCount() == 0);
        assert(queue.jobs().empty());
        assert(queue.summary().pendingCount == 0);
        assert(!queue.contains(jobId(1)));
    }

    {
        full_engine::EngineJobQueue queue;
        assert(queue.push(job(1, full_engine::EngineJobKind::ManifestAssetLoad)) ==
            full_engine::EngineJobQueueResult::Queued);
        assert(queue.push(job(2, full_engine::EngineJobKind::TerrainSetupStage)) ==
            full_engine::EngineJobQueueResult::Queued);
        assert(queue.push(job(3, full_engine::EngineJobKind::Custom)) ==
            full_engine::EngineJobQueueResult::Queued);

        assert(queue.jobCount() == 3);
        assert(queue.contains(jobId(1)));
        assert(queue.summary().manifestAssetLoadCount == 1);
        assert(queue.summary().terrainSetupStageCount == 1);
        assert(queue.summary().customCount == 1);
        assert(queue.summary().normalPriorityCount == 3);
    }

    {
        full_engine::EngineJobQueue queue;
        assert(queue.push(job(0, full_engine::EngineJobKind::Custom)) ==
            full_engine::EngineJobQueueResult::InvalidArgument);
        assert(queue.push(job(1, full_engine::EngineJobKind::Unknown)) ==
            full_engine::EngineJobQueueResult::InvalidArgument);
        assert(queue.jobCount() == 0);

        assert(queue.push(job(1, full_engine::EngineJobKind::Custom)) ==
            full_engine::EngineJobQueueResult::Queued);
        assert(queue.push(job(1, full_engine::EngineJobKind::ManifestAssetLoad)) ==
            full_engine::EngineJobQueueResult::AlreadyQueued);
        assert(queue.jobCount() == 1);
    }

    {
        full_engine::EngineJobQueue queue;
        queue.push(job(1, full_engine::EngineJobKind::Custom, full_engine::EngineJobPriority::Low));
        queue.push(job(2, full_engine::EngineJobKind::Custom, full_engine::EngineJobPriority::High));
        queue.push(job(3, full_engine::EngineJobKind::Custom, full_engine::EngineJobPriority::High));
        queue.push(job(4, full_engine::EngineJobKind::Custom, full_engine::EngineJobPriority::Normal));

        CallbackState state;
        const full_engine::EngineJobExecutionResult result =
            full_engine::runReadyJobs(queue, 3, recordingCallback, &state);

        assert(result.records.size() == 3);
        assert(result.summary.attemptedCount == 3);
        assert(result.summary.completedCount == 3);
        assert(state.calledIds.size() == 3);
        assert(state.calledIds[0] == 2);
        assert(state.calledIds[1] == 3);
        assert(state.calledIds[2] == 4);
        assert(queue.jobCount() == 1);
        assert(queue.contains(jobId(1)));
    }

    {
        full_engine::EngineJobQueue queue;
        queue.push(job(1, full_engine::EngineJobKind::Custom));
        queue.push(job(2, full_engine::EngineJobKind::Custom));
        queue.push(job(3, full_engine::EngineJobKind::Custom));

        CallbackState state;
        const full_engine::EngineJobExecutionResult result =
            full_engine::runReadyJobs(queue, 3, mixedCallback, &state);

        assert(result.summary.completedCount == 1);
        assert(result.summary.failedCount == 1);
        assert(result.summary.blockedCount == 1);
        assert(queue.jobCount() == 2);
        assert(!queue.contains(jobId(1)));
        assert(queue.contains(jobId(2)));
        assert(queue.contains(jobId(3)));
    }

    {
        full_engine::EngineJobQueue queue;
        queue.push(job(1, full_engine::EngineJobKind::Custom, full_engine::EngineJobPriority::High));
        queue.push(job(2, full_engine::EngineJobKind::Custom, full_engine::EngineJobPriority::Low));

        const full_engine::EngineJobExecutionResult result =
            full_engine::runReadyJobs(queue, 8, nullptr, nullptr);

        assert(result.summary.attemptedCount == 2);
        assert(result.summary.blockedCount == 2);
        assert(queue.jobCount() == 2);
    }

    {
        full_engine::EngineJobQueue queue;
        queue.push(job(1, full_engine::EngineJobKind::Custom));
        queue.clear();
        assert(queue.jobCount() == 0);
    }

    {
        full_engine::TerrainManifestAssetLoadRequestQueue requests;
        assert(requests.push(loadRequest(101, full_engine::AssetKind::Mesh)).result ==
            full_engine::TerrainManifestAssetLoadQueueResult::Queued);
        assert(requests.push(loadRequest(102, full_engine::AssetKind::Material)).result ==
            full_engine::TerrainManifestAssetLoadQueueResult::Queued);
        assert(requests.push(loadRequest(103, full_engine::AssetKind::Texture)).result ==
            full_engine::TerrainManifestAssetLoadQueueResult::Queued);

        full_engine::EngineJobQueue jobs;
        const full_engine::TerrainManifestAssetLoadJobMirrorResult first =
            full_engine::mirrorTerrainManifestAssetLoadRequestsToJobs(
                requests,
                jobs,
                full_engine::EngineJobPriority::High);

        assert(first.records.size() == 3);
        assert(first.summary.queuedCount == 3);
        assert(jobs.jobCount() == 3);
        assert(jobs.summary().manifestAssetLoadCount == 3);
        assert(jobs.summary().highPriorityCount == 3);
        assert(jobs.jobs()[0].request.payload0 == 101);
        assert(jobs.jobs()[0].request.payload1 == static_cast<std::uint64_t>(full_engine::AssetKind::Mesh));
        assert(requests.requestCount() == 3);

        const full_engine::TerrainManifestAssetLoadJobMirrorResult second =
            full_engine::mirrorTerrainManifestAssetLoadRequestsToJobs(requests, jobs);

        assert(second.records.size() == 3);
        assert(second.summary.alreadyQueuedCount == 3);
        assert(jobs.jobCount() == 3);
        assert(requests.requestCount() == 3);
    }

    return EXIT_SUCCESS;
}
