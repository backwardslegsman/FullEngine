#include "engine/renderer_integration/TerrainManifestRuntimeStaging.hpp"
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

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

full_engine::ChunkId chunk(const std::int32_t x) noexcept
{
    return {x, 0, 0};
}

full_engine::AssetRecord assetRecord(const std::uint64_t id, const full_engine::AssetKind kind) noexcept
{
    full_engine::AssetRecord record;
    record.id = asset(id);
    record.kind = kind;
    return record;
}

full_engine::TerrainChunkAssetDesc terrainAssets(const full_engine::ChunkId& id, const std::uint64_t meshId = 1)
{
    full_engine::TerrainChunkAssetDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = asset(meshId);
    desc.lods[0].material = asset(2);
    desc.lods[0].maxDistanceMeters = 64.0f;
    desc.splatMap = asset(3);
    return desc;
}

full_engine::WorldChunkDesc worldDesc(const full_engine::ChunkId& id, const double offset = 0.0) noexcept
{
    full_engine::WorldChunkDesc desc;
    desc.id = id;
    desc.bounds.min = {offset, 0.0, 0.0};
    desc.bounds.max = {offset + 16.0, 4.0, 16.0};
    return desc;
}

full_engine::RendererAssetHandleCatalog handles(const bool includeMesh = true)
{
    full_engine::RendererAssetHandleCatalog catalog;
    if (includeMesh)
    {
        (void)catalog.addMeshHandle(asset(1), {10});
        (void)catalog.addMeshHandle(asset(4), {40});
    }
    (void)catalog.addMaterialHandle(asset(2), {20});
    (void)catalog.addTextureHandle(asset(3), {30});
    return catalog;
}

full_engine::CookedAssetManifest manifestFor(const std::vector<full_engine::TerrainChunkAssetDesc>& terrain)
{
    full_engine::CookedAssetManifest manifest;
    manifest.assets.push_back(assetRecord(1, full_engine::AssetKind::Mesh));
    manifest.assets.push_back(assetRecord(2, full_engine::AssetKind::Material));
    manifest.assets.push_back(assetRecord(3, full_engine::AssetKind::Texture));
    manifest.assets.push_back(assetRecord(4, full_engine::AssetKind::Mesh));
    manifest.terrainChunks = terrain;
    return manifest;
}

full_engine::CookedAssetManifest validManifest()
{
    return manifestFor({terrainAssets(chunk(1))});
}

void addCurrent(
    full_engine::WorldChunkRegistry& registry,
    full_engine::WorldChunkCatalog& worldCatalog,
    full_engine::TerrainResourceCatalog& resources,
    const full_engine::WorldChunkDesc& world,
    const full_engine::TerrainChunkResourceDesc& resource)
{
    (void)registry.createChunk(world.id);
    (void)worldCatalog.addChunk(world);
    (void)resources.addChunkResources(resource);
}

full_engine::TerrainChunkResourceDesc resourceFor(const full_engine::ChunkId& id, const std::uint32_t meshId = 10)
{
    full_engine::TerrainChunkResourceDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = {meshId};
    desc.lods[0].material = {20};
    desc.lods[0].maxDistanceMeters = 64.0f;
    desc.splatMap = {30};
    return desc;
}

void testValidManifestProducesPlan(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    const full_engine::WorldChunkDesc world[] = {worldDesc(chunk(1))};

    const full_engine::TerrainManifestRuntimeStageResult result =
        full_engine::stageTerrainManifestRuntime(
            validManifest(),
            handles(),
            registry,
            worldCatalog,
            resources,
            world,
            1);

    expect(result.status == full_engine::TerrainManifestRuntimeStageStatus::Success, "valid manifest stages successfully", failures);
    expect(result.stagePlan.summary.addCount == 1, "valid manifest plans add", failures);
    expect(result.summary.desiredSetupCount == 1, "valid manifest counts desired setup", failures);
    expect(result.summary.resolvedResourceCount == 1, "valid manifest resolves resources", failures);
}

void testQueueWhenSafeQueuesRequests(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    full_engine::TerrainRuntimeState runtime;
    const full_engine::WorldChunkDesc world[] = {worldDesc(chunk(1))};

    full_engine::TerrainManifestRuntimeStageOptions options;
    options.queueWhenSafe = true;
    const full_engine::TerrainManifestRuntimeStageResult result =
        full_engine::stageTerrainManifestRuntime(
            validManifest(),
            handles(),
            registry,
            worldCatalog,
            resources,
            world,
            1,
            &runtime,
            options);

    expect(result.status == full_engine::TerrainManifestRuntimeStageStatus::Success, "safe stage queues successfully", failures);
    expect(result.queue.summary.queuedSetupCount == 1, "safe stage queues setup", failures);
    expect(result.queue.summary.queuedMakeResidentCount == 1, "safe stage queues resident", failures);
    expect(runtime.setupRequestCount() == 1, "runtime owns queued setup", failures);
    expect(runtime.residencyRequestCount() == 1, "runtime owns queued residency", failures);
}

