#include "engine/renderer_integration/TerrainSetupStaging.hpp"
#include "engine/renderer_integration/TerrainRuntimeController.hpp"

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

full_engine::ChunkId chunk(const std::int32_t x) noexcept
{
    return {x, 0, 0};
}

full_engine::WorldChunkDesc worldDesc(const full_engine::ChunkId& id, const double offset = 0.0) noexcept
{
    full_engine::WorldChunkDesc desc;
    desc.id = id;
    desc.bounds.min = {offset, 0.0, 0.0};
    desc.bounds.max = {offset + 16.0, 4.0, 16.0};
    return desc;
}

full_engine::TerrainChunkResourceDesc resources(
    const full_engine::ChunkId& id,
    const std::uint32_t meshId = 10,
    const std::uint32_t materialId = 20,
    const std::uint32_t splatId = 30) noexcept
{
    full_engine::TerrainChunkResourceDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = {meshId};
    desc.lods[0].material = {materialId};
    desc.lods[0].maxDistanceMeters = 64.0f;
    desc.splatMap = {splatId};
    return desc;
}

full_engine::TerrainSetupStageDesc desired(
    const full_engine::ChunkId& id,
    const double offset = 0.0,
    const std::uint32_t meshId = 10) noexcept
{
    full_engine::TerrainSetupStageDesc desc;
    desc.id = id;
    desc.worldDesc = worldDesc(id, offset);
    desc.resourceDesc = resources(id, meshId);
    return desc;
}

void addCurrent(
    full_engine::WorldChunkRegistry& registry,
    full_engine::WorldChunkCatalog& worldCatalog,
    full_engine::TerrainResourceCatalog& resourceCatalog,
    const full_engine::TerrainSetupStageDesc& desc)
{
    (void)registry.createChunk(desc.id);
    (void)worldCatalog.addChunk(desc.worldDesc);
    (void)resourceCatalog.addChunkResources(desc.resourceDesc);
}

void testMissingCurrentAdds(std::vector<std::string>& failures)
{
    const full_engine::TerrainSetupStageDesc target = desired(chunk(1));
    const full_engine::TerrainSetupStagePlan plan = full_engine::planTerrainSetupChanges(
        {},
        {},
        {},
        &target,
        1);

    expect(plan.operations.size() == 1, "missing current emits one op", failures);
    expect(plan.summary.addCount == 1, "missing current increments add", failures);
    expect(plan.operations[0].action == full_engine::TerrainSetupStageAction::Add, "missing current is add", failures);
    expect(!plan.operations[0].hasRegistry && !plan.operations[0].hasWorldDesc && !plan.operations[0].hasResources, "add records missing flags", failures);
}

void testMatchingCurrentKeeps(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resourceCatalog;
    const full_engine::TerrainSetupStageDesc target = desired(chunk(2));
    addCurrent(registry, worldCatalog, resourceCatalog, target);

    const full_engine::TerrainSetupStagePlan plan = full_engine::planTerrainSetupChanges(
        registry,
        worldCatalog,
        resourceCatalog,
        &target,
        1);

    expect(plan.operations.size() == 1, "matching current emits one op", failures);
    expect(plan.summary.keepCount == 1, "matching current increments keep", failures);
    expect(plan.operations[0].action == full_engine::TerrainSetupStageAction::Keep, "matching current is keep", failures);
    expect(plan.operations[0].hasRegistry && plan.operations[0].hasWorldDesc && plan.operations[0].hasResources, "keep records current flags", failures);
}

void testAbsentDesiredRemoves(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resourceCatalog;
    addCurrent(registry, worldCatalog, resourceCatalog, desired(chunk(3)));

    const full_engine::TerrainSetupStagePlan plan = full_engine::planTerrainSetupChanges(
        registry,
        worldCatalog,
        resourceCatalog,
        nullptr,
        0);

    expect(plan.operations.size() == 1, "absent desired emits one remove", failures);
    expect(plan.summary.removeCount == 1, "absent desired increments remove", failures);
    expect(plan.operations[0].action == full_engine::TerrainSetupStageAction::Remove, "absent desired is remove", failures);
    expect(plan.operations[0].id == chunk(3), "remove uses current id", failures);
}

