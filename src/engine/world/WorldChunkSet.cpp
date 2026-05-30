#include "engine/world/WorldChunkSet.hpp"

namespace full_engine
{
namespace
{
WorldChunkSetResult resultForAddFailure(const WorldResult result) noexcept
{
    switch (result)
    {
    case WorldResult::AlreadyExists:
        return WorldChunkSetResult::AlreadyExists;
    case WorldResult::InvalidArgument:
        return WorldChunkSetResult::InvalidArgument;
    case WorldResult::Success:
    case WorldResult::NotFound:
    case WorldResult::InvalidTransition:
        break;
    }

    return WorldChunkSetResult::PartialFailure;
}
} // namespace

WorldChunkSetAddResult addWorldChunk(
    WorldChunkRegistry& registry,
    WorldChunkCatalog& catalog,
    const WorldChunkDesc& desc)
{
    WorldChunkSetAddResult result;
    if (!isFinite(desc.bounds))
    {
        result.registryResult = WorldResult::InvalidArgument;
        result.catalogResult = WorldResult::InvalidArgument;
        result.result = WorldChunkSetResult::InvalidArgument;
        return result;
    }

    result.registryResult = registry.createChunk(desc.id);
    if (result.registryResult != WorldResult::Success)
    {
        result.result = resultForAddFailure(result.registryResult);
        if (result.registryResult == WorldResult::AlreadyExists)
        {
            result.catalogResult = catalog.contains(desc.id) ? WorldResult::AlreadyExists : WorldResult::NotFound;
            if (result.catalogResult == WorldResult::NotFound)
            {
                result.result = WorldChunkSetResult::PartialFailure;
            }
        }
        return result;
    }

    result.catalogResult = catalog.addChunk(desc);
    if (result.catalogResult != WorldResult::Success)
    {
        (void)registry.removeChunk(desc.id);
        result.result = WorldChunkSetResult::PartialFailure;
        return result;
    }

    result.result = WorldChunkSetResult::Success;
    return result;
}

WorldChunkSetRemoveResult removeWorldChunk(
    WorldChunkRegistry& registry,
    WorldChunkCatalog& catalog,
    const ChunkId& id)
{
    WorldChunkSetRemoveResult result;
    result.catalogResult = catalog.removeChunk(id);
    result.registryResult = registry.removeChunk(id);

    if (result.catalogResult == WorldResult::Success && result.registryResult == WorldResult::Success)
    {
        result.result = WorldChunkSetResult::Success;
    }
    else if (result.catalogResult == WorldResult::NotFound && result.registryResult == WorldResult::NotFound)
    {
        result.result = WorldChunkSetResult::NotFound;
    }
    else
    {
        result.result = WorldChunkSetResult::PartialFailure;
    }

    return result;
}
} // namespace full_engine
