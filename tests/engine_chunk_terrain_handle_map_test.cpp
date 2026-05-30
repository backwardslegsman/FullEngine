#include "engine/renderer_integration/ChunkTerrainHandleMap.hpp"

#include <cstdint>
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

full_renderer::TerrainChunkHandle handle(const std::uint32_t id, const std::uint32_t generation)
{
    return full_renderer::TerrainChunkHandle{id, generation};
}

bool sameHandle(const full_renderer::TerrainChunkHandle lhs, const full_renderer::TerrainChunkHandle rhs)
{
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
}

void testDefaultMapIsEmpty(std::vector<std::string>& failures)
{
    const full_engine::ChunkTerrainHandleMap map;

    expect(map.mappedCount() == 0, "default handle map is empty", failures);
    expect(!map.contains({1, 0, 0}), "default handle map does not contain chunks", failures);
    expect(map.findHandle({1, 0, 0}) == nullptr, "missing handle lookup returns null", failures);
}

void testMapAndFindValidHandle(std::vector<std::string>& failures)
{
    full_engine::ChunkTerrainHandleMap map;
    const full_engine::ChunkId id{1, 2, 3};
    const full_renderer::TerrainChunkHandle terrainHandle = handle(10, 20);

    expect(
        map.mapChunk(id, terrainHandle) == full_engine::ChunkTerrainHandleMapResult::Success,
        "valid terrain handle maps successfully",
        failures);
    expect(map.mappedCount() == 1, "mapped count increments", failures);
    expect(map.contains(id), "mapped chunk is contained", failures);

    const full_renderer::TerrainChunkHandle* found = map.findHandle(id);
    expect(found != nullptr, "mapped handle can be found", failures);
    expect(found != nullptr && sameHandle(*found, terrainHandle), "found handle matches mapped handle", failures);
}

void testDuplicateMapAndInvalidHandles(std::vector<std::string>& failures)
{
    full_engine::ChunkTerrainHandleMap map;
    const full_engine::ChunkId id{4, 0, 0};

    expect(
        map.mapChunk(id, handle(1, 1)) == full_engine::ChunkTerrainHandleMapResult::Success,
        "initial map succeeds",
        failures);
    expect(
        map.mapChunk(id, handle(2, 1)) == full_engine::ChunkTerrainHandleMapResult::AlreadyMapped,
        "duplicate map is rejected",
        failures);
    expect(
        map.mapChunk({5, 0, 0}, handle(0, 1)) == full_engine::ChunkTerrainHandleMapResult::InvalidHandle,
        "zero handle id is rejected",
        failures);
    expect(
        map.mapChunk({6, 0, 0}, handle(1, 0)) == full_engine::ChunkTerrainHandleMapResult::InvalidHandle,
        "zero handle generation is rejected",
        failures);
    expect(map.mappedCount() == 1, "invalid and duplicate maps do not change count", failures);
}

void testUpdateExistingMapping(std::vector<std::string>& failures)
{
    full_engine::ChunkTerrainHandleMap map;
    const full_engine::ChunkId id{7, 0, 0};
    const full_renderer::TerrainChunkHandle replacement = handle(42, 9);

    map.mapChunk(id, handle(41, 8));

    expect(
        map.updateChunk(id, replacement) == full_engine::ChunkTerrainHandleMapResult::Success,
        "existing mapping updates successfully",
        failures);
    expect(map.mappedCount() == 1, "update does not change mapped count", failures);

    const full_renderer::TerrainChunkHandle* found = map.findHandle(id);
    expect(found != nullptr && sameHandle(*found, replacement), "update replaces stored handle", failures);
}

void testUpdateAndRemoveMissingBehavior(std::vector<std::string>& failures)
{
    full_engine::ChunkTerrainHandleMap map;
    const full_engine::ChunkId missing{8, 0, 0};

    expect(
        map.updateChunk(missing, handle(3, 3)) == full_engine::ChunkTerrainHandleMapResult::NotFound,
        "updating missing chunk returns not found",
        failures);
    expect(
        map.updateChunk(missing, handle(0, 3)) == full_engine::ChunkTerrainHandleMapResult::InvalidHandle,
        "updating with invalid handle returns invalid handle",
        failures);
    expect(
        map.removeChunk(missing) == full_engine::ChunkTerrainHandleMapResult::NotFound,
        "removing missing chunk returns not found",
        failures);
}

void testRemoveAndClear(std::vector<std::string>& failures)
{
    full_engine::ChunkTerrainHandleMap map;
    const full_engine::ChunkId first{9, 0, 0};
    const full_engine::ChunkId second{10, 0, 0};

    map.mapChunk(first, handle(9, 1));
    map.mapChunk(second, handle(10, 1));

    expect(
        map.removeChunk(first) == full_engine::ChunkTerrainHandleMapResult::Success,
        "removing mapped chunk succeeds",
        failures);
    expect(!map.contains(first), "removed chunk is no longer contained", failures);
    expect(map.findHandle(first) == nullptr, "removed handle lookup returns null", failures);
    expect(map.mappedCount() == 1, "remove decreases mapped count", failures);
    expect(map.contains(second), "other mapped chunks remain", failures);

    map.clear();

    expect(map.mappedCount() == 0, "clear removes all mappings", failures);
    expect(!map.contains(second), "clear removes remaining chunk", failures);
}

void testIndependentChunkIds(std::vector<std::string>& failures)
{
    full_engine::ChunkTerrainHandleMap map;
    const full_engine::ChunkId lower{-1, 0, 2};
    const full_engine::ChunkId upper{-1, 1, 2};

    map.mapChunk(lower, handle(11, 1));
    map.mapChunk(upper, handle(12, 1));

    const full_renderer::TerrainChunkHandle* lowerHandle = map.findHandle(lower);
    const full_renderer::TerrainChunkHandle* upperHandle = map.findHandle(upper);

    expect(map.mappedCount() == 2, "distinct chunk ids are tracked independently", failures);
    expect(lowerHandle != nullptr && lowerHandle->id == 11, "lower chunk keeps its handle", failures);
    expect(upperHandle != nullptr && upperHandle->id == 12, "upper chunk keeps its handle", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testDefaultMapIsEmpty(failures);
    testMapAndFindValidHandle(failures);
    testDuplicateMapAndInvalidHandles(failures);
    testUpdateExistingMapping(failures);
    testUpdateAndRemoveMissingBehavior(failures);
    testRemoveAndClear(failures);
    testIndependentChunkIds(failures);

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
