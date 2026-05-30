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
    return latestUpdate_;
}
} // namespace full_engine
