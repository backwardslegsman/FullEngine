#include "engine/renderer_integration/TerrainLifecyclePlan.hpp"

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

bool sameId(const full_engine::ChunkId& lhs, const full_engine::ChunkId& rhs)
{
    return lhs == rhs;
}

full_engine::RenderBounds bounds(const float minX)
{
    full_engine::RenderBounds result;
    result.min = {minX, 0.0f, 0.0f};
    result.max = {minX + 10.0f, 10.0f, 10.0f};
    return result;
}

bool sameBounds(const full_engine::RenderBounds& lhs, const full_engine::RenderBounds& rhs)
{
    return lhs.min.x == rhs.min.x &&
        lhs.min.y == rhs.min.y &&
        lhs.min.z == rhs.min.z &&
        lhs.max.x == rhs.max.x &&
        lhs.max.y == rhs.max.y &&
        lhs.max.z == rhs.max.z;
}

void addPrepChunk(full_engine::TerrainRenderPrep& prep, const full_engine::ChunkId& id, const float minX)
{
    full_engine::TerrainChunkRenderPrep chunk;
    chunk.id = id;
    chunk.bounds = bounds(minX);
    chunk.sourceStatus = full_engine::RenderChunkStatus::Ready;
    prep.chunks.push_back(chunk);
    prep.summary.readyCount = prep.chunks.size();
}

void testUnmappedReadyChunkCreates(std::vector<std::string>& failures)
{
    full_engine::TerrainRenderPrep prep;
    const full_engine::ChunkId id{1, 0, 0};
    addPrepChunk(prep, id, 0.0f);

    const full_engine::ChunkTerrainHandleMap handles;
    const full_engine::TerrainLifecyclePlan plan = full_engine::planTerrainLifecycle(prep, handles);

    expect(plan.operations.size() == 1, "unmapped ready chunk emits one operation", failures);
    expect(plan.operations[0].action == full_engine::TerrainLifecycleAction::Create, "unmapped ready chunk creates", failures);
    expect(sameId(plan.operations[0].id, id), "create operation preserves chunk id", failures);
    expect(sameBounds(plan.operations[0].bounds, prep.chunks[0].bounds), "create operation copies bounds", failures);
    expect(!full_renderer::isValid(plan.operations[0].handle), "create operation has no existing handle", failures);
    expect(plan.summary.createCount == 1, "create count increments", failures);
}

void testMappedReadyChunkKeepsByDefault(std::vector<std::string>& failures)
{
    full_engine::TerrainRenderPrep prep;
    const full_engine::ChunkId id{2, 0, 0};
    const full_renderer::TerrainChunkHandle terrainHandle = handle(20, 2);
    addPrepChunk(prep, id, 20.0f);

    full_engine::ChunkTerrainHandleMap handles;
    handles.mapChunk(id, terrainHandle);

    const full_engine::TerrainLifecyclePlan plan = full_engine::planTerrainLifecycle(prep, handles);

    expect(plan.operations.size() == 1, "mapped ready chunk emits one operation", failures);
    expect(plan.operations[0].action == full_engine::TerrainLifecycleAction::Keep, "mapped ready chunk keeps by default", failures);
    expect(sameHandle(plan.operations[0].handle, terrainHandle), "keep operation copies mapped handle", failures);
    expect(sameBounds(plan.operations[0].bounds, prep.chunks[0].bounds), "keep operation copies bounds", failures);
    expect(plan.summary.keepCount == 1, "keep count increments", failures);
}

void testMappedReadyChunkUpdatesWhenRequested(std::vector<std::string>& failures)
{
    full_engine::TerrainRenderPrep prep;
    const full_engine::ChunkId id{3, 0, 0};
    const full_renderer::TerrainChunkHandle terrainHandle = handle(30, 3);
    addPrepChunk(prep, id, 30.0f);

    full_engine::ChunkTerrainHandleMap handles;
    handles.mapChunk(id, terrainHandle);

    full_engine::TerrainLifecyclePlanOptions options;
    options.updateMappedReadyChunks = true;
    const full_engine::TerrainLifecyclePlan plan = full_engine::planTerrainLifecycle(prep, handles, options);

    expect(plan.operations.size() == 1, "mapped ready update emits one operation", failures);
    expect(plan.operations[0].action == full_engine::TerrainLifecycleAction::Update, "mapped ready chunk updates when requested", failures);
    expect(sameHandle(plan.operations[0].handle, terrainHandle), "update operation copies mapped handle", failures);
    expect(sameBounds(plan.operations[0].bounds, prep.chunks[0].bounds), "update operation copies bounds", failures);
    expect(plan.summary.updateCount == 1, "update count increments", failures);
}

