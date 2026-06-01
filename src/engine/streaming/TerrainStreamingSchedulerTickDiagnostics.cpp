#include "engine/streaming/TerrainStreamingSchedulerTickDiagnostics.hpp"

namespace full_engine
{
TerrainStreamingSchedulerTickDiagnostics makeTerrainStreamingSchedulerTickDiagnostics(
    const TerrainStreamingSchedulerTickResult& result)
{
    TerrainStreamingSchedulerTickDiagnostics diagnostics;
    diagnostics.status = result.status;
    diagnostics.decisionStatus = result.decision.status;
    diagnostics.decisionReason = result.decision.reason;
    diagnostics.budgetProfile = result.decision.budgetProfile;
    diagnostics.pendingLoadRequestCount = result.decision.pendingLoadRequestCount;
    diagnostics.pendingJobCount = result.decision.pendingJobCount;
    diagnostics.deferredWorkCount = result.decision.deferredWorkCount;
    diagnostics.peakDeferredWorkCount = result.decision.peakDeferredWorkCount;
    diagnostics.runtimeBacklogCount = result.decision.runtimeBacklogCount;
    diagnostics.pressureCount = result.decision.pressureCount;
    diagnostics.maxAssetLoadJobs = result.decision.maxAssetLoadJobs;
    diagnostics.loadJobsRan = result.loadJobsRan;
    diagnostics.streamingRan = result.streamingRan;
    diagnostics.history.hasSchedulerDecision = true;
    diagnostics.history.status = diagnostics.status;
    diagnostics.history.decisionStatus = diagnostics.decisionStatus;
    diagnostics.history.decisionReason = diagnostics.decisionReason;
    diagnostics.history.budgetProfile = diagnostics.budgetProfile;
    diagnostics.history.pendingLoadRequestCount = diagnostics.pendingLoadRequestCount;
    diagnostics.history.pendingJobCount = diagnostics.pendingJobCount;
    diagnostics.history.deferredWorkCount = diagnostics.deferredWorkCount;
    diagnostics.history.peakDeferredWorkCount = diagnostics.peakDeferredWorkCount;
    diagnostics.history.runtimeBacklogCount = diagnostics.runtimeBacklogCount;
    diagnostics.history.pressureCount = diagnostics.pressureCount;
    diagnostics.history.maxAssetLoadJobs = diagnostics.maxAssetLoadJobs;
    diagnostics.history.loadJobsRan = diagnostics.loadJobsRan;
    diagnostics.history.streamingRan = diagnostics.streamingRan;

    diagnostics.loadJobStatus = result.loadJobs.status;
    diagnostics.loadJobMirror = result.loadJobs.mirror.summary;
    diagnostics.loadJobExecution = result.loadJobs.jobs.summary;
    diagnostics.loadConsume = result.loadJobs.load.consume.summary;
    diagnostics.loadConsumed = result.loadJobs.load.consume.consumed;
    diagnostics.loadJobCoordinator = result.loadJobs.summary;
    diagnostics.loadReadiness = result.loadJobs.readiness.summary;

    diagnostics.streamingStatus = result.streaming.status;
    diagnostics.manifestStreamingStatus = result.streaming.streaming.status;
    diagnostics.runtimeStatus = result.streaming.runtime.status;
    diagnostics.runtimeUpdateRan = result.streaming.runtimeUpdateRan;
    diagnostics.setupRequestsBeforeRuntime = result.streaming.setupRequestsBeforeRuntime;
    diagnostics.residencyRequestsBeforeRuntime = result.streaming.residencyRequestsBeforeRuntime;
    diagnostics.setupRequestsAfterRuntime = result.streaming.setupRequestsAfterRuntime;
    diagnostics.residencyRequestsAfterRuntime = result.streaming.residencyRequestsAfterRuntime;
    diagnostics.streamingSummary = result.streaming.streaming.summary;
    diagnostics.streamingQueue = result.streaming.streaming.streamingQueue.summary;
    return diagnostics;
}
} // namespace full_engine
