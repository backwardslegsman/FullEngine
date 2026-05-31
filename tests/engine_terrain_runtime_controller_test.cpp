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
    expect(!state.hasLatestSnapshot(), "default state has no latest snapshot", failures);
    expect(state.latestSnapshot().chunks.empty(), "default latest snapshot is empty", failures);
    expect(state.latestSnapshotDiff().changes.empty(), "default latest snapshot diff is empty", failures);

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
    expect(!state.hasLatestSnapshot(), "plain state update does not create snapshot tracking", failures);
    expect(state.latestSnapshotDiff().changes.empty(), "plain state update leaves snapshot diff empty", failures);
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

void testRuntimeStateUpdateWithSnapshotTracksFirstAddedChunk(std::vector<std::string>& failures)
{
    RuntimeFixture fixture;
    full_engine::TerrainRuntimeState state;
    const full_engine::ChunkId id{9, 0, 0};
    const std::vector<full_engine::ChunkId> trackedIds{id};

    state.queueSetupAdd(worldDesc(id), terrainResources(id));
    state.queueMakeResident(id);

    const full_engine::TerrainRuntimeUpdateResult& result = state.updateWithSnapshot(
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles,
        trackedIds.data(),
        trackedIds.size());

    expect(result.status == full_engine::TerrainRuntimeUpdateStatus::Success, "snapshot update succeeds", failures);
    expect(state.hasLatestSnapshot(), "snapshot update stores latest snapshot", failures);
    expect(state.latestSnapshot().chunks.size() == 1, "snapshot tracks one chunk", failures);
    expect(state.latestSnapshot().renderableCount == 1, "tracked chunk is renderable", failures);
    expect(state.latestSnapshotDiff().summary.addedCount == 1, "first snapshot reports tracked chunk added", failures);
    expect(state.latestSnapshotDiff().changes.size() == 1, "first snapshot has one diff record", failures);
    if (!state.latestSnapshotDiff().changes.empty())
    {
        const full_engine::TerrainRuntimeStateChange& change = state.latestSnapshotDiff().changes.front();
        expect(change.id == id, "added snapshot diff stores chunk id", failures);
        expect(
            change.type == full_engine::TerrainRuntimeStateChangeType::Added,
            "first snapshot diff reports added chunk",
            failures);
        expect(
            change.currentReadiness == full_engine::TerrainRuntimeChunkReadiness::Renderable,
            "added snapshot diff stores renderable readiness",
            failures);
    }
    expect(state.eventCount() == 1, "snapshot update still appends runtime event", failures);
    expect(fixture.handles.contains(id), "snapshot update maps terrain handle", failures);
}

void testRuntimeStateUpdateWithSnapshotReportsNoChangeOnStableState(std::vector<std::string>& failures)
{
    RuntimeFixture fixture;
    full_engine::TerrainRuntimeState state;
    const full_engine::ChunkId id{10, 0, 0};
    const std::vector<full_engine::ChunkId> trackedIds{id};

    state.queueSetupAdd(worldDesc(id), terrainResources(id));
    state.queueMakeResident(id);
    (void)state.updateWithSnapshot(
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles,
        trackedIds.data(),
        trackedIds.size());

    const full_engine::TerrainRuntimeUpdateResult& result = state.updateWithSnapshot(
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles,
        trackedIds.data(),
        trackedIds.size());

    expect(result.status == full_engine::TerrainRuntimeUpdateStatus::Success, "stable snapshot update succeeds", failures);
    expect(state.latestSnapshotDiff().changes.empty(), "stable snapshot update reports no changes", failures);
    expect(state.latestSnapshotDiff().summary.addedCount == 0, "stable snapshot has no added changes", failures);
    expect(state.latestSnapshot().renderableCount == 1, "stable snapshot remains renderable", failures);
    expect(state.eventCount() == 2, "stable snapshot update still appends event", failures);
}

void testRuntimeStateUpdateWithSnapshotReportsResidencyReadinessChange(std::vector<std::string>& failures)
{
    RuntimeFixture fixture;
    full_engine::TerrainRuntimeState state;
    const full_engine::ChunkId id{11, 0, 0};
    const std::vector<full_engine::ChunkId> trackedIds{id};

    state.queueSetupAdd(worldDesc(id), terrainResources(id));
    state.queueMakeResident(id);
    (void)state.updateWithSnapshot(
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles,
        trackedIds.data(),
        trackedIds.size());

    state.queueMakeUnloaded(id);
    const full_engine::TerrainRuntimeUpdateResult& result = state.updateWithSnapshot(
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles,
        trackedIds.data(),
        trackedIds.size());

    expect(result.status == full_engine::TerrainRuntimeUpdateStatus::Success, "unload snapshot update succeeds", failures);
    expect(state.latestSnapshot().notResidentCount == 1, "unload snapshot reports not resident", failures);
    expect(state.latestSnapshotDiff().summary.readinessChangedCount == 1, "unload snapshot reports readiness change", failures);
    expect(state.latestSnapshotDiff().changes.size() == 1, "unload snapshot has one diff record", failures);
    if (!state.latestSnapshotDiff().changes.empty())
    {
        const full_engine::TerrainRuntimeStateChange& change = state.latestSnapshotDiff().changes.front();
        expect(
            change.type == full_engine::TerrainRuntimeStateChangeType::ReadinessChanged,
            "unload snapshot diff uses readiness change",
            failures);
        expect(
            change.previousReadiness == full_engine::TerrainRuntimeChunkReadiness::Renderable,
            "unload diff previous readiness is renderable",
            failures);
        expect(
            change.currentReadiness == full_engine::TerrainRuntimeChunkReadiness::NotResident,
            "unload diff current readiness is not resident",
            failures);
    }
}

