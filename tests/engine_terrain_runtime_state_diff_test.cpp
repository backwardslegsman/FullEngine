#include "engine/renderer_integration/TerrainRuntimeStateDiff.hpp"

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

full_engine::TerrainRuntimeChunkState chunk(
    const full_engine::ChunkId& id,
    const full_engine::TerrainRuntimeChunkReadiness readiness =
        full_engine::TerrainRuntimeChunkReadiness::Renderable,
    const full_engine::ChunkResidencyState residency = full_engine::ChunkResidencyState::Resident,
    const bool hasHandle = true)
{
    full_engine::TerrainRuntimeChunkState state;
    state.id = id;
    state.hasRegistry = readiness != full_engine::TerrainRuntimeChunkReadiness::MissingRegistry;
    state.hasWorldDesc = readiness != full_engine::TerrainRuntimeChunkReadiness::MissingRegistry &&
        readiness != full_engine::TerrainRuntimeChunkReadiness::MissingWorldDesc;
    state.hasResources = readiness != full_engine::TerrainRuntimeChunkReadiness::MissingRegistry &&
        readiness != full_engine::TerrainRuntimeChunkReadiness::MissingWorldDesc &&
        readiness != full_engine::TerrainRuntimeChunkReadiness::MissingResources;
    state.hasTerrainHandle = hasHandle;
    state.residency = residency;
    state.readiness = readiness;
    return state;
}

full_engine::TerrainRuntimeStateSnapshot snapshot(
    const std::vector<full_engine::TerrainRuntimeChunkState>& chunks)
{
    full_engine::TerrainRuntimeStateSnapshot result;
    result.chunks = chunks;
    return result;
}

void testEmptyDiff(std::vector<std::string>& failures)
{
    const full_engine::TerrainRuntimeStateDiff diff =
        full_engine::diffTerrainRuntimeStateSnapshots({}, {});

    expect(diff.changes.empty(), "empty diff has no changes", failures);
    expect(diff.summary.addedCount == 0, "empty diff has zero added", failures);
    expect(diff.summary.removedCount == 0, "empty diff has zero removed", failures);
}

void testAddedAndRemoved(std::vector<std::string>& failures)
{
    const full_engine::ChunkId removed{1, 0, 0};
    const full_engine::ChunkId added{2, 0, 0};

    const full_engine::TerrainRuntimeStateDiff diff = full_engine::diffTerrainRuntimeStateSnapshots(
        snapshot({chunk(removed)}),
        snapshot({chunk(added)}));

    expect(diff.changes.size() == 2, "added/removed diff has two changes", failures);
    expect(diff.changes[0].id == removed, "removed change is sorted first", failures);
    expect(diff.changes[0].type == full_engine::TerrainRuntimeStateChangeType::Removed, "removed chunk is reported", failures);
    expect(diff.changes[1].id == added, "added change is sorted second", failures);
    expect(diff.changes[1].type == full_engine::TerrainRuntimeStateChangeType::Added, "added chunk is reported", failures);
    expect(diff.summary.addedCount == 1, "added count matches", failures);
    expect(diff.summary.removedCount == 1, "removed count matches", failures);
}

void testReadinessPrecedence(std::vector<std::string>& failures)
{
    const full_engine::ChunkId id{3, 0, 0};
    const full_engine::TerrainRuntimeStateDiff diff = full_engine::diffTerrainRuntimeStateSnapshots(
        snapshot({chunk(id, full_engine::TerrainRuntimeChunkReadiness::NotResident, full_engine::ChunkResidencyState::Loading, true)}),
        snapshot({chunk(id, full_engine::TerrainRuntimeChunkReadiness::MissingTerrainHandle, full_engine::ChunkResidencyState::Resident, false)}));

    expect(diff.changes.size() == 1, "readiness precedence emits one change", failures);
    expect(diff.changes[0].type == full_engine::TerrainRuntimeStateChangeType::ReadinessChanged, "readiness change wins precedence", failures);
    expect(diff.changes[0].previousReadiness == full_engine::TerrainRuntimeChunkReadiness::NotResident, "previous readiness is copied", failures);
    expect(diff.changes[0].currentReadiness == full_engine::TerrainRuntimeChunkReadiness::MissingTerrainHandle, "current readiness is copied", failures);
    expect(diff.summary.readinessChangedCount == 1, "readiness count matches", failures);
    expect(diff.summary.residencyChangedCount == 0, "residency count suppressed by readiness change", failures);
    expect(diff.summary.handlePresenceChangedCount == 0, "handle count suppressed by readiness change", failures);
}