void testInvalidManifestDoesNotQueue(std::vector<std::string>& failures)
{
    full_engine::CookedAssetManifest manifest = validManifest();
    manifest.assets[0].id = {};
    full_engine::TerrainRuntimeState runtime;
    const full_engine::WorldChunkDesc world[] = {worldDesc(chunk(1))};
    full_engine::TerrainManifestRuntimeStageOptions options;
    options.queueWhenSafe = true;

    const full_engine::TerrainManifestRuntimeStageResult result =
        full_engine::stageTerrainManifestRuntime(
            manifest,
            handles(),
            {},
            {},
            {},
            world,
            1,
            &runtime,
            options);

    expect(result.status == full_engine::TerrainManifestRuntimeStageStatus::InvalidManifest, "invalid manifest fails staging", failures);
    expect(runtime.setupRequestCount() == 0 && runtime.residencyRequestCount() == 0, "invalid manifest queues nothing", failures);
}

void testMissingWorldDescReportsPartialPlan(std::vector<std::string>& failures)
{
    const full_engine::CookedAssetManifest manifest =
        manifestFor({terrainAssets(chunk(1)), terrainAssets(chunk(2))});
    const full_engine::WorldChunkDesc world[] = {worldDesc(chunk(1))};

    const full_engine::TerrainManifestRuntimeStageResult result =
        full_engine::stageTerrainManifestRuntime(
            manifest,
            handles(),
            {},
            {},
            {},
            world,
            1);

    expect(result.status == full_engine::TerrainManifestRuntimeStageStatus::MissingWorldDesc, "missing world desc reports status", failures);
    expect(result.summary.missingWorldDescCount == 1, "missing world desc counted", failures);
    expect(result.stagePlan.summary.addCount == 1, "missing world desc still returns partial plan", failures);
}

void testMissingHandleReportsAssetResolveFailure(std::vector<std::string>& failures)
{
    const full_engine::WorldChunkDesc world[] = {worldDesc(chunk(1))};
    const full_engine::TerrainManifestRuntimeStageResult result =
        full_engine::stageTerrainManifestRuntime(
            validManifest(),
            handles(false),
            {},
            {},
            {},
            world,
            1);

    expect(result.status == full_engine::TerrainManifestRuntimeStageStatus::AssetResolveFailed, "missing handle reports asset resolve failure", failures);
    expect(result.assetResolve.summary.missingMeshHandleCount == 1, "missing mesh handle counted", failures);
    expect(result.stagePlan.operations.empty(), "missing handle produces no stage ops", failures);
}

void testQueueBlockedForUnsupportedChanges(std::vector<std::string>& failures)
{
    full_engine::WorldChunkRegistry registry;
    full_engine::WorldChunkCatalog worldCatalog;
    full_engine::TerrainResourceCatalog resources;
    addCurrent(registry, worldCatalog, resources, worldDesc(chunk(1), 32.0), resourceFor(chunk(1)));
    full_engine::TerrainRuntimeState runtime;
    const full_engine::WorldChunkDesc world[] = {worldDesc(chunk(1))};
    full_engine::TerrainManifestRuntimeStageOptions options;
    options.queueWhenSafe = true;

    const full_engine::TerrainManifestRuntimeStageResult result =
        full_engine::stageTerrainManifestRuntime(
            validManifest(),
            handles(),
            registry,
            worldCatalog,
            resources,
            world,
            1,
            &runtime,
            options);

    expect(result.status == full_engine::TerrainManifestRuntimeStageStatus::QueueBlocked, "unsupported changes block queueing", failures);
    expect(result.stagePlan.summary.changedUnsupportedCount == 1, "unsupported changes counted", failures);
    expect(runtime.setupRequestCount() == 0 && runtime.residencyRequestCount() == 0, "blocked queue mutates no requests", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testValidManifestProducesPlan(failures);
    testQueueWhenSafeQueuesRequests(failures);
    testInvalidManifestDoesNotQueue(failures);
    testMissingWorldDescReportsPartialPlan(failures);
    testMissingHandleReportsAssetResolveFailure(failures);
    testQueueBlockedForUnsupportedChanges(failures);

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
