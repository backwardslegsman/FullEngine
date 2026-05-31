#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace full_engine
{
/**
 * @brief Stable caller-owned identity for queued engine work.
 *
 * Job IDs are copied into the queue and used for deduplication. A default ID is
 * invalid. IDs do not imply thread ownership, file handles, renderer resources,
 * or external job-system state.
 */
struct EngineJobId
{
    /** @brief Primary caller-defined identity value. */
    std::uint64_t value = 0;

    /** @brief Optional secondary identity value for exact subsystem encodings. */
    std::uint64_t subValue = 0;
};

/** @brief Returns whether an engine job ID is non-default. */
bool isValid(EngineJobId id) noexcept;

/** @brief Compares job IDs for equality. */
bool operator==(EngineJobId lhs, EngineJobId rhs) noexcept;

/** @brief Engine work category used for diagnostics and callback dispatch. */
enum class EngineJobKind
{
    /** @brief Invalid/default kind; rejected by the queue. */
    Unknown,

    /** @brief Manifest asset load intent mirrored from terrain manifest loading. */
    ManifestAssetLoad,

    /** @brief Terrain setup staging intent reserved for future streaming work. */
    TerrainSetupStage,

    /** @brief Caller-defined CPU-side work intent. */
    Custom,
};

/** @brief Coarse deterministic priority for pending engine jobs. */
enum class EngineJobPriority
{
    /** @brief Lowest scheduler priority. */
    Low,

    /** @brief Default scheduler priority. */
    Normal,

    /** @brief Highest scheduler priority. */
    High,
};

/** @brief Result of attempting to queue one engine job. */
enum class EngineJobQueueResult
{
    /** @brief The job was accepted and retained by the queue. */
    Queued,

    /** @brief A pending job with the same ID already exists. */
    AlreadyQueued,

    /** @brief The request has an invalid ID or unsupported kind. */
    InvalidArgument,
};

/**
 * @brief Status produced by a synchronous engine job callback.
 *
 * `Pending` is used only for retained queue records. Callbacks should return
 * `Completed`, `Failed`, or `Blocked`.
 */
enum class EngineJobStatus
{
    /** @brief Retained queue record is waiting to be attempted. */
    Pending,

    /** @brief Callback finished the job and it can be removed. */
    Completed,

    /** @brief Callback failed the job; it remains pending for retry policy. */
    Failed,

    /** @brief Callback could not run the job yet; it remains pending. */
    Blocked,
};

/**
 * @brief Value request for one unit of engine work.
 *
 * Payload fields are opaque to the job queue. Higher-level systems define how
 * they encode IDs, kinds, or other lightweight values. The queue copies
 * requests by value and performs no IO, threading, renderer calls, or resource
 * creation.
 */
struct EngineJobRequest
{
    /** @brief Stable caller-defined ID used for deduplication. */
    EngineJobId id = {};

    /** @brief High-level job category for dispatch and diagnostics. */
    EngineJobKind kind = EngineJobKind::Unknown;

    /** @brief Deterministic execution priority. */
    EngineJobPriority priority = EngineJobPriority::Normal;

    /** @brief Opaque caller payload slot. */
    std::uint64_t payload0 = 0;

    /** @brief Opaque caller payload slot. */
    std::uint64_t payload1 = 0;

    /** @brief Opaque caller payload slot. */
    std::uint64_t payload2 = 0;

    /** @brief Opaque caller payload slot. */
    std::uint64_t payload3 = 0;
};

/** @brief Retained or executed job record with status diagnostics. */
struct EngineJobRecord
{
    /** @brief Copied job request. */
    EngineJobRequest request = {};

    /** @brief Current retained status or execution result status. */
    EngineJobStatus status = EngineJobStatus::Pending;
};

/** @brief Summary counters for the current pending queue contents. */
struct EngineJobQueueSummary
{
    /** @brief Total number of retained pending jobs. */
    std::size_t pendingCount = 0;

    /** @brief Number of pending manifest asset load jobs. */
    std::size_t manifestAssetLoadCount = 0;

    /** @brief Number of pending terrain setup staging jobs. */
    std::size_t terrainSetupStageCount = 0;

    /** @brief Number of pending caller-defined jobs. */
    std::size_t customCount = 0;

    /** @brief Number of pending low-priority jobs. */
    std::size_t lowPriorityCount = 0;

    /** @brief Number of pending normal-priority jobs. */
    std::size_t normalPriorityCount = 0;

    /** @brief Number of pending high-priority jobs. */
    std::size_t highPriorityCount = 0;
};

/** @brief Summary counters for one job execution pass. */
struct EngineJobExecutionSummary
{
    /** @brief Number of jobs passed to the callback or marked blocked by a null callback. */
    std::size_t attemptedCount = 0;

    /** @brief Number of attempted jobs completed and removed from the queue. */
    std::size_t completedCount = 0;

    /** @brief Number of attempted jobs reported as failed and left pending. */
    std::size_t failedCount = 0;

    /** @brief Number of attempted jobs reported as blocked and left pending. */
    std::size_t blockedCount = 0;
};

/** @brief Ordered result of one synchronous job execution pass. */
struct EngineJobExecutionResult
{
    /** @brief One record per attempted job in execution order. */
    std::vector<EngineJobRecord> records;

    /** @brief Aggregate counters for this execution pass. */
    EngineJobExecutionSummary summary = {};
};

/** @brief Callback used by `runReadyJobs` to execute one queued request. */
using EngineJobCallback = EngineJobStatus (*)(const EngineJobRequest& request, void* userData);

/**
 * @brief Single-threaded CPU-only queue of engine background-work intent.
 *
 * The queue stores copied job requests and deduplicates by job ID. It does not
 * create threads, perform IO, sleep, call renderer APIs, create resources, or
 * own external job handles. All methods are not thread-safe; callers must
 * serialize access.
 */
class EngineJobQueue
{
public:
    /** @brief Queues one valid job request when its ID is not already pending. */
    EngineJobQueueResult push(const EngineJobRequest& request);

    /** @brief Returns whether a job ID is currently pending. */
    bool contains(EngineJobId id) const noexcept;

    /** @brief Returns the number of pending jobs. */
    std::size_t jobCount() const noexcept;

    /**
     * @brief Returns pending jobs in insertion order.
     *
     * Execution order is priority-first with stable insertion order for equal
     * priority. The returned reference is invalidated by later non-const calls.
     */
    const std::vector<EngineJobRecord>& jobs() const noexcept;

    /** @brief Returns counters for currently pending jobs. */
    EngineJobQueueSummary summary() const noexcept;

    /** @brief Removes all pending jobs. */
    void clear() noexcept;

private:
    std::vector<EngineJobRecord> jobs_;
};

/**
 * @brief Executes pending jobs synchronously in deterministic priority order.
 *
 * At most `maxJobs` jobs are attempted. Completed jobs are removed from
 * `queue`; failed or blocked jobs stay pending for retry. A null callback
 * blocks the pass without mutating the queue. Equal-priority jobs execute in
 * first-queued order.
 *
 * @param queue Queue to inspect and mutate after successful completions.
 * @param maxJobs Maximum jobs to attempt. Zero attempts nothing.
 * @param callback Caller-owned synchronous executor. It may perform external
 * work, but the helper itself performs no IO, threading, or renderer calls.
 * @param userData Opaque pointer forwarded to the callback.
 * @return Ordered per-job diagnostics for attempted jobs.
 */
EngineJobExecutionResult runReadyJobs(
    EngineJobQueue& queue,
    std::size_t maxJobs,
    EngineJobCallback callback,
    void* userData = nullptr);
} // namespace full_engine
