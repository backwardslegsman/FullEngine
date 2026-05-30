#include "engine/renderer_integration/TerrainPipeline.hpp"

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

full_engine::WorldBounds worldBounds(const double minX)
{
    full_engine::WorldBounds bounds;
    bounds.min = {minX, 0.0, 0.0};
    bounds.max = {minX + 10.0, 10.0, 10.0};
    return bounds;
}

full_engine::WorldChunkDesc chunkDesc(const full_engine::ChunkId& id, const double minX)
{
    full_engine::WorldChunkDesc desc;
    desc.id = id;
    desc.bounds = worldBounds(minX);
    return desc;
}

full_engine::TerrainChunkResourceDesc resources(const full_engine::ChunkId& id)
{
    full_engine::TerrainChunkResourceDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = full_renderer::MeshHandle{100 + static_cast<std::uint32_t>(id.x)};
    desc.lods[0].material = full_renderer::MaterialHandle{200 + static_cast<std::uint32_t>(id.x)};
    desc.lods[0].maxDistanceMeters = 250.0f;
    return desc;
}

void addWorldChunk(
    full_engine::WorldChunkRegistry& registry,
    full_engine::WorldChunkCatalog& catalog,
    const full_engine::ChunkId& id,
    const double minX)
{
    registry.createChunk(id);
    catalog.addChunk(chunkDesc(id, minX));
}

void makeResident(full_engine::WorldChunkRegistry& registry, const full_engine::ChunkId& id)
{
    registry.setResidencyState(id, full_engine::ChunkResidencyState::Loading);
    registry.setResidencyState(id, full_engine::ChunkResidencyState::Resident);
}

