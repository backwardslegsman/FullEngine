#include "engine/renderer_integration/WorldRenderSnapshot.hpp"

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

bool sameId(const full_engine::ChunkId& lhs, const full_engine::ChunkId& rhs)
{
    return lhs == rhs;
}

void makeResident(full_engine::WorldChunkRegistry& registry, const full_engine::ChunkId& id, std::vector<std::string>& failures)
{
    expectResult(registry.createChunk(id), full_engine::WorldResult::Success, "create resident chunk succeeds", failures);
    expectResult(
        registry.setResidencyState(id, full_engine::ChunkResidencyState::Loading),
        full_engine::WorldResult::Success,
        "resident chunk loading transition succeeds",
        failures);
    expectResult(
        registry.setResidencyState(id, full_engine::ChunkResidencyState::Resident),
        full_engine::WorldResult::Success,
        "resident chunk resident transition succeeds",
        failures);
}

full_engine::WorldBounds boundsAround(const double x)
{
    return {{x, 0.0, 0.0}, {x + 10.0, 10.0, 10.0}};
}

void testReadyChunk(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    const full_engine::ChunkId id{1, 0, 0};
    makeResident(registry, id, failures);

    const full_engine::WorldChunkRenderDesc desc{id, {{100.0, 10.0, -30.0}, {120.0, 40.0, -10.0}}};
    const full_engine::WorldRenderSnapshotOptions options{full_engine::makeCameraRelativeOrigin({90.0, 0.0, -40.0}), {}};
    const full_engine::WorldRenderSnapshot snapshot =
        full_engine::buildWorldRenderSnapshot(registry, &desc, 1, options);

    expect(snapshot.chunks.size() == 1, "ready snapshot has one record", failures);
    expect(snapshot.readyCount == 1, "ready counter increments", failures);
    expect(snapshot.notResidentCount == 0, "not resident counter stays zero", failures);
    if (!snapshot.chunks.empty())
    {
        const full_engine::RenderChunkSnapshot& record = snapshot.chunks[0];
        expect(record.status == full_engine::RenderChunkStatus::Ready, "resident valid chunk is ready", failures);
        expect(record.residency == full_engine::ChunkResidencyState::Resident, "ready record preserves residency", failures);
        expect(record.bounds.min.x == 10.0f && record.bounds.max.z == 30.0f, "ready record stores render bounds", failures);
    }
}

void testNonResidentAndMissing(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    const full_engine::ChunkId unloaded{0, 0, 0};
    const full_engine::ChunkId loading{1, 0, 0};
    const full_engine::ChunkId missing{2, 0, 0};
    expectResult(registry.createChunk(unloaded), full_engine::WorldResult::Success, "create unloaded succeeds", failures);
    expectResult(registry.createChunk(loading), full_engine::WorldResult::Success, "create loading succeeds", failures);
    expectResult(
        registry.setResidencyState(loading, full_engine::ChunkResidencyState::Loading),
        full_engine::WorldResult::Success,
        "loading transition succeeds",
        failures);

    const full_engine::WorldChunkRenderDesc descs[] = {
        {unloaded, boundsAround(0.0)},
        {loading, boundsAround(20.0)},
        {missing, boundsAround(40.0)},
    };
    const full_engine::WorldRenderSnapshot snapshot =
        full_engine::buildWorldRenderSnapshot(registry, descs, 3, {});

    expect(snapshot.chunks.size() == 3, "nonresident snapshot preserves input count", failures);
    expect(snapshot.notResidentCount == 2, "nonresident counter includes unloaded and loading", failures);
    expect(snapshot.missingChunkCount == 1, "missing counter increments", failures);
    expect(snapshot.readyCount == 0, "no ready chunks are counted", failures);
    expect(snapshot.chunks[0].status == full_engine::RenderChunkStatus::NotResident, "unloaded chunk is not resident", failures);
    expect(snapshot.chunks[1].status == full_engine::RenderChunkStatus::NotResident, "loading chunk is not resident", failures);
    expect(snapshot.chunks[2].status == full_engine::RenderChunkStatus::MissingChunk, "missing chunk is marked missing", failures);
}

