#include "engine/assets/TerrainAssetCatalog.hpp"

#include <cmath>

namespace full_engine
{
TerrainAssetValidationResult validateTerrainChunkAssets(const TerrainChunkAssetDesc& desc)
{
    if (desc.lodCount == 0 || desc.lodCount > kMaxTerrainAssetLodLevels)
    {
        return TerrainAssetValidationResult::InvalidLodCount;
    }

    float previousDistance = 0.0f;
    for (std::uint32_t index = 0; index < desc.lodCount; ++index)
    {
        const TerrainAssetLodRef& lod = desc.lods[index];
        if (!isValid(lod.mesh))
        {
            return TerrainAssetValidationResult::InvalidMeshAsset;
        }
        if (!isValid(lod.material))
        {
            return TerrainAssetValidationResult::InvalidMaterialAsset;
        }
        if (!std::isfinite(lod.maxDistanceMeters) || lod.maxDistanceMeters < 0.0f)
        {
            return TerrainAssetValidationResult::InvalidDistance;
        }
        if (index > 0 && lod.maxDistanceMeters < previousDistance)
        {
            return TerrainAssetValidationResult::UnsortedDistance;
        }
        previousDistance = lod.maxDistanceMeters;
    }

    return TerrainAssetValidationResult::Success;
}

TerrainAssetCatalogResult TerrainAssetCatalog::addChunkAssets(const TerrainChunkAssetDesc& desc)
{
    if (validateTerrainChunkAssets(desc) != TerrainAssetValidationResult::Success)
    {
        return TerrainAssetCatalogResult::InvalidArgument;
    }

    const auto inserted = assets_.emplace(desc.id, desc);
    return inserted.second ? TerrainAssetCatalogResult::Success : TerrainAssetCatalogResult::AlreadyExists;
}

TerrainAssetCatalogResult TerrainAssetCatalog::updateChunkAssets(const TerrainChunkAssetDesc& desc)
{
    if (validateTerrainChunkAssets(desc) != TerrainAssetValidationResult::Success)
    {
        return TerrainAssetCatalogResult::InvalidArgument;
    }

    auto found = assets_.find(desc.id);
    if (found == assets_.end())
    {
        return TerrainAssetCatalogResult::NotFound;
    }

    found->second = desc;
    return TerrainAssetCatalogResult::Success;
}

TerrainAssetCatalogResult TerrainAssetCatalog::removeChunkAssets(const ChunkId& id)
{
    return assets_.erase(id) > 0 ? TerrainAssetCatalogResult::Success : TerrainAssetCatalogResult::NotFound;
}

const TerrainChunkAssetDesc* TerrainAssetCatalog::findChunkAssets(const ChunkId& id) const
{
    const auto found = assets_.find(id);
    return found == assets_.end() ? nullptr : &found->second;
}

bool TerrainAssetCatalog::contains(const ChunkId& id) const
{
    return assets_.find(id) != assets_.end();
}

std::size_t TerrainAssetCatalog::assetCount() const noexcept
{
    return assets_.size();
}

void TerrainAssetCatalog::clear() noexcept
{
    assets_.clear();
}
} // namespace full_engine
