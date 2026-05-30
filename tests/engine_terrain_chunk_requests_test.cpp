#include "engine/renderer_integration/TerrainChunkRequests.hpp"

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

full_engine::WorldChunkDesc worldDesc(const full_engine::ChunkId& id)
{
    full_engine::WorldChunkDesc desc;
    desc.id = id;
    desc.bounds.min = {0.0, 0.0, 0.0};
    desc.bounds.max = {10.0, 5.0, 10.0};
    return desc;
}

full_engine::TerrainChunkResourceDesc resources(const full_engine::ChunkId& id)
{
    full_engine::TerrainChunkResourceDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = mesh(10);
    desc.lods[0].material = material(20);
    desc.lods[0].maxDistanceMeters = 100.0f;
    return desc;
}

void expectOwnerCounts(
    const full_engine::WorldChunkRegistry& registry,
    const full_engine::WorldChunkCatalog& worldCatalog,
    const full_engine::TerrainResourceCatalog& resourceCatalog,
    const std::size_t expected,
    const char* message,
    std::vector<std::string>& failures)
{
    expect(registry.chunkCount() == expected, message, failures);
    expect(worldCatalog.chunkCount() == expected, message, failures);
    expect(resourceCatalog.resourceCount() == expected, message, failures);
}

void testQueuedAddCreatesSetupState(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resourceCatalog;
    full_engine::TerrainChunkRequestQueue queue;
    const full_engine::ChunkId id{1, 0, 0};

    queue.pushAdd(worldDesc(id), resources(id));
    const full_engine::TerrainChunkRequestApplyResult result =
        queue.applyTo(registry, worldCatalog, resourceCatalog);

    expect(result.records.size() == 1, "queued add emits one record", failures);
    expect(result.summary.appliedCount == 1, "queued add increments applied", failures);
    expect(result.records[0].status == full_engine::TerrainChunkRequestStatus::Applied, "queued add applies", failures);
    expect(result.records[0].addResult.result == full_engine::TerrainChunkSetupResult::Success, "queued add records setup success", failures);
    expect(registry.contains(id), "queued add creates registry state", failures);
    expect(worldCatalog.contains(id), "queued add creates world catalog state", failures);
    expect(resourceCatalog.contains(id), "queued add creates resource state", failures);
    expect(queue.requestCount() == 1, "apply does not clear add request", failures);
}

void testQueuedRemoveDeletesSetupState(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resourceCatalog;
    const full_engine::ChunkId id{2, 0, 0};
    (void)full_engine::addTerrainChunk(registry, worldCatalog, resourceCatalog, worldDesc(id), resources(id));

    full_engine::TerrainChunkRequestQueue queue;
    queue.pushRemove(id);
    const full_engine::TerrainChunkRequestApplyResult result =
        queue.applyTo(registry, worldCatalog, resourceCatalog);

    expect(result.summary.appliedCount == 1, "queued remove increments applied", failures);
    expect(result.records[0].status == full_engine::TerrainChunkRequestStatus::Applied, "queued remove applies", failures);
    expect(result.records[0].removeResult.result == full_engine::TerrainChunkSetupResult::Success, "queued remove records setup success", failures);
    expectOwnerCounts(registry, worldCatalog, resourceCatalog, 0, "queued remove clears all owners", failures);
}

void testFifoOrderingAndRepeatedRequests(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resourceCatalog;
    full_engine::TerrainChunkRequestQueue queue;
    const full_engine::ChunkId first{3, 0, 0};
    const full_engine::ChunkId second{4, 0, 0};

    queue.pushAdd(worldDesc(first), resources(first));
    queue.pushAdd(worldDesc(second), resources(second));
    queue.pushRemove(first);
    queue.pushAdd(worldDesc(first), resources(first));
    const full_engine::TerrainChunkRequestApplyResult result =
        queue.applyTo(registry, worldCatalog, resourceCatalog);

    expect(result.records.size() == 4, "mixed queue emits four records", failures);
    expect(result.summary.appliedCount == 4, "mixed queue applies all valid requests", failures);
    expect(result.records[0].request.type == full_engine::TerrainChunkRequestType::Add, "first request is add", failures);
    expect(result.records[0].request.id == first, "first request id is preserved", failures);
    expect(result.records[1].request.id == second, "second request id is preserved", failures);
    expect(result.records[2].request.type == full_engine::TerrainChunkRequestType::Remove, "third request is remove", failures);
    expect(result.records[2].request.id == first, "third request id is preserved", failures);
    expect(result.records[3].request.type == full_engine::TerrainChunkRequestType::Add, "fourth request is add", failures);
    expect(registry.contains(first), "repeated add/remove leaves first chunk present", failures);
    expect(registry.contains(second), "mixed queue leaves second chunk present", failures);
    expectOwnerCounts(registry, worldCatalog, resourceCatalog, 2, "mixed queue ends with two setup chunks", failures);
}