void testInvalidBoundsAndOutOfRange(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    const full_engine::ChunkId invalid{3, 0, 0};
    const full_engine::ChunkId outOfRange{4, 0, 0};
    makeResident(registry, invalid, failures);
    makeResident(registry, outOfRange, failures);

    const full_engine::WorldChunkRenderDesc descs[] = {
        {invalid, {{2.0, 0.0, 0.0}, {1.0, 1.0, 1.0}}},
        {outOfRange, {{0.0, 0.0, 0.0}, {101.0, 1.0, 1.0}}},
    };
    full_engine::WorldRenderSnapshotOptions options;
    options.limits.maxAbsCoordinate = 100.0;

    const full_engine::WorldRenderSnapshot snapshot =
        full_engine::buildWorldRenderSnapshot(registry, descs, 2, options);

    expect(snapshot.chunks.size() == 2, "failure snapshot preserves input count", failures);
    expect(snapshot.invalidBoundsCount == 1, "invalid bounds counter increments", failures);
    expect(snapshot.outOfRangeCount == 1, "out of range counter increments", failures);
    expect(snapshot.chunks[0].status == full_engine::RenderChunkStatus::InvalidBounds, "inverted bounds are invalid", failures);
    expect(snapshot.chunks[1].status == full_engine::RenderChunkStatus::OutOfRange, "out of range bounds are marked", failures);
}

void testOrderAndCounters(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    const full_engine::ChunkId first{10, 0, 0};
    const full_engine::ChunkId second{11, 0, 0};
    const full_engine::ChunkId third{12, 0, 0};
    makeResident(registry, first, failures);
    expectResult(registry.createChunk(second), full_engine::WorldResult::Success, "create second not resident succeeds", failures);

    const full_engine::WorldChunkRenderDesc descs[] = {
        {third, boundsAround(30.0)},
        {first, boundsAround(0.0)},
        {second, boundsAround(10.0)},
    };
    const full_engine::WorldRenderSnapshot snapshot =
        full_engine::buildWorldRenderSnapshot(registry, descs, 3, {});

    expect(snapshot.chunks.size() == 3, "ordered snapshot has all records", failures);
    expect(sameId(snapshot.chunks[0].id, third), "first output preserves first input id", failures);
    expect(sameId(snapshot.chunks[1].id, first), "second output preserves second input id", failures);
    expect(sameId(snapshot.chunks[2].id, second), "third output preserves third input id", failures);
    expect(snapshot.readyCount == 1, "ordered snapshot ready count is correct", failures);
    expect(snapshot.notResidentCount == 1, "ordered snapshot nonresident count is correct", failures);
    expect(snapshot.missingChunkCount == 1, "ordered snapshot missing count is correct", failures);
    expect(snapshot.invalidBoundsCount == 0, "ordered snapshot invalid count is zero", failures);
    expect(snapshot.outOfRangeCount == 0, "ordered snapshot out of range count is zero", failures);
}

void testNullAndEmptyInput(std::vector<std::string>& failures)
{
    const full_engine::WorldChunkRegistry registry;
    const full_engine::WorldRenderSnapshot nullSnapshot =
        full_engine::buildWorldRenderSnapshot(registry, nullptr, 4, {});
    expect(nullSnapshot.chunks.empty(), "null input with count returns no records", failures);
    expect(nullSnapshot.invalidInputCount == 4, "null input records invalid input count", failures);
    expect(nullSnapshot.readyCount == 0, "null input has no ready chunks", failures);

    const full_engine::WorldRenderSnapshot emptySnapshot =
        full_engine::buildWorldRenderSnapshot(registry, nullptr, 0, {});
    expect(emptySnapshot.chunks.empty(), "empty null input returns no records", failures);
    expect(emptySnapshot.invalidInputCount == 0, "empty null input has zero invalid count", failures);
    expect(emptySnapshot.readyCount == 0 && emptySnapshot.notResidentCount == 0 && emptySnapshot.missingChunkCount == 0,
        "empty input has zero counters",
        failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testReadyChunk(failures);
    testNonResidentAndMissing(failures);
    testInvalidBoundsAndOutOfRange(failures);
    testOrderAndCounters(failures);
    testNullAndEmptyInput(failures);

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
