#include "engine/renderer_integration/TerrainRendererCommands.hpp"

namespace full_engine
{
namespace
{
TerrainRendererCommandType toCommandType(const TerrainLifecycleAction action)
{
    switch (action)
    {
    case TerrainLifecycleAction::Create:
        return TerrainRendererCommandType::CreateChunk;
    case TerrainLifecycleAction::Keep:
        return TerrainRendererCommandType::KeepChunk;
    case TerrainLifecycleAction::Update:
        return TerrainRendererCommandType::UpdateChunk;
    case TerrainLifecycleAction::Release:
        return TerrainRendererCommandType::DestroyChunk;
    }

    return TerrainRendererCommandType::CreateChunk;
}

void countCommand(TerrainRendererCommandSummary& summary, const TerrainRendererCommandType type)
{
    switch (type)
    {
    case TerrainRendererCommandType::CreateChunk:
        ++summary.createCount;
        break;
    case TerrainRendererCommandType::KeepChunk:
        ++summary.keepCount;
        break;
    case TerrainRendererCommandType::UpdateChunk:
        ++summary.updateCount;
        break;
    case TerrainRendererCommandType::DestroyChunk:
        ++summary.destroyCount;
        break;
    }
}
} // namespace

TerrainRendererCommandList buildTerrainRendererCommands(const TerrainLifecyclePlan& plan)
{
    TerrainRendererCommandList commandList;
    commandList.commands.reserve(plan.operations.size());

    for (const TerrainLifecycleOp& operation : plan.operations)
    {
        TerrainRendererCommand command;
        command.id = operation.id;
        command.type = toCommandType(operation.action);
        command.bounds = operation.bounds;
        command.handle = operation.handle;

        countCommand(commandList.summary, command.type);
        commandList.commands.push_back(command);
    }

    return commandList;
}
} // namespace full_engine
