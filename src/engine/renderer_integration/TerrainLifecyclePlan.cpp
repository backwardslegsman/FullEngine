#include "engine/renderer_integration/TerrainLifecyclePlan.hpp"

#include <set>

namespace full_engine
{
namespace
{
void countOperation(TerrainLifecyclePlanSummary& summary, const TerrainLifecycleAction action)
{
    switch (action)
    {
    case TerrainLifecycleAction::Create:
        ++summary.createCount;
        break;
    case TerrainLifecycleAction::Keep:
        ++summary.keepCount;
        break;
    case TerrainLifecycleAction::Update:
        ++summary.updateCount;
        break;
    case TerrainLifecycleAction::Release:
        ++summary.releaseCount;
        break;
    }
}

bool withinBudget(const TerrainLifecyclePlanSummary& summary, const TerrainLifecycleAction action, const TerrainLifecyclePlanOptions& options)
{
    switch (action)
    {
    case TerrainLifecycleAction::Create:
        return summary.createCount < options.maxCreateCount;
    case TerrainLifecycleAction::Update:
        return summary.updateCount < options.maxUpdateCount;
    case TerrainLifecycleAction::Release:
        return summary.releaseCount < options.maxReleaseCount;
    case TerrainLifecycleAction::Keep:
        return true;
    }

    return true;
}

void countDeferred(TerrainLifecyclePlanSummary& summary, const TerrainLifecycleAction action)
{
    switch (action)
    {
    case TerrainLifecycleAction::Create:
        ++summary.deferredCreateCount;
        break;
    case TerrainLifecycleAction::Update:
        ++summary.deferredUpdateCount;
        break;
    case TerrainLifecycleAction::Release:
        ++summary.deferredReleaseCount;
        break;
    case TerrainLifecycleAction::Keep:
        break;
    }
}
} // namespace

TerrainLifecyclePlan planTerrainLifecycle(
    const TerrainRenderPrep& prep,
    const ChunkTerrainHandleMap& handles,
    const TerrainLifecyclePlanOptions& options)
{
    TerrainLifecyclePlan plan;
    std::set<ChunkId> readyIds;

    plan.operations.reserve(prep.chunks.size() + handles.mappedCount());

    for (const TerrainChunkRenderPrep& chunk : prep.chunks)
    {
        readyIds.insert(chunk.id);

        TerrainLifecycleOp operation;
        operation.id = chunk.id;
        operation.bounds = chunk.bounds;

        const full_renderer::TerrainChunkHandle* mappedHandle = handles.findHandle(chunk.id);
        if (mappedHandle == nullptr)
        {
            operation.action = TerrainLifecycleAction::Create;
        }
        else
        {
            operation.action = options.updateMappedReadyChunks ?
                TerrainLifecycleAction::Update :
                TerrainLifecycleAction::Keep;
            operation.handle = *mappedHandle;
        }

        if (withinBudget(plan.summary, operation.action, options))
        {
            countOperation(plan.summary, operation.action);
            plan.operations.push_back(operation);
        }
        else
        {
            countDeferred(plan.summary, operation.action);
        }
    }

    for (const ChunkTerrainHandleRecord& record : handles.records())
    {
        if (readyIds.find(record.id) != readyIds.end())
        {
            continue;
        }

        TerrainLifecycleOp operation;
        operation.id = record.id;
        operation.action = TerrainLifecycleAction::Release;
        operation.handle = record.handle;

        if (withinBudget(plan.summary, operation.action, options))
        {
            countOperation(plan.summary, operation.action);
            plan.operations.push_back(operation);
        }
        else
        {
            countDeferred(plan.summary, operation.action);
        }
    }

    return plan;
}
} // namespace full_engine
