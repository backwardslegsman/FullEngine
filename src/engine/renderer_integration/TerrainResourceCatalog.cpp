#include "engine/renderer_integration/TerrainResourceCatalog.hpp"

#include <cmath>

namespace full_engine
{
TerrainResourceValidationResult validateTerrainChunkResources(const TerrainChunkResourceDesc& desc)
{
    if (desc.lodCount == 0 || desc.lodCount > full_renderer::kMaxTerrainLodLevels)
    {
        return TerrainResourceValidationResult::InvalidLodCount;
    }

    float previousDistance = 0.0f;
    for (std::uint32_t index = 0; index < desc.lodCount; ++index)
    {
        const TerrainResourceLod& lod = desc.lods[index];
        if (!full_renderer::isValid(lod.mesh))
        {
            return TerrainResourceValidationResult::InvalidMesh;
        }

        if (!full_renderer::isValid(lod.material))
        {
            return TerrainResourceValidationResult::InvalidMaterial;
        }

        if (!std::isfinite(lod.maxDistanceMeters) || lod.maxDistanceMeters < 0.0f)
        {
            return TerrainResourceValidationResult::InvalidDistance;
        }

        if (index > 0 && lod.maxDistanceMeters < previousDistance)
        {
            return TerrainResourceValidationResult::UnsortedDistance;
        }

        previousDistance = lod.maxDistanceMeters;
    }

    return TerrainResourceValidationResult::Success;
}

TerrainResourceResult TerrainResourceCatalog::addChunkResources(const TerrainChunkResourceDesc& desc)
{
    if (validateTerrainChunkResources(desc) != TerrainResourceValidationResult::Success)
    {
        return TerrainResourceResult::InvalidArgument;
    }

    const auto result = resources_.emplace(desc.id, desc);
    return result.second ? TerrainResourceResult::Success : TerrainResourceResult::AlreadyExists;
}

TerrainResourceResult TerrainResourceCatalog::updateChunkResources(const TerrainChunkResourceDesc& desc)
{
    if (validateTerrainChunkResources(desc) != TerrainResourceValidationResult::Success)
    {
        return TerrainResourceResult::InvalidArgument;
    }

    auto existing = resources_.find(desc.id);
    if (existing == resources_.end())
    {
        return TerrainResourceResult::NotFound;
    }

    existing->second = desc;
    return TerrainResourceResult::Success;
}

TerrainResourceResult TerrainResourceCatalog::removeChunkResources(const ChunkId& id)
{
    const auto removed = resources_.erase(id);
    return removed == 0 ? TerrainResourceResult::NotFound : TerrainResourceResult::Success;
}

const TerrainChunkResourceDesc* TerrainResourceCatalog::findChunkResources(const ChunkId& id) const
{
    const auto existing = resources_.find(id);
    if (existing == resources_.end())
    {
        return nullptr;
    }

    return &existing->second;
}

bool TerrainResourceCatalog::contains(const ChunkId& id) const
{
    return resources_.find(id) != resources_.end();
}

std::size_t TerrainResourceCatalog::resourceCount() const noexcept
{
    return resources_.size();
}

void TerrainResourceCatalog::clear() noexcept
{
    resources_.clear();
}
} // namespace full_engine
