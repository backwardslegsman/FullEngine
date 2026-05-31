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

full_renderer::TerrainChunkHandle terrainHandle(const std::uint32_t id, const std::uint32_t generation)
{
    return full_renderer::TerrainChunkHandle{id, generation};
}

full_engine::WorldChunkDesc worldDesc(const full_engine::ChunkId& id, const double minX = 0.0)
{
    full_engine::WorldChunkDesc desc;
    desc.id = id;
    desc.bounds.min = {minX, 0.0, 0.0};
    desc.bounds.max = {minX + 10.0, 10.0, 10.0};
    return desc;
}

full_engine::TerrainChunkResourceDesc terrainResources(const full_engine::ChunkId& id)
{
    full_engine::TerrainChunkResourceDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = full_renderer::MeshHandle{100 + static_cast<std::uint32_t>(id.x)};
    desc.lods[0].material = full_renderer::MaterialHandle{200 + static_cast<std::uint32_t>(id.x)};
    desc.lods[0].maxDistanceMeters = 100.0f;
    return desc;
}

class FakeRenderer final : public full_renderer::IRenderer
{
public:
    std::uint32_t nextTerrainId = 10;
    bool failCreate = false;
    int createCalls = 0;
    int updateCalls = 0;
    int destroyCalls = 0;

