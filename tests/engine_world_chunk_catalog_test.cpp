#include "engine/world/WorldChunkCatalog.hpp"

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

void expectResult(
    const full_engine::WorldResult actual,
    const full_engine::WorldResult expected,
    const char* message,
    std::vector<std::string>& failures)
{
    expect(actual == expected, message, failures);
}

full_engine::WorldChunkDesc makeDesc(
    const full_engine::ChunkId& id,
    const double minX = 0.0,
    const double maxX = 10.0)
{
    full_engine::WorldChunkDesc desc;
    desc.id = id;
    desc.bounds.min = {minX, 0.0, -5.0};
    desc.bounds.max = {maxX, 5.0, 5.0};
    return desc;
}

void testAddFindContainsAndDuplicate(std::vector<std::string>& failures)
{
    full_engine::WorldChunkCatalog catalog;
    const full_engine::ChunkId id{2, 0, -1};
    const full_engine::WorldChunkDesc desc = makeDesc(id);

    expect(catalog.chunkCount() == 0, "catalog starts empty", failures);
    expect(!catalog.contains(id), "missing descriptor is not contained", failures);
    expect(catalog.findChunk(id) == nullptr, "missing descriptor find returns null", failures);

    expectResult(catalog.addChunk(desc), full_engine::WorldResult::Success, "valid add succeeds", failures);
    expect(catalog.chunkCount() == 1, "add increments count", failures);
    expect(catalog.contains(id), "added descriptor is contained", failures);

    const full_engine::WorldChunkDesc* found = catalog.findChunk(id);
    expect(found != nullptr, "added descriptor can be found", failures);
    if (found != nullptr)
    {
        expect(found->id == id, "found descriptor preserves id", failures);
        expect(found->bounds.max.x == desc.bounds.max.x, "found descriptor preserves bounds", failures);
    }

    expectResult(catalog.addChunk(desc), full_engine::WorldResult::AlreadyExists, "duplicate add reports already exists", failures);
    expect(catalog.chunkCount() == 1, "duplicate add preserves count", failures);
}

void testUpdateRemoveAndClear(std::vector<std::string>& failures)
{
    full_engine::WorldChunkCatalog catalog;
    const full_engine::ChunkId id{1, 0, 0};
    const full_engine::ChunkId missing{9, 0, 0};
    expectResult(catalog.addChunk(makeDesc(id)), full_engine::WorldResult::Success, "add before update succeeds", failures);

    full_engine::WorldChunkDesc replacement = makeDesc(id, 20.0, 40.0);
    expectResult(catalog.updateChunk(replacement), full_engine::WorldResult::Success, "update existing succeeds", failures);
    const full_engine::WorldChunkDesc* found = catalog.findChunk(id);
    expect(found != nullptr && found->bounds.min.x == 20.0, "update replaces bounds", failures);

    expectResult(catalog.updateChunk(makeDesc(missing)), full_engine::WorldResult::NotFound, "missing update reports not found", failures);
    expectResult(catalog.removeChunk(missing), full_engine::WorldResult::NotFound, "missing remove reports not found", failures);
    expectResult(catalog.removeChunk(id), full_engine::WorldResult::Success, "remove existing succeeds", failures);
    expect(catalog.chunkCount() == 0, "remove decrements count", failures);
    expect(!catalog.contains(id), "removed descriptor is gone", failures);

    expectResult(catalog.addChunk(makeDesc(id)), full_engine::WorldResult::Success, "re-add succeeds", failures);
    catalog.clear();
    expect(catalog.chunkCount() == 0, "clear removes descriptors", failures);
    expect(catalog.descs().empty(), "descs empty after clear", failures);
}

void testInvalidDescriptorsDoNotMutate(std::vector<std::string>& failures)
{
    full_engine::WorldChunkCatalog catalog;
    const full_engine::ChunkId id{3, 0, 0};
    full_engine::WorldChunkDesc valid = makeDesc(id);
    expectResult(catalog.addChunk(valid), full_engine::WorldResult::Success, "add valid before invalid update succeeds", failures);

    full_engine::WorldChunkDesc inverted = makeDesc({4, 0, 0});
    inverted.bounds.min.x = 10.0;
    inverted.bounds.max.x = 5.0;
    expectResult(catalog.addChunk(inverted), full_engine::WorldResult::InvalidArgument, "inverted add is rejected", failures);
    expect(catalog.chunkCount() == 1, "invalid add preserves count", failures);

    full_engine::WorldChunkDesc nonFinite = makeDesc(id);
    nonFinite.bounds.max.y = std::numeric_limits<double>::infinity();
    expectResult(catalog.updateChunk(nonFinite), full_engine::WorldResult::InvalidArgument, "non-finite update is rejected", failures);
    const full_engine::WorldChunkDesc* found = catalog.findChunk(id);
    expect(found != nullptr && found->bounds.max.y == valid.bounds.max.y, "invalid update preserves existing descriptor", failures);
}

void testDeterministicSnapshots(std::vector<std::string>& failures)
{
    full_engine::WorldChunkCatalog catalog;
    const full_engine::ChunkId high{3, 0, 0};
    const full_engine::ChunkId low{-1, 0, 0};
    const full_engine::ChunkId mid{1, 2, 0};

    expectResult(catalog.addChunk(makeDesc(high)), full_engine::WorldResult::Success, "add high succeeds", failures);
    expectResult(catalog.addChunk(makeDesc(low)), full_engine::WorldResult::Success, "add low succeeds", failures);
    expectResult(catalog.addChunk(makeDesc(mid)), full_engine::WorldResult::Success, "add mid succeeds", failures);

    const std::vector<full_engine::WorldChunkDesc> descs = catalog.descs();
    expect(descs.size() == 3, "snapshot includes all descriptors", failures);
    expect(descs[0].id == low, "snapshot order starts with low id", failures);
    expect(descs[1].id == mid, "snapshot order keeps middle id", failures);
    expect(descs[2].id == high, "snapshot order ends with high id", failures);

    full_engine::WorldChunkDesc copy = descs[0];
    copy.bounds.min.x = 1234.0;
    const full_engine::WorldChunkDesc* stored = catalog.findChunk(low);
    expect(stored != nullptr && stored->bounds.min.x != 1234.0, "snapshot is returned by value", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testAddFindContainsAndDuplicate(failures);
    testUpdateRemoveAndClear(failures);
    testInvalidDescriptorsDoNotMutate(failures);
    testDeterministicSnapshots(failures);

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
