#include "engine/renderer_integration/TerrainManifestDevAssetLoadCallback.hpp"

#include "engine/renderer_integration/TerrainManifestAssetLoadJobCoordinator.hpp"
#include "engine/renderer_integration/TerrainManifestLoadState.hpp"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#ifndef FULL_RENDERER_TEST_FIXTURE_DIR
#define FULL_RENDERER_TEST_FIXTURE_DIR "."
#endif

namespace
{
void expect(const bool condition, const char* const message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

std::string fixturePath(const char* const name)
{
    return std::string(FULL_RENDERER_TEST_FIXTURE_DIR) + "/" + name;
}

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

full_engine::AssetRecord assetRecord(
    const std::uint64_t id,
    const full_engine::AssetKind kind) noexcept
{
    full_engine::AssetRecord record;
    record.id = asset(id);
    record.kind = kind;
    return record;
}

full_engine::TerrainManifestAssetLoadRequest request(
    const std::uint64_t id,
    const full_engine::AssetKind kind) noexcept
{
    full_engine::TerrainManifestAssetLoadRequest result;
    result.id = asset(id);
    result.kind = kind;
    return result;
}

full_engine::AssetSourceDescriptor meshDescriptor()
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.mesh.vertexCount = 3;
    descriptor.mesh.indexCount = 3;
    descriptor.mesh.localBounds.min[0] = 0.0f;
    descriptor.mesh.localBounds.min[1] = 0.0f;
    descriptor.mesh.localBounds.min[2] = 0.0f;
    descriptor.mesh.localBounds.max[0] = 1.0f;
    descriptor.mesh.localBounds.max[1] = 1.0f;
    descriptor.mesh.localBounds.max[2] = 0.0f;
    return descriptor;
}

full_engine::AssetSourceDescriptor textureDescriptor()
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.texture.width = 2;
    descriptor.texture.height = 2;
    descriptor.texture.mipCount = 1;
    descriptor.texture.format = full_engine::AssetSourceTextureFormat::Rgba8;
    descriptor.texture.semantic = full_engine::AssetSourceTextureSemantic::Color;
    descriptor.texture.colorSpace = full_engine::AssetSourceTextureColorSpace::Srgb;
    return descriptor;
}

full_engine::AssetSourceDescriptor materialDescriptor()
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.material.model = full_engine::AssetSourceMaterialModel::Basic;
    descriptor.material.alphaMode = full_engine::AssetSourceMaterialAlphaMode::Opaque;
    descriptor.material.textureRefs[0] = asset(3);
    descriptor.material.textureRefCount = 1;
    return descriptor;
}

full_engine::AssetSourceRecord source(
    const std::uint64_t id,
    const full_engine::AssetKind kind,
    const std::string& uri,
    const full_engine::AssetSourceDescriptor& descriptor)
{
    full_engine::AssetSourceRecord record;
    record.id = asset(id);
    record.kind = kind;
    record.uri = uri;
    record.descriptor = descriptor;
    return record;
}

full_engine::AssetSourceCatalog sourceCatalog()
{
    full_engine::AssetSourceCatalog catalog;
    (void)catalog.addSource(source(
        1,
        full_engine::AssetKind::Mesh,
        fixturePath("dev_triangle.fmeshdev"),
        meshDescriptor()));
    (void)catalog.addSource(source(
        3,
        full_engine::AssetKind::Texture,
        fixturePath("dev_checker_2x2.ftexdev"),
        textureDescriptor()));
    (void)catalog.addSource(source(
        2,
        full_engine::AssetKind::Material,
        fixturePath("dev_basic_texture3.fmatdev"),
        materialDescriptor()));
    return catalog;
}

full_engine::AssetSourceCatalog mismatchedSourceCatalog()
{
    full_engine::AssetSourceCatalog catalog;
    full_engine::AssetSourceDescriptor descriptor = meshDescriptor();
    descriptor.mesh.vertexCount = 4;
    (void)catalog.addSource(source(
        1,
        full_engine::AssetKind::Mesh,
        fixturePath("dev_triangle.fmeshdev"),
        descriptor));
    return catalog;
}

