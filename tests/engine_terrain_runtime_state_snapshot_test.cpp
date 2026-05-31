#include "engine/renderer_integration/TerrainRuntimeStateSnapshot.hpp"

#include <cstdlib>
#include <cstdint>
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

full_engine::WorldChunkDesc worldDesc(const full_engine::ChunkId& id)
{
    full_engine::WorldChunkDesc desc;
    desc.id = id;
    desc.bounds.min = {0.0, 0.0, 0.0};
    desc.bounds.max = {1.0, 1.0, 1.0};
    return desc;
}

full_engine::TerrainChunkResourceDesc terrainResources(const full_engine::ChunkId& id)
{
    full_engine::TerrainChunkResourceDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = full_renderer::MeshHandle{10};
    desc.lods[0].material = full_renderer::MaterialHandle{20};
    desc.lods[0].maxDistanceMeters = 100.0f;
    return desc;
}

void makeResident(full_engine::WorldChunkRegistry& registry, const full_engine::ChunkId& id)
{
    (void)registry.setResidencyState(id, full_engine::ChunkResidencyState::Loading);
    (void)registry.setResidencyState(id, full_engine::ChunkResidencyState::Resident);
}

struct Fixture
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog catalog;
    full_engine::TerrainResourceCatalog resources;
    full_engine::ChunkTerrainHandleMap handles;
};

full_engine::TerrainRuntimeStateSnapshot snapshot(Fixture& fixture, const std::vector<full_engine::ChunkId>& ids)
{
    return full_engine::buildTerrainRuntimeStateSnapshot(
        fixture.registry,
        fixture.catalog,
        fixture.resources,
        fixture.handles,
        ids.data(),
        ids.size());
}

void testEmptyAndInvalidInput(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::TerrainRuntimeStateSnapshot empty =
        full_engine::buildTerrainRuntimeStateSnapshot(
            fixture.registry,
            fixture.catalog,
            fixture.resources,
            fixture.handles,
            nullptr,
            0);
    expect(empty.chunks.empty(), "empty null input returns empty snapshot", failures);
    expect(empty.invalidInputCount == 0, "empty null input has zero invalid input count", failures);

    const full_engine::TerrainRuntimeStateSnapshot invalid =
        full_engine::buildTerrainRuntimeStateSnapshot(
            fixture.registry,
            fixture.catalog,
            fixture.resources,
            fixture.handles,
            nullptr,
            3);
    expect(invalid.chunks.empty(), "null non-empty input returns no chunks", failures);
    expect(invalid.invalidInputCount == 3, "null non-empty input counts invalid ids", failures);
}

void testReadinessPrecedence(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId missing{1, 0, 0};
    const full_engine::ChunkId missingWorldDesc{2, 0, 0};
    const full_engine::ChunkId missingResources{3, 0, 0};
    const full_engine::ChunkId notResident{4, 0, 0};
    const full_engine::ChunkId missingHandle{5, 0, 0};
    const full_engine::ChunkId renderable{6, 0, 0};

    (void)fixture.registry.createChunk(missingWorldDesc);

    (void)fixture.registry.createChunk(missingResources);
    (void)fixture.catalog.addChunk(worldDesc(missingResources));

    (void)fixture.registry.createChunk(notResident);
    (void)fixture.catalog.addChunk(worldDesc(notResident));
    (void)fixture.resources.addChunkResources(terrainResources(notResident));

    (void)fixture.registry.createChunk(missingHandle);
    (void)fixture.catalog.addChunk(worldDesc(missingHandle));
    (void)fixture.resources.addChunkResources(terrainResources(missingHandle));
    makeResident(fixture.registry, missingHandle);

    (void)fixture.registry.createChunk(renderable);
    (void)fixture.catalog.addChunk(worldDesc(renderable));
    (void)fixture.resources.addChunkResources(terrainResources(renderable));
    makeResident(fixture.registry, renderable);
    (void)fixture.handles.mapChunk(renderable, full_renderer::TerrainChunkHandle{11, 1});

    const std::vector<full_engine::ChunkId> ids = {
        renderable,
        missingHandle,
        notResident,
        missingResources,
        missingWorldDesc,
        missing};
    const full_engine::TerrainRuntimeStateSnapshot result = snapshot(fixture, ids);

    expect(result.chunks.size() == ids.size(), "snapshot returns one record per id", failures);
    expect(result.chunks[0].id == renderable, "snapshot preserves first input order", failures);
    expect(result.chunks[5].id == missing, "snapshot preserves last input order", failures);
    expect(result.chunks[0].readiness == full_engine::TerrainRuntimeChunkReadiness::Renderable, "resident with handle is renderable", failures);
    expect(result.chunks[1].readiness == full_engine::TerrainRuntimeChunkReadiness::MissingTerrainHandle, "resident without handle reports missing handle", failures);
    expect(result.chunks[2].readiness == full_engine::TerrainRuntimeChunkReadiness::NotResident, "unloaded chunk reports not resident", failures);
    expect(result.chunks[3].readiness == full_engine::TerrainRuntimeChunkReadiness::MissingResources, "missing resources are reported", failures);
    expect(result.chunks[4].readiness == full_engine::TerrainRuntimeChunkReadiness::MissingWorldDesc, "missing world desc is reported", failures);
    expect(result.chunks[5].readiness == full_engine::TerrainRuntimeChunkReadiness::MissingRegistry, "missing registry is reported first", failures);
    expect(result.renderableCount == 1, "renderable count matches", failures);
    expect(result.missingTerrainHandleCount == 1, "missing handle count matches", failures);
    expect(result.notResidentCount == 1, "not resident count matches", failures);
    expect(result.missingResourcesCount == 1, "missing resources count matches", failures);
    expect(result.missingWorldDescCount == 1, "missing world desc count matches", failures);
    expect(result.missingRegistryCount == 1, "missing registry count matches", failures);
}