void testDuplicateAddAndMissingRemove(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resourceCatalog;
    const full_engine::ChunkId existing{5, 0, 0};
    const full_engine::ChunkId missing{6, 0, 0};
    (void)full_engine::addTerrainChunk(registry, worldCatalog, resourceCatalog, worldDesc(existing), resources(existing));

    full_engine::TerrainChunkRequestQueue queue;
    queue.pushAdd(worldDesc(existing), resources(existing));
    queue.pushRemove(missing);
    const full_engine::TerrainChunkRequestApplyResult result =
        queue.applyTo(registry, worldCatalog, resourceCatalog);

    expect(result.records.size() == 2, "duplicate and missing emit records", failures);
    expect(result.summary.alreadySatisfiedCount == 1, "duplicate add counts already satisfied", failures);
    expect(result.summary.notFoundCount == 1, "missing remove counts not found", failures);
    expect(result.records[0].status == full_engine::TerrainChunkRequestStatus::AlreadySatisfied, "duplicate add is already satisfied", failures);
    expect(result.records[1].status == full_engine::TerrainChunkRequestStatus::NotFound, "missing remove is not found", failures);
    expectOwnerCounts(registry, worldCatalog, resourceCatalog, 1, "duplicate and missing preserve existing owners", failures);
}

void testInvalidAddMutatesNothing(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resourceCatalog;
    full_engine::TerrainChunkRequestQueue queue;
    const full_engine::ChunkId id{7, 0, 0};
    full_engine::WorldChunkDesc invalidWorld = worldDesc(id);
    invalidWorld.bounds.max.x = std::numeric_limits<double>::infinity();

    queue.pushAdd(invalidWorld, resources(id));
    const full_engine::TerrainChunkRequestApplyResult result =
        queue.applyTo(registry, worldCatalog, resourceCatalog);

    expect(result.summary.invalidArgumentCount == 1, "invalid add counts invalid argument", failures);
    expect(result.records[0].status == full_engine::TerrainChunkRequestStatus::InvalidArgument, "invalid add reports invalid argument", failures);
    expectOwnerCounts(registry, worldCatalog, resourceCatalog, 0, "invalid add mutates nothing", failures);
}

void testPartialDriftRepair(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resourceCatalog;
    const full_engine::ChunkId id{8, 0, 0};
    (void)full_engine::addWorldChunk(registry, worldCatalog, worldDesc(id));

    full_engine::TerrainChunkRequestQueue queue;
    queue.pushRemove(id);
    const full_engine::TerrainChunkRequestApplyResult result =
        queue.applyTo(registry, worldCatalog, resourceCatalog);

    expect(result.summary.partialFailureCount == 1, "drift repair counts partial failure", failures);
    expect(result.records[0].status == full_engine::TerrainChunkRequestStatus::PartialFailure, "drift repair reports partial failure", failures);
    expectOwnerCounts(registry, worldCatalog, resourceCatalog, 0, "drift repair removes existing setup state", failures);
}

void testClearAndEmptyApply(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resourceCatalog;
    full_engine::TerrainChunkRequestQueue queue;
    const full_engine::TerrainChunkRequestApplyResult empty = queue.applyTo(registry, worldCatalog, resourceCatalog);

    expect(empty.records.empty(), "empty apply emits no records", failures);
    expect(empty.summary.appliedCount == 0, "empty apply has zero applied", failures);
    expect(empty.summary.alreadySatisfiedCount == 0, "empty apply has zero already satisfied", failures);
    expect(empty.summary.notFoundCount == 0, "empty apply has zero missing", failures);
    expect(empty.summary.invalidArgumentCount == 0, "empty apply has zero invalid", failures);
    expect(empty.summary.partialFailureCount == 0, "empty apply has zero partial failures", failures);

    queue.pushRemove({9, 0, 0});
    expect(queue.requestCount() == 1, "push remove increments request count", failures);
    expect(queue.requests().size() == 1, "requests exposes queued remove", failures);
    queue.clear();
    expect(queue.requestCount() == 0, "clear removes queued requests", failures);
    expect(queue.requests().empty(), "requests empty after clear", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testQueuedAddCreatesSetupState(failures);
    testQueuedRemoveDeletesSetupState(failures);
    testFifoOrderingAndRepeatedRequests(failures);
    testDuplicateAddAndMissingRemove(failures);
    testInvalidAddMutatesNothing(failures);
    testPartialDriftRepair(failures);
    testClearAndEmptyApply(failures);

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
