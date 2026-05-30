#include "engine/renderer_integration/TerrainRenderPrep.hpp"

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

bool sameId(const full_engine::ChunkId& lhs, const full_engine::ChunkId& rhs)
{
    return lhs == rhs;
}

full_engine::RenderChunkSnapshot snapshotRecord(
    const full_engine::ChunkId& id,
    const full_engine::RenderChunkStatus status,
    const float minX)
{
    full_engine::RenderChunkSnapshot record;
    record.id = id;
    record.residency = status == full_engine::RenderChunkStatus::Ready ?
        full_engine::ChunkResidencyState::Resident :
        full_engine::ChunkResidencyState::Unloaded;
    record.status = status;
    record.bounds.min = {minX, 0.0f, 0.0f};
    record.bounds.max = {minX + 10.0f, 10.0f, 10.0f};
    return record;
}

void testReadyRecordsBecomePrep(std::vector<std::string>& failures)
{
    full_engine::WorldRenderSnapshot snapshot;
    const full_engine::ChunkId first{1, 0, 0};
    const full_engine::ChunkId second{2, 0, 0};
    snapshot.chunks.push_back(snapshotRecord(first, full_engine::RenderChunkStatus::Ready, 0.0f));
    snapshot.chunks.push_back(snapshotRecord(second, full_engine::RenderChunkStatus::Ready, 20.0f));
    snapshot.readyCount = 2;

    const full_engine::TerrainRenderPrep prep = full_engine::prepareTerrainRenderChunks(snapshot);

    expect(prep.chunks.size() == 2, "ready records produce terrain prep chunks", failures);
    expect(prep.summary.readyCount == 2, "ready summary count is copied from included records", failures);
    expect(sameId(prep.chunks[0].id, first), "first ready chunk id is preserved", failures);
    expect(sameId(prep.chunks[1].id, second), "second ready chunk id is preserved", failures);
    expect(prep.chunks[0].sourceStatus == full_engine::RenderChunkStatus::Ready, "source status is ready", failures);
    expect(prep.chunks[1].bounds.min.x == 20.0f && prep.chunks[1].bounds.max.x == 30.0f, "bounds are copied", failures);
}

void testNonReadyRecordsAreSkippedAndCounted(std::vector<std::string>& failures)
{
    full_engine::WorldRenderSnapshot snapshot;
    snapshot.chunks.push_back(snapshotRecord({1, 0, 0}, full_engine::RenderChunkStatus::NotResident, 0.0f));
    snapshot.chunks.push_back(snapshotRecord({2, 0, 0}, full_engine::RenderChunkStatus::MissingChunk, 20.0f));
    snapshot.chunks.push_back(snapshotRecord({3, 0, 0}, full_engine::RenderChunkStatus::InvalidBounds, 40.0f));
    snapshot.chunks.push_back(snapshotRecord({4, 0, 0}, full_engine::RenderChunkStatus::OutOfRange, 60.0f));
    snapshot.notResidentCount = 1;
    snapshot.missingChunkCount = 1;
    snapshot.invalidBoundsCount = 1;
    snapshot.outOfRangeCount = 1;
    snapshot.invalidInputCount = 3;

    const full_engine::TerrainRenderPrep prep = full_engine::prepareTerrainRenderChunks(snapshot);

    expect(prep.chunks.empty(), "non-ready records produce no terrain chunks", failures);
    expect(prep.summary.readyCount == 0, "non-ready records do not increment ready count", failures);
    expect(prep.summary.skippedNotResidentCount == 1, "not resident skip count is tracked", failures);
    expect(prep.summary.skippedMissingChunkCount == 1, "missing skip count is tracked", failures);
    expect(prep.summary.skippedInvalidBoundsCount == 1, "invalid bounds skip count is tracked", failures);
    expect(prep.summary.skippedOutOfRangeCount == 1, "out of range skip count is tracked", failures);
    expect(prep.summary.invalidInputCount == 3, "invalid input count is carried through", failures);
}

void testReadyOrderWithSkippedRecords(std::vector<std::string>& failures)
{
    full_engine::WorldRenderSnapshot snapshot;
    const full_engine::ChunkId firstReady{10, 0, 0};
    const full_engine::ChunkId secondReady{11, 0, 0};
    snapshot.chunks.push_back(snapshotRecord({9, 0, 0}, full_engine::RenderChunkStatus::MissingChunk, -20.0f));
    snapshot.chunks.push_back(snapshotRecord(firstReady, full_engine::RenderChunkStatus::Ready, 0.0f));
    snapshot.chunks.push_back(snapshotRecord({12, 0, 0}, full_engine::RenderChunkStatus::OutOfRange, 20.0f));
    snapshot.chunks.push_back(snapshotRecord(secondReady, full_engine::RenderChunkStatus::Ready, 40.0f));

    const full_engine::TerrainRenderPrep prep = full_engine::prepareTerrainRenderChunks(snapshot);

    expect(prep.chunks.size() == 2, "only ready records are included", failures);
    expect(sameId(prep.chunks[0].id, firstReady), "first ready output preserves input order", failures);
    expect(sameId(prep.chunks[1].id, secondReady), "second ready output preserves input order", failures);
    expect(prep.summary.readyCount == 2, "ready count includes both ready records", failures);
    expect(prep.summary.skippedMissingChunkCount == 1, "missing record skipped", failures);
    expect(prep.summary.skippedOutOfRangeCount == 1, "out of range record skipped", failures);
}

void testEmptySnapshot(std::vector<std::string>& failures)
{
    const full_engine::WorldRenderSnapshot snapshot;
    const full_engine::TerrainRenderPrep prep = full_engine::prepareTerrainRenderChunks(snapshot);

    expect(prep.chunks.empty(), "empty snapshot produces no terrain chunks", failures);
    expect(prep.summary.readyCount == 0, "empty snapshot ready count is zero", failures);
    expect(prep.summary.skippedNotResidentCount == 0, "empty snapshot skip count is zero", failures);
    expect(prep.summary.invalidInputCount == 0, "empty snapshot invalid input count is zero", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testReadyRecordsBecomePrep(failures);
    testNonReadyRecordsAreSkippedAndCounted(failures);
    testReadyOrderWithSkippedRecords(failures);
    testEmptySnapshot(failures);

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
