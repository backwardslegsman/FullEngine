#include "engine/renderer_integration/TerrainSetupStaging.hpp"

#include "engine/renderer_integration/TerrainRuntimeController.hpp"

#include <map>
#include <set>

namespace full_engine
{
namespace
{
bool positionsEqual(const WorldPosition& lhs, const WorldPosition& rhs) noexcept
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool boundsEqual(const WorldBounds& lhs, const WorldBounds& rhs) noexcept
{
    return positionsEqual(lhs.min, rhs.min) && positionsEqual(lhs.max, rhs.max);
}

bool worldDescsEqual(const WorldChunkDesc& lhs, const WorldChunkDesc& rhs) noexcept
{
    return lhs.id == rhs.id && boundsEqual(lhs.bounds, rhs.bounds);
}

bool resourceLodsEqual(const TerrainResourceLod& lhs, const TerrainResourceLod& rhs) noexcept
{
    return lhs.mesh.id == rhs.mesh.id &&
        lhs.material.id == rhs.material.id &&
        lhs.maxDistanceMeters == rhs.maxDistanceMeters;
}

bool resourceDescsEqual(const TerrainChunkResourceDesc& lhs, const TerrainChunkResourceDesc& rhs) noexcept
{
    if (!(lhs.id == rhs.id) ||
        lhs.lodCount != rhs.lodCount ||
        lhs.splatMap.id != rhs.splatMap.id)
    {
        return false;
    }

    for (std::uint32_t index = 0; index < lhs.lodCount; ++index)
    {
        if (!resourceLodsEqual(lhs.lods[index], rhs.lods[index]))
        {
            return false;
        }
    }

    return true;
}

void incrementSummary(TerrainSetupStageSummary& summary, const TerrainSetupStageAction action) noexcept
{
    switch (action)
    {
    case TerrainSetupStageAction::Add:
        ++summary.addCount;
        break;
    case TerrainSetupStageAction::Keep:
        ++summary.keepCount;
        break;
    case TerrainSetupStageAction::Remove:
        ++summary.removeCount;
        break;
    case TerrainSetupStageAction::ChangedUnsupported:
        ++summary.changedUnsupportedCount;
        break;
    }
}

TerrainSetupStageOp makeDesiredOp(
    const TerrainSetupStageDesc& desired,
    const TerrainSetupStageAction action,
    const bool hasRegistry,
    const bool hasWorldDesc,
    const bool hasResources)
{
    TerrainSetupStageOp op;
    op.id = desired.id;
    op.action = action;
    op.worldDesc = desired.worldDesc;
    op.resourceDesc = desired.resourceDesc;
    op.hasRegistry = hasRegistry;
    op.hasWorldDesc = hasWorldDesc;
    op.hasResources = hasResources;
    return op;
}
} // namespace

TerrainSetupStagePlan planTerrainSetupChanges(
    const WorldChunkRegistry& registry,
    const WorldChunkCatalog& worldCatalog,
    const TerrainResourceCatalog& resources,
    const TerrainSetupStageDesc* desired,
    const std::size_t desiredCount)
{
    TerrainSetupStagePlan plan;
    if (desired == nullptr && desiredCount > 0)
    {
        return plan;
    }

    std::set<ChunkId> desiredIds;
    std::set<ChunkId> seenDesiredIds;
    for (std::size_t index = 0; index < desiredCount; ++index)
    {
        const TerrainSetupStageDesc& desiredDesc = desired[index];
        const bool hasRegistry = registry.contains(desiredDesc.id);
        const WorldChunkDesc* const currentWorldDesc = worldCatalog.findChunk(desiredDesc.id);
        const TerrainChunkResourceDesc* const currentResources = resources.findChunkResources(desiredDesc.id);
        const bool hasWorldDesc = currentWorldDesc != nullptr;
        const bool hasResources = currentResources != nullptr;

        TerrainSetupStageAction action = TerrainSetupStageAction::Add;
        if (seenDesiredIds.find(desiredDesc.id) != seenDesiredIds.end())
        {
            action = TerrainSetupStageAction::ChangedUnsupported;
        }
        else
        {
            seenDesiredIds.insert(desiredDesc.id);
            desiredIds.insert(desiredDesc.id);

            if (hasRegistry && hasWorldDesc && hasResources)
            {
                action =
                    worldDescsEqual(*currentWorldDesc, desiredDesc.worldDesc) &&
                        resourceDescsEqual(*currentResources, desiredDesc.resourceDesc)
                    ? TerrainSetupStageAction::Keep
                    : TerrainSetupStageAction::ChangedUnsupported;
            }
        }

        plan.operations.push_back(makeDesiredOp(
            desiredDesc,
            action,
            hasRegistry,
            hasWorldDesc,
            hasResources));
        incrementSummary(plan.summary, action);
    }

    for (const WorldChunkDesc& currentDesc : worldCatalog.descs())
    {
        if (desiredIds.find(currentDesc.id) != desiredIds.end())
        {
            continue;
        }

        TerrainSetupStageOp op;
        op.id = currentDesc.id;
        op.action = TerrainSetupStageAction::Remove;
        op.worldDesc = currentDesc;
        op.hasRegistry = registry.contains(currentDesc.id);
        op.hasWorldDesc = true;
        op.hasResources = resources.contains(currentDesc.id);
        if (const TerrainChunkResourceDesc* const currentResources = resources.findChunkResources(currentDesc.id))
        {
            op.resourceDesc = *currentResources;
        }
        plan.operations.push_back(op);
        incrementSummary(plan.summary, op.action);
    }

    return plan;
}

TerrainChunkRequestQueue buildTerrainChunkRequestsFromStagePlan(const TerrainSetupStagePlan& plan)
{
    TerrainChunkRequestQueue requests;
    for (const TerrainSetupStageOp& op : plan.operations)
    {
        switch (op.action)
        {
        case TerrainSetupStageAction::Add:
            requests.pushAdd(op.worldDesc, op.resourceDesc);
            break;
        case TerrainSetupStageAction::Remove:
            requests.pushRemove(op.id);
            break;
        case TerrainSetupStageAction::Keep:
        case TerrainSetupStageAction::ChangedUnsupported:
            break;
        }
    }

    return requests;
}

TerrainSetupStageQueueApplyResult queueTerrainSetupStagePlan(
    TerrainRuntimeState& runtime,
    const TerrainSetupStagePlan& plan,
    const bool makeAddedChunksResident)
{
    TerrainSetupStageQueueApplyResult result;
    if (plan.summary.changedUnsupportedCount > 0)
    {
        result.result = TerrainSetupStageQueueResult::BlockedUnsupportedChanges;
        result.summary.skippedChangedCount = plan.summary.changedUnsupportedCount;
        return result;
    }

    for (const TerrainSetupStageOp& op : plan.operations)
    {
        switch (op.action)
        {
        case TerrainSetupStageAction::Add:
            runtime.queueSetupAdd(op.worldDesc, op.resourceDesc);
            ++result.summary.queuedSetupCount;
            if (makeAddedChunksResident)
            {
                runtime.queueMakeResident(op.id);
                ++result.summary.queuedMakeResidentCount;
            }
            break;
        case TerrainSetupStageAction::Remove:
            runtime.queueSetupRemove(op.id);
            ++result.summary.queuedSetupCount;
            break;
        case TerrainSetupStageAction::Keep:
            ++result.summary.skippedKeepCount;
            break;
        case TerrainSetupStageAction::ChangedUnsupported:
            break;
        }
    }

    return result;
}
} // namespace full_engine
