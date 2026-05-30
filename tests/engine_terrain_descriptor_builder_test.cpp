#include "engine/renderer_integration/TerrainDescriptorBuilder.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

full_renderer::MeshHandle mesh(const std::uint32_t id)
{
    return full_renderer::MeshHandle{id};
}

full_renderer::MaterialHandle material(const std::uint32_t id)
{
    return full_renderer::MaterialHandle{id};
}

full_renderer::TextureHandle texture(const std::uint32_t id)
{
    return full_renderer::TextureHandle{id};
}

full_renderer::TerrainChunkHandle terrainHandle(const std::uint32_t id, const std::uint32_t generation)
{
    return full_renderer::TerrainChunkHandle{id, generation};
}

full_engine::RenderBounds bounds(const float minX)
{
    full_engine::RenderBounds result;
    result.min = {minX, 1.0f, 2.0f};
    result.max = {minX + 10.0f, 11.0f, 12.0f};
    return result;
}

full_engine::TerrainRendererCommand command(
    const full_engine::ChunkId& id,
    const full_engine::TerrainRendererCommandType type,
    const float minX,
    const full_renderer::TerrainChunkHandle handle = {})
{
    full_engine::TerrainRendererCommand result;
    result.id = id;
    result.type = type;
    result.bounds = bounds(minX);
    result.handle = handle;
    return result;
}

full_engine::TerrainChunkResourceDesc resources(
    const full_engine::ChunkId& id,
    const std::uint32_t lodCount = 1,
    const full_renderer::TextureHandle splatMap = {})
{
    full_engine::TerrainChunkResourceDesc result;
    result.id = id;
    result.lodCount = lodCount;
    result.splatMap = splatMap;
    for (std::uint32_t index = 0; index < lodCount && index < full_renderer::kMaxTerrainLodLevels; ++index)
    {
        result.lods[index].mesh = mesh(100 + index);
        result.lods[index].material = material(200 + index);
        result.lods[index].maxDistanceMeters = static_cast<float>((index + 1) * 50);
    }
    return result;
}

bool sameId(const full_engine::ChunkId& lhs, const full_engine::ChunkId& rhs)
{
    return lhs == rhs;
}

bool sameHandle(const full_renderer::TerrainChunkHandle lhs, const full_renderer::TerrainChunkHandle rhs)
{
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
}

void expectBoundsCopied(
    const full_engine::RenderBounds& source,
    const full_renderer::Aabb& actual,
    std::vector<std::string>& failures)
{
    expect(actual.min[0] == source.min.x, "bounds min x is copied", failures);
    expect(actual.min[1] == source.min.y, "bounds min y is copied", failures);
    expect(actual.min[2] == source.min.z, "bounds min z is copied", failures);
    expect(actual.max[0] == source.max.x, "bounds max x is copied", failures);
    expect(actual.max[1] == source.max.y, "bounds max y is copied", failures);
    expect(actual.max[2] == source.max.z, "bounds max z is copied", failures);
}

void testCreateCommandBuildsReadyDescriptor(std::vector<std::string>& failures)
{
    const full_engine::ChunkId id{1, 0, 0};
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command(id, full_engine::TerrainRendererCommandType::CreateChunk, 10.0f));

    full_engine::TerrainResourceCatalog catalog;
    catalog.addChunkResources(resources(id, 2, texture(77)));

    const full_engine::TerrainDescriptorBuildResult result = full_engine::buildTerrainDescriptors(commands, catalog);

    expect(result.intents.size() == 1, "create command produces one descriptor intent", failures);
    const full_engine::TerrainDescriptorIntent& intent = result.intents[0];
    expect(intent.status == full_engine::TerrainDescriptorBuildStatus::Ready, "create with resources is ready", failures);
    expect(intent.sourceCommand == full_engine::TerrainRendererCommandType::CreateChunk, "source command is create", failures);
    expect(sameId(intent.id, id), "ready descriptor preserves chunk id", failures);
    expect(!full_renderer::isValid(intent.handle), "create descriptor keeps default terrain handle", failures);
    expectBoundsCopied(commands.commands[0].bounds, intent.desc.bounds, failures);
    expect(intent.desc.lodCount == 2, "descriptor copies lod count", failures);
    expect(intent.desc.lods == intent.lods.data(), "descriptor lod pointer references owned storage", failures);
    expect(intent.desc.splatMap.id == 77, "descriptor copies splat map", failures);
    expect(intent.lods[0].mesh.id == 100 && intent.lods[1].mesh.id == 101, "descriptor copies lod mesh handles", failures);
    expect(intent.lods[0].material.id == 200 && intent.lods[1].material.id == 201, "descriptor copies lod material handles", failures);
    expect(intent.lods[0].maxDistanceMeters == 50.0f && intent.lods[1].maxDistanceMeters == 100.0f, "descriptor copies lod distances", failures);
    expect(result.summary.readyCount == 1, "ready summary increments", failures);
}

