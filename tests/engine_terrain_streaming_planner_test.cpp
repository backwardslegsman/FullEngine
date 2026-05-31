#include "engine/streaming/TerrainStreamingPlanner.hpp"

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

full_engine::TerrainRuntimeChunkState chunkState(
    const full_engine::ChunkId& id,
    const bool setup,
    const full_engine::ChunkResidencyState residency = full_engine::ChunkResidencyState::Unloaded,
    const bool handle = false)
{
    full_engine::TerrainRuntimeChunkState state;
    state.id = id;
    state.hasRegistry = setup;
    state.hasWorldDesc = setup;
    state.hasResources = setup;
    state.hasTerrainHandle = handle;
    state.residency = residency;
    if (!setup)
    {
        state.readiness = full_engine::TerrainRuntimeChunkReadiness::MissingRegistry;
    }
    else if (residency != full_engine::ChunkResidencyState::Resident)
    {
        state.readiness = full_engine::TerrainRuntimeChunkReadiness::NotResident;
    }
    else if (!handle)
    {
        state.readiness = full_engine::TerrainRuntimeChunkReadiness::MissingTerrainHandle;
    }
    else
    {
        state.readiness = full_engine::TerrainRuntimeChunkReadiness::Renderable;
    }
    return state;
}

full_engine::TerrainRuntimeStateSnapshot snapshot(
    const std::vector<full_engine::TerrainRuntimeChunkState>& chunks)
{
    full_engine::TerrainRuntimeStateSnapshot result;
    result.chunks = chunks;
    return result;
}

full_engine::TerrainStreamingPlannerConfig config(
    const double chunkSize = 10.0,
    const int loadRadius = 1,
    const int residentRadius = 0)
{
    full_engine::TerrainStreamingPlannerConfig result;
    result.chunkSizeMeters = chunkSize;
    result.loadRadiusChunks = loadRadius;
    result.residentRadiusChunks = residentRadius;
    return result;
}

std::vector<full_engine::TerrainStreamingChunkAction> actions(
    const full_engine::TerrainStreamingPlan& plan)
{
    std::vector<full_engine::TerrainStreamingChunkAction> result;
    for (const full_engine::TerrainStreamingPlanOp& op : plan.operations)
    {
        result.push_back(op.action);
    }
    return result;
}

void testInvalidInput(std::vector<std::string>& failures)
{
    const full_engine::ChunkId ids[] = {{0, 0, 0}, {1, 0, 0}};
    const full_engine::TerrainRuntimeStateSnapshot current = snapshot({chunkState(ids[0], true)});

    full_engine::TerrainStreamingPlannerConfig invalid = config(0.0, 1, 0);
    const full_engine::TerrainStreamingPlan invalidConfig =
        full_engine::planTerrainStreaming(invalid, {0.0, 0.0, 0.0}, ids, 2, current);
    expect(invalidConfig.operations.size() == 2, "invalid config emits one skipped op per known id", failures);
    expect(invalidConfig.summary.invalidInputCount == 2, "invalid config counts invalid inputs", failures);
    expect(invalidConfig.summary.skippedCount == 2, "invalid config counts skipped ops", failures);
    expect(invalidConfig.operations[0].id == ids[0], "invalid config preserves known id order", failures);

    invalid = config(-10.0, 1, 0);
    const full_engine::TerrainStreamingPlan negativeChunkSize =
        full_engine::planTerrainStreaming(invalid, {0.0, 0.0, 0.0}, ids, 2, current);
    expect(negativeChunkSize.summary.invalidInputCount == 2, "negative chunk size counts invalid inputs", failures);
    expect(negativeChunkSize.summary.skippedCount == 2, "negative chunk size counts skipped ops", failures);

    invalid = config(10.0, 0, 1);
    const full_engine::TerrainStreamingPlan residentLargerThanLoad =
        full_engine::planTerrainStreaming(invalid, {0.0, 0.0, 0.0}, ids, 2, current);
    expect(residentLargerThanLoad.summary.invalidInputCount == 2, "resident radius beyond load radius is invalid", failures);

    const full_engine::TerrainStreamingPlan nullIds =
        full_engine::planTerrainStreaming(config(), {0.0, 0.0, 0.0}, nullptr, 3, current);
    expect(nullIds.operations.empty(), "null non-empty known id input emits no ops", failures);
    expect(nullIds.summary.invalidInputCount == 3, "null non-empty known ids count invalid inputs", failures);
    expect(nullIds.summary.skippedCount == 3, "null non-empty known ids count skipped ids", failures);
}