void testChangedUnsupported(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resourceCatalog;
    const full_engine::TerrainSetupStageDesc current = desired(chunk(4));
    const full_engine::TerrainSetupStageDesc changedWorld = desired(chunk(4), 32.0);
    addCurrent(registry, worldCatalog, resourceCatalog, current);

    full_engine::TerrainSetupStagePlan plan = full_engine::planTerrainSetupChanges(
        registry,
        worldCatalog,
        resourceCatalog,
        &changedWorld,
        1);
    expect(plan.operations[0].action == full_engine::TerrainSetupStageAction::ChangedUnsupported, "changed world is unsupported", failures);
    expect(plan.summary.changedUnsupportedCount == 1, "changed world increments unsupported", failures);

    const full_engine::TerrainSetupStageDesc changedResources = desired(chunk(4), 0.0, 99);
    plan = full_engine::planTerrainSetupChanges(
        registry,
        worldCatalog,
        resourceCatalog,
        &changedResources,
        1);
    expect(plan.operations[0].action == full_engine::TerrainSetupStageAction::ChangedUnsupported, "changed resources are unsupported", failures);
}

void testMixedOrderAndRequestConversion(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resourceCatalog;
    addCurrent(registry, worldCatalog, resourceCatalog, desired(chunk(3)));
    addCurrent(registry, worldCatalog, resourceCatalog, desired(chunk(9)));

    const full_engine::TerrainSetupStageDesc desiredRecords[] = {
        desired(chunk(5)),
        desired(chunk(3)),
    };
    const full_engine::TerrainSetupStagePlan plan = full_engine::planTerrainSetupChanges(
        registry,
        worldCatalog,
        resourceCatalog,
        desiredRecords,
        2);

    expect(plan.operations.size() == 3, "mixed plan emits three ops", failures);
    expect(plan.operations[0].id == chunk(5) && plan.operations[0].action == full_engine::TerrainSetupStageAction::Add, "mixed first op is desired add", failures);
    expect(plan.operations[1].id == chunk(3) && plan.operations[1].action == full_engine::TerrainSetupStageAction::Keep, "mixed second op is desired keep", failures);
    expect(plan.operations[2].id == chunk(9) && plan.operations[2].action == full_engine::TerrainSetupStageAction::Remove, "mixed removal is appended", failures);
    expect(plan.summary.addCount == 1 && plan.summary.keepCount == 1 && plan.summary.removeCount == 1, "mixed summary counts", failures);

    const full_engine::TerrainChunkRequestQueue requests =
        full_engine::buildTerrainChunkRequestsFromStagePlan(plan);
    expect(requests.requestCount() == 2, "request conversion skips keep", failures);
    expect(requests.requests()[0].type == full_engine::TerrainChunkRequestType::Add, "first converted request is add", failures);
    expect(requests.requests()[1].type == full_engine::TerrainChunkRequestType::Remove, "second converted request is remove", failures);
}

void testDuplicateDesiredIds(std::vector<std::string>& failures)
{
    const full_engine::TerrainSetupStageDesc desiredRecords[] = {
        desired(chunk(7)),
        desired(chunk(7)),
    };
    const full_engine::TerrainSetupStagePlan plan = full_engine::planTerrainSetupChanges(
        {},
        {},
        {},
        desiredRecords,
        2);

    expect(plan.operations.size() == 2, "duplicate desired emits two diagnostics", failures);
    expect(plan.operations[0].action == full_engine::TerrainSetupStageAction::Add, "first duplicate is planned normally", failures);
    expect(plan.operations[1].action == full_engine::TerrainSetupStageAction::ChangedUnsupported, "later duplicate is unsupported", failures);
    expect(plan.summary.addCount == 1 && plan.summary.changedUnsupportedCount == 1, "duplicate summary counts", failures);

    const full_engine::TerrainChunkRequestQueue requests =
        full_engine::buildTerrainChunkRequestsFromStagePlan(plan);
    expect(requests.requestCount() == 1, "duplicate unsupported is not converted", failures);
}

void testQueueStagePlanAddResidentDefault(std::vector<std::string>& failures)
{
    full_engine::TerrainSetupStagePlan plan;
    plan.operations.push_back({
        chunk(10),
        full_engine::TerrainSetupStageAction::Add,
        worldDesc(chunk(10)),
        resources(chunk(10)),
        false,
        false,
        false});
    plan.summary.addCount = 1;

    full_engine::TerrainRuntimeState runtime;
    const full_engine::TerrainSetupStageQueueApplyResult result =
        full_engine::queueTerrainSetupStagePlan(runtime, plan);

    expect(result.result == full_engine::TerrainSetupStageQueueResult::Success, "queue add succeeds", failures);
    expect(result.summary.queuedSetupCount == 1, "queue add counts setup", failures);
    expect(result.summary.queuedMakeResidentCount == 1, "queue add defaults resident", failures);
    expect(runtime.setupRequestCount() == 1, "queue add adds setup request", failures);
    expect(runtime.residencyRequestCount() == 1, "queue add adds resident request", failures);
}

