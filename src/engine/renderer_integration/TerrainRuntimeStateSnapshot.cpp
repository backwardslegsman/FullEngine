#include "engine/renderer_integration/TerrainRuntimeStateSnapshot.hpp"

namespace full_engine
{
namespace
{
void countReadiness(
    TerrainRuntimeStateSnapshot& snapshot,
    const TerrainRuntimeChunkReadiness readiness) noexcept
{
    switch (readiness)
    {
    case TerrainRuntimeChunkReadiness::Renderable:
        ++snapshot.renderableCount;
        break;
    case TerrainRuntimeChunkReadiness::MissingRegistry:
        ++snapshot.missingRegistryCount;
        break;
    case TerrainRuntimeChunkReadiness::MissingWorldDesc:
        ++snapshot.missingWorldDescCount;
        break;
    case TerrainRuntimeChunkReadiness::MissingResources:
        ++snapshot.missingResourcesCount;
        break;
    case TerrainRuntimeChunkReadiness::NotResident:
        ++snapshot.notResidentCount;
        break;
    case TerrainRuntimeChunkReadiness::MissingTerrainHandle:
        ++snapshot.missingTerrainHandleCount;
        break;
    }
}

TerrainRuntimeChunkReadiness classifyChunk(const TerrainRuntimeChunkState& state) noexcept
{
    if (!state.hasRegistry)
    {
        return TerrainRuntimeChunkReadiness::MissingRegistry;
    }
    if (!state.hasWorldDesc)
    {
        return TerrainRuntimeChunkReadiness::MissingWorldDesc;
    }
    if (!state.hasResources)
    {
        return TerrainRuntimeChunkReadiness::MissingResources;
    }
    if (state.residency != ChunkResidencyState::Resident)
    {
        return TerrainRuntimeChunkReadiness::NotResident;
    }
    if (!state.hasTerrainHandle)
    {
        return TerrainRuntimeChunkReadiness::MissingTerrainHandle;
    }

    return TerrainRuntimeChunkReadiness::Renderable;
}
} // namespace

const char* terrainRuntimeChunkReadinessName(const TerrainRuntimeChunkReadiness readiness) noexcept
{
    switch (readiness)
    {
    case TerrainRuntimeChunkReadiness::Renderable:
        return "Renderable";
    case TerrainRuntimeChunkReadiness::MissingRegistry:
        return "MissingRegistry";
    case TerrainRuntimeChunkReadiness::MissingWorldDesc:
        return "MissingWorldDesc";
    case TerrainRuntimeChunkReadiness::MissingResources:
        return "MissingResources";
    case TerrainRuntimeChunkReadiness::NotResident:
        return "NotResident";
    case TerrainRuntimeChunkReadiness::MissingTerrainHandle:
        return "MissingTerrainHandle";
    }

    return "Unknown";
}

TerrainRuntimeStateSnapshot buildTerrainRuntimeStateSnapshot(
    const WorldChunkRegistry& registry,
    const WorldChunkCatalog& worldCatalog,
    const TerrainResourceCatalog& resources,
    const ChunkTerrainHandleMap& handles,
    const ChunkId* ids,
    const std::size_t idCount)
{
    TerrainRuntimeStateSnapshot snapshot;
    if (ids == nullptr)
    {
        snapshot.invalidInputCount = idCount;
        return snapshot;
    }

    snapshot.chunks.reserve(idCount);
    for (std::size_t index = 0; index < idCount; ++index)
    {
        TerrainRuntimeChunkState state;
        state.id = ids[index];

        const WorldChunkInfo* info = registry.findChunk(state.id);
        state.hasRegistry = info != nullptr;
        if (info != nullptr)
        {
            state.residency = info->residency;
        }
        state.hasWorldDesc = worldCatalog.contains(state.id);
        state.hasResources = resources.contains(state.id);
        state.hasTerrainHandle = handles.contains(state.id);
        state.readiness = classifyChunk(state);

        countReadiness(snapshot, state.readiness);
        snapshot.chunks.push_back(state);
    }

    return snapshot;
}
} // namespace full_engine