void testAbsentMappedHandleReleases(std::vector<std::string>& failures)
{
    const full_engine::TerrainRenderPrep prep;
    const full_engine::ChunkId id{4, 0, 0};
    const full_renderer::TerrainChunkHandle terrainHandle = handle(40, 4);

    full_engine::ChunkTerrainHandleMap handles;
    handles.mapChunk(id, terrainHandle);

    const full_engine::TerrainLifecyclePlan plan = full_engine::planTerrainLifecycle(prep, handles);

    expect(plan.operations.size() == 1, "mapped handle absent from prep emits one operation", failures);
    expect(plan.operations[0].action == full_engine::TerrainLifecycleAction::Release, "absent mapped handle releases", failures);
    expect(sameId(plan.operations[0].id, id), "release operation preserves chunk id", failures);
    expect(sameHandle(plan.operations[0].handle, terrainHandle), "release operation copies mapped handle", failures);
    expect(sameBounds(plan.operations[0].bounds, full_engine::RenderBounds{}), "release operation has default bounds", failures);
    expect(plan.summary.releaseCount == 1, "release count increments", failures);
}

void testMixedOperationsAndOrdering(std::vector<std::string>& failures)
{
    full_engine::TerrainRenderPrep prep;
    const full_engine::ChunkId createId{10, 0, 0};
    const full_engine::ChunkId keepId{11, 0, 0};
    const full_engine::ChunkId releaseA{1, 0, 0};
    const full_engine::ChunkId releaseB{20, 0, 0};
    addPrepChunk(prep, createId, 100.0f);
    addPrepChunk(prep, keepId, 110.0f);

    full_engine::ChunkTerrainHandleMap handles;
    handles.mapChunk(keepId, handle(11, 1));
    handles.mapChunk(releaseB, handle(20, 1));
    handles.mapChunk(releaseA, handle(1, 1));

    const full_engine::TerrainLifecyclePlan plan = full_engine::planTerrainLifecycle(prep, handles);

    expect(plan.operations.size() == 4, "mixed lifecycle emits all expected operations", failures);
    expect(plan.operations[0].action == full_engine::TerrainLifecycleAction::Create, "first prep chunk creates", failures);
    expect(sameId(plan.operations[0].id, createId), "create keeps prep order", failures);
    expect(plan.operations[1].action == full_engine::TerrainLifecycleAction::Keep, "second prep chunk keeps", failures);
    expect(sameId(plan.operations[1].id, keepId), "keep keeps prep order", failures);
    expect(plan.operations[2].action == full_engine::TerrainLifecycleAction::Release, "first appended release follows prep operations", failures);
    expect(sameId(plan.operations[2].id, releaseA), "releases are deterministic by chunk id", failures);
    expect(plan.operations[3].action == full_engine::TerrainLifecycleAction::Release, "second appended release follows prep operations", failures);
    expect(sameId(plan.operations[3].id, releaseB), "second release follows deterministic chunk order", failures);
    expect(plan.summary.createCount == 1, "mixed create count is correct", failures);
    expect(plan.summary.keepCount == 1, "mixed keep count is correct", failures);
    expect(plan.summary.updateCount == 0, "mixed update count is correct", failures);
    expect(plan.summary.releaseCount == 2, "mixed release count is correct", failures);
}

void testEmptyInputs(std::vector<std::string>& failures)
{
    const full_engine::TerrainRenderPrep prep;
    const full_engine::ChunkTerrainHandleMap handles;
    const full_engine::TerrainLifecyclePlan plan = full_engine::planTerrainLifecycle(prep, handles);

    expect(plan.operations.empty(), "empty prep and empty map emits no operations", failures);
    expect(plan.summary.createCount == 0, "empty create count is zero", failures);
    expect(plan.summary.releaseCount == 0, "empty release count is zero", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testUnmappedReadyChunkCreates(failures);
    testMappedReadyChunkKeepsByDefault(failures);
    testMappedReadyChunkUpdatesWhenRequested(failures);
    testAbsentMappedHandleReleases(failures);
    testMixedOperationsAndOrdering(failures);
    testEmptyInputs(failures);

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
