#include "engine/renderer_integration/TerrainDescriptorBuilder.hpp"
#include "engine/renderer_integration/TerrainRendererCommands.hpp"
#include "engine/renderer_integration/TerrainResourceCatalog.hpp"
#include "engine/renderer_integration/TerrainSubmissionAdapter.hpp"
#include "engine/renderer_integration/TerrainRenderPrep.hpp"
#include "engine/renderer_integration/WorldRenderSnapshot.hpp"

#include "full_renderer/Renderer.hpp"

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

bool sameId(const full_engine::ChunkId& lhs, const full_engine::ChunkId& rhs)
{
    return lhs == rhs;
}

full_engine::WorldBounds worldBounds(const double minX)
{
    full_engine::WorldBounds bounds;
    bounds.min = {minX, 0.0, 0.0};
    bounds.max = {minX + 10.0, 10.0, 10.0};
    return bounds;
}

full_engine::WorldChunkRenderDesc chunkDesc(const full_engine::ChunkId& id, const double minX)
{
    full_engine::WorldChunkRenderDesc desc;
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

void makeResident(full_engine::WorldChunkRegistry& registry, const full_engine::ChunkId& id)
{
    registry.createChunk(id);
    registry.setResidencyState(id, full_engine::ChunkResidencyState::Loading);
    registry.setResidencyState(id, full_engine::ChunkResidencyState::Resident);
}

class FakeRenderer final : public full_renderer::IRenderer
{
public:
    std::uint32_t nextTerrainId = 10;
    int createCalls = 0;
    int updateCalls = 0;
    int destroyCalls = 0;
    std::vector<full_renderer::TerrainChunkHandle> createdHandles;
    std::vector<full_renderer::TerrainChunkHandle> updatedHandles;
    std::vector<full_renderer::TerrainChunkHandle> destroyedHandles;

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
        const full_renderer::TerrainChunkHandle handle = terrainHandle(nextTerrainId++, 1);
        createdHandles.push_back(handle);
        return handle;
    }

    full_renderer::RendererResult updateTerrainChunk(
        const full_renderer::TerrainChunkHandle handle,
        const full_renderer::TerrainChunkDesc&) override
    {
        ++updateCalls;
        updatedHandles.push_back(handle);
        return full_renderer::RendererResult::Success;
    }

    void destroyTerrainChunk(const full_renderer::TerrainChunkHandle handle) noexcept override
    {
        ++destroyCalls;
        destroyedHandles.push_back(handle);
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

struct PipelineResult
{
    full_engine::WorldRenderSnapshot snapshot;
    full_engine::TerrainRenderPrep prep;
    full_engine::TerrainLifecyclePlan lifecycle;
    full_engine::TerrainRendererCommandList commands;
    full_engine::TerrainDescriptorBuildResult descriptors;
    full_engine::TerrainSubmissionResult submission;
};

PipelineResult runPipeline(
    full_engine::WorldChunkRegistry& registry,
    const std::vector<full_engine::WorldChunkRenderDesc>& chunkDescs,
    const full_engine::TerrainResourceCatalog& resources,
    full_engine::ChunkTerrainHandleMap& handles,
    FakeRenderer& renderer,
    const full_engine::TerrainLifecyclePlanOptions& lifecycleOptions = {})
{
    PipelineResult result;
    result.snapshot = full_engine::buildWorldRenderSnapshot(
        registry,
        chunkDescs.data(),
        chunkDescs.size(),
        {});
    result.prep = full_engine::prepareTerrainRenderChunks(result.snapshot);
    result.lifecycle = full_engine::planTerrainLifecycle(result.prep, handles, lifecycleOptions);
    result.commands = full_engine::buildTerrainRendererCommands(result.lifecycle);
    result.descriptors = full_engine::buildTerrainDescriptors(result.commands, resources);
    result.submission = full_engine::submitTerrainCommands(renderer, result.descriptors, result.commands, handles);
    return result;
}

void testCreateThenUpdateAndDestroy(std::vector<std::string>& failures)
{
    const full_engine::ChunkId first{1, 0, 0};
    const full_engine::ChunkId second{2, 0, 0};
    full_engine::WorldChunkRegistry registry;
    makeResident(registry, first);
    makeResident(registry, second);

    std::vector<full_engine::WorldChunkRenderDesc> chunks;
    chunks.push_back(chunkDesc(first, 0.0));
    chunks.push_back(chunkDesc(second, 20.0));

    full_engine::TerrainResourceCatalog catalog;
    catalog.addChunkResources(resources(first));
    catalog.addChunkResources(resources(second));

    full_engine::ChunkTerrainHandleMap handles;
    FakeRenderer renderer;

    PipelineResult firstPass = runPipeline(registry, chunks, catalog, handles, renderer);

    expect(firstPass.snapshot.readyCount == 2, "first pass snapshot has two ready chunks", failures);
    expect(firstPass.prep.summary.readyCount == 2, "first pass prep has two ready chunks", failures);
    expect(firstPass.lifecycle.summary.createCount == 2, "first pass lifecycle creates two chunks", failures);
    expect(firstPass.commands.summary.createCount == 2, "first pass commands create two chunks", failures);
    expect(firstPass.descriptors.summary.readyCount == 2, "first pass descriptors are ready", failures);
    expect(firstPass.submission.summary.createdCount == 2, "first pass submission creates two chunks", failures);
    expect(renderer.createCalls == 2, "first pass calls renderer create twice", failures);
    expect(handles.mappedCount() == 2, "first pass maps two terrain handles", failures);
    expect(handles.contains(first), "first chunk is mapped after create", failures);
    expect(handles.contains(second), "second chunk is mapped after create", failures);

    registry.setResidencyState(second, full_engine::ChunkResidencyState::Unloading);

    full_engine::TerrainLifecyclePlanOptions options;
    options.updateMappedReadyChunks = true;
    PipelineResult secondPass = runPipeline(registry, chunks, catalog, handles, renderer, options);

    expect(secondPass.snapshot.readyCount == 1, "second pass snapshot has one ready chunk", failures);
    expect(secondPass.snapshot.notResidentCount == 1, "second pass snapshot has one non-resident chunk", failures);
    expect(secondPass.prep.summary.readyCount == 1, "second pass prep has one ready chunk", failures);
    expect(secondPass.prep.summary.skippedNotResidentCount == 1, "second pass prep skips non-resident chunk", failures);
    expect(secondPass.lifecycle.summary.updateCount == 1, "second pass lifecycle updates kept resident chunk", failures);
    expect(secondPass.lifecycle.summary.releaseCount == 1, "second pass lifecycle releases non-resident mapped chunk", failures);
    expect(secondPass.commands.summary.updateCount == 1, "second pass command list updates one chunk", failures);
    expect(secondPass.commands.summary.destroyCount == 1, "second pass command list destroys one chunk", failures);
    expect(secondPass.descriptors.summary.readyCount == 1, "second pass descriptors include one update descriptor", failures);
    expect(secondPass.descriptors.summary.ignoredCount == 1, "second pass descriptors ignore destroy command", failures);
    expect(secondPass.submission.summary.updatedCount == 1, "second pass submission updates one chunk", failures);
    expect(secondPass.submission.summary.destroyedCount == 1, "second pass submission destroys one chunk", failures);
    expect(renderer.updateCalls == 1, "second pass calls renderer update once", failures);
    expect(renderer.destroyCalls == 1, "second pass calls renderer destroy once", failures);
    expect(handles.mappedCount() == 1, "second pass leaves one mapped terrain handle", failures);
    expect(handles.contains(first), "first chunk remains mapped after update", failures);
    expect(!handles.contains(second), "second chunk is removed from map after destroy", failures);
}

void testMissingResourcesStayDiagnosticAndDoNotCallRenderer(std::vector<std::string>& failures)
{
    const full_engine::ChunkId missing{3, 0, 0};
    full_engine::WorldChunkRegistry registry;
    makeResident(registry, missing);

    std::vector<full_engine::WorldChunkRenderDesc> chunks;
    chunks.push_back(chunkDesc(missing, 40.0));

    const full_engine::TerrainResourceCatalog emptyCatalog;
    full_engine::ChunkTerrainHandleMap handles;
    FakeRenderer renderer;

    PipelineResult result = runPipeline(registry, chunks, emptyCatalog, handles, renderer);

    expect(result.snapshot.readyCount == 1, "missing-resource path still has ready world chunk", failures);
    expect(result.prep.summary.readyCount == 1, "missing-resource path reaches terrain prep", failures);
    expect(result.lifecycle.summary.createCount == 1, "missing-resource path creates lifecycle intent", failures);
    expect(result.commands.summary.createCount == 1, "missing-resource path creates renderer command intent", failures);
    expect(result.descriptors.summary.missingResourcesCount == 1, "missing-resource path is reported by descriptor builder", failures);
    expect(result.submission.summary.skippedCount == 1, "missing-resource path skips submission", failures);
    expect(renderer.createCalls == 0, "missing resources do not call renderer create", failures);
    expect(handles.mappedCount() == 0, "missing resources do not mutate handle map", failures);
    expect(result.submission.operations.size() == 1, "missing-resource path has one submission op", failures);
    expect(sameId(result.submission.operations[0].id, missing), "missing-resource submission op preserves chunk id", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testCreateThenUpdateAndDestroy(failures);
    testMissingResourcesStayDiagnosticAndDoNotCallRenderer(failures);

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