void testUpdateCommandBuildsReadyDescriptorAndPreservesHandle(std::vector<std::string>& failures)
{
    const full_engine::ChunkId id{2, 0, 0};
    const full_renderer::TerrainChunkHandle existing = terrainHandle(9, 3);
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command(id, full_engine::TerrainRendererCommandType::UpdateChunk, 20.0f, existing));

    full_engine::TerrainResourceCatalog catalog;
    catalog.addChunkResources(resources(id, 1));

    const full_engine::TerrainDescriptorBuildResult result = full_engine::buildTerrainDescriptors(commands, catalog);

    expect(result.intents.size() == 1, "update command produces one descriptor intent", failures);
    const full_engine::TerrainDescriptorIntent& intent = result.intents[0];
    expect(intent.status == full_engine::TerrainDescriptorBuildStatus::Ready, "update with resources is ready", failures);
    expect(intent.sourceCommand == full_engine::TerrainRendererCommandType::UpdateChunk, "source command is update", failures);
    expect(sameHandle(intent.handle, existing), "update descriptor preserves terrain handle", failures);
    expect(intent.desc.lods == intent.lods.data(), "update descriptor lod pointer references owned storage", failures);
}

void testIgnoredCommands(std::vector<std::string>& failures)
{
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command({3, 0, 0}, full_engine::TerrainRendererCommandType::KeepChunk, 30.0f, terrainHandle(3, 1)));
    commands.commands.push_back(command({4, 0, 0}, full_engine::TerrainRendererCommandType::DestroyChunk, 40.0f, terrainHandle(4, 1)));

    const full_engine::TerrainResourceCatalog catalog;
    const full_engine::TerrainDescriptorBuildResult result = full_engine::buildTerrainDescriptors(commands, catalog);

    expect(result.intents.size() == 2, "ignored commands still produce diagnostic intents", failures);
    expect(result.intents[0].status == full_engine::TerrainDescriptorBuildStatus::IgnoredCommand, "keep command is ignored", failures);
    expect(result.intents[1].status == full_engine::TerrainDescriptorBuildStatus::IgnoredCommand, "destroy command is ignored", failures);
    expect(result.intents[0].desc.lods == nullptr && result.intents[0].desc.lodCount == 0, "ignored keep has no descriptor lods", failures);
    expect(result.intents[1].desc.lods == nullptr && result.intents[1].desc.lodCount == 0, "ignored destroy has no descriptor lods", failures);
    expect(result.summary.ignoredCount == 2, "ignored summary increments", failures);
}

void testMissingResources(std::vector<std::string>& failures)
{
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command({5, 0, 0}, full_engine::TerrainRendererCommandType::CreateChunk, 50.0f));

    const full_engine::TerrainResourceCatalog catalog;
    const full_engine::TerrainDescriptorBuildResult result = full_engine::buildTerrainDescriptors(commands, catalog);

    expect(result.intents.size() == 1, "missing resources produces one intent", failures);
    expect(result.intents[0].status == full_engine::TerrainDescriptorBuildStatus::MissingResources, "missing resources are reported", failures);
    expect(result.intents[0].desc.lods == nullptr && result.intents[0].desc.lodCount == 0, "missing resources produce no descriptor lods", failures);
    expect(result.summary.missingResourcesCount == 1, "missing resource summary increments", failures);
}

void testInvalidResourcesAreReported(std::vector<std::string>& failures)
{
    const full_engine::ChunkId id{6, 0, 0};
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command(id, full_engine::TerrainRendererCommandType::CreateChunk, 60.0f));

    full_engine::TerrainResourceCatalog catalog;
    catalog.addChunkResources(resources(id, 1));

    full_engine::TerrainChunkResourceDesc* stored =
        const_cast<full_engine::TerrainChunkResourceDesc*>(catalog.findChunkResources(id));
    stored->lodCount = 0;

    const full_engine::TerrainDescriptorBuildResult result = full_engine::buildTerrainDescriptors(commands, catalog);

    expect(result.intents.size() == 1, "invalid resources produces one intent", failures);
    expect(result.intents[0].status == full_engine::TerrainDescriptorBuildStatus::InvalidResources, "invalid resources are reported defensively", failures);
    expect(result.intents[0].desc.lods == nullptr && result.intents[0].desc.lodCount == 0, "invalid resources produce no descriptor lods", failures);
    expect(result.summary.invalidResourcesCount == 1, "invalid resource summary increments", failures);
}

