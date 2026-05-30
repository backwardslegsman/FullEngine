#include "engine/renderer_integration/TerrainDescriptorBuilder.hpp"

#include <utility>

namespace full_engine
{
namespace
{
bool requiresResources(const TerrainRendererCommandType type)
{
    return type == TerrainRendererCommandType::CreateChunk ||
        type == TerrainRendererCommandType::UpdateChunk;
}

void copyBounds(const RenderBounds& source, full_renderer::Aabb& destination)
{
    destination.min[0] = source.min.x;
    destination.min[1] = source.min.y;
    destination.min[2] = source.min.z;
    destination.max[0] = source.max.x;
    destination.max[1] = source.max.y;
    destination.max[2] = source.max.z;
}

void countStatus(TerrainDescriptorBuildSummary& summary, const TerrainDescriptorBuildStatus status)
{
    switch (status)
    {
    case TerrainDescriptorBuildStatus::Ready:
        ++summary.readyCount;
        break;
    case TerrainDescriptorBuildStatus::IgnoredCommand:
        ++summary.ignoredCount;
        break;
    case TerrainDescriptorBuildStatus::MissingResources:
        ++summary.missingResourcesCount;
        break;
    case TerrainDescriptorBuildStatus::InvalidResources:
        ++summary.invalidResourcesCount;
        break;
    }
}
} // namespace

TerrainDescriptorIntent::TerrainDescriptorIntent()
{
    refreshDescriptorPointers();
}

TerrainDescriptorIntent::TerrainDescriptorIntent(const TerrainDescriptorIntent& other)
    : id(other.id)
    , sourceCommand(other.sourceCommand)
    , status(other.status)
    , handle(other.handle)
    , lods(other.lods)
    , desc(other.desc)
{
    refreshDescriptorPointers();
}

TerrainDescriptorIntent& TerrainDescriptorIntent::operator=(const TerrainDescriptorIntent& other)
{
    if (this != &other)
    {
        id = other.id;
        sourceCommand = other.sourceCommand;
        status = other.status;
        handle = other.handle;
        lods = other.lods;
        desc = other.desc;
        refreshDescriptorPointers();
    }

    return *this;
}

TerrainDescriptorIntent::TerrainDescriptorIntent(TerrainDescriptorIntent&& other) noexcept
    : id(other.id)
    , sourceCommand(other.sourceCommand)
    , status(other.status)
    , handle(other.handle)
    , lods(std::move(other.lods))
    , desc(other.desc)
{
    refreshDescriptorPointers();
}

TerrainDescriptorIntent& TerrainDescriptorIntent::operator=(TerrainDescriptorIntent&& other) noexcept
{
    if (this != &other)
    {
        id = other.id;
        sourceCommand = other.sourceCommand;
        status = other.status;
        handle = other.handle;
        lods = std::move(other.lods);
        desc = other.desc;
        refreshDescriptorPointers();
    }

    return *this;
}

void TerrainDescriptorIntent::refreshDescriptorPointers() noexcept
{
    if (status == TerrainDescriptorBuildStatus::Ready && desc.lodCount > 0)
    {
        desc.lods = lods.data();
        return;
    }

    desc.lods = nullptr;
    desc.lodCount = 0;
}

TerrainDescriptorBuildResult buildTerrainDescriptors(
    const TerrainRendererCommandList& commands,
    const TerrainResourceCatalog& resources)
{
    TerrainDescriptorBuildResult result;
    result.intents.reserve(commands.commands.size());

    for (const TerrainRendererCommand& command : commands.commands)
    {
        TerrainDescriptorIntent intent;
        intent.id = command.id;
        intent.sourceCommand = command.type;
        intent.handle = command.handle;

        if (!requiresResources(command.type))
        {
            intent.status = TerrainDescriptorBuildStatus::IgnoredCommand;
            intent.refreshDescriptorPointers();
            countStatus(result.summary, intent.status);
            result.intents.push_back(intent);
            continue;
        }

        const TerrainChunkResourceDesc* resourcesForChunk = resources.findChunkResources(command.id);
        if (resourcesForChunk == nullptr)
        {
            intent.status = TerrainDescriptorBuildStatus::MissingResources;
            intent.refreshDescriptorPointers();
            countStatus(result.summary, intent.status);
            result.intents.push_back(intent);
            continue;
        }

        if (validateTerrainChunkResources(*resourcesForChunk) != TerrainResourceValidationResult::Success)
        {
            intent.status = TerrainDescriptorBuildStatus::InvalidResources;
            intent.refreshDescriptorPointers();
            countStatus(result.summary, intent.status);
            result.intents.push_back(intent);
            continue;
        }

        intent.status = TerrainDescriptorBuildStatus::Ready;
        copyBounds(command.bounds, intent.desc.bounds);
        intent.desc.lodCount = resourcesForChunk->lodCount;
        intent.desc.splatMap = resourcesForChunk->splatMap;
        for (std::uint32_t index = 0; index < resourcesForChunk->lodCount; ++index)
        {
            intent.lods[index].mesh = resourcesForChunk->lods[index].mesh;
            intent.lods[index].material = resourcesForChunk->lods[index].material;
            intent.lods[index].maxDistanceMeters = resourcesForChunk->lods[index].maxDistanceMeters;
        }
        intent.refreshDescriptorPointers();
        countStatus(result.summary, intent.status);
        result.intents.push_back(intent);
    }

    return result;
}
} // namespace full_engine