void testCameraFlooringAndRadiusWindows(std::vector<std::string>& failures)
{
    const full_engine::ChunkId ids[] = {
        {-2, 0, -1},
        {-1, 0, -1},
        {0, 0, -1},
        {-1, 0, 0},
        {1, 0, 1}};
    const full_engine::TerrainStreamingPlan plan = full_engine::planTerrainStreaming(
        config(10.0, 1, 0),
        {-0.1, 0.0, -0.1},
        ids,
        5,
        snapshot({}));

    expect(plan.operations.size() == 8, "load radius selects four chunks and emits setup/residency intent", failures);
    expect(plan.operations[0].id == ids[0], "negative camera flooring selects chunk -1,-1 neighborhood", failures);
    expect(plan.operations[0].action == full_engine::TerrainStreamingChunkAction::AddSetup, "missing setup inside load emits add", failures);
    expect(plan.operations[1].action == full_engine::TerrainStreamingChunkAction::KeepUnloaded, "load-only missing chunk stays unloaded", failures);
    expect(plan.operations[2].id == ids[1], "resident center chunk follows known order", failures);
    expect(plan.operations[3].action == full_engine::TerrainStreamingChunkAction::MakeResident, "resident radius subset emits make resident", failures);
    expect(plan.summary.addSetupCount == 4, "load radius add setup count matches", failures);
    expect(plan.summary.makeResidentCount == 1, "resident radius make resident count matches", failures);
    expect(plan.summary.keepUnloadedCount == 3, "load-only keep unloaded count matches", failures);
}

void testSetupAndResidencyIntent(std::vector<std::string>& failures)
{
    const full_engine::ChunkId center{0, 0, 0};
    const full_engine::ChunkId edge{1, 0, 0};
    const full_engine::ChunkId residentEdge{0, 0, 1};
    const full_engine::ChunkId ids[] = {center, edge, residentEdge};
    const full_engine::TerrainRuntimeStateSnapshot current = snapshot({
        chunkState(center, true, full_engine::ChunkResidencyState::Resident, true),
        chunkState(edge, true, full_engine::ChunkResidencyState::Unloaded, false),
        chunkState(residentEdge, true, full_engine::ChunkResidencyState::Resident, true),
    });

    const full_engine::TerrainStreamingPlan plan =
        full_engine::planTerrainStreaming(config(10.0, 1, 0), {0.0, 0.0, 0.0}, ids, 3, current);
    const std::vector<full_engine::TerrainStreamingChunkAction> expected = {
        full_engine::TerrainStreamingChunkAction::KeepSetup,
        full_engine::TerrainStreamingChunkAction::KeepResident,
        full_engine::TerrainStreamingChunkAction::KeepSetup,
        full_engine::TerrainStreamingChunkAction::KeepUnloaded,
        full_engine::TerrainStreamingChunkAction::KeepSetup,
        full_engine::TerrainStreamingChunkAction::MakeUnloaded,
    };

    expect(actions(plan) == expected, "setup and residency actions match current state", failures);
    expect(plan.summary.keepSetupCount == 3, "setup keep count matches", failures);
    expect(plan.summary.keepResidentCount == 1, "resident keep count matches", failures);
    expect(plan.summary.keepUnloadedCount == 1, "unloaded keep count matches", failures);
    expect(plan.summary.makeUnloadedCount == 1, "resident outside resident radius unload count matches", failures);
}

