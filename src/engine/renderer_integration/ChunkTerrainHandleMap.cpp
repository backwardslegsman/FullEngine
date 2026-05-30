#include "engine/renderer_integration/ChunkTerrainHandleMap.hpp"

namespace full_engine
{
ChunkTerrainHandleMapResult ChunkTerrainHandleMap::mapChunk(
    const ChunkId& id,
    const full_renderer::TerrainChunkHandle handle)
{
    if (!full_renderer::isValid(handle))
    {
        return ChunkTerrainHandleMapResult::InvalidHandle;
    }

    const auto result = handles_.emplace(id, handle);
    return result.second ? ChunkTerrainHandleMapResult::Success : ChunkTerrainHandleMapResult::AlreadyMapped;
}

ChunkTerrainHandleMapResult ChunkTerrainHandleMap::updateChunk(
    const ChunkId& id,
    const full_renderer::TerrainChunkHandle handle)
{
    if (!full_renderer::isValid(handle))
    {
        return ChunkTerrainHandleMapResult::InvalidHandle;
    }

    auto existing = handles_.find(id);
    if (existing == handles_.end())
    {
        return ChunkTerrainHandleMapResult::NotFound;
    }

    existing->second = handle;
    return ChunkTerrainHandleMapResult::Success;
}

ChunkTerrainHandleMapResult ChunkTerrainHandleMap::removeChunk(const ChunkId& id)
{
    const auto removed = handles_.erase(id);
    return removed == 0 ? ChunkTerrainHandleMapResult::NotFound : ChunkTerrainHandleMapResult::Success;
}

const full_renderer::TerrainChunkHandle* ChunkTerrainHandleMap::findHandle(const ChunkId& id) const
{
    const auto existing = handles_.find(id);
    if (existing == handles_.end())
    {
        return nullptr;
    }

    return &existing->second;
}

bool ChunkTerrainHandleMap::contains(const ChunkId& id) const
{
    return handles_.find(id) != handles_.end();
}

std::size_t ChunkTerrainHandleMap::mappedCount() const noexcept
{
    return handles_.size();
}

std::vector<ChunkTerrainHandleRecord> ChunkTerrainHandleMap::records() const
{
    std::vector<ChunkTerrainHandleRecord> result;
    result.reserve(handles_.size());

    for (const auto& entry : handles_)
    {
        result.push_back(ChunkTerrainHandleRecord{entry.first, entry.second});
    }

    return result;
}

void ChunkTerrainHandleMap::clear() noexcept
{
    handles_.clear();
}
} // namespace full_engine