void testResidencyAndHandleChanges(std::vector<std::string>& failures)
{
    const full_engine::ChunkId residencyId{4, 0, 0};
    const full_engine::ChunkId handleId{5, 0, 0};
    const full_engine::TerrainRuntimeStateDiff diff = full_engine::diffTerrainRuntimeStateSnapshots(
        snapshot({
            chunk(handleId, full_engine::TerrainRuntimeChunkReadiness::Renderable, full_engine::ChunkResidencyState::Resident, false),
            chunk(residencyId, full_engine::TerrainRuntimeChunkReadiness::Renderable, full_engine::ChunkResidencyState::Loading, true)}),
        snapshot({
            chunk(residencyId, full_engine::TerrainRuntimeChunkReadiness::Renderable, full_engine::ChunkResidencyState::Resident, true),
            chunk(handleId, full_engine::TerrainRuntimeChunkReadiness::Renderable, full_engine::ChunkResidencyState::Resident, true)}));

    expect(diff.changes.size() == 2, "residency/handle diff has two changes", failures);
    expect(diff.changes[0].id == residencyId, "diff output sorted by chunk id", failures);
    expect(diff.changes[0].type == full_engine::TerrainRuntimeStateChangeType::ResidencyChanged, "residency-only change is reported", failures);
    expect(diff.changes[0].previousResidency == full_engine::ChunkResidencyState::Loading, "previous residency is copied", failures);
    expect(diff.changes[0].currentResidency == full_engine::ChunkResidencyState::Resident, "current residency is copied", failures);
    expect(diff.changes[1].id == handleId, "handle change sorted after residency id", failures);
    expect(diff.changes[1].type == full_engine::TerrainRuntimeStateChangeType::HandlePresenceChanged, "handle-only change is reported", failures);
    expect(!diff.changes[1].previousHasTerrainHandle, "previous handle presence is copied", failures);
    expect(diff.changes[1].currentHasTerrainHandle, "current handle presence is copied", failures);
    expect(diff.summary.residencyChangedCount == 1, "residency count matches", failures);
    expect(diff.summary.handlePresenceChangedCount == 1, "handle count matches", failures);
}

void testUnchangedIgnoredAndDeterministicOrder(std::vector<std::string>& failures)
{
    const full_engine::ChunkId unchanged{6, 0, 0};
    const full_engine::ChunkId low{7, 0, 0};
    const full_engine::ChunkId high{8, 0, 0};

    const full_engine::TerrainRuntimeStateDiff diff = full_engine::diffTerrainRuntimeStateSnapshots(
        snapshot({
            chunk(high, full_engine::TerrainRuntimeChunkReadiness::Renderable),
            chunk(unchanged, full_engine::TerrainRuntimeChunkReadiness::Renderable),
            chunk(low, full_engine::TerrainRuntimeChunkReadiness::Renderable)}),
        snapshot({
            chunk(unchanged, full_engine::TerrainRuntimeChunkReadiness::Renderable),
            chunk(high, full_engine::TerrainRuntimeChunkReadiness::NotResident),
            chunk(low, full_engine::TerrainRuntimeChunkReadiness::MissingResources)}));

    expect(diff.changes.size() == 2, "unchanged chunk is ignored", failures);
    expect(diff.changes[0].id == low, "low changed id is first", failures);
    expect(diff.changes[1].id == high, "high changed id is second", failures);
    expect(diff.summary.readinessChangedCount == 2, "summary counts readiness changes", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testEmptyDiff(failures);
    testAddedAndRemoved(failures);
    testReadinessPrecedence(failures);
    testResidencyAndHandleChanges(failures);
    testUnchangedIgnoredAndDeterministicOrder(failures);

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
