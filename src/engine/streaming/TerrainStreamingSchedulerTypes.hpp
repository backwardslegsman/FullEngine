#pragma once

namespace full_engine
{
/** @brief High-level scheduler action selected from streaming diagnostics. */
enum class TerrainStreamingSchedulerStatus
{
    Idle,
    RunStreaming,
    RunAssetLoadJobs,
    RunStreamingAndAssetLoadJobs,
};

/** @brief Primary pressure source that caused a scheduler decision. */
enum class TerrainStreamingSchedulerReason
{
    NoWork,
    PendingAssetLoads,
    PendingJobs,
    DeferredWorkPressure,
    StreamingBacklog,
    CatchUp,
};

/** @brief High-level result for one policy-driven synchronous streaming scheduler tick. */
enum class TerrainStreamingSchedulerTickStatus
{
    Idle,
    Success,
    LoadJobsBlocked,
    StreamingBlocked,
    RuntimeSetupFailed,
    RuntimeResidencyFailed,
    RuntimePipelineFailed,
};
} // namespace full_engine