void testQueueStagePlanAddCanSkipResident(std::vector<std::string>& failures)
{
    full_engine::TerrainSetupStagePlan plan;
    plan.operations.push_back({
        chunk(11),
        full_engine::TerrainSetupStageAction::Add,
        worldDesc(chunk(11)),
        resources(chunk(11)),
        false,
        false,
        false});
    plan.summary.addCount = 1;

    full_engine::TerrainRuntimeState runtime;
    const full_engine::TerrainSetupStageQueueApplyResult result =
        full_engine::queueTerrainSetupStagePlan(runtime, plan, false);

    expect(result.result == full_engine::TerrainSetupStageQueueResult::Success, "queue add no resident succeeds", failures);
    expect(result.summary.queuedSetupCount == 1, "queue add no resident counts setup", failures);
    expect(result.summary.queuedMakeResidentCount == 0, "queue add no resident skips resident", failures);
    expect(runtime.setupRequestCount() == 1, "queue add no resident adds setup", failures);
    expect(runtime.residencyRequestCount() == 0, "queue add no resident has no residency", failures);
}

void testQueueStagePlanRemoveAndKeep(std::vector<std::string>& failures)
{
    full_engine::TerrainSetupStagePlan plan;
    plan.operations.push_back({
        chunk(12),
        full_engine::TerrainSetupStageAction::Keep,
        worldDesc(chunk(12)),
        resources(chunk(12)),
        true,
        true,
        true});
    plan.operations.push_back({
        chunk(13),
        full_engine::TerrainSetupStageAction::Remove,
        worldDesc(chunk(13)),
        {},
        true,
        true,
        true});
    plan.summary.keepCount = 1;
    plan.summary.removeCount = 1;

    full_engine::TerrainRuntimeState runtime;
    const full_engine::TerrainSetupStageQueueApplyResult result =
        full_engine::queueTerrainSetupStagePlan(runtime, plan);

    expect(result.result == full_engine::TerrainSetupStageQueueResult::Success, "queue remove/keep succeeds", failures);
    expect(result.summary.queuedSetupCount == 1, "queue remove counts one setup request", failures);
    expect(result.summary.queuedMakeResidentCount == 0, "queue remove does not queue residency", failures);
    expect(result.summary.skippedKeepCount == 1, "queue keep is skipped", failures);
    expect(runtime.setupRequestCount() == 1, "queue remove adds one setup request", failures);
    expect(runtime.residencyRequestCount() == 0, "queue remove has no residency request", failures);
}

void testQueueStagePlanBlocksUnsupportedChanges(std::vector<std::string>& failures)
{
    full_engine::TerrainSetupStagePlan plan;
    plan.operations.push_back({
        chunk(14),
        full_engine::TerrainSetupStageAction::Add,
        worldDesc(chunk(14)),
        resources(chunk(14)),
        false,
        false,
        false});
    plan.operations.push_back({
        chunk(15),
        full_engine::TerrainSetupStageAction::ChangedUnsupported,
        worldDesc(chunk(15)),
        resources(chunk(15)),
        true,
        true,
        true});
    plan.summary.addCount = 1;
    plan.summary.changedUnsupportedCount = 1;

    full_engine::TerrainRuntimeState runtime;
    const full_engine::TerrainSetupStageQueueApplyResult result =
        full_engine::queueTerrainSetupStagePlan(runtime, plan);

    expect(result.result == full_engine::TerrainSetupStageQueueResult::BlockedUnsupportedChanges, "unsupported changes block queueing", failures);
    expect(result.summary.skippedChangedCount == 1, "unsupported changes are counted", failures);
    expect(result.summary.queuedSetupCount == 0, "blocked plan queues no setup", failures);
    expect(result.summary.queuedMakeResidentCount == 0, "blocked plan queues no residency", failures);
    expect(runtime.setupRequestCount() == 0, "blocked plan leaves setup empty", failures);
    expect(runtime.residencyRequestCount() == 0, "blocked plan leaves residency empty", failures);
}

void testEmptyInputs(std::vector<std::string>& failures)
{
    const full_engine::TerrainSetupStagePlan plan = full_engine::planTerrainSetupChanges(
        {},
        {},
        {},
        nullptr,
        0);
    expect(plan.operations.empty(), "empty inputs emit no ops", failures);
    expect(plan.summary.addCount == 0 && plan.summary.keepCount == 0 && plan.summary.removeCount == 0 && plan.summary.changedUnsupportedCount == 0, "empty summary is zero", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testMissingCurrentAdds(failures);
    testMatchingCurrentKeeps(failures);
    testAbsentDesiredRemoves(failures);
    testChangedUnsupported(failures);
    testMixedOrderAndRequestConversion(failures);
    testDuplicateDesiredIds(failures);
    testQueueStagePlanAddResidentDefault(failures);
    testQueueStagePlanAddCanSkipResident(failures);
    testQueueStagePlanRemoveAndKeep(failures);
    testQueueStagePlanBlocksUnsupportedChanges(failures);
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
