#include "engine/renderer_integration/TerrainChunkSetup.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
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

full_engine::WorldChunkDesc makeWorldDesc(const full_engine::ChunkId& id)
{
    full_engine::WorldChunkDesc desc;
    desc.id = id;
    desc.bounds.min = {0.0, 0.0, 0.0};
    desc.bounds.max = {10.0, 5.0, 10.0};
    return desc;
}

full_engine::TerrainChunkResourceDesc makeResourceDesc(const full_engine::ChunkId& id)
{
    full_engine::TerrainChunkResourceDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = mesh(10);
    desc.lods[0].material = material(20);
    desc.lods[0].maxDistanceMeters = 100.0f;
    return desc;
}

void expectCounts(
    const full_engine::WorldChunkRegistry& registry,
    const full_engine::WorldChunkCatalog& worldCatalog,
    const full_engine::TerrainResourceCatalog& resources,
    const std::size_t expected,
    const char* message,
    std::vector<std::string>& failures)
{
    expect(registry.chunkCount() == expected, message, failures);
    expect(worldCatalog.chunkCount() == expected, message, failures);
    expect(resources.resourceCount() == expected, message, failures);
}

void testAddCreatesAllOwners(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    const full_engine::ChunkId id{1, 0, 0};

    const full_engine::TerrainChunkSetupAddResult result =
        full_engine::addTerrainChunk(registry, worldCatalog, resources, makeWorldDesc(id), makeResourceDesc(id));

    expect(result.result == full_engine::TerrainChunkSetupResult::Success, "terrain add reports success", failures);
    expect(result.worldResult.result == full_engine::WorldChunkSetResult::Success, "terrain add world setup succeeds", failures);
    expect(result.resourceResult == full_engine::TerrainResourceResult::Success, "terrain add resources succeed", failures);
    expect(registry.contains(id), "terrain add creates registry state", failures);
    expect(worldCatalog.contains(id), "terrain add creates world catalog state", failures);
    expect(resources.contains(id), "terrain add creates resource state", failures);
    expectCounts(registry, worldCatalog, resources, 1, "terrain add creates one entry in all owners", failures);
}

void testDuplicateAddPreservesCounts(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    const full_engine::ChunkId id{2, 0, 0};
    (void)full_engine::addTerrainChunk(registry, worldCatalog, resources, makeWorldDesc(id), makeResourceDesc(id));

    const full_engine::TerrainChunkSetupAddResult duplicate =
        full_engine::addTerrainChunk(registry, worldCatalog, resources, makeWorldDesc(id), makeResourceDesc(id));

    expect(
        duplicate.result == full_engine::TerrainChunkSetupResult::AlreadyExists,
        "duplicate terrain add reports already exists",
        failures);
    expectCounts(registry, worldCatalog, resources, 1, "duplicate terrain add preserves counts", failures);
}

void testInvalidAddsMutateNothing(std::vector<std::string>& failures)
{
    {
        full_engine::WorldChunkRegistry registry;
        full_engine::WorldChunkCatalog worldCatalog;
        full_engine::TerrainResourceCatalog resources;
        const full_engine::ChunkId worldId{3, 0, 0};
        const full_engine::ChunkId resourceId{4, 0, 0};

        const full_engine::TerrainChunkSetupAddResult result = full_engine::addTerrainChunk(
            registry,
            worldCatalog,
            resources,
            makeWorldDesc(worldId),
            makeResourceDesc(resourceId));

        expect(result.result == full_engine::TerrainChunkSetupResult::InvalidArgument, "mismatched ids are rejected", failures);
        expectCounts(registry, worldCatalog, resources, 0, "mismatched ids mutate nothing", failures);
    }

    {
        full_engine::WorldChunkRegistry registry;
        full_engine::WorldChunkCatalog worldCatalog;
        full_engine::TerrainResourceCatalog resources;
        const full_engine::ChunkId id{5, 0, 0};
        full_engine::WorldChunkDesc invalidWorld = makeWorldDesc(id);
        invalidWorld.bounds.max.x = std::numeric_limits<double>::infinity();

        const full_engine::TerrainChunkSetupAddResult result =
            full_engine::addTerrainChunk(registry, worldCatalog, resources, invalidWorld, makeResourceDesc(id));

        expect(result.result == full_engine::TerrainChunkSetupResult::InvalidArgument, "invalid world desc is rejected", failures);
        expectCounts(registry, worldCatalog, resources, 0, "invalid world desc mutates nothing", failures);
    }

    {
        full_engine::WorldChunkRegistry registry;
        full_engine::WorldChunkCatalog worldCatalog;
        full_engine::TerrainResourceCatalog resources;
        const full_engine::ChunkId id{6, 0, 0};
        full_engine::TerrainChunkResourceDesc invalidResource = makeResourceDesc(id);
        invalidResource.lods[0].mesh = {};

        const full_engine::TerrainChunkSetupAddResult result =
            full_engine::addTerrainChunk(registry, worldCatalog, resources, makeWorldDesc(id), invalidResource);

        expect(
            result.result == full_engine::TerrainChunkSetupResult::InvalidArgument,
            "invalid terrain resources are rejected",
            failures);
        expectCounts(registry, worldCatalog, resources, 0, "invalid terrain resources mutate nothing", failures);
    }
}