void testRuntimeStateUpdateWithSnapshotKeepsTrackedIdentityAfterSetupRemove(std::vector<std::string>& failures)
{
    RuntimeFixture fixture;
    full_engine::TerrainRuntimeState state;
    const full_engine::ChunkId id{12, 0, 0};
    const std::vector<full_engine::ChunkId> trackedIds{id};

    state.queueSetupAdd(worldDesc(id), terrainResources(id));
    state.queueMakeResident(id);
    (void)state.updateWithSnapshot(
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles,
        trackedIds.data(),
        trackedIds.size());

    state.queueSetupRemove(id);
    const full_engine::TerrainRuntimeUpdateResult& result = state.updateWithSnapshot(
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles,
        trackedIds.data(),
        trackedIds.size());

    expect(result.status == full_engine::TerrainRuntimeUpdateStatus::Success, "remove snapshot update succeeds", failures);
    expect(state.latestSnapshot().chunks.size() == 1, "removed setup snapshot still contains tracked id", failures);
    expect(state.latestSnapshot().missingRegistryCount == 1, "removed setup snapshot reports missing registry", failures);
    expect(state.latestSnapshotDiff().summary.readinessChangedCount == 1, "removed setup reports readiness change", failures);
    expect(state.latestSnapshotDiff().changes.size() == 1, "removed setup snapshot has one diff record", failures);
    if (!state.latestSnapshotDiff().changes.empty())
    {
        const full_engine::TerrainRuntimeStateChange& change = state.latestSnapshotDiff().changes.front();
        expect(change.id == id, "removed setup diff keeps chunk id", failures);
        expect(
            change.currentReadiness == full_engine::TerrainRuntimeChunkReadiness::MissingRegistry,
            "removed setup diff current readiness is missing registry",
            failures);
    }
}

void testRuntimeStateClearSnapshotTrackingPreservesOtherState(std::vector<std::string>& failures)
{
    RuntimeFixture fixture;
    full_engine::TerrainRuntimeState state;
    const full_engine::ChunkId id{13, 0, 0};
    const std::vector<full_engine::ChunkId> trackedIds{id};

    state.queueSetupAdd(worldDesc(id), terrainResources(id));
    state.queueMakeResident(id);
    (void)state.updateWithSnapshot(
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles,
        trackedIds.data(),
        trackedIds.size());
    state.queueMakeUnloaded(id);

    state.clearSnapshotTracking();

    expect(!state.hasLatestSnapshot(), "clear snapshot tracking clears presence flag", failures);
    expect(state.latestSnapshot().chunks.empty(), "clear snapshot tracking clears latest snapshot", failures);
    expect(state.latestSnapshotDiff().changes.empty(), "clear snapshot tracking clears latest diff", failures);
    expect(state.residencyRequestCount() == 1, "clear snapshot tracking preserves pending requests", failures);
    expect(state.latestUpdate().status == full_engine::TerrainRuntimeUpdateStatus::Success, "clear snapshot tracking preserves latest update", failures);
    expect(state.eventCount() == 1, "clear snapshot tracking preserves events", failures);
}

void testRuntimeStateUpdateWithSnapshotStoresFailureSnapshot(std::vector<std::string>& failures)
{
    RuntimeFixture fixture;
    full_engine::TerrainRuntimeState state;
    const full_engine::ChunkId id{14, 0, 0};
    const std::vector<full_engine::ChunkId> trackedIds{id};
    full_engine::TerrainChunkResourceDesc invalidResources = terrainResources(id);
    invalidResources.lodCount = 0;

    state.queueSetupAdd(worldDesc(id), invalidResources);
    const full_engine::TerrainRuntimeUpdateResult& result = state.updateWithSnapshot(
        fixture.renderer,
        fixture.registry,
        fixture.worldCatalog,
        fixture.resources,
        fixture.handles,
        trackedIds.data(),
        trackedIds.size());

    expect(result.status == full_engine::TerrainRuntimeUpdateStatus::SetupFailed, "snapshot update preserves setup failure", failures);
    expect(state.hasLatestSnapshot(), "failed snapshot update still stores snapshot", failures);
    expect(state.latestSnapshot().missingRegistryCount == 1, "failed snapshot reports post-failure missing registry", failures);
    expect(state.latestSnapshotDiff().summary.addedCount == 1, "failed first snapshot still reports tracked id added", failures);
    expect(state.eventCount() == 1, "failed snapshot update appends event", failures);
    expect(state.latestEvent() != nullptr && state.latestEvent()->status == full_engine::TerrainRuntimeUpdateStatus::SetupFailed, "failed snapshot event stores status", failures);
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
    testRuntimeStateUpdateWithSnapshotTracksFirstAddedChunk(failures);
    testRuntimeStateUpdateWithSnapshotReportsNoChangeOnStableState(failures);
    testRuntimeStateUpdateWithSnapshotReportsResidencyReadinessChange(failures);
    testRuntimeStateUpdateWithSnapshotKeepsTrackedIdentityAfterSetupRemove(failures);
    testRuntimeStateClearSnapshotTrackingPreservesOtherState(failures);
    testRuntimeStateUpdateWithSnapshotStoresFailureSnapshot(failures);
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