full_engine::TerrainChunkAssetDesc terrainAssets()
{
    full_engine::TerrainChunkAssetDesc desc;
    desc.id = {1, 0, 0};
    desc.lodCount = 1;
    desc.lods[0].mesh = asset(1);
    desc.lods[0].material = asset(2);
    desc.lods[0].maxDistanceMeters = 64.0f;
    desc.splatMap = asset(3);
    return desc;
}

full_engine::CookedAssetManifest manifest()
{
    full_engine::CookedAssetManifest result;
    result.assets.push_back(assetRecord(1, full_engine::AssetKind::Mesh));
    result.assets.push_back(assetRecord(2, full_engine::AssetKind::Material));
    result.assets.push_back(assetRecord(3, full_engine::AssetKind::Texture));
    result.terrainChunks.push_back(terrainAssets());
    return result;
}

class FakeRenderer final : public full_renderer::IRenderer
{
public:
    int meshCreateCalls = 0;
    int textureCreateCalls = 0;
    int materialCreateCalls = 0;
    full_renderer::MeshHandle nextMesh = {101};
    full_renderer::TextureHandle nextTexture = {303};
    full_renderer::MaterialHandle nextMaterial = {202};

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
        ++meshCreateCalls;
        return nextMesh;
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
        ++textureCreateCalls;
        return nextTexture;
    }

    void destroyTexture(full_renderer::TextureHandle) noexcept override {}

    full_renderer::MaterialHandle createMaterial(const full_renderer::MaterialDesc&) override
    {
        ++materialCreateCalls;
        return nextMaterial;
    }

    void destroyMaterial(full_renderer::MaterialHandle) noexcept override {}

    full_renderer::TerrainChunkHandle createTerrainChunk(const full_renderer::TerrainChunkDesc&) override
    {
        return {};
    }

    full_renderer::RendererResult updateTerrainChunk(
        full_renderer::TerrainChunkHandle,
        const full_renderer::TerrainChunkDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    void destroyTerrainChunk(full_renderer::TerrainChunkHandle) noexcept override {}

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

void queueLoadRequests(
    full_engine::TerrainManifestLoadState& state,
    const full_engine::RendererAssetHandleCatalog& readinessHandles)
{
    state.setManifest(manifest());
    (void)state.planAssetReadiness(readinessHandles);
    (void)state.planAssetLoadRequests();
    (void)state.queueLatestAssetLoadRequests();
}

void testCallbackImportsMeshAndTexture(std::vector<std::string>& failures)
{
    const full_engine::AssetSourceCatalog sources = sourceCatalog();
    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog completed;
    full_engine::TerrainManifestDevAssetLoadContext context;
    context.sources = &sources;
    context.renderer = &renderer;
    context.completedHandles = &completed;

    const full_engine::TerrainManifestAssetLoadCallbackResult mesh =
        full_engine::terrainManifestDevAssetLoadCallback(
            request(1, full_engine::AssetKind::Mesh),
            &context);
    const full_engine::TerrainManifestAssetLoadCallbackResult texture =
        full_engine::terrainManifestDevAssetLoadCallback(
            request(3, full_engine::AssetKind::Texture),
            &context);
    const full_engine::TerrainManifestAssetLoadCallbackResult material =
        full_engine::terrainManifestDevAssetLoadCallback(
            request(2, full_engine::AssetKind::Material),
            &context);

    expect(mesh.status == full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded, "mesh dev callback loads", failures);
    expect(texture.status == full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded, "texture dev callback loads", failures);
    expect(material.status == full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded, "material dev callback loads after texture", failures);
    expect(renderer.meshCreateCalls == 1, "mesh callback uploads once", failures);
    expect(renderer.textureCreateCalls == 1, "texture callback uploads once", failures);
    expect(renderer.materialCreateCalls == 1, "material callback uploads once", failures);
    expect(completed.findMeshHandle(asset(1)) != nullptr, "mesh callback stores completed mesh handle", failures);
    expect(completed.findTextureHandle(asset(3)) != nullptr, "texture callback stores completed texture handle", failures);
    expect(completed.findMaterialHandle(asset(2)) != nullptr, "material callback stores completed material handle", failures);

    const full_engine::TerrainManifestAssetLoadCallbackResult secondMesh =
        full_engine::terrainManifestDevAssetLoadCallback(
            request(1, full_engine::AssetKind::Mesh),
            &context);
    expect(secondMesh.status == full_engine::TerrainManifestAssetLoadCallbackStatus::Loaded, "completed handle satisfies second mesh request", failures);
    expect(renderer.meshCreateCalls == 1, "completed handle avoids duplicate mesh upload", failures);
}

void testCallbackFailures(std::vector<std::string>& failures)
{
    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog completed;
    const full_engine::AssetSourceCatalog sources = sourceCatalog();
    full_engine::TerrainManifestDevAssetLoadContext context;
    context.sources = &sources;
    context.renderer = &renderer;
    context.completedHandles = &completed;

    const full_engine::TerrainManifestAssetLoadCallbackResult missing =
        full_engine::terrainManifestDevAssetLoadCallback(
            request(99, full_engine::AssetKind::Mesh),
            &context);
    expect(missing.status == full_engine::TerrainManifestAssetLoadCallbackStatus::Missing, "missing source remains retryable", failures);

    const full_engine::AssetSourceCatalog mismatched = mismatchedSourceCatalog();
    context.sources = &mismatched;
    const full_engine::TerrainManifestAssetLoadCallbackResult mismatch =
        full_engine::terrainManifestDevAssetLoadCallback(
            request(1, full_engine::AssetKind::Mesh),
            &context);
    expect(mismatch.status == full_engine::TerrainManifestAssetLoadCallbackStatus::Failed, "descriptor mismatch fails callback", failures);

    context.sources = &sources;
    renderer.nextMesh = {};
    const full_engine::TerrainManifestAssetLoadCallbackResult rendererFailed =
        full_engine::terrainManifestDevAssetLoadCallback(
            request(1, full_engine::AssetKind::Mesh),
            &context);
    expect(rendererFailed.status == full_engine::TerrainManifestAssetLoadCallbackStatus::Failed, "invalid renderer handle fails callback", failures);
    expect(completed.findMeshHandle(asset(1)) == nullptr, "failed renderer upload does not publish completed handle", failures);

    const full_engine::TerrainManifestAssetLoadCallbackResult materialMissingTexture =
        full_engine::terrainManifestDevAssetLoadCallback(
            request(2, full_engine::AssetKind::Material),
            &context);
    expect(materialMissingTexture.status == full_engine::TerrainManifestAssetLoadCallbackStatus::Missing, "material missing texture handle is retryable", failures);
}

void testRetainedServiceFlowReconcilesReady(std::vector<std::string>& failures)
{
    full_engine::RendererAssetHandleCatalog destination;
    full_engine::TerrainManifestLoadState state;
    queueLoadRequests(state, destination);

    full_engine::EngineJobQueue jobs;
    const full_engine::TerrainManifestAssetLoadJobScheduleResult scheduled =
        full_engine::scheduleTerrainManifestAssetLoadJobs(state, jobs);
    expect(scheduled.mirror.summary.queuedCount == 3, "service flow schedules mesh material and texture jobs", failures);

    full_engine::TerrainManifestAssetLoadService service;
    const full_engine::TerrainManifestAssetLoadJobWorkPacketResult packets =
        full_engine::buildTerrainManifestAssetLoadJobWorkPackets(jobs);
    (void)service.enqueueWorkPackets(packets);

    const full_engine::AssetSourceCatalog sources = sourceCatalog();
    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog completed;
    full_engine::TerrainManifestDevAssetLoadContext context;
    context.sources = &sources;
    context.renderer = &renderer;
    context.completedHandles = &completed;
    context.alreadyLoadedHandles = &destination;

    const full_engine::TerrainManifestAssetLoadServiceTickResult firstTick =
        service.tick(8, full_engine::terrainManifestDevAssetLoadCallback, &context);
    expect(firstTick.summary.loadedCount == 2, "service first tick loads mesh and texture", failures);
    expect(firstTick.summary.missingCount == 1, "service first tick leaves material pending", failures);
    expect(service.completions().size() == 2, "service first tick emits two completions", failures);

    const full_engine::TerrainManifestAssetLoadServiceTickResult secondTick =
        service.tick(8, full_engine::terrainManifestDevAssetLoadCallback, &context);
    expect(secondTick.summary.loadedCount == 1, "service second tick loads material", failures);
    expect(service.completions().size() == 3, "service emits three completions", failures);

    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult reconciled =
        full_engine::reconcileTerrainManifestAssetLoadJobCompletions(
            state,
            jobs,
            service.completions().data(),
            service.completions().size(),
            destination);
    expect(reconciled.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::Success, "service completions reconcile", failures);
    expect(state.pendingLoadRequestCount() == 0, "service reconcile clears retained load requests", failures);
    expect(jobs.jobCount() == 0, "service reconcile removes scheduled jobs", failures);
    expect(destination.findMeshHandle(asset(1)) != nullptr, "service reconcile stores mesh in destination", failures);
    expect(destination.findTextureHandle(asset(3)) != nullptr, "service reconcile stores texture in destination", failures);
    expect(destination.findMaterialHandle(asset(2)) != nullptr, "service reconcile stores material in destination", failures);
    const full_engine::TerrainManifestAssetReadinessPlan readiness = state.planAssetReadiness(destination);
    expect(readiness.summary.missingHandleCount == 0, "service flow leaves manifest ready", failures);
}

void testExternalWorkerPublishesAndReconciles(std::vector<std::string>& failures)
{
    full_engine::RendererAssetHandleCatalog destination;
    full_engine::TerrainManifestLoadState state;
    queueLoadRequests(state, destination);

    full_engine::EngineJobQueue jobs;
    (void)full_engine::scheduleTerrainManifestAssetLoadJobs(state, jobs);

    const full_engine::AssetSourceCatalog sources = sourceCatalog();
    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog completed;
    full_engine::TerrainManifestAssetLoadCompletionInbox inbox;
    full_engine::TerrainManifestDevAssetLoadContext context;
    context.sources = &sources;
    context.renderer = &renderer;
    context.completedHandles = &completed;
    context.alreadyLoadedHandles = &destination;

    const full_engine::TerrainManifestDevAssetLoadWorkerResult firstWorker =
        full_engine::runTerrainManifestDevAssetLoadWorker(jobs, inbox, context);
    expect(firstWorker.packets.summary.packetizedCount == 3, "dev worker packetizes scheduled jobs", failures);
    expect(firstWorker.publish.publish.summary.publishedCount == 2, "dev worker publishes mesh and texture completions", failures);
    expect(inbox.completionCount() == 2, "dev worker retains inbox completions", failures);

    const full_engine::TerrainManifestDevAssetLoadWorkerResult secondWorker =
        full_engine::runTerrainManifestDevAssetLoadWorker(jobs, inbox, context);
    expect(secondWorker.publish.publish.summary.publishedCount == 1, "dev worker second pass publishes material completion", failures);
    expect(inbox.completionCount() == 3, "dev worker retains all completions", failures);

    const full_engine::TerrainManifestAssetLoadJobCompletionReconcileResult reconciled =
        full_engine::reconcileTerrainManifestAssetLoadJobCompletions(
            state,
            jobs,
            inbox.completions().data(),
            inbox.completionCount(),
            destination);
    expect(reconciled.status == full_engine::TerrainManifestAssetLoadJobCompletionReconcileStatus::Success, "dev worker completions reconcile", failures);
    expect(destination.findMeshHandle(asset(1)) != nullptr, "dev worker reconcile stores mesh", failures);
    expect(destination.findTextureHandle(asset(3)) != nullptr, "dev worker reconcile stores texture", failures);
    expect(destination.findMaterialHandle(asset(2)) != nullptr, "dev worker reconcile stores material", failures);
    const full_engine::TerrainManifestAssetReadinessPlan readiness = state.planAssetReadiness(destination);
    expect(readiness.summary.readyCount == readiness.summary.requestedCount, "dev worker flow leaves readiness complete", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testCallbackImportsMeshAndTexture(failures);
    testCallbackFailures(failures);
    testRetainedServiceFlowReconcilesReady(failures);
    testExternalWorkerPublishesAndReconciles(failures);

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
