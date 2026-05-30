#include "engine/world/WorldChunkRegistry.hpp"

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

void expectResult(
    const full_engine::WorldResult actual,
    const full_engine::WorldResult expected,
    const char* message,
    std::vector<std::string>& failures)
{
    expect(actual == expected, message, failures);
}

void expectResidency(
    const full_engine::WorldChunkRegistry& registry,
    const full_engine::ChunkId& id,
    const full_engine::ChunkResidencyState expected,
    const char* message,
    std::vector<std::string>& failures)
{
    const full_engine::WorldChunkInfo* info = registry.findChunk(id);
    expect(info != nullptr, "expected chunk to exist for residency check", failures);
    if (info != nullptr)
    {
        expect(info->id == id, "chunk snapshot preserves requested id", failures);
        expect(info->residency == expected, message, failures);
    }
}

void testChunkIdValueSemantics(std::vector<std::string>& failures)
{
    const full_engine::ChunkId defaultId;
    expect(defaultId.x == 0 && defaultId.y == 0 && defaultId.z == 0, "default ChunkId is origin", failures);

    const full_engine::ChunkId a{1, -2, 3};
    const full_engine::ChunkId same{1, -2, 3};
    const full_engine::ChunkId different{1, -2, 4};
    expect(a == same, "equal ChunkId coordinates compare equal", failures);
    expect(!(a == different), "different ChunkId coordinates do not compare equal", failures);
    expect(a < different, "ChunkId ordering is deterministic by coordinates", failures);
    expect(!(different < a), "ChunkId ordering is asymmetric", failures);
}

void testCreateFindAndDuplicate(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    const full_engine::ChunkId id{4, 0, -7};

    expect(registry.chunkCount() == 0, "new registry starts empty", failures);
    expect(!registry.contains(id), "missing chunk is not contained", failures);
    expect(registry.findChunk(id) == nullptr, "missing chunk find returns null", failures);

    expectResult(registry.createChunk(id), full_engine::WorldResult::Success, "create chunk succeeds", failures);
    expect(registry.chunkCount() == 1, "create chunk increments count", failures);
    expect(registry.contains(id), "created chunk is contained", failures);
    expectResidency(registry, id, full_engine::ChunkResidencyState::Unloaded, "created chunk starts unloaded", failures);

    expectResult(
        registry.createChunk(id),
        full_engine::WorldResult::AlreadyExists,
        "duplicate create reports already exists",
        failures);
    expect(registry.chunkCount() == 1, "duplicate create preserves count", failures);
}

void testMissingOperations(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    const full_engine::ChunkId missing{9, 1, 9};

    expectResult(registry.removeChunk(missing), full_engine::WorldResult::NotFound, "missing remove reports not found", failures);
    expectResult(
        registry.setResidencyState(missing, full_engine::ChunkResidencyState::Loading),
        full_engine::WorldResult::NotFound,
        "missing update reports not found",
        failures);
    expect(!registry.contains(missing), "missing operations do not create chunk", failures);
    expect(registry.chunkCount() == 0, "missing operations preserve count", failures);
}

void testResidencyTransitions(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    const full_engine::ChunkId id{2, 0, 3};
    expectResult(registry.createChunk(id), full_engine::WorldResult::Success, "create transition chunk succeeds", failures);

    expectResult(
        registry.setResidencyState(id, full_engine::ChunkResidencyState::Resident),
        full_engine::WorldResult::InvalidTransition,
        "skipped unloaded to resident transition is invalid",
        failures);
    expectResidency(
        registry,
        id,
        full_engine::ChunkResidencyState::Unloaded,
        "invalid transition preserves previous state",
        failures);

    expectResult(
        registry.setResidencyState(id, full_engine::ChunkResidencyState::Unloaded),
        full_engine::WorldResult::Success,
        "same-state transition succeeds",
        failures);
    expect(registry.chunkCount() == 1, "same-state transition preserves count", failures);

    expectResult(
        registry.setResidencyState(id, full_engine::ChunkResidencyState::Loading),
        full_engine::WorldResult::Success,
        "unloaded to loading succeeds",
        failures);
    expectResult(
        registry.setResidencyState(id, full_engine::ChunkResidencyState::Resident),
        full_engine::WorldResult::Success,
        "loading to resident succeeds",
        failures);
    expectResult(
        registry.setResidencyState(id, full_engine::ChunkResidencyState::Unloading),
        full_engine::WorldResult::Success,
        "resident to unloading succeeds",
        failures);
    expectResult(
        registry.setResidencyState(id, full_engine::ChunkResidencyState::Unloaded),
        full_engine::WorldResult::Success,
        "unloading to unloaded succeeds",
        failures);
    expectResidency(
        registry,
        id,
        full_engine::ChunkResidencyState::Unloaded,
        "valid transition chain ends unloaded",
        failures);
}

void testRemoveClearAndIndependentChunks(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    const full_engine::ChunkId first{0, 0, 0};
    const full_engine::ChunkId second{1, 0, 0};
    const full_engine::ChunkId third{1, 1, 0};

    expectResult(registry.createChunk(first), full_engine::WorldResult::Success, "create first chunk succeeds", failures);
    expectResult(registry.createChunk(second), full_engine::WorldResult::Success, "create second chunk succeeds", failures);
    expectResult(registry.createChunk(third), full_engine::WorldResult::Success, "create third chunk succeeds", failures);
    expect(registry.chunkCount() == 3, "multiple chunks are tracked independently", failures);

    expectResult(
        registry.setResidencyState(second, full_engine::ChunkResidencyState::Loading),
        full_engine::WorldResult::Success,
        "second chunk transition succeeds",
        failures);
    expectResidency(registry, first, full_engine::ChunkResidencyState::Unloaded, "first chunk remains unloaded", failures);
    expectResidency(registry, second, full_engine::ChunkResidencyState::Loading, "second chunk becomes loading", failures);
    expectResidency(registry, third, full_engine::ChunkResidencyState::Unloaded, "third chunk remains unloaded", failures);

    expectResult(registry.removeChunk(second), full_engine::WorldResult::Success, "remove existing chunk succeeds", failures);
    expect(registry.chunkCount() == 2, "remove decreases count", failures);
    expect(!registry.contains(second), "removed chunk is no longer contained", failures);
    expect(registry.findChunk(second) == nullptr, "removed chunk find returns null", failures);

    registry.clear();
    expect(registry.chunkCount() == 0, "clear removes all chunks", failures);
    expect(!registry.contains(first) && !registry.contains(third), "clear removes remaining lookups", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testChunkIdValueSemantics(failures);
    testCreateFindAndDuplicate(failures);
    testMissingOperations(failures);
    testResidencyTransitions(failures);
    testRemoveClearAndIndependentChunks(failures);

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