    full_renderer::RendererResult initialize(const full_renderer::RendererInitDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    void shutdown() noexcept override {}

    bool isInitialized() const noexcept override
    {
        return true;
    }

    full_renderer::MeshHandle createMesh(const full_renderer::MeshDesc&) override
    {
        return {};
    }

    void destroyMesh(full_renderer::MeshHandle) noexcept override {}

    full_renderer::SkeletonHandle createSkeleton(const full_renderer::SkeletonDesc&) override
    {
        return {};
    }

    void destroySkeleton(full_renderer::SkeletonHandle) noexcept override {}

    full_renderer::SkinnedMeshHandle createSkinnedMesh(const full_renderer::SkinnedMeshDesc&) override
    {
        return {};
    }

    void destroySkinnedMesh(full_renderer::SkinnedMeshHandle) noexcept override {}

    full_renderer::TextureHandle createTexture(const full_renderer::TextureDesc&) override
    {
        return {};
    }

    void destroyTexture(full_renderer::TextureHandle) noexcept override {}

    full_renderer::MaterialHandle createMaterial(const full_renderer::MaterialDesc&) override
    {
        return {};
    }

    void destroyMaterial(full_renderer::MaterialHandle) noexcept override {}

    full_renderer::TerrainChunkHandle createTerrainChunk(const full_renderer::TerrainChunkDesc&) override
    {
        ++createCalls;
        if (failCreate)
        {
            return {};
        }
        return terrainHandle(nextTerrainId++, 1);
    }

    full_renderer::RendererResult updateTerrainChunk(
        full_renderer::TerrainChunkHandle,
        const full_renderer::TerrainChunkDesc&) override
    {
        ++updateCalls;
        return full_renderer::RendererResult::Success;
    }

    void destroyTerrainChunk(full_renderer::TerrainChunkHandle) noexcept override
    {
        ++destroyCalls;
    }

    full_renderer::RendererResult resize(const full_renderer::RendererResizeDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult beginFrame(const full_renderer::FrameDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult submit(const full_renderer::RenderPacket&) override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult endFrame() override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererStats getStats() const noexcept override
    {
        return {};
    }

    full_renderer::TerrainStats getTerrainStats() const noexcept override
    {
        return {};
    }

    std::uint32_t copyTerrainDebugInfo(full_renderer::TerrainChunkDebugInfo*, std::uint32_t) const noexcept override
    {
        return 0;
    }

    std::uint32_t copyTerrainBatchDebugInfo(full_renderer::TerrainBatchDebugInfo*, std::uint32_t) const noexcept override
    {
        return 0;
    }

    std::uint32_t copyTerrainShadowCasterDebugInfo(
        full_renderer::TerrainChunkDebugInfo*,
        std::uint32_t) const noexcept override
    {
        return 0;
    }
};

struct RuntimeFixture
{
    FakeRenderer renderer;
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    full_engine::ChunkTerrainHandleMap handles;
    full_engine::TerrainChunkRequestQueue setupRequests;
    full_engine::WorldChunkResidencyRequestQueue residencyRequests;
};

full_engine::TerrainRuntimeUpdateResult update(RuntimeFixture& fixture)
{
    return full_engine::updateTerrainRuntime(
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles,
        fixture.setupRequests,
        fixture.residencyRequests);
}

void queueAddResident(RuntimeFixture& fixture, const full_engine::ChunkId& id)
{
    fixture.setupRequests.pushAdd(worldDesc(id), terrainResources(id));
    fixture.residencyRequests.push(id, full_engine::WorldChunkResidencyRequestType::MakeResident);
}

void testSetupAddAndMakeResidentCreatesTerrain(std::vector<std::string>& failures)
{
    RuntimeFixture fixture;
    const full_engine::ChunkId id{1, 0, 0};
    queueAddResident(fixture, id);

    const full_engine::TerrainRuntimeUpdateResult result = update(fixture);

    expect(result.status == full_engine::TerrainRuntimeUpdateStatus::Success, "add resident update succeeds", failures);
    expect(result.setup.summary.appliedCount == 1, "setup add is applied", failures);
    expect(result.residency.summary.appliedCount == 1, "residency request is applied", failures);
    expect(result.pipeline.submission.summary.createdCount == 1, "pipeline creates terrain chunk", failures);
    expect(result.diagnostics.setupRequests.addCount == 1, "diagnostics count setup add", failures);
    expect(result.diagnostics.residencyRequests.makeResidentCount == 1, "diagnostics count make resident", failures);
    expect(result.diagnostics.pipeline.handleCount == 1, "diagnostics count mapped handle", failures);
    expect(fixture.registry.contains(id), "registry contains added chunk", failures);
    expect(fixture.worldCatalog.contains(id), "catalog contains added chunk", failures);
    expect(fixture.resources.contains(id), "resources contain added chunk", failures);
    expect(fixture.handles.contains(id), "handle map contains created terrain", failures);
    expect(fixture.setupRequests.requestCount() == 0, "setup queue is cleared", failures);
    expect(fixture.residencyRequests.requestCount() == 0, "residency queue is cleared", failures);
}

void testSetupRemoveReleasesTerrain(std::vector<std::string>& failures)
{
    RuntimeFixture fixture;
    const full_engine::ChunkId id{2, 0, 0};
    queueAddResident(fixture, id);
    (void)update(fixture);

    fixture.setupRequests.pushRemove(id);
    const full_engine::TerrainRuntimeUpdateResult result = update(fixture);

    expect(result.status == full_engine::TerrainRuntimeUpdateStatus::Success, "remove update succeeds", failures);
    expect(result.setup.summary.appliedCount == 1, "setup remove is applied", failures);
    expect(result.pipeline.submission.summary.destroyedCount == 1, "pipeline destroys removed setup handle", failures);
    expect(result.diagnostics.pipeline.handleCount == 0, "diagnostics report no mapped handles", failures);
    expect(!fixture.registry.contains(id), "registry no longer contains removed chunk", failures);
    expect(!fixture.worldCatalog.contains(id), "catalog no longer contains removed chunk", failures);
    expect(!fixture.resources.contains(id), "resources no longer contain removed chunk", failures);
    expect(!fixture.handles.contains(id), "handle map no longer contains removed chunk", failures);
    expect(fixture.renderer.destroyCalls == 1, "renderer destroy is called once", failures);
}

void testResidencyUnloadAndReload(std::vector<std::string>& failures)
{
    RuntimeFixture fixture;
    const full_engine::ChunkId id{3, 0, 0};
    queueAddResident(fixture, id);
    (void)update(fixture);

    fixture.residencyRequests.push(id, full_engine::WorldChunkResidencyRequestType::MakeUnloaded);
    const full_engine::TerrainRuntimeUpdateResult unload = update(fixture);
    expect(unload.status == full_engine::TerrainRuntimeUpdateStatus::Success, "unload succeeds", failures);
    expect(unload.residency.summary.appliedCount == 1, "unload residency request is applied", failures);
    expect(unload.pipeline.submission.summary.destroyedCount == 1, "unload destroys mapped terrain", failures);
    expect(fixture.handles.mappedCount() == 0, "unload clears handle map", failures);

    fixture.residencyRequests.push(id, full_engine::WorldChunkResidencyRequestType::MakeResident);
    const full_engine::TerrainRuntimeUpdateResult reload = update(fixture);
    expect(reload.status == full_engine::TerrainRuntimeUpdateStatus::Success, "reload succeeds", failures);
    expect(reload.residency.summary.appliedCount == 1, "reload residency request is applied", failures);
    expect(reload.pipeline.submission.summary.createdCount == 1, "reload recreates terrain", failures);
    expect(fixture.handles.mappedCount() == 1, "reload maps terrain handle", failures);
}

void testInvalidSetupFailsBeforePipeline(std::vector<std::string>& failures)
{
    RuntimeFixture fixture;
    const full_engine::ChunkId id{4, 0, 0};
    full_engine::TerrainChunkResourceDesc invalidResources = terrainResources(id);
    invalidResources.lodCount = 0;
    fixture.setupRequests.pushAdd(worldDesc(id), invalidResources);

    const full_engine::TerrainRuntimeUpdateResult result = update(fixture);

    expect(result.status == full_engine::TerrainRuntimeUpdateStatus::SetupFailed, "invalid setup fails update", failures);
    expect(result.setup.summary.invalidArgumentCount == 1, "invalid setup is counted", failures);
    expect(result.diagnostics.setupRequests.summary.invalidArgumentCount == 1, "diagnostics count invalid setup", failures);
    expect(result.pipeline.submission.operations.empty(), "pipeline is not run after setup failure", failures);
    expect(fixture.setupRequests.requestCount() == 0, "setup queue is cleared after setup failure", failures);
    expect(fixture.handles.mappedCount() == 0, "invalid setup does not mutate handle map", failures);
}

void testUnregisteredResidencyRequestIsDiagnosticNotFailure(std::vector<std::string>& failures)
{
    RuntimeFixture fixture;
    const full_engine::ChunkId id{5, 0, 0};
    fixture.residencyRequests.push(id, full_engine::WorldChunkResidencyRequestType::MakeResident);

    const full_engine::TerrainRuntimeUpdateResult result = update(fixture);

    expect(result.status == full_engine::TerrainRuntimeUpdateStatus::Success, "unregistered residency is not fatal", failures);
    expect(result.residency.summary.notFoundCount == 1, "unregistered residency is counted as not found", failures);
    expect(result.diagnostics.residencyRequests.makeResidentCount == 1, "diagnostics count skipped make resident", failures);
    expect(result.diagnostics.residencyRequests.summary.notFoundCount == 1, "diagnostics count skipped not found", failures);
    expect(fixture.residencyRequests.requestCount() == 0, "residency queue is cleared", failures);
    expect(fixture.renderer.createCalls == 0, "unregistered residency does not create terrain", failures);
}

void testRendererFailureReportsPipelineFailure(std::vector<std::string>& failures)
{
    RuntimeFixture fixture;
    fixture.renderer.failCreate = true;
    const full_engine::ChunkId id{6, 0, 0};
    queueAddResident(fixture, id);

    const full_engine::TerrainRuntimeUpdateResult result = update(fixture);

    expect(result.status == full_engine::TerrainRuntimeUpdateStatus::PipelineFailed, "renderer failure fails pipeline", failures);
    expect(result.pipeline.submission.summary.rendererFailedCount == 1, "renderer failure is counted", failures);
    expect(result.diagnostics.pipeline.submission.rendererFailedCount == 1, "diagnostics count renderer failure", failures);
    expect(fixture.handles.mappedCount() == 0, "failed create does not map terrain handle", failures);
}

void testRuntimeStateOwnsQueuesAndLatestResult(std::vector<std::string>& failures)
{
    RuntimeFixture fixture;
    full_engine::TerrainRuntimeState state;
    const full_engine::ChunkId id{7, 0, 0};

    expect(state.setupRequestCount() == 0, "default state has no setup requests", failures);
    expect(state.residencyRequestCount() == 0, "default state has no residency requests", failures);
    expect(!state.hasPendingRequests(), "default state has no pending requests", failures);
    expect(state.latestUpdate().status == full_engine::TerrainRuntimeUpdateStatus::Success, "default latest status is success", failures);
    expect(state.latestDiagnostics().pipeline.handleCount == 0, "default latest diagnostics are empty", failures);
    expect(state.eventCount() == 0, "default state has no runtime events", failures);
    expect(state.latestEvent() == nullptr, "default latest event is null", failures);

    state.queueSetupAdd(worldDesc(id), terrainResources(id));
    state.queueMakeResident(id);
    expect(state.setupRequestCount() == 1, "state queues setup add", failures);
    expect(state.residencyRequestCount() == 1, "state queues residency request", failures);
    expect(state.hasPendingRequests(), "state reports pending setup/residency requests", failures);

    const full_engine::TerrainRuntimeUpdateResult& addResult = state.update(
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles);
    expect(addResult.status == full_engine::TerrainRuntimeUpdateStatus::Success, "state update succeeds", failures);
    expect(state.setupRequestCount() == 0, "state update clears setup requests", failures);
    expect(state.residencyRequestCount() == 0, "state update clears residency requests", failures);
    expect(!state.hasPendingRequests(), "state update clears pending request state", failures);
    expect(state.latestUpdate().pipeline.submission.summary.createdCount == 1, "state stores latest create result", failures);
    expect(state.latestDiagnostics().pipeline.handleCount == 1, "state stores latest diagnostics", failures);
    expect(state.eventCount() == 1, "state update appends runtime event", failures);
    expect(state.latestEvent() != nullptr, "state latest event exists after update", failures);
    if (state.latestEvent() != nullptr)
    {
        expect(state.latestEvent()->sequence == 1, "first runtime event sequence is one", failures);
        expect(state.latestEvent()->status == full_engine::TerrainRuntimeUpdateStatus::Success, "runtime event stores success status", failures);
        expect(state.latestEvent()->diagnostics.pipeline.handleCount == 1, "runtime event stores copied diagnostics", failures);
    }
    expect(fixture.handles.contains(id), "state update maps terrain handle", failures);

    state.queueMakeUnloaded(id);
    expect(state.residencyRequestCount() == 1, "state queues pending unload", failures);
    expect(state.hasPendingRequests(), "state reports pending unload", failures);
    state.clearRequests();
    expect(state.residencyRequestCount() == 0, "state clearRequests clears residency queue", failures);
    expect(!state.hasPendingRequests(), "state clearRequests clears pending state", failures);
    expect(state.latestDiagnostics().pipeline.handleCount == 1, "state clearRequests preserves latest diagnostics", failures);
    expect(state.eventCount() == 1, "state clearRequests preserves runtime events", failures);

    state.queueSetupRemove(id);
    const full_engine::TerrainRuntimeUpdateResult& removeResult = state.update(
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles);
    expect(removeResult.status == full_engine::TerrainRuntimeUpdateStatus::Success, "state remove update succeeds", failures);
    expect(state.latestUpdate().pipeline.submission.summary.destroyedCount == 1, "state stores latest destroy result", failures);
    expect(state.latestDiagnostics().pipeline.handleCount == 0, "state diagnostics update after remove", failures);
    expect(state.eventCount() == 2, "second state update appends second event", failures);
    expect(state.latestEvent() != nullptr && state.latestEvent()->sequence == 2, "latest event sequence advances", failures);
    expect(!fixture.handles.contains(id), "state remove releases mapped handle", failures);

    state.clearEvents();
    expect(state.eventCount() == 0, "state clearEvents clears runtime events", failures);
    expect(state.latestEvent() == nullptr, "state latest event is null after clearEvents", failures);
    expect(state.latestDiagnostics().pipeline.handleCount == 0, "state clearEvents preserves latest diagnostics", failures);
}

void testRuntimeStatePreservesFailureStatus(std::vector<std::string>& failures)
{
    RuntimeFixture fixture;
    full_engine::TerrainRuntimeState state;
    const full_engine::ChunkId id{8, 0, 0};
    full_engine::TerrainChunkResourceDesc invalidResources = terrainResources(id);
    invalidResources.lodCount = 0;

    state.queueSetupAdd(worldDesc(id), invalidResources);
    const full_engine::TerrainRuntimeUpdateResult& result = state.update(
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles);

    expect(result.status == full_engine::TerrainRuntimeUpdateStatus::SetupFailed, "state update preserves setup failure", failures);
    expect(state.latestUpdate().status == full_engine::TerrainRuntimeUpdateStatus::SetupFailed, "state latest update stores failure", failures);
    expect(state.latestDiagnostics().setupRequests.summary.invalidArgumentCount == 1, "state latest diagnostics store failure", failures);
    expect(state.setupRequestCount() == 0, "state clears setup queue after failure", failures);
    expect(state.eventCount() == 1, "state failure update appends runtime event", failures);
    expect(state.latestEvent() != nullptr, "state latest event exists after failure", failures);
    if (state.latestEvent() != nullptr)
    {
        expect(state.latestEvent()->status == full_engine::TerrainRuntimeUpdateStatus::SetupFailed, "failure event stores status", failures);
        expect(
            state.latestEvent()->diagnostics.setupRequests.summary.invalidArgumentCount == 1,
            "failure event stores copied diagnostics",
            failures);
    }
}

void testRuntimeEventLogCapacityAndOrdering(std::vector<std::string>& failures)
{
    full_engine::TerrainRuntimeEventLog log;
    expect(log.eventCount() == 0, "default event log is empty", failures);
    expect(log.latestEvent() == nullptr, "default event log latest is null", failures);

    for (std::size_t index = 0; index < full_engine::kTerrainRuntimeEventLogCapacity + 5; ++index)
    {
        full_engine::TerrainRuntimeUpdateResult result;
        result.status = index % 2 == 0
            ? full_engine::TerrainRuntimeUpdateStatus::Success
            : full_engine::TerrainRuntimeUpdateStatus::PipelineFailed;
        result.diagnostics.pipeline.handleCount = index;
        log.append(result);
    }

    expect(
        log.eventCount() == full_engine::kTerrainRuntimeEventLogCapacity,
        "event log retains fixed capacity",
        failures);

    const std::vector<full_engine::TerrainRuntimeEvent> events = log.events();
    expect(events.size() == full_engine::kTerrainRuntimeEventLogCapacity, "event snapshot size matches count", failures);
    expect(events.front().sequence == 6, "event log drops oldest overflow events", failures);
    expect(events.back().sequence == full_engine::kTerrainRuntimeEventLogCapacity + 5, "event log keeps newest event", failures);
    expect(events.front().diagnostics.pipeline.handleCount == 5, "oldest retained event keeps copied diagnostics", failures);
    expect(
        log.latestEvent() != nullptr &&
            log.latestEvent()->sequence == full_engine::kTerrainRuntimeEventLogCapacity + 5,
        "latest event points at newest retained event",
        failures);

    log.clear();
    expect(log.eventCount() == 0, "event log clear removes events", failures);
    expect(log.latestEvent() == nullptr, "event log clear removes latest pointer", failures);

    full_engine::TerrainRuntimeUpdateResult result;
    log.append(result);
    expect(log.latestEvent() != nullptr && log.latestEvent()->sequence == 1, "event log clear restarts sequence", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testSetupAddAndMakeResidentCreatesTerrain(failures);
    testSetupRemoveReleasesTerrain(failures);
    testResidencyUnloadAndReload(failures);
    testInvalidSetupFailsBeforePipeline(failures);
    testUnregisteredResidencyRequestIsDiagnosticNotFailure(failures);
    testRendererFailureReportsPipelineFailure(failures);
    testRuntimeStateOwnsQueuesAndLatestResult(failures);
    testRuntimeStatePreservesFailureStatus(failures);
    testRuntimeEventLogCapacityAndOrdering(failures);

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
