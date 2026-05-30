#include "engine/renderer_integration/TerrainSubmissionAdapter.hpp"

namespace full_engine
{
namespace
{
void countStatus(TerrainSubmissionSummary& summary, const TerrainSubmissionOpStatus status)
{
    switch (status)
    {
    case TerrainSubmissionOpStatus::Created:
        ++summary.createdCount;
        break;
    case TerrainSubmissionOpStatus::Updated:
        ++summary.updatedCount;
        break;
    case TerrainSubmissionOpStatus::Destroyed:
        ++summary.destroyedCount;
        break;
    case TerrainSubmissionOpStatus::Kept:
        ++summary.keptCount;
        break;
    case TerrainSubmissionOpStatus::Skipped:
        ++summary.skippedCount;
        break;
    case TerrainSubmissionOpStatus::RendererFailed:
        ++summary.rendererFailedCount;
        break;
    case TerrainSubmissionOpStatus::HandleMapFailed:
        ++summary.handleMapFailedCount;
        break;
    }
}

bool sameCommandIdentity(const TerrainDescriptorIntent& descriptor, const TerrainRendererCommand& command)
{
    return descriptor.id == command.id && descriptor.sourceCommand == command.type;
}

const TerrainDescriptorIntent* matchingDescriptorAt(
    const TerrainDescriptorBuildResult& descriptors,
    const TerrainRendererCommand& command,
    const std::size_t index)
{
    if (index >= descriptors.intents.size())
    {
        return nullptr;
    }

    const TerrainDescriptorIntent& descriptor = descriptors.intents[index];
    return sameCommandIdentity(descriptor, command) ? &descriptor : nullptr;
}

TerrainSubmissionOp makeBaseOperation(const TerrainRendererCommand& command)
{
    TerrainSubmissionOp operation;
    operation.id = command.id;
    operation.sourceCommand = command.type;
    operation.handle = command.handle;
    return operation;
}

TerrainSubmissionOp submitCreate(
    full_renderer::IRenderer& renderer,
    const TerrainRendererCommand& command,
    const TerrainDescriptorIntent* descriptor,
    ChunkTerrainHandleMap& handles)
{
    TerrainSubmissionOp operation = makeBaseOperation(command);
    if (descriptor == nullptr || descriptor->status != TerrainDescriptorBuildStatus::Ready)
    {
        operation.status = TerrainSubmissionOpStatus::Skipped;
        return operation;
    }

    const full_renderer::TerrainChunkHandle created = renderer.createTerrainChunk(descriptor->desc);
    operation.handle = created;
    if (!full_renderer::isValid(created))
    {
        operation.status = TerrainSubmissionOpStatus::RendererFailed;
        operation.rendererResult = full_renderer::RendererResult::BackendFailure;
        return operation;
    }

    const ChunkTerrainHandleMapResult mapResult = handles.mapChunk(command.id, created);
    if (mapResult != ChunkTerrainHandleMapResult::Success)
    {
        operation.status = TerrainSubmissionOpStatus::HandleMapFailed;
        return operation;
    }

    operation.status = TerrainSubmissionOpStatus::Created;
    return operation;
}

TerrainSubmissionOp submitUpdate(
    full_renderer::IRenderer& renderer,
    const TerrainRendererCommand& command,
    const TerrainDescriptorIntent* descriptor)
{
    TerrainSubmissionOp operation = makeBaseOperation(command);
    if (descriptor == nullptr ||
        descriptor->status != TerrainDescriptorBuildStatus::Ready ||
        !full_renderer::isValid(command.handle))
    {
        operation.status = TerrainSubmissionOpStatus::Skipped;
        return operation;
    }

    operation.rendererResult = renderer.updateTerrainChunk(command.handle, descriptor->desc);
    if (operation.rendererResult != full_renderer::RendererResult::Success)
    {
        operation.status = TerrainSubmissionOpStatus::RendererFailed;
        return operation;
    }

    operation.status = TerrainSubmissionOpStatus::Updated;
    return operation;
}

TerrainSubmissionOp submitDestroy(
    full_renderer::IRenderer& renderer,
    const TerrainRendererCommand& command,
    ChunkTerrainHandleMap& handles)
{
    TerrainSubmissionOp operation = makeBaseOperation(command);
    if (!full_renderer::isValid(command.handle))
    {
        operation.status = TerrainSubmissionOpStatus::Skipped;
        return operation;
    }

    renderer.destroyTerrainChunk(command.handle);
    const ChunkTerrainHandleMapResult removeResult = handles.removeChunk(command.id);
    if (removeResult != ChunkTerrainHandleMapResult::Success &&
        removeResult != ChunkTerrainHandleMapResult::NotFound)
    {
        operation.status = TerrainSubmissionOpStatus::HandleMapFailed;
        return operation;
    }

    operation.status = TerrainSubmissionOpStatus::Destroyed;
    return operation;
}
} // namespace

TerrainSubmissionResult submitTerrainCommands(
    full_renderer::IRenderer& renderer,
    const TerrainDescriptorBuildResult& descriptors,
    const TerrainRendererCommandList& commands,
    ChunkTerrainHandleMap& handles)
{
    TerrainSubmissionResult result;
    result.operations.reserve(commands.commands.size());

    for (std::size_t index = 0; index < commands.commands.size(); ++index)
    {
        const TerrainRendererCommand& command = commands.commands[index];
        const TerrainDescriptorIntent* descriptor = matchingDescriptorAt(descriptors, command, index);

        TerrainSubmissionOp operation;
        switch (command.type)
        {
        case TerrainRendererCommandType::CreateChunk:
            operation = submitCreate(renderer, command, descriptor, handles);
            break;
        case TerrainRendererCommandType::UpdateChunk:
            operation = submitUpdate(renderer, command, descriptor);
            break;
        case TerrainRendererCommandType::DestroyChunk:
            operation = submitDestroy(renderer, command, handles);
            break;
        case TerrainRendererCommandType::KeepChunk:
            operation = makeBaseOperation(command);
            operation.status = TerrainSubmissionOpStatus::Kept;
            break;
        }

        countStatus(result.summary, operation.status);
        result.operations.push_back(operation);
    }

    return result;
}
} // namespace full_engine
