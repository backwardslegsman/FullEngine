#include "engine/renderer_integration/TerrainChunkRequests.hpp"

namespace full_engine
{
namespace
{
TerrainChunkRequestStatus statusFromSetupResult(const TerrainChunkSetupResult result) noexcept
{
    switch (result)
    {
    case TerrainChunkSetupResult::Success:
        return TerrainChunkRequestStatus::Applied;
    case TerrainChunkSetupResult::AlreadyExists:
        return TerrainChunkRequestStatus::AlreadySatisfied;
    case TerrainChunkSetupResult::NotFound:
        return TerrainChunkRequestStatus::NotFound;
    case TerrainChunkSetupResult::InvalidArgument:
        return TerrainChunkRequestStatus::InvalidArgument;
    case TerrainChunkSetupResult::PartialFailure:
        return TerrainChunkRequestStatus::PartialFailure;
    }

    return TerrainChunkRequestStatus::PartialFailure;
}

void countRecord(const TerrainChunkRequestRecord& record, TerrainChunkRequestApplySummary& summary) noexcept
{
    switch (record.status)
    {
    case TerrainChunkRequestStatus::Applied:
        ++summary.appliedCount;
        break;
    case TerrainChunkRequestStatus::AlreadySatisfied:
        ++summary.alreadySatisfiedCount;
        break;
    case TerrainChunkRequestStatus::NotFound:
        ++summary.notFoundCount;
        break;
    case TerrainChunkRequestStatus::InvalidArgument:
        ++summary.invalidArgumentCount;
        break;
    case TerrainChunkRequestStatus::PartialFailure:
        ++summary.partialFailureCount;
        break;
    }
}
} // namespace

void TerrainChunkRequestQueue::push(const TerrainChunkRequest& request)
{
    requests_.push_back(request);
}

void TerrainChunkRequestQueue::pushAdd(
    const WorldChunkDesc& worldDesc,
    const TerrainChunkResourceDesc& resourceDesc)
{
    TerrainChunkRequest request;
    request.type = TerrainChunkRequestType::Add;
    request.id = worldDesc.id;
    request.worldDesc = worldDesc;
    request.resourceDesc = resourceDesc;
    push(request);
}

void TerrainChunkRequestQueue::pushRemove(const ChunkId& id)
{
    TerrainChunkRequest request;
    request.type = TerrainChunkRequestType::Remove;
    request.id = id;
    push(request);
}

std::size_t TerrainChunkRequestQueue::requestCount() const noexcept
{
    return requests_.size();
}

const std::vector<TerrainChunkRequest>& TerrainChunkRequestQueue::requests() const noexcept
{
    return requests_;
}

TerrainChunkRequestApplyResult TerrainChunkRequestQueue::applyTo(
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resourceCatalog) const
{
    TerrainChunkRequestApplyResult result;
    result.records.reserve(requests_.size());

    for (const TerrainChunkRequest& request : requests_)
    {
        TerrainChunkRequestRecord record;
        record.request = request;
        if (request.type == TerrainChunkRequestType::Add)
        {
            record.addResult = addTerrainChunk(registry, worldCatalog, resourceCatalog, request.worldDesc, request.resourceDesc);
            record.status = statusFromSetupResult(record.addResult.result);
        }
        else
        {
            record.removeResult = removeTerrainChunk(registry, worldCatalog, resourceCatalog, request.id);
            record.status = statusFromSetupResult(record.removeResult.result);
        }

        countRecord(record, result.summary);
        result.records.push_back(record);
    }

    return result;
}

void TerrainChunkRequestQueue::clear() noexcept
{
    requests_.clear();
}
} // namespace full_engine
