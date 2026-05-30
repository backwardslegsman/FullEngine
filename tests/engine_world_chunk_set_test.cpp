#include "engine/world/WorldChunkSet.hpp"

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

full_engine::WorldChunkDesc makeDesc(const full_engine::ChunkId& id)
{
    full_engine::WorldChunkDesc desc;
    desc.id = id;
    desc.bounds.min = {0.0, 0.0, 0.0};
    desc.bounds.max = {10.0, 5.0, 10.0};
    return desc;
}

void expectCounts(
    const full_engine::WorldChunkRegistry& registry,
    const full_engine::WorldChunkCatalog& catalog,
    const std::size_t expected,
    const char* message,
    std::vector<std::string>& failures)
{
    expect(registry.chunkCount() == expected, message, failures);
    expect(catalog.chunkCount() == expected, message, failures);
}

void testAddCreatesBothOwners(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog catalog;
    const full_engine::ChunkId id{1, 0, 0};
    const full_engine::WorldChunkSetAddResult result = full_engine::addWorldChunk(registry, catalog, makeDesc(id));

    expect(result.result == full_engine::WorldChunkSetResult::Success, "add reports success", failures);
    expect(result.registryResult == full_engine::WorldResult::Success, "add registry result succeeds", failures);
    expect(result.catalogResult == full_engine::WorldResult::Success, "add catalog result succeeds", failures);
    expect(registry.contains(id), "add creates registry state", failures);
    expect(catalog.contains(id), "add creates catalog state", failures);
    expectCounts(registry, catalog, 1, "add creates one chunk in both owners", failures);
}

void testDuplicateAndInvalidAdd(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog catalog;
    const full_engine::ChunkId id{2, 0, 0};
    (void)full_engine::addWorldChunk(registry, catalog, makeDesc(id));

    const full_engine::WorldChunkSetAddResult duplicate = full_engine::addWorldChunk(registry, catalog, makeDesc(id));
    expect(duplicate.result == full_engine::WorldChunkSetResult::AlreadyExists, "duplicate add reports already exists", failures);
    expectCounts(registry, catalog, 1, "duplicate add preserves counts", failures);

    full_engine::WorldChunkDesc invalid = makeDesc({3, 0, 0});
    invalid.bounds.max.x = std::numeric_limits<double>::infinity();
    const full_engine::WorldChunkSetAddResult invalidResult = full_engine::addWorldChunk(registry, catalog, invalid);
    expect(invalidResult.result == full_engine::WorldChunkSetResult::InvalidArgument, "invalid add reports invalid argument", failures);
    expect(!registry.contains(invalid.id), "invalid add does not create registry state", failures);
    expect(!catalog.contains(invalid.id), "invalid add does not create catalog state", failures);
    expectCounts(registry, catalog, 1, "invalid add preserves existing counts", failures);
}

void testRemoveDeletesBothOwnersAndMissing(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog catalog;
    const full_engine::ChunkId id{4, 0, 0};
    (void)full_engine::addWorldChunk(registry, catalog, makeDesc(id));

    const full_engine::WorldChunkSetRemoveResult removed = full_engine::removeWorldChunk(registry, catalog, id);
    expect(removed.result == full_engine::WorldChunkSetResult::Success, "remove reports success", failures);
    expect(removed.registryResult == full_engine::WorldResult::Success, "remove registry result succeeds", failures);
    expect(removed.catalogResult == full_engine::WorldResult::Success, "remove catalog result succeeds", failures);
    expect(!registry.contains(id), "remove deletes registry state", failures);
    expect(!catalog.contains(id), "remove deletes catalog state", failures);
    expectCounts(registry, catalog, 0, "remove clears both owners", failures);

    const full_engine::WorldChunkSetRemoveResult missing = full_engine::removeWorldChunk(registry, catalog, id);
    expect(missing.result == full_engine::WorldChunkSetResult::NotFound, "missing remove reports not found", failures);
}

void testRemoveRepairsDrift(std::vector<std::string>& failures)
{
    {
        full_engine::WorldChunkRegistry registry;
        full_engine::WorldChunkCatalog catalog;
        const full_engine::ChunkId id{5, 0, 0};
        (void)catalog.addChunk(makeDesc(id));
        const full_engine::WorldChunkSetRemoveResult result = full_engine::removeWorldChunk(registry, catalog, id);
        expect(result.result == full_engine::WorldChunkSetResult::PartialFailure, "catalog-only remove reports partial failure", failures);
        expect(result.registryResult == full_engine::WorldResult::NotFound, "catalog-only remove reports missing registry", failures);
        expect(result.catalogResult == full_engine::WorldResult::Success, "catalog-only remove deletes catalog", failures);
        expect(!catalog.contains(id), "catalog-only drift is repaired", failures);
    }

    {
        full_engine::WorldChunkRegistry registry;
        full_engine::WorldChunkCatalog catalog;
        const full_engine::ChunkId id{6, 0, 0};
        (void)registry.createChunk(id);
        const full_engine::WorldChunkSetRemoveResult result = full_engine::removeWorldChunk(registry, catalog, id);
        expect(result.result == full_engine::WorldChunkSetResult::PartialFailure, "registry-only remove reports partial failure", failures);
        expect(result.registryResult == full_engine::WorldResult::Success, "registry-only remove deletes registry", failures);
        expect(result.catalogResult == full_engine::WorldResult::NotFound, "registry-only remove reports missing catalog", failures);
        expect(!registry.contains(id), "registry-only drift is repaired", failures);
    }
}

void testAddRollbackOnCatalogFailure(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog catalog;
    const full_engine::ChunkId id{7, 0, 0};
    (void)catalog.addChunk(makeDesc(id));

    const full_engine::WorldChunkSetAddResult result = full_engine::addWorldChunk(registry, catalog, makeDesc(id));
    expect(result.result == full_engine::WorldChunkSetResult::PartialFailure, "catalog failure add reports partial failure", failures);
    expect(result.registryResult == full_engine::WorldResult::Success, "rollback path creates registry before catalog failure", failures);
    expect(result.catalogResult == full_engine::WorldResult::AlreadyExists, "rollback path records catalog duplicate", failures);
    expect(!registry.contains(id), "rollback removes created registry state", failures);
    expect(catalog.contains(id), "rollback preserves pre-existing catalog state", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testAddCreatesBothOwners(failures);
    testDuplicateAndInvalidAdd(failures);
    testRemoveDeletesBothOwnersAndMissing(failures);
    testRemoveRepairsDrift(failures);
    testAddRollbackOnCatalogFailure(failures);

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
