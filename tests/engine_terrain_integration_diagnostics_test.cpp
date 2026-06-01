#include "engine/renderer_integration/TerrainIntegrationDiagnostics.hpp"

#include <cassert>

int main()
{
    {
        const full_engine::TerrainIntegrationDiagnostics empty;
        assert(empty.setupRequests.requestCount == 0);
        assert(empty.residencyRequests.requestCount == 0);
        assert(empty.pipeline.handleCount == 0);

        const full_engine::TerrainManifestAssetLoadJobDiagnostics loadJobs;
        assert(loadJobs.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::NoPendingLoads);
        assert(loadJobs.jobQueue.pendingJobCount == 0);
        assert(loadJobs.mirror.queuedCount == 0);
        assert(loadJobs.execution.completedCount == 0);
        assert(loadJobs.loadConsume.loadedCount == 0);
        assert(!loadJobs.loadConsumed);
        assert(loadJobs.coordinator.finalPendingLoadRequestCount == 0);
        assert(loadJobs.readiness.readyCount == 0);

        const full_engine::TerrainManifestAssetLoadJobScheduleDiagnostics scheduledLoadJobs;
        assert(scheduledLoadJobs.status == full_engine::TerrainManifestAssetLoadJobScheduleStatus::NoPendingLoads);
        assert(scheduledLoadJobs.jobQueue.pendingJobCount == 0);
        assert(scheduledLoadJobs.mirror.queuedCount == 0);
        assert(scheduledLoadJobs.initialPendingLoadRequestCount == 0);
        assert(scheduledLoadJobs.finalPendingLoadRequestCount == 0);

        const full_engine::TerrainManifestAssetLoadJobReconcileDiagnostics reconciledLoadJobs;
        assert(reconciledLoadJobs.status == full_engine::TerrainManifestAssetLoadJobReconcileStatus::NoPendingLoads);
        assert(reconciledLoadJobs.jobQueue.pendingJobCount == 0);
        assert(reconciledLoadJobs.loadConsume.loadedCount == 0);
        assert(!reconciledLoadJobs.loadConsumed);
        assert(reconciledLoadJobs.reconcile.finalPendingLoadRequestCount == 0);
        assert(reconciledLoadJobs.readiness.readyCount == 0);

        const full_engine::TerrainManifestAssetLoadServiceDiagnostics loadService;
        assert(loadService.workPackets.packetizedCount == 0);
        assert(loadService.enqueue.queuedCount == 0);
        assert(loadService.tickStatus == full_engine::TerrainManifestAssetLoadServiceTickStatus::Idle);
        assert(loadService.tick.attemptedCount == 0);
        assert(loadService.retainedRequestCount == 0);
        assert(loadService.retainedPendingCount == 0);
        assert(loadService.retainedCompletedCount == 0);
        assert(loadService.retainedFailedCount == 0);
        assert(loadService.retainedCompletionCount == 0);
        assert(
            loadService.completionReconcileStatus ==
            full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::NoPendingLoads);
        assert(loadService.completionPublish.publishedCount == 0);
        assert(loadService.completionLoadConsume.loadedCount == 0);
        assert(!loadService.completionLoadConsumed);
        assert(loadService.completionReconcile.finalReadyHandleCount == 0);
        assert(loadService.completionReadiness.readyCount == 0);

        const full_engine::TerrainManifestAssetSourceDiagnostics assetSources;
        assert(assetSources.retainedSourceCount == 0);
        assert(assetSources.requests.mappedCount == 0);
        assert(assetSources.requests.missingSourceCount == 0);
        assert(assetSources.requests.invalidRequestCount == 0);
    }

    {
        full_engine::EngineJobQueue jobs;
        assert(jobs.push({
            full_engine::EngineJobId{1, 0},
            full_engine::EngineJobKind::ManifestAssetLoad,
            full_engine::EngineJobPriority::High,
            10,
            20,
            0,
            0}) == full_engine::EngineJobQueueResult::Queued);
        assert(jobs.push({
            full_engine::EngineJobId{2, 0},
            full_engine::EngineJobKind::Custom,
            full_engine::EngineJobPriority::Low,
            0,
            0,
            0,
            0}) == full_engine::EngineJobQueueResult::Queued);

        const std::size_t sourceJobCount = jobs.jobCount();
        const full_engine::EngineJobQueueDiagnostics diagnostics =
            full_engine::makeEngineJobQueueDiagnostics(jobs);

        assert(diagnostics.pendingJobCount == 2);
        assert(diagnostics.summary.pendingCount == 2);
        assert(diagnostics.summary.manifestAssetLoadCount == 1);
        assert(diagnostics.summary.customCount == 1);
        assert(diagnostics.summary.highPriorityCount == 1);
        assert(diagnostics.summary.lowPriorityCount == 1);
        assert(jobs.jobCount() == sourceJobCount);
    }

    {
        full_engine::EngineJobQueue jobs;
        assert(jobs.push({
            full_engine::EngineJobId{3, 0},
            full_engine::EngineJobKind::ManifestAssetLoad,
            full_engine::EngineJobPriority::Normal,
            30,
            40,
            0,
            0}) == full_engine::EngineJobQueueResult::Queued);

        full_engine::TerrainManifestAssetLoadJobCoordinatorResult result;
        result.status = full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::Success;
        result.mirror.summary.queuedCount = 1;
        result.mirror.summary.alreadyQueuedCount = 2;
        result.mirror.summary.invalidArgumentCount = 3;
        result.mirror.records.push_back({});
        result.jobs.summary.attemptedCount = 4;
        result.jobs.summary.completedCount = 5;
        result.jobs.summary.failedCount = 6;
        result.jobs.summary.blockedCount = 7;
        result.jobs.records.push_back({});
        result.load.consume.summary.loadedCount = 8;
        result.load.consume.summary.alreadyLoadedCount = 9;
        result.load.consume.summary.missingHandleCount = 10;
        result.load.consume.summary.catalogRejectedCount = 11;
        result.load.consume.consumed = true;
        result.load.consume.records.push_back({});
        result.summary.initialPendingLoadRequestCount = 12;
        result.summary.finalPendingLoadRequestCount = 13;
        result.summary.finalReadyHandleCount = 14;
        result.summary.finalMissingHandleCount = 15;
        result.readiness.summary.requestedCount = 16;
        result.readiness.summary.readyCount = 17;
        result.readiness.summary.missingHandleCount = 18;
        result.readiness.records.push_back({});

        const std::size_t sourceJobCount = jobs.jobCount();
        const std::size_t sourceMirrorRecordCount = result.mirror.records.size();
        const std::size_t sourceJobRecordCount = result.jobs.records.size();
        const std::size_t sourceLoadRecordCount = result.load.consume.records.size();
        const std::size_t sourceReadinessRecordCount = result.readiness.records.size();
        const full_engine::TerrainManifestAssetLoadJobDiagnostics diagnostics =
            full_engine::makeTerrainManifestAssetLoadJobDiagnostics(result, jobs);

        assert(diagnostics.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::Success);
        assert(diagnostics.jobQueue.pendingJobCount == 1);
        assert(diagnostics.jobQueue.summary.manifestAssetLoadCount == 1);
        assert(diagnostics.mirror.queuedCount == 1);
        assert(diagnostics.mirror.alreadyQueuedCount == 2);
        assert(diagnostics.mirror.invalidArgumentCount == 3);
        assert(diagnostics.execution.attemptedCount == 4);
        assert(diagnostics.execution.completedCount == 5);
        assert(diagnostics.execution.failedCount == 6);
        assert(diagnostics.execution.blockedCount == 7);
        assert(diagnostics.loadConsume.loadedCount == 8);
        assert(diagnostics.loadConsume.alreadyLoadedCount == 9);
        assert(diagnostics.loadConsume.missingHandleCount == 10);
        assert(diagnostics.loadConsume.catalogRejectedCount == 11);
        assert(diagnostics.loadConsumed);
        assert(diagnostics.coordinator.initialPendingLoadRequestCount == 12);
        assert(diagnostics.coordinator.finalPendingLoadRequestCount == 13);
        assert(diagnostics.coordinator.finalReadyHandleCount == 14);
        assert(diagnostics.coordinator.finalMissingHandleCount == 15);
        assert(diagnostics.readiness.requestedCount == 16);
        assert(diagnostics.readiness.readyCount == 17);
        assert(diagnostics.readiness.missingHandleCount == 18);
        assert(jobs.jobCount() == sourceJobCount);
        assert(result.mirror.records.size() == sourceMirrorRecordCount);
        assert(result.jobs.records.size() == sourceJobRecordCount);
        assert(result.load.consume.records.size() == sourceLoadRecordCount);
        assert(result.readiness.records.size() == sourceReadinessRecordCount);
    }

    {
        const full_engine::EngineJobQueue jobs;
        full_engine::TerrainManifestAssetLoadJobCoordinatorResult result;
        result.status = full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::JobsBlocked;
        result.jobs.summary.failedCount = 1;
        result.jobs.summary.blockedCount = 2;
        result.summary.finalPendingLoadRequestCount = 3;

        const full_engine::TerrainManifestAssetLoadJobDiagnostics diagnostics =
            full_engine::makeTerrainManifestAssetLoadJobDiagnostics(result, jobs);

        assert(diagnostics.status == full_engine::TerrainManifestAssetLoadJobCoordinatorStatus::JobsBlocked);
        assert(diagnostics.execution.failedCount == 1);
        assert(diagnostics.execution.blockedCount == 2);
        assert(diagnostics.coordinator.finalPendingLoadRequestCount == 3);
    }

    {
        full_engine::EngineJobQueue jobs;
        assert(jobs.push({
            full_engine::EngineJobId{4, 0},
            full_engine::EngineJobKind::ManifestAssetLoad,
            full_engine::EngineJobPriority::Normal,
            40,
            50,
            0,
            0}) == full_engine::EngineJobQueueResult::Queued);

        full_engine::TerrainManifestAssetLoadJobScheduleResult result;
        result.status = full_engine::TerrainManifestAssetLoadJobScheduleStatus::Scheduled;
        result.mirror.summary.queuedCount = 1;
        result.mirror.summary.alreadyQueuedCount = 2;
        result.mirror.summary.invalidArgumentCount = 3;
        result.initialPendingLoadRequestCount = 4;
        result.finalPendingLoadRequestCount = 5;
        result.pendingJobCount = 6;
        result.mirror.records.push_back({});

        const std::size_t sourceJobCount = jobs.jobCount();
        const std::size_t sourceMirrorRecordCount = result.mirror.records.size();
        const full_engine::TerrainManifestAssetLoadJobScheduleDiagnostics diagnostics =
            full_engine::makeTerrainManifestAssetLoadJobScheduleDiagnostics(result, jobs);

        assert(diagnostics.status == full_engine::TerrainManifestAssetLoadJobScheduleStatus::Scheduled);
        assert(diagnostics.jobQueue.pendingJobCount == 1);
        assert(diagnostics.jobQueue.summary.manifestAssetLoadCount == 1);
        assert(diagnostics.mirror.queuedCount == 1);
        assert(diagnostics.mirror.alreadyQueuedCount == 2);
        assert(diagnostics.mirror.invalidArgumentCount == 3);
        assert(diagnostics.initialPendingLoadRequestCount == 4);
        assert(diagnostics.finalPendingLoadRequestCount == 5);
        assert(diagnostics.pendingJobCount == 6);
        assert(jobs.jobCount() == sourceJobCount);
        assert(result.mirror.records.size() == sourceMirrorRecordCount);
    }

    {
        full_engine::EngineJobQueue jobs;
        assert(jobs.push({
            full_engine::EngineJobId{5, 0},
            full_engine::EngineJobKind::ManifestAssetLoad,
            full_engine::EngineJobPriority::High,
            50,
            60,
            0,
            0}) == full_engine::EngineJobQueueResult::Queued);

        full_engine::TerrainManifestAssetLoadJobReconcileResult result;
        result.status = full_engine::TerrainManifestAssetLoadJobReconcileStatus::Success;
        result.load.summary.loadedCount = 1;
        result.load.summary.alreadyLoadedCount = 2;
        result.load.summary.missingHandleCount = 3;
        result.load.summary.catalogRejectedCount = 4;
        result.load.consumed = true;
        result.load.records.push_back({});
        result.summary.initialPendingLoadRequestCount = 5;
        result.summary.finalPendingLoadRequestCount = 6;
        result.summary.initialPendingJobCount = 7;
        result.summary.finalPendingJobCount = 8;
        result.summary.removedScheduledJobCount = 9;
        result.summary.finalReadyHandleCount = 10;
        result.summary.finalMissingHandleCount = 11;
        result.readiness.summary.requestedCount = 12;
        result.readiness.summary.readyCount = 13;
        result.readiness.summary.missingHandleCount = 14;
        result.readiness.records.push_back({});

        const std::size_t sourceJobCount = jobs.jobCount();
        const std::size_t sourceLoadRecordCount = result.load.records.size();
        const std::size_t sourceReadinessRecordCount = result.readiness.records.size();
        const full_engine::TerrainManifestAssetLoadJobReconcileDiagnostics diagnostics =
            full_engine::makeTerrainManifestAssetLoadJobReconcileDiagnostics(result, jobs);

        assert(diagnostics.status == full_engine::TerrainManifestAssetLoadJobReconcileStatus::Success);
        assert(diagnostics.jobQueue.pendingJobCount == 1);
        assert(diagnostics.jobQueue.summary.manifestAssetLoadCount == 1);
        assert(diagnostics.loadConsume.loadedCount == 1);
        assert(diagnostics.loadConsume.alreadyLoadedCount == 2);
        assert(diagnostics.loadConsume.missingHandleCount == 3);
        assert(diagnostics.loadConsume.catalogRejectedCount == 4);
        assert(diagnostics.loadConsumed);
        assert(diagnostics.reconcile.initialPendingLoadRequestCount == 5);
        assert(diagnostics.reconcile.finalPendingLoadRequestCount == 6);
        assert(diagnostics.reconcile.initialPendingJobCount == 7);
        assert(diagnostics.reconcile.finalPendingJobCount == 8);
        assert(diagnostics.reconcile.removedScheduledJobCount == 9);
        assert(diagnostics.reconcile.finalReadyHandleCount == 10);
        assert(diagnostics.reconcile.finalMissingHandleCount == 11);
        assert(diagnostics.readiness.requestedCount == 12);
        assert(diagnostics.readiness.readyCount == 13);
        assert(diagnostics.readiness.missingHandleCount == 14);
        assert(jobs.jobCount() == sourceJobCount);
        assert(result.load.records.size() == sourceLoadRecordCount);
        assert(result.readiness.records.size() == sourceReadinessRecordCount);
    }

    {
        const full_engine::TerrainManifestAssetLoadRequest request{
            full_engine::AssetId{70},
            full_engine::AssetKind::Mesh};
        const full_engine::TerrainManifestAssetLoadJobWorkPacket packet{
            full_engine::engineJobIdForTerrainManifestAssetLoadRequest(request),
            request,
            full_engine::EngineJobPriority::High};

        full_engine::TerrainManifestAssetLoadJobWorkPacketResult packets;
        packets.packets.push_back(packet);
        packets.records.push_back({});
        packets.summary.packetizedCount = 1;
        packets.summary.skippedUnsupportedJobCount = 2;
        packets.summary.invalidPayloadCount = 3;

        full_engine::TerrainManifestAssetLoadService service;
        const full_engine::TerrainManifestAssetLoadServiceEnqueueResult enqueue =
            service.enqueueWorkPackets(packets);
        const full_engine::TerrainManifestAssetLoadServiceTickResult tick =
            service.tick(
                1,
                [](const full_engine::TerrainManifestAssetLoadRequest&, void*) {
                    full_engine::TerrainManifestAssetLoadCallbackResult result;
                    result.status = full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded;
                    result.mesh = {70};
                    return result;
                });

        full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult completion;
        completion.status = full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::Success;
        completion.publish.summary.publishedCount = 4;
        completion.publish.summary.alreadyPublishedCount = 5;
        completion.publish.summary.missingHandleCount = 6;
        completion.reconcile.load.summary.loadedCount = 7;
        completion.reconcile.load.summary.alreadyLoadedCount = 8;
        completion.reconcile.load.consumed = true;
        completion.reconcile.summary.finalReadyHandleCount = 9;
        completion.reconcile.summary.finalMissingHandleCount = 10;
        completion.reconcile.readiness.summary.readyCount = 11;
        completion.reconcile.readiness.summary.missingHandleCount = 12;
        completion.publish.records.push_back({});
        completion.reconcile.load.records.push_back({});
        completion.reconcile.readiness.records.push_back({});

        const std::size_t sourcePacketRecordCount = packets.records.size();
        const std::size_t sourcePacketCount = packets.packets.size();
        const std::size_t sourceEnqueueRecordCount = enqueue.records.size();
        const std::size_t sourceTickRecordCount = tick.records.size();
        const std::size_t sourceCompletionCount = service.completions().size();
        const std::size_t sourceCompletionPublishRecordCount = completion.publish.records.size();
        const std::size_t sourceLoadRecordCount = completion.reconcile.load.records.size();
        const std::size_t sourceReadinessRecordCount = completion.reconcile.readiness.records.size();

        const full_engine::TerrainManifestAssetLoadServiceDiagnostics diagnostics =
            full_engine::makeTerrainManifestAssetLoadServiceDiagnostics(
                packets,
                enqueue,
                tick,
                completion,
                service);

        assert(diagnostics.workPackets.packetizedCount == 1);
        assert(diagnostics.workPackets.skippedUnsupportedJobCount == 2);
        assert(diagnostics.workPackets.invalidPayloadCount == 3);
        assert(diagnostics.enqueue.queuedCount == 1);
        assert(diagnostics.enqueue.alreadyQueuedCount == 0);
        assert(diagnostics.tickStatus == full_engine::TerrainManifestAssetLoadServiceTickStatus::Progressed);
        assert(diagnostics.tick.attemptedCount == 1);
        assert(diagnostics.tick.loadedCount == 1);
        assert(diagnostics.retainedRequestCount == 1);
        assert(diagnostics.retainedPendingCount == 0);
        assert(diagnostics.retainedCompletedCount == 1);
        assert(diagnostics.retainedFailedCount == 0);
        assert(diagnostics.retainedCompletionCount == 1);
        assert(
            diagnostics.completionReconcileStatus ==
            full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::Success);
        assert(diagnostics.completionPublish.publishedCount == 4);
        assert(diagnostics.completionPublish.alreadyPublishedCount == 5);
        assert(diagnostics.completionPublish.missingHandleCount == 6);
        assert(diagnostics.completionLoadConsume.loadedCount == 7);
        assert(diagnostics.completionLoadConsume.alreadyLoadedCount == 8);
        assert(diagnostics.completionLoadConsumed);
        assert(diagnostics.completionReconcile.finalReadyHandleCount == 9);
        assert(diagnostics.completionReconcile.finalMissingHandleCount == 10);
        assert(diagnostics.completionReadiness.readyCount == 11);
        assert(diagnostics.completionReadiness.missingHandleCount == 12);
        assert(packets.records.size() == sourcePacketRecordCount);
        assert(packets.packets.size() == sourcePacketCount);
        assert(enqueue.records.size() == sourceEnqueueRecordCount);
        assert(tick.records.size() == sourceTickRecordCount);
        assert(service.completions().size() == sourceCompletionCount);
        assert(completion.publish.records.size() == sourceCompletionPublishRecordCount);
        assert(completion.reconcile.load.records.size() == sourceLoadRecordCount);
        assert(completion.reconcile.readiness.records.size() == sourceReadinessRecordCount);
    }

    {
        const full_engine::TerrainManifestAssetLoadService service;
        full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult completion;
        completion.status =
            full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPublishFailed;
        completion.publish.summary.catalogRejectedCount = 1;

        const full_engine::TerrainManifestAssetLoadServiceDiagnostics diagnostics =
            full_engine::makeTerrainManifestAssetLoadServiceDiagnostics(
                {},
                {},
                {},
                completion,
                service);

        assert(
            diagnostics.completionReconcileStatus ==
            full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::CompletionPublishFailed);
        assert(diagnostics.completionPublish.catalogRejectedCount == 1);
        assert(!diagnostics.completionLoadConsumed);
    }

    {
        full_engine::AssetSourceCatalog sources;
        full_engine::AssetSourceRecord meshSource;
        meshSource.id = full_engine::AssetId{80};
        meshSource.kind = full_engine::AssetKind::Mesh;
        meshSource.uri = "meshes/source.mesh";
        meshSource.descriptor.mesh.vertexCount = 4;
        meshSource.descriptor.mesh.indexCount = 6;
        meshSource.descriptor.mesh.localBounds.max[0] = 1.0f;
        meshSource.descriptor.mesh.localBounds.max[1] = 1.0f;
        meshSource.descriptor.mesh.localBounds.max[2] = 1.0f;
        assert(sources.addSource(meshSource) == full_engine::AssetSourceCatalogResult::Success);

        full_engine::AssetSourceRecord textureSource;
        textureSource.id = full_engine::AssetId{90};
        textureSource.kind = full_engine::AssetKind::Texture;
        textureSource.uri = "textures/source.dds";
        textureSource.descriptor.texture.width = 64;
        textureSource.descriptor.texture.height = 64;
        textureSource.descriptor.texture.mipCount = 1;
        textureSource.descriptor.texture.format = full_engine::AssetSourceTextureFormat::Rgba8;
        textureSource.descriptor.texture.semantic = full_engine::AssetSourceTextureSemantic::Color;
        textureSource.descriptor.texture.colorSpace = full_engine::AssetSourceTextureColorSpace::Srgb;
        assert(sources.addSource(textureSource) == full_engine::AssetSourceCatalogResult::Success);

        full_engine::TerrainManifestAssetSourceRequestPlan plan;
        plan.summary.mappedCount = 3;
        plan.summary.missingSourceCount = 4;
        plan.summary.invalidRequestCount = 5;
        plan.records.push_back({});

        const std::size_t sourceCount = sources.sourceCount();
        const std::size_t recordCount = plan.records.size();
        const full_engine::TerrainManifestAssetSourceDiagnostics diagnostics =
            full_engine::makeTerrainManifestAssetSourceDiagnostics(sources, plan);

        assert(diagnostics.retainedSourceCount == 2);
        assert(diagnostics.requests.mappedCount == 3);
        assert(diagnostics.requests.missingSourceCount == 4);
        assert(diagnostics.requests.invalidRequestCount == 5);
        assert(sources.sourceCount() == sourceCount);
        assert(plan.records.size() == recordCount);
    }

    {
        full_engine::TerrainAssetBatchResolveResult result;
        result.records.push_back({
            {1, 0, 0},
            full_engine::TerrainAssetBatchResolveStatus::Resolved,
            {},
            full_engine::TerrainResourceResult::Success});
        result.records.push_back({
            {2, 0, 0},
            full_engine::TerrainAssetBatchResolveStatus::MissingMeshHandle,
            {},
            full_engine::TerrainResourceResult::NotFound});
        result.records.push_back({
            {3, 0, 0},
            full_engine::TerrainAssetBatchResolveStatus::ResourceCatalogFailed,
            {},
            full_engine::TerrainResourceResult::AlreadyExists});
        result.summary.resolvedCount = 1;
        result.summary.missingChunkAssetsCount = 2;
        result.summary.invalidChunkAssetsCount = 3;
        result.summary.missingMeshHandleCount = 4;
        result.summary.missingMaterialHandleCount = 5;
        result.summary.missingSplatMapHandleCount = 6;
        result.summary.resourceCatalogFailedCount = 7;

        const std::size_t sourceRecordCount = result.records.size();
        const std::size_t sourceResourceCount = result.resources.resourceCount();
        const full_engine::TerrainAssetBatchResolveDiagnostics diagnostics =
            full_engine::makeTerrainAssetBatchResolveDiagnostics(result);

        assert(diagnostics.requestCount == 3);
        assert(diagnostics.summary.resolvedCount == 1);
        assert(diagnostics.summary.missingChunkAssetsCount == 2);
        assert(diagnostics.summary.invalidChunkAssetsCount == 3);
        assert(diagnostics.summary.missingMeshHandleCount == 4);
        assert(diagnostics.summary.missingMaterialHandleCount == 5);
        assert(diagnostics.summary.missingSplatMapHandleCount == 6);
        assert(diagnostics.summary.resourceCatalogFailedCount == 7);
        assert(result.records.size() == sourceRecordCount);
        assert(result.resources.resourceCount() == sourceResourceCount);
    }

    {
        const full_engine::TerrainManifestRuntimeStageResult result;
        const full_engine::TerrainManifestRuntimeStageDiagnostics diagnostics =
            full_engine::makeTerrainManifestRuntimeStageDiagnostics(result);

        assert(diagnostics.status == full_engine::TerrainManifestRuntimeStageStatus::Success);
        assert(diagnostics.manifestAssetCount == 0);
        assert(diagnostics.manifestTerrainChunkCount == 0);
        assert(diagnostics.resolvedResourceCount == 0);
        assert(diagnostics.missingWorldDescCount == 0);
        assert(diagnostics.desiredSetupCount == 0);
        assert(diagnostics.assetResolve.resolvedCount == 0);
        assert(diagnostics.stage.addCount == 0);
        assert(diagnostics.queue.queuedSetupCount == 0);
    }

    {
        full_engine::TerrainManifestRuntimeStageResult result;
        result.status = full_engine::TerrainManifestRuntimeStageStatus::Success;
        result.summary.manifestAssetCount = 1;
        result.summary.manifestTerrainChunkCount = 2;
        result.summary.resolvedResourceCount = 3;
        result.summary.missingWorldDescCount = 4;
        result.summary.desiredSetupCount = 5;
        result.assetResolve.summary.resolvedCount = 6;
        result.assetResolve.summary.missingMeshHandleCount = 7;
        result.stagePlan.summary.addCount = 8;
        result.stagePlan.summary.keepCount = 9;
        result.stagePlan.summary.removeCount = 10;
        result.stagePlan.summary.changedUnsupportedCount = 11;
        result.queue.summary.queuedSetupCount = 12;
        result.queue.summary.queuedMakeResidentCount = 13;
        result.queue.summary.skippedKeepCount = 14;
        result.queue.summary.skippedChangedCount = 15;
        result.assetResolve.records.push_back({});
        result.stagePlan.operations.push_back({});

        const std::size_t sourceResolveRecordCount = result.assetResolve.records.size();
        const std::size_t sourceStageOpCount = result.stagePlan.operations.size();
        const full_engine::TerrainManifestRuntimeStageDiagnostics diagnostics =
            full_engine::makeTerrainManifestRuntimeStageDiagnostics(result);

        assert(diagnostics.status == full_engine::TerrainManifestRuntimeStageStatus::Success);
        assert(diagnostics.manifestAssetCount == 1);
        assert(diagnostics.manifestTerrainChunkCount == 2);
        assert(diagnostics.resolvedResourceCount == 3);
        assert(diagnostics.missingWorldDescCount == 4);
        assert(diagnostics.desiredSetupCount == 5);
        assert(diagnostics.assetResolve.resolvedCount == 6);
        assert(diagnostics.assetResolve.missingMeshHandleCount == 7);
        assert(diagnostics.stage.addCount == 8);
        assert(diagnostics.stage.keepCount == 9);
        assert(diagnostics.stage.removeCount == 10);
        assert(diagnostics.stage.changedUnsupportedCount == 11);
        assert(diagnostics.queue.queuedSetupCount == 12);
        assert(diagnostics.queue.queuedMakeResidentCount == 13);
        assert(diagnostics.queue.skippedKeepCount == 14);
        assert(diagnostics.queue.skippedChangedCount == 15);
        assert(result.assetResolve.records.size() == sourceResolveRecordCount);
        assert(result.stagePlan.operations.size() == sourceStageOpCount);
    }

    {
        full_engine::TerrainManifestRuntimeStageResult result;
        result.status = full_engine::TerrainManifestRuntimeStageStatus::InvalidManifest;
        assert(
            full_engine::makeTerrainManifestRuntimeStageDiagnostics(result).status ==
            full_engine::TerrainManifestRuntimeStageStatus::InvalidManifest);

        result.status = full_engine::TerrainManifestRuntimeStageStatus::MissingWorldDesc;
        assert(
            full_engine::makeTerrainManifestRuntimeStageDiagnostics(result).status ==
            full_engine::TerrainManifestRuntimeStageStatus::MissingWorldDesc);

        result.status = full_engine::TerrainManifestRuntimeStageStatus::AssetResolveFailed;
        assert(
            full_engine::makeTerrainManifestRuntimeStageDiagnostics(result).status ==
            full_engine::TerrainManifestRuntimeStageStatus::AssetResolveFailed);

        result.status = full_engine::TerrainManifestRuntimeStageStatus::QueueBlocked;
        assert(
            full_engine::makeTerrainManifestRuntimeStageDiagnostics(result).status ==
            full_engine::TerrainManifestRuntimeStageStatus::QueueBlocked);
    }

    {
        full_engine::TerrainChunkRequestApplyResult result;
        result.records.push_back({
            full_engine::TerrainChunkRequest{full_engine::TerrainChunkRequestType::Add, {1, 0, 0}, {}, {}},
            full_engine::TerrainChunkRequestStatus::Applied,
            {},
            {}});
        result.records.push_back({
            full_engine::TerrainChunkRequest{full_engine::TerrainChunkRequestType::Remove, {2, 0, 0}, {}, {}},
            full_engine::TerrainChunkRequestStatus::NotFound,
            {},
            {}});
        result.records.push_back({
            full_engine::TerrainChunkRequest{full_engine::TerrainChunkRequestType::Add, {3, 0, 0}, {}, {}},
            full_engine::TerrainChunkRequestStatus::PartialFailure,
            {},
            {}});
        result.summary.appliedCount = 1;
        result.summary.alreadySatisfiedCount = 2;
        result.summary.notFoundCount = 3;
        result.summary.invalidArgumentCount = 4;
        result.summary.partialFailureCount = 5;

        const std::size_t sourceRecordCount = result.records.size();
        const full_engine::TerrainSetupRequestDiagnostics diagnostics =
            full_engine::makeTerrainSetupRequestDiagnostics(result);

        assert(diagnostics.requestCount == 3);
        assert(diagnostics.addCount == 2);
        assert(diagnostics.removeCount == 1);
        assert(diagnostics.summary.appliedCount == 1);
        assert(diagnostics.summary.alreadySatisfiedCount == 2);
        assert(diagnostics.summary.notFoundCount == 3);
        assert(diagnostics.summary.invalidArgumentCount == 4);
        assert(diagnostics.summary.partialFailureCount == 5);
        assert(result.records.size() == sourceRecordCount);
    }

    {
        full_engine::WorldChunkResidencyApplyResult result;
        result.records.push_back({
            full_engine::WorldChunkResidencyRequest{
                {1, 0, 0},
                full_engine::WorldChunkResidencyRequestType::MakeResident},
            full_engine::WorldChunkResidencyRequestStatus::Applied,
            full_engine::ChunkResidencyState::Resident});
        result.records.push_back({
            full_engine::WorldChunkResidencyRequest{
                {2, 0, 0},
                full_engine::WorldChunkResidencyRequestType::MakeUnloaded},
            full_engine::WorldChunkResidencyRequestStatus::AlreadySatisfied,
            full_engine::ChunkResidencyState::Unloaded});
        result.records.push_back({
            full_engine::WorldChunkResidencyRequest{
                {3, 0, 0},
                full_engine::WorldChunkResidencyRequestType::MakeResident},
            full_engine::WorldChunkResidencyRequestStatus::NotFound,
            full_engine::ChunkResidencyState::Unloaded});
        result.summary.appliedCount = 6;
        result.summary.alreadySatisfiedCount = 7;
        result.summary.notFoundCount = 8;
        result.summary.invalidTransitionCount = 9;

        const std::size_t sourceRecordCount = result.records.size();
        const full_engine::TerrainResidencyRequestDiagnostics diagnostics =
            full_engine::makeTerrainResidencyRequestDiagnostics(result);

        assert(diagnostics.requestCount == 3);
        assert(diagnostics.makeResidentCount == 2);
        assert(diagnostics.makeUnloadedCount == 1);
        assert(diagnostics.summary.appliedCount == 6);
        assert(diagnostics.summary.alreadySatisfiedCount == 7);
        assert(diagnostics.summary.notFoundCount == 8);
        assert(diagnostics.summary.invalidTransitionCount == 9);
        assert(result.records.size() == sourceRecordCount);
    }

    {
        full_engine::WorldRenderSnapshot snapshot;
        snapshot.readyCount = 1;
        snapshot.notResidentCount = 2;
        snapshot.missingChunkCount = 3;
        snapshot.invalidBoundsCount = 4;
        snapshot.outOfRangeCount = 5;
        snapshot.invalidInputCount = 6;
        snapshot.chunks.push_back({});

        full_engine::TerrainRenderPrep prep;
        prep.summary.readyCount = 7;
        prep.summary.skippedNotResidentCount = 8;
        prep.summary.skippedMissingChunkCount = 9;
        prep.summary.skippedInvalidBoundsCount = 10;
        prep.summary.skippedOutOfRangeCount = 11;
        prep.summary.invalidInputCount = 12;
        prep.chunks.push_back({});

        full_engine::TerrainLifecyclePlan lifecycle;
        lifecycle.summary.createCount = 13;
        lifecycle.summary.keepCount = 14;
        lifecycle.summary.updateCount = 15;
        lifecycle.summary.releaseCount = 16;
        lifecycle.operations.push_back({});

        full_engine::TerrainRendererCommandList commands;
        commands.summary.createCount = 17;
        commands.summary.keepCount = 18;
        commands.summary.updateCount = 19;
        commands.summary.destroyCount = 20;
        commands.commands.push_back({});

        full_engine::TerrainDescriptorBuildResult descriptors;
        descriptors.summary.readyCount = 21;
        descriptors.summary.ignoredCount = 22;
        descriptors.summary.missingResourcesCount = 23;
        descriptors.summary.invalidResourcesCount = 24;
        descriptors.intents.push_back({});

        full_engine::TerrainSubmissionResult submission;
        submission.summary.createdCount = 25;
        submission.summary.updatedCount = 26;
        submission.summary.destroyedCount = 27;
        submission.summary.keptCount = 28;
        submission.summary.skippedCount = 29;
        submission.summary.rendererFailedCount = 30;
        submission.summary.handleMapFailedCount = 31;
        submission.operations.push_back({});

        const full_engine::TerrainPipelineDiagnostics diagnostics =
            full_engine::makeTerrainPipelineDiagnostics(
                snapshot,
                prep,
                lifecycle,
                commands,
                descriptors,
                submission,
                32);

        assert(diagnostics.handleCount == 32);
        assert(diagnostics.snapshotReadyCount == 1);
        assert(diagnostics.snapshotNotResidentCount == 2);
        assert(diagnostics.snapshotMissingChunkCount == 3);
        assert(diagnostics.snapshotInvalidBoundsCount == 4);
        assert(diagnostics.snapshotOutOfRangeCount == 5);
        assert(diagnostics.snapshotInvalidInputCount == 6);
        assert(diagnostics.prep.readyCount == 7);
        assert(diagnostics.prep.skippedNotResidentCount == 8);
        assert(diagnostics.prep.skippedMissingChunkCount == 9);
        assert(diagnostics.prep.skippedInvalidBoundsCount == 10);
        assert(diagnostics.prep.skippedOutOfRangeCount == 11);
        assert(diagnostics.prep.invalidInputCount == 12);
        assert(diagnostics.lifecycle.createCount == 13);
        assert(diagnostics.lifecycle.keepCount == 14);
        assert(diagnostics.lifecycle.updateCount == 15);
        assert(diagnostics.lifecycle.releaseCount == 16);
        assert(diagnostics.commands.createCount == 17);
        assert(diagnostics.commands.keepCount == 18);
        assert(diagnostics.commands.updateCount == 19);
        assert(diagnostics.commands.destroyCount == 20);
        assert(diagnostics.descriptors.readyCount == 21);
        assert(diagnostics.descriptors.ignoredCount == 22);
        assert(diagnostics.descriptors.missingResourcesCount == 23);
        assert(diagnostics.descriptors.invalidResourcesCount == 24);
        assert(diagnostics.submission.createdCount == 25);
        assert(diagnostics.submission.updatedCount == 26);
        assert(diagnostics.submission.destroyedCount == 27);
        assert(diagnostics.submission.keptCount == 28);
        assert(diagnostics.submission.skippedCount == 29);
        assert(diagnostics.submission.rendererFailedCount == 30);
        assert(diagnostics.submission.handleMapFailedCount == 31);

        assert(snapshot.chunks.size() == 1);
        assert(prep.chunks.size() == 1);
        assert(lifecycle.operations.size() == 1);
        assert(commands.commands.size() == 1);
        assert(descriptors.intents.size() == 1);
        assert(submission.operations.size() == 1);
    }

    return 0;
}
