#include "engine/renderer_integration/TerrainChunkSetup.hpp"

namespace full_engine
{
namespace
{
TerrainChunkSetupResult resultFromWorldAdd(const WorldChunkSetResult result) noexcept
{
    switch (result)
    {
    case WorldChunkSetResult::Success:
        return TerrainChunkSetupResult::Success;
    case WorldChunkSetResult::AlreadyExists:
        return TerrainChunkSetupResult::AlreadyExists;
    case WorldChunkSetResult::InvalidArgument:
        return TerrainChunkSetupResult::InvalidArgument;
    case WorldChunkSetResult::NotFound:
    case WorldChunkSetResult::PartialFailure:
        break;
    }

    return TerrainChunkSetupResult::PartialFailure;
}

TerrainChunkSetupResult resultFromResourceAdd(const TerrainResourceResult result) noexcept
{
    switch (result)
    {
    case TerrainResourceResult::Success:
        return TerrainChunkSetupResult::Success;
    case TerrainResourceResult::AlreadyExists:
        return TerrainChunkSetupResult::AlreadyExists;
    case TerrainResourceResult::InvalidArgument:
        return TerrainChunkSetupResult::InvalidArgument;
    case TerrainResourceResult::NotFound:
        break;
    }

    return TerrainChunkSetupResult::PartialFailure;
}
} // namespace

TerrainChunkSetupAddResult addTerrainChunk(
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resourceCatalog,
    const WorldChunkDesc& worldDesc,
    const TerrainChunkResourceDesc& resourceDesc)
{
    TerrainChunkSetupAddResult result;
    if (!(worldDesc.id == resourceDesc.id) ||
        validateTerrainChunkResources(resourceDesc) != TerrainResourceValidationResult::Success)
    {
        result.result = TerrainChunkSetupResult::InvalidArgument;
        result.worldResult.registryResult = WorldResult::InvalidArgument;
        result.worldResult.catalogResult = WorldResult::InvalidArgument;
        result.resourceResult = TerrainResourceResult::InvalidArgument;
        return result;
    }

    if (registry.contains(worldDesc.id) && worldCatalog.contains(worldDesc.id) && resourceCatalog.contains(worldDesc.id))
    {
        result.result = TerrainChunkSetupResult::AlreadyExists;
        result.worldResult.result = WorldChunkSetResult::AlreadyExists;
        result.worldResult.registryResult = WorldResult::AlreadyExists;
        result.worldResult.catalogResult = WorldResult::AlreadyExists;
        result.resourceResult = TerrainResourceResult::AlreadyExists;
        return result;
    }

    result.worldResult = addWorldChunk(registry, worldCatalog, worldDesc);
    if (result.worldResult.result != WorldChunkSetResult::Success &&
        result.worldResult.result != WorldChunkSetResult::AlreadyExists)
    {
        result.result = resultFromWorldAdd(result.worldResult.result);
        result.resourceResult = resourceCatalog.contains(worldDesc.id) ? TerrainResourceResult::AlreadyExists
                                                                       : TerrainResourceResult::NotFound;
        return result;
    }

    result.resourceResult = resourceCatalog.addChunkResources(resourceDesc);
    if (result.resourceResult == TerrainResourceResult::Success)
    {
        result.result = result.worldResult.result == WorldChunkSetResult::Success ? TerrainChunkSetupResult::Success
                                                                                  : TerrainChunkSetupResult::PartialFailure;
        return result;
    }

    if (result.worldResult.result == WorldChunkSetResult::Success)
    {
        (void)removeWorldChunk(registry, worldCatalog, worldDesc.id);
    }

    result.result = result.resourceResult == TerrainResourceResult::InvalidArgument
        ? resultFromResourceAdd(result.resourceResult)
        : TerrainChunkSetupResult::PartialFailure;
    return result;
}

TerrainChunkSetupRemoveResult removeTerrainChunk(
    WorldChunkRegistry& registry,
    WorldChunkCatalog& worldCatalog,
    TerrainResourceCatalog& resourceCatalog,
    const ChunkId& id)
{
    TerrainChunkSetupRemoveResult result;
    result.resourceResult = resourceCatalog.removeChunkResources(id);
    result.worldResult = removeWorldChunk(registry, worldCatalog, id);

    if (result.resourceResult == TerrainResourceResult::Success &&
        result.worldResult.result == WorldChunkSetResult::Success)
    {
        result.result = TerrainChunkSetupResult::Success;
    }
    else if (
        result.resourceResult == TerrainResourceResult::NotFound &&
        result.worldResult.result == WorldChunkSetResult::NotFound)
    {
        result.result = TerrainChunkSetupResult::NotFound;
    }
    else
    {
        result.result = TerrainChunkSetupResult::PartialFailure;
    }

    return result;
}
} // namespace full_engine