void testCleanupOrdering(std::vector<std::string>& failures)
{
    const full_engine::ChunkId known[] = {{0, 0, 0}};
    const full_engine::ChunkId farA{4, 0, 2};
    const full_engine::ChunkId farB{-3, 0, 0};
    const full_engine::TerrainRuntimeStateSnapshot current = snapshot({
        chunkState(farA, true, full_engine::ChunkResidencyState::Resident, true),
        chunkState(farB, true, full_engine::ChunkResidencyState::Unloaded, false),
    });

    const full_engine::TerrainStreamingPlan plan =
        full_engine::planTerrainStreaming(config(10.0, 0, 0), {0.0, 0.0, 0.0}, known, 1, current);

    expect(plan.operations.size() == 5, "known add/resident plus sorted cleanup ops are emitted", failures);
    expect(plan.operations[0].id == known[0], "known desired work is emitted first", failures);
    expect(plan.operations[2].id == farB, "cleanup removals are sorted by chunk id", failures);
    expect(plan.operations[2].action == full_engine::TerrainStreamingChunkAction::RemoveSetup, "unloaded far chunk removes setup", failures);
    expect(plan.operations[3].id == farA, "second cleanup chunk follows deterministic order", failures);
    expect(plan.operations[3].action == full_engine::TerrainStreamingChunkAction::MakeUnloaded, "resident far chunk unloads before removal", failures);
    expect(plan.operations[4].action == full_engine::TerrainStreamingChunkAction::RemoveSetup, "resident far chunk removes setup after unload", failures);
    expect(plan.summary.addSetupCount == 1, "cleanup plan add setup count matches", failures);
    expect(plan.summary.makeResidentCount == 1, "cleanup plan make resident count matches", failures);
    expect(plan.summary.makeUnloadedCount == 1, "cleanup plan make unloaded count matches", failures);
    expect(plan.summary.removeSetupCount == 2, "cleanup plan remove setup count matches", failures);
}

void testActionNames(std::vector<std::string>& failures)
{
    expect(std::string(full_engine::terrainStreamingChunkActionName(full_engine::TerrainStreamingChunkAction::AddSetup)) == "AddSetup", "add setup name is stable", failures);
    expect(std::string(full_engine::terrainStreamingChunkActionName(full_engine::TerrainStreamingChunkAction::KeepSetup)) == "KeepSetup", "keep setup name is stable", failures);
    expect(std::string(full_engine::terrainStreamingChunkActionName(full_engine::TerrainStreamingChunkAction::RemoveSetup)) == "RemoveSetup", "remove setup name is stable", failures);
    expect(std::string(full_engine::terrainStreamingChunkActionName(full_engine::TerrainStreamingChunkAction::MakeResident)) == "MakeResident", "make resident name is stable", failures);
    expect(std::string(full_engine::terrainStreamingChunkActionName(full_engine::TerrainStreamingChunkAction::KeepResident)) == "KeepResident", "keep resident name is stable", failures);
    expect(std::string(full_engine::terrainStreamingChunkActionName(full_engine::TerrainStreamingChunkAction::MakeUnloaded)) == "MakeUnloaded", "make unloaded name is stable", failures);
    expect(std::string(full_engine::terrainStreamingChunkActionName(full_engine::TerrainStreamingChunkAction::KeepUnloaded)) == "KeepUnloaded", "keep unloaded name is stable", failures);
    expect(std::string(full_engine::terrainStreamingChunkActionName(full_engine::TerrainStreamingChunkAction::Skipped)) == "Skipped", "skipped name is stable", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testInvalidInput(failures);
    testCameraFlooringAndRadiusWindows(failures);
    testSetupAndResidencyIntent(failures);
    testCleanupOrdering(failures);
    testActionNames(failures);

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