class FakeRenderer final : public full_renderer::IRenderer
{
public:
    std::uint32_t nextTerrainId = 10;
    full_renderer::TerrainChunkHandle nextCreateHandle = {};
    full_renderer::RendererResult nextUpdateResult = full_renderer::RendererResult::Success;
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
        if (full_renderer::isValid(nextCreateHandle))
        {
            return nextCreateHandle;
        }
        return terrainHandle(nextTerrainId++, 1);
    }

    full_renderer::RendererResult updateTerrainChunk(
        full_renderer::TerrainChunkHandle,
        const full_renderer::TerrainChunkDesc&) override
    {
        ++updateCalls;
        return nextUpdateResult;
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

void testCreateUpdateDestroyAndDiagnostics(std::vector<std::string>& failures)
{
    const full_engine::ChunkId first{1, 0, 0};
    const full_engine::ChunkId second{2, 0, 0};
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog catalog;
    addWorldChunk(registry, catalog, first, 0.0);
    addWorldChunk(registry, catalog, second, 20.0);
    makeResident(registry, first);
    makeResident(registry, second);

    full_engine::TerrainResourceCatalog resourcesCatalog;
    resourcesCatalog.addChunkResources(resources(first));
    resourcesCatalog.addChunkResources(resources(second));

    full_engine::ChunkTerrainHandleMap handles;
    FakeRenderer renderer;

    full_engine::TerrainPipelineRunResult firstPass =
        full_engine::runTerrainPipeline(renderer, registry, catalog, resourcesCatalog, handles);

    expect(firstPass.succeeded, "first pass succeeds", failures);
    expect(firstPass.snapshot.readyCount == 2, "first pass snapshot has two ready chunks", failures);
    expect(firstPass.lifecycle.summary.createCount == 2, "first pass plans two creates", failures);
    expect(firstPass.commands.summary.createCount == 2, "first pass builds two create commands", failures);
    expect(firstPass.descriptors.summary.readyCount == 2, "first pass builds two ready descriptors", failures);
    expect(firstPass.submission.summary.createdCount == 2, "first pass submits two creates", failures);
    expect(firstPass.diagnostics.handleCount == 2, "first pass diagnostics reports two handles", failures);
    expect(firstPass.diagnostics.snapshotReadyCount == 2, "first pass diagnostics copies snapshot count", failures);
    expect(firstPass.diagnostics.submission.createdCount == 2, "first pass diagnostics copies submission count", failures);
    expect(renderer.createCalls == 2, "first pass calls renderer create twice", failures);
    expect(handles.mappedCount() == 2, "first pass maps two handles", failures);

    registry.setResidencyState(second, full_engine::ChunkResidencyState::Unloading);
    full_engine::TerrainPipelineRunOptions options;
    options.lifecycleOptions.updateMappedReadyChunks = true;

    full_engine::TerrainPipelineRunResult secondPass =
        full_engine::runTerrainPipeline(renderer, registry, catalog, resourcesCatalog, handles, options);

    expect(secondPass.succeeded, "second pass succeeds", failures);
    expect(secondPass.snapshot.readyCount == 1, "second pass has one ready chunk", failures);
    expect(secondPass.snapshot.notResidentCount == 1, "second pass has one not-resident chunk", failures);
    expect(secondPass.lifecycle.summary.updateCount == 1, "second pass plans one update", failures);
    expect(secondPass.lifecycle.summary.releaseCount == 1, "second pass plans one release", failures);
    expect(secondPass.commands.summary.updateCount == 1, "second pass builds one update command", failures);
    expect(secondPass.commands.summary.destroyCount == 1, "second pass builds one destroy command", failures);
    expect(secondPass.submission.summary.updatedCount == 1, "second pass submits one update", failures);
    expect(secondPass.submission.summary.destroyedCount == 1, "second pass submits one destroy", failures);
    expect(secondPass.diagnostics.handleCount == 1, "second pass diagnostics reports one handle", failures);
    expect(secondPass.diagnostics.lifecycle.releaseCount == 1, "second pass diagnostics copies lifecycle release", failures);
    expect(renderer.updateCalls == 1, "second pass calls renderer update once", failures);
    expect(renderer.destroyCalls == 1, "second pass calls renderer destroy once", failures);
    expect(handles.mappedCount() == 1, "second pass leaves one mapped handle", failures);
    expect(handles.contains(first), "first chunk remains mapped", failures);
    expect(!handles.contains(second), "second chunk mapping is released", failures);
}

void testMissingResourcesSkipWithoutFailure(std::vector<std::string>& failures)
{
    const full_engine::ChunkId id{3, 0, 0};
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog catalog;
    addWorldChunk(registry, catalog, id, 40.0);
    makeResident(registry, id);

    const full_engine::TerrainResourceCatalog emptyResources;
    full_engine::ChunkTerrainHandleMap handles;
    FakeRenderer renderer;

    const full_engine::TerrainPipelineRunResult result =
        full_engine::runTerrainPipeline(renderer, registry, catalog, emptyResources, handles);

    expect(result.succeeded, "missing resources are diagnostic skips, not pipeline failure", failures);
    expect(result.snapshot.readyCount == 1, "missing resources still produce ready world snapshot", failures);
    expect(result.descriptors.summary.missingResourcesCount == 1, "missing resources are reported by descriptors", failures);
    expect(result.submission.summary.skippedCount == 1, "missing resources skip submission", failures);
    expect(result.diagnostics.descriptors.missingResourcesCount == 1, "diagnostics copies missing-resource count", failures);
    expect(result.diagnostics.submission.skippedCount == 1, "diagnostics copies skipped submission count", failures);
    expect(renderer.createCalls == 0, "missing resources do not create renderer terrain chunks", failures);
    expect(handles.mappedCount() == 0, "missing resources do not mutate handle map", failures);
}

void testRendererFailureMarksPipelineFailed(std::vector<std::string>& failures)
{
    const full_engine::ChunkId id{4, 0, 0};
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog catalog;
    addWorldChunk(registry, catalog, id, 60.0);
    makeResident(registry, id);

    full_engine::TerrainResourceCatalog resourcesCatalog;
    resourcesCatalog.addChunkResources(resources(id));
    full_engine::ChunkTerrainHandleMap handles;
    FakeRenderer renderer;
    renderer.nextCreateHandle = {};
    renderer.nextTerrainId = 0;

    const full_engine::TerrainPipelineRunResult result =
        full_engine::runTerrainPipeline(renderer, registry, catalog, resourcesCatalog, handles);

    expect(!result.succeeded, "invalid renderer create handle fails pipeline", failures);
    expect(result.submission.summary.rendererFailedCount == 1, "renderer failure is counted", failures);
    expect(result.diagnostics.submission.rendererFailedCount == 1, "diagnostics copies renderer failure", failures);
    expect(handles.mappedCount() == 0, "failed create does not mutate handle map", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testCreateUpdateDestroyAndDiagnostics(failures);
    testMissingResourcesSkipWithoutFailure(failures);
    testRendererFailureMarksPipelineFailed(failures);

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
