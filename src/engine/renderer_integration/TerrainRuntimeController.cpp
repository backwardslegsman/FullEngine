#include "engine/renderer_integration/TerrainRuntimeController.hpp"

#include <cstddef>
#include <vector>

namespace full_engine
{
namespace
{
bool isTerrainRuntimeChunkRegistered(
    const WorldChunkRegistry& registry,
    const WorldChunkCatalog& worldCatalog,
    const TerrainResourceCatalog& resources,
    const ChunkId& id)
{
    return registry.contains(id) && worldCatalog.contains(id) && resources.contains(id);
}

void countResidencyRecord(
    const WorldChunkResidencyRequestRecord& record,
    WorldChunkResidencyApplySummary& summary) noexcept
{
    switch (record.status)
    {
    case WorldChunkResidencyRequestStatus::Applied:
        ++summary.appliedCount;
        break;
    case WorldChunkResidencyRequestStatus::AlreadySatisfied:
        ++summary.alreadySatisfiedCount;
        break;
    case WorldChunkResidencyRequestStatus::NotFound:
        ++summary.notFoundCount;
        break;
    case WorldChunkResidencyRequestStatus::InvalidTransition:
        ++summary.invalidTransitionCount;
        break;
    }
}
} // namespace

const char* terrainRuntimeUpdateStatusName(const TerrainRuntimeUpdateStatus status) noexcept
{
    switch (status)
    {
    case TerrainRuntimeUpdateStatus::Success:
        return "Success";
    case TerrainRuntimeUpdateStatus::SetupFailed:
        return "SetupFailed";
    case TerrainRuntimeUpdateStatus::ResidencyFailed:
        return "ResidencyFailed";
    case TerrainRuntimeUpdateStatus::PipelineFailed:
        return "PipelineFailed";
    }

    return "Unknown";
}

TerrainRuntimeUpdateResult updateTerrainRuntime(
    full_renderer::IRenderer& renderer,
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resources,
    ChunkTerrainHandleMap& handles,
    TerrainChunkRequestQueue& setupRequests,
    WorldChunkResidencyRequestQueue& residencyRequests,
    const TerrainRuntimeUpdateOptions& options)
{
    TerrainRuntimeUpdateResult result;

    result.setup = setupRequests.applyTo(registry, worldCatalog, resources);
    setupRequests.clear();
    result.diagnostics.setupRequests = makeTerrainSetupRequestDiagnostics(result.setup);
    if (result.setup.summary.invalidArgumentCount > 0)
    {
        result.status = TerrainRuntimeUpdateStatus::SetupFailed;
        return result;
    }

    const std::vector<WorldChunkResidencyRequest> capturedResidencyRequests = residencyRequests.requests();
    residencyRequests.clear();

    WorldChunkResidencyRequestQueue registeredResidencyRequests;
    std::vector<bool> requestWasRegistered;
    requestWasRegistered.reserve(capturedResidencyRequests.size());
    for (const WorldChunkResidencyRequest& request : capturedResidencyRequests)
    {
        const bool registered = isTerrainRuntimeChunkRegistered(registry, worldCatalog, resources, request.id);
        requestWasRegistered.push_back(registered);
        if (registered)
        {
            registeredResidencyRequests.push(request);
        }
    }

    const WorldChunkResidencyApplyResult appliedResidency = registeredResidencyRequests.applyTo(registry);
    std::size_t appliedRecordIndex = 0;
    for (std::size_t requestIndex = 0; requestIndex < capturedResidencyRequests.size(); ++requestIndex)
    {
        WorldChunkResidencyRequestRecord record;
        if (requestWasRegistered[requestIndex])
        {
            record = appliedResidency.records[appliedRecordIndex];
            ++appliedRecordIndex;
        }
        else
        {
            record.request = capturedResidencyRequests[requestIndex];
            record.status = WorldChunkResidencyRequestStatus::NotFound;
            record.finalState = ChunkResidencyState::Unloaded;
        }

        result.residency.records.push_back(record);
        countResidencyRecord(record, result.residency.summary);
    }
    result.diagnostics.residencyRequests = makeTerrainResidencyRequestDiagnostics(result.residency);
    if (result.residency.summary.invalidTransitionCount > 0)
    {
        result.status = TerrainRuntimeUpdateStatus::ResidencyFailed;
        return result;
    }

    result.pipeline = runTerrainPipeline(
        renderer,
        registry,
        worldCatalog,
        resources,
        handles,
        options.pipelineOptions);
    result.diagnostics.pipeline = result.pipeline.diagnostics;
    if (!result.pipeline.succeeded)
    {
        result.status = TerrainRuntimeUpdateStatus::PipelineFailed;
        return result;
    }

    result.status = TerrainRuntimeUpdateStatus::Success;
    return result;
}

void TerrainRuntimeEventLog::append(const TerrainRuntimeUpdateResult& update)
{
    TerrainRuntimeEvent event;
    event.sequence = nextSequence_;
    event.status = update.status;
    event.diagnostics = update.diagnostics;

    events_[nextIndex_] = event;
    nextIndex_ = (nextIndex_ + 1) % kTerrainRuntimeEventLogCapacity;
    if (count_ < kTerrainRuntimeEventLogCapacity)
    {
        ++count_;
    }
    ++nextSequence_;
}

std::vector<TerrainRuntimeEvent> TerrainRuntimeEventLog::events() const
{
    std::vector<TerrainRuntimeEvent> result;
    result.reserve(count_);

    const std::size_t startIndex = count_ == kTerrainRuntimeEventLogCapacity ? nextIndex_ : 0;
    for (std::size_t offset = 0; offset < count_; ++offset)
    {
        result.push_back(events_[(startIndex + offset) % kTerrainRuntimeEventLogCapacity]);
    }
    return result;
}

std::size_t TerrainRuntimeEventLog::eventCount() const noexcept
{
    return count_;
}

const TerrainRuntimeEvent* TerrainRuntimeEventLog::latestEvent() const noexcept
{
    if (count_ == 0)
    {
        return nullptr;
    }

    const std::size_t latestIndex =
        nextIndex_ == 0 ? kTerrainRuntimeEventLogCapacity - 1 : nextIndex_ - 1;
    return &events_[latestIndex];
}

void TerrainRuntimeEventLog::clear() noexcept
{
    nextIndex_ = 0;
    count_ = 0;
    nextSequence_ = 1;
}

void TerrainRuntimeState::queueSetupAdd(
    const WorldChunkDesc& worldDesc,
    const TerrainChunkResourceDesc& resourceDesc)
{
    setupRequests_.pushAdd(worldDesc, resourceDesc);
}

void TerrainRuntimeState::queueSetupRemove(const ChunkId& id)
{
    setupRequests_.pushRemove(id);
}

void TerrainRuntimeState::queueMakeResident(const ChunkId& id)
{
    residencyRequests_.push(id, WorldChunkResidencyRequestType::MakeResident);
}

void TerrainRuntimeState::queueMakeUnloaded(const ChunkId& id)
{
    residencyRequests_.push(id, WorldChunkResidencyRequestType::MakeUnloaded);
}

std::size_t TerrainRuntimeState::setupRequestCount() const noexcept
{
    return setupRequests_.requestCount();
}

std::size_t TerrainRuntimeState::residencyRequestCount() const noexcept
{
    return residencyRequests_.requestCount();
}

bool TerrainRuntimeState::hasPendingRequests() const noexcept
{
    return setupRequestCount() > 0 || residencyRequestCount() > 0;
}

const TerrainRuntimeUpdateResult& TerrainRuntimeState::latestUpdate() const noexcept
{
    return latestUpdate_;
}

const TerrainIntegrationDiagnostics& TerrainRuntimeState::latestDiagnostics() const noexcept
{
    return latestUpdate_.diagnostics;
}

const TerrainRuntimeEventLog& TerrainRuntimeState::eventLog() const noexcept
{
    return eventLog_;
}

std::size_t TerrainRuntimeState::eventCount() const noexcept
{
    return eventLog_.eventCount();
}

std::vector<TerrainRuntimeEvent> TerrainRuntimeState::events() const
{
    return eventLog_.events();
}

const TerrainRuntimeEvent* TerrainRuntimeState::latestEvent() const noexcept
{
    return eventLog_.latestEvent();
}

void TerrainRuntimeState::clearEvents() noexcept
{
    eventLog_.clear();
}

void TerrainRuntimeState::clearRequests() noexcept
{
    setupRequests_.clear();
    residencyRequests_.clear();
}

const TerrainRuntimeUpdateResult& TerrainRuntimeState::update(
    full_renderer::IRenderer& renderer,
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resources,
    ChunkTerrainHandleMap& handles,
    const TerrainRuntimeUpdateOptions& options)
{
    latestUpdate_ = updateTerrainRuntime(
        renderer,
        registry,
        worldCatalog,
        resources,
        handles,
        setupRequests_,
        residencyRequests_,
        options);
    eventLog_.append(latestUpdate_);
    return latestUpdate_;
}
} // namespace full_engine
