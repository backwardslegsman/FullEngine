#include "engine/world/WorldChunkCatalog.hpp"

namespace full_engine
{
namespace
{
bool isValidDesc(const WorldChunkDesc& desc) noexcept
{
    return isFinite(desc.bounds);
}
} // namespace

WorldResult WorldChunkCatalog::addChunk(const WorldChunkDesc& desc)
{
    if (!isValidDesc(desc))
    {
        return WorldResult::InvalidArgument;
    }

    const auto inserted = chunks_.emplace(desc.id, desc);
    return inserted.second ? WorldResult::Success : WorldResult::AlreadyExists;
}

WorldResult WorldChunkCatalog::updateChunk(const WorldChunkDesc& desc)
{
    if (!isValidDesc(desc))
    {
        return WorldResult::InvalidArgument;
    }

    auto found = chunks_.find(desc.id);
    if (found == chunks_.end())
    {
        return WorldResult::NotFound;
    }

    found->second = desc;
    return WorldResult::Success;
}

WorldResult WorldChunkCatalog::removeChunk(const ChunkId& id)
{
    const auto erasedCount = chunks_.erase(id);
    return erasedCount == 1 ? WorldResult::Success : WorldResult::NotFound;
}

const WorldChunkDesc* WorldChunkCatalog::findChunk(const ChunkId& id) const
{
    const auto found = chunks_.find(id);
    return found != chunks_.end() ? &found->second : nullptr;
}

bool WorldChunkCatalog::contains(const ChunkId& id) const
{
    return chunks_.find(id) != chunks_.end();
}

std::size_t WorldChunkCatalog::chunkCount() const noexcept
{
    return chunks_.size();
}

std::vector<WorldChunkDesc> WorldChunkCatalog::descs() const
{
    std::vector<WorldChunkDesc> result;
    result.reserve(chunks_.size());
    for (const auto& entry : chunks_)
    {
        result.push_back(entry.second);
    }
    return result;
}

void WorldChunkCatalog::clear() noexcept
{
    chunks_.clear();
}
} // namespace full_engine