void testResourceFailureRollsBackWorldState(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    const full_engine::ChunkId id{7, 0, 0};
    (void)resources.addChunkResources(makeResourceDesc(id));

    const full_engine::TerrainChunkSetupAddResult result =
        full_engine::addTerrainChunk(registry, worldCatalog, resources, makeWorldDesc(id), makeResourceDesc(id));

    expect(
        result.result == full_engine::TerrainChunkSetupResult::PartialFailure,
        "resource duplicate after world add reports partial failure",
        failures);
    expect(!registry.contains(id), "resource duplicate rollback removes registry state", failures);
    expect(!worldCatalog.contains(id), "resource duplicate rollback removes world catalog state", failures);
    expect(resources.contains(id), "resource duplicate preserves pre-existing resource state", failures);
    expect(registry.chunkCount() == 0, "rollback leaves registry empty", failures);
    expect(worldCatalog.chunkCount() == 0, "rollback leaves world catalog empty", failures);
    expect(resources.resourceCount() == 1, "rollback preserves resource count", failures);
}

void testRemoveDeletesAllOwnersAndMissing(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    const full_engine::ChunkId id{8, 0, 0};
    (void)full_engine::addTerrainChunk(registry, worldCatalog, resources, makeWorldDesc(id), makeResourceDesc(id));

    const full_engine::TerrainChunkSetupRemoveResult removed =
        full_engine::removeTerrainChunk(registry, worldCatalog, resources, id);
    expect(removed.result == full_engine::TerrainChunkSetupResult::Success, "terrain remove reports success", failures);
    expect(!registry.contains(id), "terrain remove deletes registry state", failures);
    expect(!worldCatalog.contains(id), "terrain remove deletes world catalog state", failures);
    expect(!resources.contains(id), "terrain remove deletes resources", failures);
    expectCounts(registry, worldCatalog, resources, 0, "terrain remove clears all owners", failures);

    const full_engine::TerrainChunkSetupRemoveResult missing =
        full_engine::removeTerrainChunk(registry, worldCatalog, resources, id);
    expect(missing.result == full_engine::TerrainChunkSetupResult::NotFound, "missing terrain remove reports not found", failures);
}

void testRemoveRepairsDrift(std::vector<std::string>& failures)
{
    {
        full_engine::WorldChunkRegistry registry;
        full_engine::WorldChunkCatalog worldCatalog;
        full_engine::TerrainResourceCatalog resources;
        const full_engine::ChunkId id{9, 0, 0};
        (void)resources.addChunkResources(makeResourceDesc(id));

        const full_engine::TerrainChunkSetupRemoveResult result =
            full_engine::removeTerrainChunk(registry, worldCatalog, resources, id);
        expect(
            result.result == full_engine::TerrainChunkSetupResult::PartialFailure,
            "resource-only drift remove reports partial failure",
            failures);
        expect(!resources.contains(id), "resource-only drift is repaired", failures);
    }

    {
        full_engine::WorldChunkRegistry registry;
        full_engine::WorldChunkCatalog worldCatalog;
        full_engine::TerrainResourceCatalog resources;
        const full_engine::ChunkId id{10, 0, 0};
        (void)full_engine::addWorldChunk(registry, worldCatalog, makeWorldDesc(id));

        const full_engine::TerrainChunkSetupRemoveResult result =
            full_engine::removeTerrainChunk(registry, worldCatalog, resources, id);
        expect(
            result.result == full_engine::TerrainChunkSetupResult::PartialFailure,
            "world-only drift remove reports partial failure",
            failures);
        expect(!registry.contains(id), "world-only drift removes registry", failures);
        expect(!worldCatalog.contains(id), "world-only drift removes world catalog", failures);
    }

    {
        full_engine::WorldChunkRegistry registry;
        full_engine::WorldChunkCatalog worldCatalog;
        full_engine::TerrainResourceCatalog resources;
        const full_engine::ChunkId id{11, 0, 0};
        (void)worldCatalog.addChunk(makeWorldDesc(id));
        (void)resources.addChunkResources(makeResourceDesc(id));

        const full_engine::TerrainChunkSetupRemoveResult result =
            full_engine::removeTerrainChunk(registry, worldCatalog, resources, id);
        expect(
            result.result == full_engine::TerrainChunkSetupResult::PartialFailure,
            "mixed drift remove reports partial failure",
            failures);
        expect(!registry.contains(id), "mixed drift registry remains empty", failures);
        expect(!worldCatalog.contains(id), "mixed drift removes world catalog", failures);
        expect(!resources.contains(id), "mixed drift removes resources", failures);
    }
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testAddCreatesAllOwners(failures);
    testDuplicateAddPreservesCounts(failures);
    testInvalidAddsMutateNothing(failures);
    testResourceFailureRollsBackWorldState(failures);
    testRemoveDeletesAllOwnersAndMissing(failures);
    testRemoveRepairsDrift(failures);

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
