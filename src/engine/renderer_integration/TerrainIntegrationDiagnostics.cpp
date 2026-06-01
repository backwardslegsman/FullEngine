#include "engine/renderer_integration/TerrainIntegrationDiagnostics.hpp"

namespace full_engine
{
namespace
{
bool requestMatches(
    const TerrainManifestAssetLoadRequest& request,
    const AssetId id,
    const AssetKind kind) noexcept
{
    return request.id == id && request.kind == kind;
}

const TerrainManifestAssetSourceRequestRecord* findSourceRequest(
    const TerrainManifestAssetSourceRequestPlan& sourceRequests,
    const TerrainManifestAssetLoadRequest& request) noexcept
{
    for (const TerrainManifestAssetSourceRequestRecord& record : sourceRequests.records)
    {
        if (requestMatches(request, record.request.id, record.request.kind))
        {
            return &record;
        }
    }
    return nullptr;
}

const AssetSourceUploadIntentRecord* findUploadIntent(
    const AssetSourceUploadIntentPlan& uploadIntents,
    const TerrainManifestAssetLoadRequest& request) noexcept
{
    for (const AssetSourceUploadIntentRecord& record : uploadIntents.records)
    {
        if (requestMatches(request, record.id, record.kind))
        {
            return &record;
        }
    }
    return nullptr;
}

void countSourceRequest(
    TerrainManifestAssetLoadServiceInputDiagnostics& diagnostics,
    const TerrainManifestAssetSourceRequestRecord* const record) noexcept
{
    if (record == nullptr)
    {
        ++diagnostics.sourceMissingCount;
        return;
    }

    switch (record->status)
    {
    case TerrainManifestAssetSourceRequestStatus::Mapped:
        ++diagnostics.sourceMappedCount;
        break;
    case TerrainManifestAssetSourceRequestStatus::MissingSource:
        ++diagnostics.sourceMissingCount;
        break;
    case TerrainManifestAssetSourceRequestStatus::InvalidRequest:
        ++diagnostics.sourceInvalidCount;
        break;
    }
}

void countUploadIntent(
    TerrainManifestAssetLoadServiceInputDiagnostics& diagnostics,
    const AssetSourceUploadIntentRecord* const record) noexcept
{
    if (record == nullptr)
    {
        ++diagnostics.uploadIntentMissingCount;
        return;
    }

    switch (record->status)
    {
    case AssetSourceUploadIntentStatus::Planned:
        ++diagnostics.uploadIntentPlannedCount;
        break;
    case AssetSourceUploadIntentStatus::SourceNotMapped:
        ++diagnostics.uploadIntentMissingCount;
        break;
    case AssetSourceUploadIntentStatus::InvalidSource:
        ++diagnostics.uploadIntentInvalidSourceCount;
        break;
    case AssetSourceUploadIntentStatus::UnsupportedRendererContract:
        ++diagnostics.uploadIntentUnsupportedRendererContractCount;
        break;
    }
}

TerrainManifestAssetLoadServiceInputDiagnostics makeLoadServiceInputDiagnostics(
    const TerrainManifestAssetLoadService& service,
    const TerrainManifestAssetSourceRequestPlan& sourceRequests,
    const AssetSourceUploadIntentPlan& uploadIntents) noexcept
{
    TerrainManifestAssetLoadServiceInputDiagnostics diagnostics;
    diagnostics.retainedRequestCount = service.requestCount();

    for (const TerrainManifestAssetLoadServiceRecord& serviceRecord : service.records())
    {
        const TerrainManifestAssetLoadRequest& request = serviceRecord.packet.request;
        countSourceRequest(diagnostics, findSourceRequest(sourceRequests, request));
        countUploadIntent(diagnostics, findUploadIntent(uploadIntents, request));
    }

    return diagnostics;
}
} // namespace

EngineJobQueueDiagnostics makeEngineJobQueueDiagnostics(const EngineJobQueue& queue)
{
    EngineJobQueueDiagnostics diagnostics;
    diagnostics.pendingJobCount = queue.jobCount();
    diagnostics.summary = queue.summary();
    return diagnostics;
}

TerrainManifestAssetLoadJobDiagnostics makeTerrainManifestAssetLoadJobDiagnostics(
    const TerrainManifestAssetLoadJobCoordinatorResult& result,
    const EngineJobQueue& jobs)
{
    TerrainManifestAssetLoadJobDiagnostics diagnostics;
    diagnostics.status = result.status;
    diagnostics.jobQueue = makeEngineJobQueueDiagnostics(jobs);
    diagnostics.mirror = result.mirror.summary;
    diagnostics.execution = result.jobs.summary;
    diagnostics.loadConsume = result.load.consume.summary;
    diagnostics.loadConsumed = result.load.consume.consumed;
    diagnostics.coordinator = result.summary;
    diagnostics.readiness = result.readiness.summary;
    return diagnostics;
}

TerrainManifestAssetLoadJobScheduleDiagnostics makeTerrainManifestAssetLoadJobScheduleDiagnostics(
    const TerrainManifestAssetLoadJobScheduleResult& result,
    const EngineJobQueue& jobs)
{
    TerrainManifestAssetLoadJobScheduleDiagnostics diagnostics;
    diagnostics.status = result.status;
    diagnostics.jobQueue = makeEngineJobQueueDiagnostics(jobs);
    diagnostics.mirror = result.mirror.summary;
    diagnostics.initialPendingLoadRequestCount = result.initialPendingLoadRequestCount;
    diagnostics.finalPendingLoadRequestCount = result.finalPendingLoadRequestCount;
    diagnostics.pendingJobCount = result.pendingJobCount;
    return diagnostics;
}

TerrainManifestAssetLoadJobReconcileDiagnostics makeTerrainManifestAssetLoadJobReconcileDiagnostics(
    const TerrainManifestAssetLoadJobReconcileResult& result,
    const EngineJobQueue& jobs)
{
    TerrainManifestAssetLoadJobReconcileDiagnostics diagnostics;
    diagnostics.status = result.status;
    diagnostics.jobQueue = makeEngineJobQueueDiagnostics(jobs);
    diagnostics.loadConsume = result.load.summary;
    diagnostics.loadConsumed = result.load.consumed;
    diagnostics.reconcile = result.summary;
    diagnostics.readiness = result.readiness.summary;
    return diagnostics;
}

TerrainManifestAssetLoadServiceDiagnostics makeTerrainManifestAssetLoadServiceDiagnostics(
    const TerrainManifestAssetLoadJobWorkPacketResult& workPackets,
    const TerrainManifestAssetLoadServiceEnqueueResult& enqueue,
    const TerrainManifestAssetLoadServiceTickResult& tick,
    const TerrainManifestAssetLoadJobCompletionReconcileResult& completionReconcile,
    const TerrainManifestAssetLoadService& service)
{
    return makeTerrainManifestAssetLoadServiceDiagnostics(
        workPackets,
        enqueue,
        tick,
        completionReconcile,
        service,
        TerrainManifestAssetSourceRequestPlan{},
        AssetSourceUploadIntentPlan{});
}

TerrainManifestAssetLoadServiceDiagnostics makeTerrainManifestAssetLoadServiceDiagnostics(
    const TerrainManifestAssetLoadJobWorkPacketResult& workPackets,
    const TerrainManifestAssetLoadServiceEnqueueResult& enqueue,
    const TerrainManifestAssetLoadServiceTickResult& tick,
    const TerrainManifestAssetLoadJobCompletionReconcileResult& completionReconcile,
    const TerrainManifestAssetLoadService& service,
    const TerrainManifestAssetSourceRequestPlan& sourceRequests,
    const AssetSourceUploadIntentPlan& uploadIntents)
{
    TerrainManifestAssetLoadServiceDiagnostics diagnostics;
    diagnostics.workPackets = workPackets.summary;
    diagnostics.enqueue = enqueue.summary;
    diagnostics.input = makeLoadServiceInputDiagnostics(service, sourceRequests, uploadIntents);
    diagnostics.tickStatus = tick.status;
    diagnostics.tick = tick.summary;
    diagnostics.retainedRequestCount = service.requestCount();
    diagnostics.retainedPendingCount = service.pendingCount();
    diagnostics.retainedCompletedCount = service.completedCount();
    diagnostics.retainedFailedCount = service.failedCount();
    diagnostics.retainedCompletionCount = service.completions().size();
    diagnostics.completionReconcileStatus = completionReconcile.status;
    diagnostics.completionPublish = completionReconcile.publish.summary;
    diagnostics.completionLoadConsume = completionReconcile.reconcile.load.summary;
    diagnostics.completionLoadConsumed = completionReconcile.reconcile.load.consumed;
    diagnostics.completionReconcile = completionReconcile.reconcile.summary;
    diagnostics.completionReadiness = completionReconcile.reconcile.readiness.summary;
    return diagnostics;
}

TerrainManifestAssetSourceDiagnostics makeTerrainManifestAssetSourceDiagnostics(
    const AssetSourceCatalog& sources,
    const TerrainManifestAssetSourceRequestPlan& plan)
{
    TerrainManifestAssetSourceDiagnostics diagnostics;
    diagnostics.retainedSourceCount = sources.sourceCount();
    diagnostics.requests = plan.summary;
    return diagnostics;
}

AssetSourceUploadIntentDiagnostics makeAssetSourceUploadIntentDiagnostics(
    const AssetSourceUploadIntentPlan& plan)
{
    AssetSourceUploadIntentDiagnostics diagnostics;
    diagnostics.recordCount = plan.records.size();
    diagnostics.summary = plan.summary;
    return diagnostics;
}

TerrainAssetBatchResolveDiagnostics makeTerrainAssetBatchResolveDiagnostics(
    const TerrainAssetBatchResolveResult& result)
{
    TerrainAssetBatchResolveDiagnostics diagnostics;
    diagnostics.requestCount = result.records.size();
    diagnostics.summary = result.summary;
    return diagnostics;
}

TerrainManifestRuntimeStageDiagnostics makeTerrainManifestRuntimeStageDiagnostics(
    const TerrainManifestRuntimeStageResult& result)
{
    TerrainManifestRuntimeStageDiagnostics diagnostics;
    diagnostics.status = result.status;
    diagnostics.manifestAssetCount = result.summary.manifestAssetCount;
    diagnostics.manifestTerrainChunkCount = result.summary.manifestTerrainChunkCount;
    diagnostics.resolvedResourceCount = result.summary.resolvedResourceCount;
    diagnostics.missingWorldDescCount = result.summary.missingWorldDescCount;
    diagnostics.desiredSetupCount = result.summary.desiredSetupCount;
    diagnostics.assetResolve = result.assetResolve.summary;
    diagnostics.stage = result.stagePlan.summary;
    diagnostics.queue = result.queue.summary;
    return diagnostics;
}

TerrainSetupRequestDiagnostics makeTerrainSetupRequestDiagnostics(
    const TerrainChunkRequestApplyResult& result)
{
    TerrainSetupRequestDiagnostics diagnostics;
    diagnostics.requestCount = result.records.size();
    diagnostics.summary = result.summary;

    for (const TerrainChunkRequestRecord& record : result.records)
    {
        if (record.request.type == TerrainChunkRequestType::Add)
        {
            ++diagnostics.addCount;
        }
        else
        {
            ++diagnostics.removeCount;
        }
    }

    return diagnostics;
}

TerrainResidencyRequestDiagnostics makeTerrainResidencyRequestDiagnostics(
    const WorldChunkResidencyApplyResult& result)
{
    TerrainResidencyRequestDiagnostics diagnostics;
    diagnostics.requestCount = result.records.size();
    diagnostics.summary = result.summary;

    for (const WorldChunkResidencyRequestRecord& record : result.records)
    {
        if (record.request.type == WorldChunkResidencyRequestType::MakeResident)
        {
            ++diagnostics.makeResidentCount;
        }
        else
        {
            ++diagnostics.makeUnloadedCount;
        }
    }

    return diagnostics;
}

TerrainPipelineDiagnostics makeTerrainPipelineDiagnostics(
    const WorldRenderSnapshot& snapshot,
    const TerrainRenderPrep& prep,
    const TerrainLifecyclePlan& lifecycle,
    const TerrainRendererCommandList& commands,
    const TerrainDescriptorBuildResult& descriptors,
    const TerrainSubmissionResult& submission,
    const std::size_t handleCount)
{
    TerrainPipelineDiagnostics diagnostics;
    diagnostics.handleCount = handleCount;
    diagnostics.snapshotReadyCount = snapshot.readyCount;
    diagnostics.snapshotNotResidentCount = snapshot.notResidentCount;
    diagnostics.snapshotMissingChunkCount = snapshot.missingChunkCount;
    diagnostics.snapshotInvalidBoundsCount = snapshot.invalidBoundsCount;
    diagnostics.snapshotOutOfRangeCount = snapshot.outOfRangeCount;
    diagnostics.snapshotInvalidInputCount = snapshot.invalidInputCount;
    diagnostics.prep = prep.summary;
    diagnostics.lifecycle = lifecycle.summary;
    diagnostics.commands = commands.summary;
    diagnostics.descriptors = descriptors.summary;
    diagnostics.submission = submission.summary;
    return diagnostics;
}
} // namespace full_engine
