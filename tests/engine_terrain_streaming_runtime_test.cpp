#include "engine/streaming/TerrainStreamingRuntime.hpp"

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

full_engine::TerrainStreamingPlannerConfig config(
    const double chunkSize = 10.0,
    const int loadRadius = 0,
    const int residentRadius = 0)
{
    full_engine::TerrainStreamingPlannerConfig result;
    result.chunkSizeMeters = chunkSize;
    result.loadRadiusChunks = loadRadius;
    result.residentRadiusChunks = residentRadius;
    return result;
}

full_engine::TerrainRuntimeChunkState chunkState(
    const full_engine::ChunkId& id,
    const bool setup,
    const full_engine::ChunkResidencyState residency = full_engine::ChunkResidencyState::Unloaded)
{
    full_engine::TerrainRuntimeChunkState state;
    state.id = id;
    state.hasRegistry = setup;
    state.hasWorldDesc = setup;
    state.hasResources = setup;
    state.residency = residency;
    state.readiness = setup && residency == full_engine::ChunkResidencyState::Resident
        ? full_engine::TerrainRuntimeChunkReadiness::MissingTerrainHandle
        : full_engine::TerrainRuntimeChunkReadiness::NotResident;
    if (!setup)
    {
        state.readiness = full_engine::TerrainRuntimeChunkReadiness::MissingRegistry;
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

full_engine::WorldChunkDesc worldDesc(const full_engine::ChunkId& id)
{
    full_engine::WorldChunkDesc desc;
    desc.id = id;
    desc.bounds.min = {static_cast<double>(id.x), 0.0, static_cast<double>(id.z)};
    desc.bounds.max = {static_cast<double>(id.x + 1), 1.0, static_cast<double>(id.z + 1)};
    return desc;
}

full_engine::TerrainChunkResourceDesc resources(const full_engine::ChunkId& id)
{
    full_engine::TerrainChunkResourceDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = full_renderer::MeshHandle{10};
    desc.lods[0].material = full_renderer::MaterialHandle{20};
    desc.lods[0].maxDistanceMeters = 100.0f;
    return desc;
}

full_engine::TerrainSetupStageDesc setupDesc(const full_engine::ChunkId& id)
{
    full_engine::TerrainSetupStageDesc desc;
    desc.id = id;
    desc.worldDesc = worldDesc(id);
    desc.resourceDesc = resources(id);
    return desc;
}

void testDefaultStateAndNoPlan(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingRuntimeState streaming;
    full_engine::TerrainRuntimeState runtime;

    expect(!streaming.hasLatestPlan(), "default streaming state has no plan", failures);
    expect(streaming.latestDiagnostics().latestPlanOperationCount == 0, "default diagnostics have zero plan ops", failures);
    expect(streaming.latestDiagnostics().latestQueueSummary.queuedSetupAddCount == 0, "default diagnostics have zero queue counts", failures);

    const full_engine::TerrainStreamingQueueResult& result =
        streaming.queueLatestPlan(runtime, nullptr, 0);
    expect(result.status == full_engine::TerrainStreamingQueueStatus::NoPlan, "queue before plan returns no plan", failures);
    expect(runtime.setupRequestCount() == 0, "no plan queues no setup requests", failures);
    expect(runtime.residencyRequestCount() == 0, "no plan queues no residency requests", failures);
    expect(std::string(full_engine::terrainStreamingQueueStatusName(result.status)) == "NoPlan", "no plan status name is stable", failures);
}

void testPlanStoresDiagnostics(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingRuntimeState streaming;
    const full_engine::ChunkId ids[] = {{0, 0, 0}, {1, 0, 0}};

    const full_engine::TerrainStreamingPlan& plan =
        streaming.plan(config(10.0, 1, 0), {0.0, 0.0, 0.0}, ids, 2, snapshot({}));

    expect(streaming.hasLatestPlan(), "planning stores latest plan flag", failures);
    expect(&streaming.latestPlan() == &plan, "plan returns retained plan reference", failures);
    expect(streaming.latestDiagnostics().hasLatestPlan, "diagnostics report latest plan", failures);
    expect(streaming.latestDiagnostics().latestPlanOperationCount == plan.operations.size(), "diagnostics copy plan operation count", failures);
    expect(streaming.latestDiagnostics().latestPlanSummary.addSetupCount == 2, "diagnostics copy plan summary", failures);
}

void testInvalidPlanBlocksQueueing(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingRuntimeState streaming;
    full_engine::TerrainRuntimeState runtime;
    const full_engine::ChunkId ids[] = {{0, 0, 0}};

    (void)streaming.plan(config(0.0, 1, 0), {0.0, 0.0, 0.0}, ids, 1, snapshot({}));
    const full_engine::TerrainStreamingQueueResult& result =
        streaming.queueLatestPlan(runtime, nullptr, 0);

    expect(result.status == full_engine::TerrainStreamingQueueStatus::BlockedInvalidPlan, "invalid plan blocks queueing", failures);
    expect(result.summary.skippedPlanSkippedCount == 1, "invalid skipped ops are counted", failures);
    expect(runtime.setupRequestCount() == 0, "invalid plan queues no setup requests", failures);
    expect(runtime.residencyRequestCount() == 0, "invalid plan queues no residency requests", failures);
}

void testMissingSetupDescBlocksQueueing(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingRuntimeState streaming;
    full_engine::TerrainRuntimeState runtime;
    const full_engine::ChunkId ids[] = {{0, 0, 0}, {1, 0, 0}};
    const full_engine::TerrainSetupStageDesc onlySecond = setupDesc(ids[1]);

    (void)streaming.plan(config(10.0, 1, 0), {0.0, 0.0, 0.0}, ids, 2, snapshot({}));
    const full_engine::TerrainStreamingQueueResult& result =
        streaming.queueLatestPlan(runtime, &onlySecond, 1);

    expect(result.status == full_engine::TerrainStreamingQueueStatus::MissingSetupDesc, "missing add setup descriptor blocks queueing", failures);
    expect(result.summary.missingSetupDescCount == 1, "missing setup descriptor is counted", failures);
    expect(runtime.setupRequestCount() == 0, "missing setup descriptor queues no setup requests", failures);
    expect(runtime.residencyRequestCount() == 0, "missing setup descriptor queues no residency requests", failures);
}

void testMissingSetupDescIgnoresDeferredAdds(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingRuntimeState streaming;
    full_engine::TerrainRuntimeState runtime;
    const full_engine::ChunkId ids[] = {{0, 0, 0}, {1, 0, 0}};
    const full_engine::TerrainSetupStageDesc onlyFirst = setupDesc(ids[0]);
    full_engine::TerrainStreamingQueueOptions options;
    options.maxSetupAdds = 1;

    (void)streaming.plan(config(10.0, 1, 0), {0.0, 0.0, 0.0}, ids, 2, snapshot({}));
    const full_engine::TerrainStreamingQueueResult& result =
        streaming.queueLatestPlan(runtime, &onlyFirst, 1, options);

    expect(result.status == full_engine::TerrainStreamingQueueStatus::Success, "deferred add does not require setup desc", failures);
    expect(result.summary.queuedSetupAddCount == 1, "budgeted setup add queues one request", failures);
    expect(result.summary.deferredSetupAddCount == 1, "budgeted setup add defers one request", failures);
    expect(result.summary.missingSetupDescCount == 0, "deferred missing setup desc is not counted", failures);
    expect(runtime.setupRequestCount() == 1, "budgeted add queues one setup request", failures);
}

void testAddAndMakeResidentQueueing(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingRuntimeState streaming;
    full_engine::TerrainRuntimeState runtime;
    const full_engine::ChunkId id{0, 0, 0};
    const full_engine::TerrainSetupStageDesc setup = setupDesc(id);

    (void)streaming.plan(config(), {0.0, 0.0, 0.0}, &id, 1, snapshot({}));
    const full_engine::TerrainStreamingQueueResult& result =
        streaming.queueLatestPlan(runtime, &setup, 1);

    expect(result.status == full_engine::TerrainStreamingQueueStatus::Success, "valid add plan queues successfully", failures);
    expect(result.summary.queuedSetupAddCount == 1, "add setup request is counted", failures);
    expect(result.summary.queuedMakeResidentCount == 1, "make resident request is counted", failures);
    expect(runtime.setupRequestCount() == 1, "runtime receives setup add request", failures);
    expect(runtime.residencyRequestCount() == 1, "runtime receives make resident request", failures);
}

void testBudgetedAddAndResidentQueueing(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingRuntimeState streaming;
    full_engine::TerrainRuntimeState runtime;
    const full_engine::ChunkId ids[] = {{0, 0, 0}, {1, 0, 0}};
    const full_engine::TerrainSetupStageDesc setup[] = {setupDesc(ids[0]), setupDesc(ids[1])};
    full_engine::TerrainStreamingQueueOptions options;
    options.maxSetupAdds = 1;
    options.maxMakeResident = 1;

    (void)streaming.plan(config(10.0, 1, 1), {0.0, 0.0, 0.0}, ids, 2, snapshot({}));
    const full_engine::TerrainStreamingQueueResult& result =
        streaming.queueLatestPlan(runtime, setup, 2, options);

    expect(result.status == full_engine::TerrainStreamingQueueStatus::Success, "budgeted add/resident plan succeeds", failures);
    expect(result.summary.queuedSetupAddCount == 1, "setup add budget queues one", failures);
    expect(result.summary.deferredSetupAddCount == 1, "setup add budget defers one", failures);
    expect(result.summary.queuedMakeResidentCount == 1, "resident budget queues one", failures);
    expect(result.summary.deferredMakeResidentCount == 1, "resident budget defers one", failures);
    expect(runtime.setupRequestCount() == 1, "budgeted add sends one setup request", failures);
    expect(runtime.residencyRequestCount() == 1, "budgeted resident sends one residency request", failures);
}

void testZeroBudgetsDeferWithoutFailure(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingRuntimeState streaming;
    full_engine::TerrainRuntimeState runtime;
    const full_engine::ChunkId id{0, 0, 0};
    full_engine::TerrainStreamingQueueOptions options;
    options.maxSetupAdds = 0;
    options.maxMakeResident = 0;

    (void)streaming.plan(config(), {0.0, 0.0, 0.0}, &id, 1, snapshot({}));
    const full_engine::TerrainStreamingQueueResult& result =
        streaming.queueLatestPlan(runtime, nullptr, 0, options);

    expect(result.status == full_engine::TerrainStreamingQueueStatus::Success, "zero budget defers without failure", failures);
    expect(result.summary.deferredSetupAddCount == 1, "zero setup add budget defers add", failures);
    expect(result.summary.deferredMakeResidentCount == 1, "zero resident budget defers resident", failures);
    expect(runtime.setupRequestCount() == 0, "zero setup budget queues no setup", failures);
    expect(runtime.residencyRequestCount() == 0, "zero resident budget queues no residency", failures);
}

void testRemoveAndMakeUnloadedQueueing(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingRuntimeState streaming;
    full_engine::TerrainRuntimeState runtime;
    const full_engine::ChunkId id{5, 0, 0};
    const full_engine::ChunkId dummy{0, 0, 0};

    (void)streaming.plan(
        config(),
        {0.0, 0.0, 0.0},
        &dummy,
        0,
        snapshot({chunkState(id, true, full_engine::ChunkResidencyState::Resident)}));
    const full_engine::TerrainStreamingQueueResult& result =
        streaming.queueLatestPlan(runtime, nullptr, 0);

    expect(result.status == full_engine::TerrainStreamingQueueStatus::Success, "valid cleanup plan queues successfully", failures);
    expect(result.summary.queuedMakeUnloadedCount == 1, "make unloaded request is counted", failures);
    expect(result.summary.queuedSetupRemoveCount == 1, "setup remove request is counted", failures);
    expect(runtime.setupRequestCount() == 1, "runtime receives setup remove request", failures);
    expect(runtime.residencyRequestCount() == 1, "runtime receives make unloaded request", failures);
}

void testBudgetedRemoveAndUnloadQueueing(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingRuntimeState streaming;
    full_engine::TerrainRuntimeState runtime;
    const full_engine::ChunkId ids[] = {{5, 0, 0}, {6, 0, 0}};
    const full_engine::ChunkId dummy{0, 0, 0};
    full_engine::TerrainStreamingQueueOptions options;
    options.maxSetupRemoves = 1;
    options.maxMakeUnloaded = 1;

    (void)streaming.plan(
        config(),
        {0.0, 0.0, 0.0},
        &dummy,
        0,
        snapshot({
            chunkState(ids[0], true, full_engine::ChunkResidencyState::Resident),
            chunkState(ids[1], true, full_engine::ChunkResidencyState::Resident)}));
    const full_engine::TerrainStreamingQueueResult& result =
        streaming.queueLatestPlan(runtime, nullptr, 0, options);

    expect(result.status == full_engine::TerrainStreamingQueueStatus::Success, "budgeted cleanup plan succeeds", failures);
    expect(result.summary.queuedMakeUnloadedCount == 1, "unload budget queues one", failures);
    expect(result.summary.deferredMakeUnloadedCount == 1, "unload budget defers one", failures);
    expect(result.summary.queuedSetupRemoveCount == 1, "remove budget queues one", failures);
    expect(result.summary.deferredSetupRemoveCount == 1, "remove budget defers one", failures);
    expect(runtime.setupRequestCount() == 1, "budgeted remove sends one setup request", failures);
    expect(runtime.residencyRequestCount() == 1, "budgeted unload sends one residency request", failures);
}

void testKeepOperationsAreSkipped(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingRuntimeState streaming;
    full_engine::TerrainRuntimeState runtime;
    const full_engine::ChunkId id{0, 0, 0};

    (void)streaming.plan(
        config(),
        {0.0, 0.0, 0.0},
        &id,
        1,
        snapshot({chunkState(id, true, full_engine::ChunkResidencyState::Resident)}));
    const full_engine::TerrainStreamingQueueResult& result =
        streaming.queueLatestPlan(runtime, nullptr, 0);

    expect(result.status == full_engine::TerrainStreamingQueueStatus::Success, "keep-only plan queues successfully", failures);
    expect(result.summary.skippedKeepSetupCount == 1, "keep setup is counted as skipped", failures);
    expect(result.summary.skippedKeepResidentCount == 1, "keep resident is counted as skipped", failures);
    expect(runtime.setupRequestCount() == 0, "keep-only plan queues no setup requests", failures);
    expect(runtime.residencyRequestCount() == 0, "keep-only plan queues no residency requests", failures);
}

void testClear(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingRuntimeState streaming;
    const full_engine::ChunkId id{0, 0, 0};

    (void)streaming.plan(config(), {0.0, 0.0, 0.0}, &id, 1, snapshot({}));
    streaming.clear();

    expect(!streaming.hasLatestPlan(), "clear removes latest plan flag", failures);
    expect(streaming.latestPlan().operations.empty(), "clear removes latest plan operations", failures);
    expect(streaming.latestDiagnostics().latestPlanOperationCount == 0, "clear resets diagnostics", failures);
    expect(streaming.latestQueueResult().status == full_engine::TerrainStreamingQueueStatus::Success, "clear resets queue result", failures);
}

void testStatusNames(std::vector<std::string>& failures)
{
    expect(std::string(full_engine::terrainStreamingQueueStatusName(full_engine::TerrainStreamingQueueStatus::Success)) == "Success", "success status name is stable", failures);
    expect(std::string(full_engine::terrainStreamingQueueStatusName(full_engine::TerrainStreamingQueueStatus::NoPlan)) == "NoPlan", "no plan status name is stable", failures);
    expect(std::string(full_engine::terrainStreamingQueueStatusName(full_engine::TerrainStreamingQueueStatus::BlockedInvalidPlan)) == "BlockedInvalidPlan", "blocked invalid plan status name is stable", failures);
    expect(std::string(full_engine::terrainStreamingQueueStatusName(full_engine::TerrainStreamingQueueStatus::MissingSetupDesc)) == "MissingSetupDesc", "missing setup desc status name is stable", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testDefaultStateAndNoPlan(failures);
    testPlanStoresDiagnostics(failures);
    testInvalidPlanBlocksQueueing(failures);
    testMissingSetupDescBlocksQueueing(failures);
    testMissingSetupDescIgnoresDeferredAdds(failures);
    testAddAndMakeResidentQueueing(failures);
    testBudgetedAddAndResidentQueueing(failures);
    testZeroBudgetsDeferWithoutFailure(failures);
    testRemoveAndMakeUnloadedQueueing(failures);
    testBudgetedRemoveAndUnloadQueueing(failures);
    testKeepOperationsAreSkipped(failures);
    testClear(failures);
    testStatusNames(failures);

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
