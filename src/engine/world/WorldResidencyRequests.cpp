#include "engine/world/WorldResidencyRequests.hpp"

namespace full_engine
{
namespace
{
ChunkResidencyState targetStateForRequest(const WorldChunkResidencyRequestType type) noexcept
{
    return type == WorldChunkResidencyRequestType::MakeResident ?
        ChunkResidencyState::Resident :
        ChunkResidencyState::Unloaded;
}

bool transition(WorldChunkRegistry& registry, const ChunkId& id, ChunkResidencyState& state, const ChunkResidencyState next)
{
    const WorldResult result = registry.setResidencyState(id, next);
    if (result != WorldResult::Success)
    {
        return false;
    }

    state = next;
    return true;
}

WorldChunkResidencyRequestRecord applyRequest(
    WorldChunkRegistry& registry,
    const WorldChunkResidencyRequest& request)
{
    WorldChunkResidencyRequestRecord record;
    record.request = request;

    const WorldChunkInfo* chunk = registry.findChunk(request.id);
    if (chunk == nullptr)
    {
        record.status = WorldChunkResidencyRequestStatus::NotFound;
        return record;
    }

    ChunkResidencyState state = chunk->residency;
    const ChunkResidencyState targetState = targetStateForRequest(request.type);
    if (state == targetState)
    {
        record.status = WorldChunkResidencyRequestStatus::AlreadySatisfied;
        record.finalState = state;
        return record;
    }

    const auto fail = [&]() {
        record.status = WorldChunkResidencyRequestStatus::InvalidTransition;
        record.finalState = state;
        return record;
    };

    if (request.type == WorldChunkResidencyRequestType::MakeResident)
    {
        if (state == ChunkResidencyState::Unloading &&
            !transition(registry, request.id, state, ChunkResidencyState::Unloaded))
        {
            return fail();
        }
        if (state == ChunkResidencyState::Unloaded &&
            !transition(registry, request.id, state, ChunkResidencyState::Loading))
        {
            return fail();
        }
        if (state == ChunkResidencyState::Loading &&
            !transition(registry, request.id, state, ChunkResidencyState::Resident))
        {
            return fail();
        }
    }
    else
    {
        if (state == ChunkResidencyState::Loading &&
            !transition(registry, request.id, state, ChunkResidencyState::Resident))
        {
            return fail();
        }
        if (state == ChunkResidencyState::Resident &&
            !transition(registry, request.id, state, ChunkResidencyState::Unloading))
        {
            return fail();
        }
        if (state == ChunkResidencyState::Unloading &&
            !transition(registry, request.id, state, ChunkResidencyState::Unloaded))
        {
            return fail();
        }
    }

    record.status = state == targetState ?
        WorldChunkResidencyRequestStatus::Applied :
        WorldChunkResidencyRequestStatus::InvalidTransition;
    record.finalState = state;
    return record;
}

void countRecord(const WorldChunkResidencyRequestRecord& record, WorldChunkResidencyApplySummary& summary) noexcept
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

void WorldChunkResidencyRequestQueue::push(const WorldChunkResidencyRequest& request)
{
    requests_.push_back(request);
}

void WorldChunkResidencyRequestQueue::push(const ChunkId& id, const WorldChunkResidencyRequestType type)
{
    WorldChunkResidencyRequest request;
    request.id = id;
    request.type = type;
    push(request);
}

std::size_t WorldChunkResidencyRequestQueue::requestCount() const noexcept
{
    return requests_.size();
}

const std::vector<WorldChunkResidencyRequest>& WorldChunkResidencyRequestQueue::requests() const noexcept
{
    return requests_;
}

WorldChunkResidencyApplyResult WorldChunkResidencyRequestQueue::applyTo(WorldChunkRegistry& registry) const
{
    WorldChunkResidencyApplyResult result;
    result.records.reserve(requests_.size());

    for (const WorldChunkResidencyRequest& request : requests_)
    {
        WorldChunkResidencyRequestRecord record = applyRequest(registry, request);
        countRecord(record, result.summary);
        result.records.push_back(record);
    }

    return result;
}

void WorldChunkResidencyRequestQueue::clear() noexcept
{
    requests_.clear();
}
} // namespace full_engine
