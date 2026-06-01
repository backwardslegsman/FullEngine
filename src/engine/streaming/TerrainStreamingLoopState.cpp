#include "engine/streaming/TerrainStreamingLoopState.hpp"

#include "engine/streaming/TerrainStreamingBudgetPolicy.hpp"

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

const TerrainManifestFileReloadPlanResult& TerrainStreamingLoopState::latestManifestReload() const noexcept
{
    return latestManifestReload_;
}

const TerrainManifestAssetLoadJobCoordinatorResult& TerrainStreamingLoopState::latestLoadJobResult() const noexcept
{
    return latestLoadJobResult_;
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
    resetLoadJobDiagnostics();
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
    }
    refreshDiagnostics();
    return latestLoadJobResult_;
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
    tickHistory_.clear();
    latestManifestReload_ = {};
    resetLoadJobDiagnostics();
    resetStreamingUpdate();
    refreshDiagnostics();
}

void TerrainStreamingLoopState::clearManifest()
{
    manifestLoad_.clearManifest();
    streamingRuntime_.clear();
    manifestAssetLoadJobs_.clear();
    tickHistory_.clear();
    latestManifestReload_ = {};
    resetLoadJobDiagnostics();
    resetStreamingUpdate();
    refreshDiagnostics();
}

void TerrainStreamingLoopState::clearJobs() noexcept
{
    manifestAssetLoadJobs_.clear();
    resetLoadJobDiagnostics();
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
    latestDiagnostics_.latestStreamingStatus = latestStreamingUpdate_.status;
    latestDiagnostics_.latestStreamingSummary = latestStreamingUpdate_.summary;
}

void TerrainStreamingLoopState::resetLoadJobDiagnostics() noexcept
{
    latestLoadJobResult_ = {};
    latestLoadJobDiagnostics_ = makeTerrainManifestAssetLoadJobDiagnostics(
        latestLoadJobResult_,
        manifestAssetLoadJobs_);
}

void TerrainStreamingLoopState::resetStreamingUpdate() noexcept
{
    latestStreamingUpdate_ = {};
}
} // namespace full_engine