void testOrderAndSummary(std::vector<std::string>& failures)
{
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command({10, 0, 0}, full_engine::TerrainRendererCommandType::CreateChunk, 10.0f));
    commands.commands.push_back(command({11, 0, 0}, full_engine::TerrainRendererCommandType::KeepChunk, 11.0f, terrainHandle(11, 1)));
    commands.commands.push_back(command({12, 0, 0}, full_engine::TerrainRendererCommandType::UpdateChunk, 12.0f, terrainHandle(12, 1)));
    commands.commands.push_back(command({13, 0, 0}, full_engine::TerrainRendererCommandType::DestroyChunk, 13.0f, terrainHandle(13, 1)));
    commands.commands.push_back(command({14, 0, 0}, full_engine::TerrainRendererCommandType::CreateChunk, 14.0f));

    full_engine::TerrainResourceCatalog catalog;
    catalog.addChunkResources(resources({10, 0, 0}, 1));
    catalog.addChunkResources(resources({12, 0, 0}, 1));

    const full_engine::TerrainDescriptorBuildResult result = full_engine::buildTerrainDescriptors(commands, catalog);

    expect(result.intents.size() == 5, "one intent is produced per command", failures);
    expect(sameId(result.intents[0].id, {10, 0, 0}), "first intent preserves command order", failures);
    expect(sameId(result.intents[1].id, {11, 0, 0}), "second intent preserves command order", failures);
    expect(sameId(result.intents[2].id, {12, 0, 0}), "third intent preserves command order", failures);
    expect(sameId(result.intents[3].id, {13, 0, 0}), "fourth intent preserves command order", failures);
    expect(sameId(result.intents[4].id, {14, 0, 0}), "fifth intent preserves command order", failures);
    expect(result.summary.readyCount == 2, "ready summary count is correct", failures);
    expect(result.summary.ignoredCount == 2, "ignored summary count is correct", failures);
    expect(result.summary.missingResourcesCount == 1, "missing summary count is correct", failures);
}

void testIntentCopyRepairsDescriptorPointer(std::vector<std::string>& failures)
{
    const full_engine::ChunkId id{20, 0, 0};
    full_engine::TerrainRendererCommandList commands;
    commands.commands.push_back(command(id, full_engine::TerrainRendererCommandType::CreateChunk, 20.0f));

    full_engine::TerrainResourceCatalog catalog;
    catalog.addChunkResources(resources(id, 1));

    const full_engine::TerrainDescriptorBuildResult result = full_engine::buildTerrainDescriptors(commands, catalog);
    full_engine::TerrainDescriptorIntent copied = result.intents[0];

    expect(copied.status == full_engine::TerrainDescriptorBuildStatus::Ready, "copied intent remains ready", failures);
    expect(copied.desc.lods == copied.lods.data(), "copied intent repairs descriptor lod pointer", failures);
}

void testEmptyCommands(std::vector<std::string>& failures)
{
    const full_engine::TerrainRendererCommandList commands;
    const full_engine::TerrainResourceCatalog catalog;
    const full_engine::TerrainDescriptorBuildResult result = full_engine::buildTerrainDescriptors(commands, catalog);

    expect(result.intents.empty(), "empty command list produces no descriptor intents", failures);
    expect(result.summary.readyCount == 0, "empty ready count is zero", failures);
    expect(result.summary.ignoredCount == 0, "empty ignored count is zero", failures);
    expect(result.summary.missingResourcesCount == 0, "empty missing count is zero", failures);
    expect(result.summary.invalidResourcesCount == 0, "empty invalid count is zero", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testCreateCommandBuildsReadyDescriptor(failures);
    testUpdateCommandBuildsReadyDescriptorAndPreservesHandle(failures);
    testIgnoredCommands(failures);
    testMissingResources(failures);
    testInvalidResourcesAreReported(failures);
    testOrderAndSummary(failures);
    testIntentCopyRepairsDescriptorPointer(failures);
    testEmptyCommands(failures);

    if (!failures.empty())
    {
        for (const std::string& failure : failures)
        {
            std::cerr << failure << '\n';
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
