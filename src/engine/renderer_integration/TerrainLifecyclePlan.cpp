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

        countOperation(plan.summary, operation.action);
        plan.operations.push_back(operation);
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

        countOperation(plan.summary, operation.action);
        plan.operations.push_back(operation);
    }

    return plan;
}
} // namespace full_engine
