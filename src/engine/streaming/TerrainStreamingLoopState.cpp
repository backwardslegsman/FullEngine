#include "engine/streaming/TerrainStreamingLoopState.hpp"

#include "engine/streaming/TerrainStreamingBudgetPolicy.hpp"

#include <utility>

namespace full_engine
{
void TerrainStreamingTickHistory::append(const TerrainStreamingTickEvent& event)
{
    TerrainStreamingTickEvent stored = event;
    stored.sequence = nextSequence_;
    events_[nextIndex_] = stored;
    nextIndex_ = (nextIndex_ + 1) % kTerrainStreamingTickHistoryCapacity;
    if (count_ < kTerrainStreamingTickHistoryCapacity)
    {
        ++count_;
    }
    ++nextSequence_;
}

std::vector<TerrainStreamingTickEvent> TerrainStreamingTickHistory::events() const
{
    std::vector<TerrainStreamingTickEvent> result;
    result.reserve(count_);

    const std::size_t startIndex = count_ == kTerrainStreamingTickHistoryCapacity ? nextIndex_ : 0;
    for (std::size_t offset = 0; offset < count_; ++offset)
    {
        result.push_back(events_[(startIndex + offset) % kTerrainStreamingTickHistoryCapacity]);
    }
    return result;
}

std::size_t TerrainStreamingTickHistory::eventCount() const noexcept
{
    return count_;
}

const TerrainStreamingTickEvent* TerrainStreamingTickHistory::latestEvent() const noexcept
{
    if (count_ == 0)
    {
        return nullptr;
    }

    const std::size_t latestIndex =
        nextIndex_ == 0 ? kTerrainStreamingTickHistoryCapacity - 1 : nextIndex_ - 1;
    return &events_[latestIndex];
}

void TerrainStreamingTickHistory::annotateLatestSchedulerDiagnostics(
    const TerrainStreamingTickSchedulerDiagnostics& diagnostics) noexcept
{
    if (count_ == 0)
    {
        return;
    }

    const std::size_t latestIndex =
        nextIndex_ == 0 ? kTerrainStreamingTickHistoryCapacity - 1 : nextIndex_ - 1;
    events_[latestIndex].scheduler = diagnostics;
    if (diagnostics.hasSchedulerDecision)
    {
        events_[latestIndex].budgetProfile = diagnostics.budgetProfile;
    }
}

void TerrainStreamingTickHistory::clear() noexcept
{
    nextIndex_ = 0;
    count_ = 0;
    nextSequence_ = 1;
}

TerrainManifestLoadState& TerrainStreamingLoopState::manifestLoad() noexcept
{
    return manifestLoad_;
}

const TerrainManifestLoadState& TerrainStreamingLoopState::manifestLoad() const noexcept
{
    return manifestLoad_;
}

TerrainStreamingRuntimeState& TerrainStreamingLoopState::streamingRuntime() noexcept
{
    return streamingRuntime_;
}

const TerrainStreamingRuntimeState& TerrainStreamingLoopState::streamingRuntime() const noexcept
{
    return streamingRuntime_;
}

EngineJobQueue& TerrainStreamingLoopState::manifestAssetLoadJobs() noexcept
{
    return manifestAssetLoadJobs_;
}

const EngineJobQueue& TerrainStreamingLoopState::manifestAssetLoadJobs() const noexcept
{
    return manifestAssetLoadJobs_;
}

TerrainManifestAssetLoadService& TerrainStreamingLoopState::manifestAssetLoadService() noexcept
{
    return manifestAssetLoadService_;
}

const TerrainManifestAssetLoadService& TerrainStreamingLoopState::manifestAssetLoadService() const noexcept
{
    return manifestAssetLoadService_;
}

TerrainManifestAssetLoadCompletionInbox& TerrainStreamingLoopState::externalLoadCompletions() noexcept
{
    return externalLoadCompletions_;
}

const TerrainManifestAssetLoadCompletionInbox& TerrainStreamingLoopState::externalLoadCompletions() const noexcept
{
    return externalLoadCompletions_;
}

void TerrainStreamingLoopState::setAssetSources(AssetSourceCatalog sources)
{
    manifestLoad_.setAssetSources(std::move(sources));
    (void)manifestLoad_.planAssetSources();
    refreshDiagnostics();
}

void TerrainStreamingLoopState::clearAssetSources()
{
    manifestLoad_.clearAssetSources();
    refreshDiagnostics();
}

const TerrainManifestFileReloadPlanResult& TerrainStreamingLoopState::latestManifestReload() const noexcept
{
    return latestManifestReload_;
}

const TerrainManifestAssetLoadJobCoordinatorResult& TerrainStreamingLoopState::latestLoadJobResult() const noexcept
{
    return latestLoadJobResult_;
}

const TerrainManifestAssetLoadJobScheduleResult& TerrainStreamingLoopState::latestLoadJobScheduleResult() const noexcept
{
    return latestLoadJobScheduleResult_;
}

const TerrainManifestAssetLoadJobReconcileResult& TerrainStreamingLoopState::latestLoadJobReconcileResult() const noexcept
{
    return latestLoadJobReconcileResult_;
}

const TerrainManifestAssetLoadJobCompletionReconcileResult&
TerrainStreamingLoopState::latestLoadJobCompletionReconcileResult() const noexcept
{
    return latestLoadJobCompletionReconcileResult_;
}

const TerrainManifestAssetLoadJobWorkPacketResult& TerrainStreamingLoopState::latestLoadServiceWorkPackets() const noexcept
{
    return latestLoadServiceWorkPackets_;
}

const TerrainManifestAssetLoadServiceEnqueueResult& TerrainStreamingLoopState::latestLoadServiceEnqueueResult() const noexcept
{
    return latestLoadServiceEnqueueResult_;
}

const TerrainManifestAssetLoadServiceTickResult& TerrainStreamingLoopState::latestLoadServiceTickResult() const noexcept
{
    return latestLoadServiceTickResult_;
}

const TerrainManifestAssetLoadJobCompletionReconcileResult&
TerrainStreamingLoopState::latestLoadServiceCompletionReconcileResult() const noexcept
{
    return latestLoadServiceCompletionReconcileResult_;
}

const TerrainManifestAssetLoadCompletionInboxPublishResult&
TerrainStreamingLoopState::latestExternalCompletionPublishResult() const noexcept
{
    return latestExternalCompletionPublishResult_;
}

const TerrainManifestAssetSourceRequestPlan& TerrainStreamingLoopState::latestAssetSourceRequests() const noexcept
{
    return manifestLoad_.latestSourceRequests();
}

const AssetSourceUploadIntentPlan& TerrainStreamingLoopState::latestAssetSourceUploadIntents() const noexcept
{
    return manifestLoad_.latestSourceUploadIntents();
}

const TerrainManifestAssetSourceRequestPlan& TerrainStreamingLoopState::planAssetSources()
{
    const TerrainManifestAssetSourceRequestPlan& result = manifestLoad_.planAssetSources();
    refreshDiagnostics();
    return result;
}

const AssetSourceUploadIntentPlan& TerrainStreamingLoopState::planAssetSourceUploadIntents()
{
    const AssetSourceUploadIntentPlan& result = manifestLoad_.planAssetSourceUploadIntents();
    refreshDiagnostics();
    return result;
}

const TerrainStreamingManifestUpdateResult& TerrainStreamingLoopState::latestStreamingUpdate() const noexcept
{
    return latestStreamingUpdate_;
}

const TerrainStreamingLoopDiagnostics& TerrainStreamingLoopState::latestDiagnostics() const noexcept
{
    return latestDiagnostics_;
}

const TerrainStreamingTickHistory& TerrainStreamingLoopState::tickHistory() const noexcept
{
    return tickHistory_;
}

std::size_t TerrainStreamingLoopState::tickHistoryCount() const noexcept
{
    return tickHistory_.eventCount();
}

std::vector<TerrainStreamingTickEvent> TerrainStreamingLoopState::tickEvents() const
{
    return tickHistory_.events();
}

const TerrainStreamingTickEvent* TerrainStreamingLoopState::latestTickEvent() const noexcept
{
    return tickHistory_.latestEvent();
}

void TerrainStreamingLoopState::appendTickEvent(const TerrainStreamingTickEvent& event)
{
    tickHistory_.append(event);
    refreshDiagnostics();
}

void TerrainStreamingLoopState::annotateLatestTickSchedulerDiagnostics(
    const TerrainStreamingTickSchedulerDiagnostics& diagnostics) noexcept
{
    tickHistory_.annotateLatestSchedulerDiagnostics(diagnostics);
    refreshDiagnostics();
}

void TerrainStreamingLoopState::clearTickHistory() noexcept
{
    tickHistory_.clear();
    refreshDiagnostics();
}

const TerrainManifestFileReloadPlanResult& TerrainStreamingLoopState::reloadManifestAndQueueMissingAssetLoads(
    const char* const path,
    const RendererAssetHandleCatalog& handles)
{
    latestManifestReload_ = reloadTerrainManifestFileAndQueueMissingAssetLoads(path, manifestLoad_, handles);
    manifestAssetLoadJobs_.clear();
    manifestAssetLoadService_.clear();
    externalLoadCompletions_.clear();
    resetLoadJobDiagnostics();
    resetLoadServiceDiagnostics();
    streamingRuntime_.clear();
    resetStreamingUpdate();
    refreshDiagnostics();
    return latestManifestReload_;
}

const TerrainManifestAssetLoadJobCoordinatorResult& TerrainStreamingLoopState::runAssetLoadJobs(
    RendererAssetHandleCatalog& destinationHandles,
    const TerrainManifestAssetLoadCallback callback,
    void* const userData,
    const std::size_t maxJobs,
    const EngineJobPriority priority)
{
    latestLoadJobResult_ = runTerrainManifestAssetLoadJobs(
        manifestLoad_,
        manifestAssetLoadJobs_,
        destinationHandles,
        callback,
        userData,
        maxJobs,
        priority);
    latestLoadJobDiagnostics_ = makeTerrainManifestAssetLoadJobDiagnostics(
        latestLoadJobResult_,
        manifestAssetLoadJobs_);
    if (latestLoadJobResult_.status == TerrainManifestAssetLoadJobCoordinatorStatus::Success)
    {
        (void)manifestLoad_.planAssetLoadRequests();
        manifestAssetLoadService_.clear();
        externalLoadCompletions_.clear();
        resetLoadServiceDiagnostics();
        latestExternalCompletionPublishResult_ = {};
        latestExternalCompletionPublishRecord_ = {};
    }
    refreshDiagnostics();
    return latestLoadJobResult_;
}

const TerrainManifestAssetLoadJobScheduleResult& TerrainStreamingLoopState::scheduleAssetLoadJobs(
    const EngineJobPriority priority)
{
    latestLoadJobScheduleResult_ = scheduleTerrainManifestAssetLoadJobs(
        manifestLoad_,
        manifestAssetLoadJobs_,
        priority);
    latestLoadJobScheduleDiagnostics_ = makeTerrainManifestAssetLoadJobScheduleDiagnostics(
        latestLoadJobScheduleResult_,
        manifestAssetLoadJobs_);
    refreshDiagnostics();
    return latestLoadJobScheduleResult_;
}

const TerrainManifestAssetLoadServiceEnqueueResult& TerrainStreamingLoopState::enqueueScheduledAssetLoadWork()
{
    latestLoadServiceWorkPackets_ = buildTerrainManifestAssetLoadJobWorkPackets(manifestAssetLoadJobs_);
    latestLoadServiceEnqueueResult_ = manifestAssetLoadService_.enqueueWorkPackets(
        latestLoadServiceWorkPackets_);
    (void)manifestLoad_.planAssetSources();
    refreshDiagnostics();
    return latestLoadServiceEnqueueResult_;
}

const TerrainManifestAssetLoadServiceTickResult& TerrainStreamingLoopState::tickAssetLoadService(
    const std::size_t maxLoads,
    const TerrainManifestAssetLoadCallback callback,
    void* const userData)
{
    (void)manifestLoad_.planAssetSources();
    latestLoadServiceTickResult_ = manifestAssetLoadService_.tick(maxLoads, callback, userData);
    refreshDiagnostics();
    return latestLoadServiceTickResult_;
}

const TerrainManifestAssetLoadJobCompletionReconcileResult&
TerrainStreamingLoopState::reconcileAssetLoadServiceCompletions(
    RendererAssetHandleCatalog& destinationHandles)
{
    latestLoadServiceCompletionReconcileResult_ =
        reconcileTerrainManifestAssetLoadJobCompletions(
            manifestLoad_,
            manifestAssetLoadJobs_,
            manifestAssetLoadService_.completions().data(),
            manifestAssetLoadService_.completions().size(),
            destinationHandles);
    latestLoadJobReconcileResult_ = latestLoadServiceCompletionReconcileResult_.reconcile;
    latestLoadJobReconcileDiagnostics_ = makeTerrainManifestAssetLoadJobReconcileDiagnostics(
        latestLoadJobReconcileResult_,
        manifestAssetLoadJobs_);
    if (latestLoadServiceCompletionReconcileResult_.status ==
        TerrainManifestAssetLoadJobCompletionReconcileStatus::Success)
    {
        manifestAssetLoadService_.clearCompletions();
        (void)manifestLoad_.planAssetLoadRequests();
        externalLoadCompletions_.clear();
        latestExternalCompletionPublishResult_ = {};
        latestExternalCompletionPublishRecord_ = {};
    }
    refreshDiagnostics();
    return latestLoadServiceCompletionReconcileResult_;
}

const TerrainManifestAssetLoadJobReconcileResult& TerrainStreamingLoopState::reconcileScheduledAssetLoadJobs(
    const RendererAssetHandleCatalog& completedHandles,
    RendererAssetHandleCatalog& destinationHandles)
{
    latestLoadJobReconcileResult_ = reconcileTerrainManifestAssetLoadJobs(
        manifestLoad_,
        manifestAssetLoadJobs_,
        completedHandles,
        destinationHandles);
    latestLoadJobReconcileDiagnostics_ = makeTerrainManifestAssetLoadJobReconcileDiagnostics(
        latestLoadJobReconcileResult_,
        manifestAssetLoadJobs_);
    if (latestLoadJobReconcileResult_.status == TerrainManifestAssetLoadJobReconcileStatus::Success)
    {
        (void)manifestLoad_.planAssetLoadRequests();
        manifestAssetLoadService_.clear();
        externalLoadCompletions_.clear();
        resetLoadServiceDiagnostics();
        latestExternalCompletionPublishResult_ = {};
        latestExternalCompletionPublishRecord_ = {};
    }
    refreshDiagnostics();
    return latestLoadJobReconcileResult_;
}

const TerrainManifestAssetLoadJobCompletionReconcileResult&
TerrainStreamingLoopState::reconcileScheduledAssetLoadCompletions(
    const TerrainManifestAssetLoadJobCompletion* const completions,
    const std::size_t completionCount,
    RendererAssetHandleCatalog& destinationHandles)
{
    latestLoadJobCompletionReconcileResult_ = reconcileTerrainManifestAssetLoadJobCompletions(
        manifestLoad_,
        manifestAssetLoadJobs_,
        completions,
        completionCount,
        destinationHandles);
    latestLoadJobReconcileResult_ = latestLoadJobCompletionReconcileResult_.reconcile;
    latestLoadJobReconcileDiagnostics_ = makeTerrainManifestAssetLoadJobReconcileDiagnostics(
        latestLoadJobReconcileResult_,
        manifestAssetLoadJobs_);
    if (latestLoadJobCompletionReconcileResult_.status ==
        TerrainManifestAssetLoadJobCompletionReconcileStatus::Success)
    {
        (void)manifestLoad_.planAssetLoadRequests();
    }
    refreshDiagnostics();
    return latestLoadJobCompletionReconcileResult_;
}

const TerrainManifestAssetLoadCompletionInboxRecord&
TerrainStreamingLoopState::publishExternalAssetLoadCompletion(
    const TerrainManifestAssetLoadJobCompletion& completion)
{
    const TerrainManifestAssetLoadWorkerCompletionPublishResult published =
        publishTerrainManifestAssetLoadWorkerCompletion(externalLoadCompletions_, completion);
    latestExternalCompletionPublishResult_ = published.publish;
    latestExternalCompletionPublishRecord_ =
        latestExternalCompletionPublishResult_.records.empty() ?
        TerrainManifestAssetLoadCompletionInboxRecord{} :
        latestExternalCompletionPublishResult_.records.back();
    refreshDiagnostics();
    return latestExternalCompletionPublishRecord_;
}

const TerrainManifestAssetLoadCompletionInboxPublishResult&
TerrainStreamingLoopState::publishExternalAssetLoadCompletions(
    const TerrainManifestAssetLoadJobCompletion* const completions,
    const std::size_t completionCount)
{
    const TerrainManifestAssetLoadWorkerCompletionPublishResult published =
        publishTerrainManifestAssetLoadWorkerCompletions(
            externalLoadCompletions_,
            completions,
            completionCount);
    latestExternalCompletionPublishResult_ = published.publish;
    latestExternalCompletionPublishRecord_ =
        latestExternalCompletionPublishResult_.records.empty() ?
        TerrainManifestAssetLoadCompletionInboxRecord{} :
        latestExternalCompletionPublishResult_.records.back();
    refreshDiagnostics();
    return latestExternalCompletionPublishResult_;
}

void TerrainStreamingLoopState::recordExternalAssetLoadCompletionPublish(
    const TerrainManifestAssetLoadWorkerCompletionPublishResult& result) noexcept
{
    latestExternalCompletionPublishResult_ = result.publish;
    latestExternalCompletionPublishRecord_ =
        latestExternalCompletionPublishResult_.records.empty() ?
        TerrainManifestAssetLoadCompletionInboxRecord{} :
        latestExternalCompletionPublishResult_.records.back();
    refreshDiagnostics();
}

void TerrainStreamingLoopState::clearExternalAssetLoadCompletions() noexcept
{
    externalLoadCompletions_.clear();
    latestExternalCompletionPublishResult_ = {};
    latestExternalCompletionPublishRecord_ = {};
    refreshDiagnostics();
}

const TerrainManifestAssetLoadJobCompletionReconcileResult&
TerrainStreamingLoopState::reconcileExternalAssetLoadCompletions(
    RendererAssetHandleCatalog& destinationHandles)
{
    const std::vector<TerrainManifestAssetLoadJobCompletion>& completions =
        externalLoadCompletions_.completions();
    latestLoadJobCompletionReconcileResult_ = reconcileTerrainManifestAssetLoadJobCompletions(
        manifestLoad_,
        manifestAssetLoadJobs_,
        completions.data(),
        completions.size(),
        destinationHandles);
    latestLoadJobReconcileResult_ = latestLoadJobCompletionReconcileResult_.reconcile;
    latestLoadJobReconcileDiagnostics_ = makeTerrainManifestAssetLoadJobReconcileDiagnostics(
        latestLoadJobReconcileResult_,
        manifestAssetLoadJobs_);
    if (latestLoadJobCompletionReconcileResult_.status ==
            TerrainManifestAssetLoadJobCompletionReconcileStatus::Success ||
        latestLoadJobCompletionReconcileResult_.status ==
            TerrainManifestAssetLoadJobCompletionReconcileStatus::NoPendingLoads)
    {
        externalLoadCompletions_.clear();
        latestExternalCompletionPublishResult_ = {};
        latestExternalCompletionPublishRecord_ = {};
        (void)manifestLoad_.planAssetLoadRequests();
    }
    refreshDiagnostics();
    return latestLoadJobCompletionReconcileResult_;
}

const TerrainStreamingManifestUpdateResult& TerrainStreamingLoopState::updateStreamingFromManifest(
    const RendererAssetHandleCatalog& handles,
    const WorldChunkRegistry& registry,
    const WorldChunkCatalog& worldCatalog,
    const TerrainResourceCatalog& resources,
    const WorldChunkDesc* const worldDescs,
    const std::size_t worldDescCount,
    TerrainRuntimeState& runtime,
    const TerrainStreamingPlannerConfig& streamingConfig,
    const WorldPosition& cameraWorld,
    const TerrainRuntimeStateSnapshot& currentSnapshot,
    const TerrainStreamingManifestUpdateOptions& options)
{
    latestStreamingUpdate_ = updateTerrainStreamingFromManifest(
        manifestLoad_,
        handles,
        registry,
        worldCatalog,
        resources,
        worldDescs,
        worldDescCount,
        streamingRuntime_,
        runtime,
        streamingConfig,
        cameraWorld,
        currentSnapshot,
        options);
    refreshDiagnostics();
    return latestStreamingUpdate_;
}

void TerrainStreamingLoopState::clear() noexcept
{
    manifestLoad_.clearManifest();
    streamingRuntime_.clear();
    manifestAssetLoadJobs_.clear();
    manifestAssetLoadService_.clear();
    externalLoadCompletions_.clear();
    tickHistory_.clear();
    latestManifestReload_ = {};
    resetLoadJobDiagnostics();
    resetLoadServiceDiagnostics();
    resetStreamingUpdate();
    refreshDiagnostics();
}

void TerrainStreamingLoopState::clearManifest()
{
    manifestLoad_.clearManifest();
    streamingRuntime_.clear();
    manifestAssetLoadJobs_.clear();
    manifestAssetLoadService_.clear();
    externalLoadCompletions_.clear();
    tickHistory_.clear();
    latestManifestReload_ = {};
    resetLoadJobDiagnostics();
    resetLoadServiceDiagnostics();
    resetStreamingUpdate();
    refreshDiagnostics();
}

void TerrainStreamingLoopState::clearJobs() noexcept
{
    manifestAssetLoadJobs_.clear();
    manifestAssetLoadService_.clear();
    externalLoadCompletions_.clear();
    resetLoadJobDiagnostics();
    resetLoadServiceDiagnostics();
    latestExternalCompletionPublishResult_ = {};
    latestExternalCompletionPublishRecord_ = {};
    refreshDiagnostics();
}

void TerrainStreamingLoopState::refreshDiagnostics() noexcept
{
    latestDiagnostics_.hasManifest = manifestLoad_.hasManifest();
    latestDiagnostics_.pendingLoadRequestCount = manifestLoad_.pendingLoadRequestCount();
    latestDiagnostics_.pendingJobCount = manifestAssetLoadJobs_.jobCount();
    latestDiagnostics_.tickHistoryCount = tickHistory_.eventCount();
    latestDiagnostics_.adaptiveBudget =
        selectAdaptiveTerrainStreamingBudgetProfile(tickHistory_);
    latestDiagnostics_.latestFileLoadStatus = latestManifestReload_.load.status;
    latestDiagnostics_.latestFileAssetCount = latestManifestReload_.load.assetCount;
    latestDiagnostics_.latestFileTerrainChunkCount = latestManifestReload_.load.terrainChunkCount;
    latestDiagnostics_.loadJobs = latestLoadJobDiagnostics_;
    latestDiagnostics_.scheduledLoadJobs = latestLoadJobScheduleDiagnostics_;
    latestDiagnostics_.reconciledLoadJobs = latestLoadJobReconcileDiagnostics_;
    latestDiagnostics_.loadService = makeTerrainManifestAssetLoadServiceDiagnostics(
        latestLoadServiceWorkPackets_,
        latestLoadServiceEnqueueResult_,
        latestLoadServiceTickResult_,
        latestLoadServiceCompletionReconcileResult_,
        manifestAssetLoadService_,
        manifestLoad_.latestSourceRequests(),
        manifestLoad_.latestSourceUploadIntents());
    latestDiagnostics_.assetSources = makeTerrainManifestAssetSourceDiagnostics(
        manifestLoad_.assetSources(),
        manifestLoad_.latestSourceRequests());
    latestDiagnostics_.assetSourceUploadIntents = makeAssetSourceUploadIntentDiagnostics(
        manifestLoad_.latestSourceUploadIntents());
    latestDiagnostics_.pendingExternalCompletionCount = externalLoadCompletions_.completionCount();
    latestDiagnostics_.externalCompletionPublish = latestExternalCompletionPublishResult_.summary;
    latestDiagnostics_.latestStreamingStatus = latestStreamingUpdate_.status;
    latestDiagnostics_.latestStreamingSummary = latestStreamingUpdate_.summary;
}

void TerrainStreamingLoopState::resetLoadJobDiagnostics() noexcept
{
    latestLoadJobResult_ = {};
    latestLoadJobDiagnostics_ = makeTerrainManifestAssetLoadJobDiagnostics(
        latestLoadJobResult_,
        manifestAssetLoadJobs_);
    latestLoadJobScheduleResult_ = {};
    latestLoadJobScheduleDiagnostics_ = makeTerrainManifestAssetLoadJobScheduleDiagnostics(
        latestLoadJobScheduleResult_,
        manifestAssetLoadJobs_);
    latestLoadJobReconcileResult_ = {};
    latestLoadJobReconcileDiagnostics_ = makeTerrainManifestAssetLoadJobReconcileDiagnostics(
        latestLoadJobReconcileResult_,
        manifestAssetLoadJobs_);
    latestLoadJobCompletionReconcileResult_ = {};
    latestExternalCompletionPublishResult_ = {};
    latestExternalCompletionPublishRecord_ = {};
}

void TerrainStreamingLoopState::resetLoadServiceDiagnostics() noexcept
{
    latestLoadServiceWorkPackets_ = {};
    latestLoadServiceEnqueueResult_ = {};
    latestLoadServiceTickResult_ = {};
    latestLoadServiceCompletionReconcileResult_ = {};
}

void TerrainStreamingLoopState::resetStreamingUpdate() noexcept
{
    latestStreamingUpdate_ = {};
}
} // namespace full_engine