void testResidencyStatesAreNotResident(std::vector<std::string>& failures)
{
    Fixture fixture;
    const full_engine::ChunkId unloaded{10, 0, 0};
    const full_engine::ChunkId loading{11, 0, 0};
    const full_engine::ChunkId unloading{12, 0, 0};

    for (const full_engine::ChunkId& id : {unloaded, loading, unloading})
    {
        (void)fixture.registry.createChunk(id);
        (void)fixture.catalog.addChunk(worldDesc(id));
        (void)fixture.resources.addChunkResources(terrainResources(id));
        (void)fixture.handles.mapChunk(id, full_renderer::TerrainChunkHandle{static_cast<std::uint32_t>(id.x), 1});
    }
    (void)fixture.registry.setResidencyState(loading, full_engine::ChunkResidencyState::Loading);
    makeResident(fixture.registry, unloading);
    (void)fixture.registry.setResidencyState(unloading, full_engine::ChunkResidencyState::Unloading);

    const std::vector<full_engine::ChunkId> ids = {unloaded, loading, unloading};
    const full_engine::TerrainRuntimeStateSnapshot result = snapshot(fixture, ids);

    expect(result.chunks.size() == 3, "residency snapshot returns three chunks", failures);
    expect(result.notResidentCount == 3, "all non-resident states count as not resident", failures);
    expect(result.chunks[0].residency == full_engine::ChunkResidencyState::Unloaded, "unloaded residency is copied", failures);
    expect(result.chunks[1].residency == full_engine::ChunkResidencyState::Loading, "loading residency is copied", failures);
    expect(result.chunks[2].residency == full_engine::ChunkResidencyState::Unloading, "unloading residency is copied", failures);
}

void testReadinessNames(std::vector<std::string>& failures)
{
    expect(
        std::string(full_engine::terrainRuntimeChunkReadinessName(
            full_engine::TerrainRuntimeChunkReadiness::Renderable)) == "Renderable",
        "renderable name is stable",
        failures);
    expect(
        std::string(full_engine::terrainRuntimeChunkReadinessName(
            full_engine::TerrainRuntimeChunkReadiness::MissingRegistry)) == "MissingRegistry",
        "missing registry name is stable",
        failures);
    expect(
        std::string(full_engine::terrainRuntimeChunkReadinessName(
            full_engine::TerrainRuntimeChunkReadiness::MissingWorldDesc)) == "MissingWorldDesc",
        "missing world desc name is stable",
        failures);
    expect(
        std::string(full_engine::terrainRuntimeChunkReadinessName(
            full_engine::TerrainRuntimeChunkReadiness::MissingResources)) == "MissingResources",
        "missing resources name is stable",
        failures);
    expect(
        std::string(full_engine::terrainRuntimeChunkReadinessName(
            full_engine::TerrainRuntimeChunkReadiness::NotResident)) == "NotResident",
        "not resident name is stable",
        failures);
    expect(
        std::string(full_engine::terrainRuntimeChunkReadinessName(
            full_engine::TerrainRuntimeChunkReadiness::MissingTerrainHandle)) == "MissingTerrainHandle",
        "missing terrain handle name is stable",
        failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testEmptyAndInvalidInput(failures);
    testReadinessPrecedence(failures);
    testResidencyStatesAreNotResident(failures);
    testReadinessNames(failures);

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
