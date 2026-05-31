#include "engine/renderer_integration/TerrainIntegrationDiagnostics.hpp"

#include <cassert>

int main()
{
    {
        const full_engine::TerrainIntegrationDiagnostics empty;
        assert(empty.setupRequests.requestCount == 0);
        assert(empty.residencyRequests.requestCount == 0);
        assert(empty.pipeline.handleCount == 0);
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
