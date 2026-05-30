#include "engine/world/WorldChunkRegistry.hpp"

#include <tuple>

namespace full_engine
{
namespace
{
bool isValidTransition(const ChunkResidencyState from, const ChunkResidencyState to) noexcept
{
    if (from == to)
    {
        return true;
    }

    switch (from)
    {
    case ChunkResidencyState::Unloaded:
        return to == ChunkResidencyState::Loading;
    case ChunkResidencyState::Loading:
        return to == ChunkResidencyState::Resident;
    case ChunkResidencyState::Resident:
        return to == ChunkResidencyState::Unloading;
    case ChunkResidencyState::Unloading:
        return to == ChunkResidencyState::Unloaded;
    }

    return false;
}
} // namespace

bool operator==(const ChunkId& lhs, const ChunkId& rhs) noexcept
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool operator<(const ChunkId& lhs, const ChunkId& rhs) noexcept
{
    return std::tie(lhs.x, lhs.y, lhs.z) < std::tie(rhs.x, rhs.y, rhs.z);
}

WorldResult WorldChunkRegistry::createChunk(const ChunkId& id)
{
    WorldChunkInfo info;
    info.id = id;
    info.residency = ChunkResidencyState::Unloaded;

    const auto inserted = chunks_.emplace(id, info);
    return inserted.second ? WorldResult::Success : WorldResult::AlreadyExists;
}

WorldResult WorldChunkRegistry::removeChunk(const ChunkId& id)
{
    const auto erasedCount = chunks_.erase(id);
    return erasedCount == 1 ? WorldResult::Success : WorldResult::NotFound;
}

WorldResult WorldChunkRegistry::setResidencyState(const ChunkId& id, const ChunkResidencyState state)
{
    auto found = chunks_.find(id);
    if (found == chunks_.end())
    {
        return WorldResult::NotFound;
    }

    if (!isValidTransition(found->second.residency, state))
    {
        return WorldResult::InvalidTransition;
    }

    found->second.residency = state;
    return WorldResult::Success;
}

bool WorldChunkRegistry::contains(const ChunkId& id) const
{
    return chunks_.find(id) != chunks_.end();
}

const WorldChunkInfo* WorldChunkRegistry::findChunk(const ChunkId& id) const
{
    const auto found = chunks_.find(id);
    return found != chunks_.end() ? &found->second : nullptr;
}

std::size_t WorldChunkRegistry::chunkCount() const noexcept
{
    return chunks_.size();
}

void WorldChunkRegistry::clear() noexcept
{
    chunks_.clear();
}
} // namespace full_engine
