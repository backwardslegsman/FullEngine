#include "engine/jobs/JobQueue.hpp"

#include <algorithm>

namespace full_engine
{
namespace
{
bool isSupportedKind(const EngineJobKind kind) noexcept
{
    return kind == EngineJobKind::ManifestAssetLoad ||
        kind == EngineJobKind::TerrainSetupStage ||
        kind == EngineJobKind::Custom;
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

void countJob(EngineJobQueueSummary& summary, const EngineJobRequest& request) noexcept
{
    ++summary.pendingCount;

    switch (request.kind)
    {
    case EngineJobKind::ManifestAssetLoad:
        ++summary.manifestAssetLoadCount;
        break;
    case EngineJobKind::TerrainSetupStage:
        ++summary.terrainSetupStageCount;
        break;
    case EngineJobKind::Custom:
        ++summary.customCount;
        break;
    case EngineJobKind::Unknown:
        break;
    }

    switch (request.priority)
    {
    case EngineJobPriority::Low:
        ++summary.lowPriorityCount;
        break;
    case EngineJobPriority::Normal:
        ++summary.normalPriorityCount;
        break;
    case EngineJobPriority::High:
        ++summary.highPriorityCount;
        break;
    }
}

std::vector<std::size_t> executionOrder(const std::vector<EngineJobRecord>& jobs)
{
    std::vector<std::size_t> indices;
    indices.reserve(jobs.size());
    for (std::size_t index = 0; index < jobs.size(); ++index)
    {
        indices.push_back(index);
    }

    std::stable_sort(indices.begin(), indices.end(), [&jobs](const std::size_t lhs, const std::size_t rhs) {
        return priorityRank(jobs[lhs].request.priority) > priorityRank(jobs[rhs].request.priority);
    });

    return indices;
}
} // namespace

bool isValid(const EngineJobId id) noexcept
{
    return id.value != 0 || id.subValue != 0;
}

bool operator==(const EngineJobId lhs, const EngineJobId rhs) noexcept
{
    return lhs.value == rhs.value && lhs.subValue == rhs.subValue;
}

EngineJobQueueResult EngineJobQueue::push(const EngineJobRequest& request)
{
    if (!isValid(request.id) || !isSupportedKind(request.kind))
    {
        return EngineJobQueueResult::InvalidArgument;
    }

    if (contains(request.id))
    {
        return EngineJobQueueResult::AlreadyQueued;
    }

    EngineJobRecord record;
    record.request = request;
    record.status = EngineJobStatus::Pending;
    jobs_.push_back(record);
    return EngineJobQueueResult::Queued;
}

bool EngineJobQueue::contains(const EngineJobId id) const noexcept
{
    for (const EngineJobRecord& record : jobs_)
    {
        if (record.request.id == id)
        {
            return true;
        }
    }

    return false;
}

std::size_t EngineJobQueue::jobCount() const noexcept
{
    return jobs_.size();
}

const std::vector<EngineJobRecord>& EngineJobQueue::jobs() const noexcept
{
    return jobs_;
}

EngineJobQueueSummary EngineJobQueue::summary() const noexcept
{
    EngineJobQueueSummary result;
    for (const EngineJobRecord& record : jobs_)
    {
        countJob(result, record.request);
    }
    return result;
}

void EngineJobQueue::clear() noexcept
{
    jobs_.clear();
}

EngineJobExecutionResult runReadyJobs(
    EngineJobQueue& queue,
    const std::size_t maxJobs,
    const EngineJobCallback callback,
    void* const userData)
{
    EngineJobExecutionResult result;
    if (maxJobs == 0 || queue.jobCount() == 0)
    {
        return result;
    }

    const std::vector<EngineJobRecord> before = queue.jobs();
    std::vector<EngineJobId> completed;
    const std::vector<std::size_t> order = executionOrder(before);

    if (callback == nullptr)
    {
        for (const std::size_t index : order)
        {
            if (result.summary.attemptedCount >= maxJobs)
            {
                break;
            }

            EngineJobRecord record = before[index];
            record.status = EngineJobStatus::Blocked;
            result.records.push_back(record);
            ++result.summary.attemptedCount;
            ++result.summary.blockedCount;
        }
        return result;
    }

    for (const std::size_t index : order)
    {
        if (result.summary.attemptedCount >= maxJobs)
        {
            break;
        }

        EngineJobRecord record = before[index];
        const EngineJobStatus status = callback(record.request, userData);
        record.status = status == EngineJobStatus::Completed ||
                status == EngineJobStatus::Failed ||
                status == EngineJobStatus::Blocked
            ? status
            : EngineJobStatus::Blocked;
        result.records.push_back(record);
        ++result.summary.attemptedCount;

        switch (record.status)
        {
        case EngineJobStatus::Completed:
            ++result.summary.completedCount;
            completed.push_back(record.request.id);
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

    if (!completed.empty())
    {
        EngineJobQueue retained;
        for (const EngineJobRecord& record : before)
        {
            const bool wasCompleted = std::find_if(
                                          completed.begin(),
                                          completed.end(),
                                          [&record](const EngineJobId id) {
                                              return record.request.id == id;
                                          }) != completed.end();
            if (!wasCompleted)
            {
                retained.push(record.request);
            }
        }
        queue = retained;
    }

    return result;
}
} // namespace full_engine
