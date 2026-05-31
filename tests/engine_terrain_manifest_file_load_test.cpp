#include "engine/assets/CookedAssetManifestJson.hpp"
#include "engine/renderer_integration/TerrainManifestFileLoad.hpp"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <fstream>
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

void writeText(const char* const path, const char* const text)
{
    std::ofstream output(path, std::ios::out | std::ios::trunc);
    output << text;
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

full_engine::TerrainChunkAssetDesc terrainAssets()
{
    full_engine::TerrainChunkAssetDesc desc;
    desc.id = {4, 0, -2};
    desc.lodCount = 1;
    desc.lods[0].mesh = asset(10);
    desc.lods[0].material = asset(20);
    desc.lods[0].maxDistanceMeters = 96.0f;
    desc.splatMap = asset(30);
    return desc;
}

full_engine::CookedAssetManifest validManifest()
{
    full_engine::CookedAssetManifest manifest;
    manifest.assets.push_back(assetRecord(10, full_engine::AssetKind::Mesh));
    manifest.assets.push_back(assetRecord(20, full_engine::AssetKind::Material));
    manifest.assets.push_back(assetRecord(30, full_engine::AssetKind::Texture));
    manifest.terrainChunks.push_back(terrainAssets());
    return manifest;
}

void seedStaleLoadState(full_engine::TerrainManifestLoadState& state)
{
    state.setManifest(validManifest());
    (void)state.planAssetReadiness({});
    (void)state.planAssetLoadRequests();
    (void)state.queueLatestAssetLoadRequests();
}

full_engine::RendererAssetHandleCatalog handles()
{
    full_engine::RendererAssetHandleCatalog catalog;
    (void)catalog.addMeshHandle(asset(10), {10});
    (void)catalog.addMaterialHandle(asset(20), {20});
    (void)catalog.addTextureHandle(asset(30), {30});
    return catalog;
}

void writeValidManifest(const char* const path)
{
    std::remove(path);
    const full_engine::CookedAssetManifestExportResult exported =
        full_engine::exportCookedAssetManifestJsonLines(validManifest(), path);
    (void)exported;
}

void testValidManifestLoadsIntoState(std::vector<std::string>& failures)
{
    const char* path = "terrain_manifest_file_load_valid.jsonl";
    writeValidManifest(path);

    full_engine::TerrainManifestLoadState state;
    seedStaleLoadState(state);
    expect(state.pendingLoadRequestCount() == 3, "stale state has pending load requests before load", failures);

    const full_engine::TerrainManifestFileLoadResult loaded =
        full_engine::loadTerrainManifestFileIntoState(path, state);

    expect(loaded.status == full_engine::TerrainManifestFileLoadStatus::Success, "valid file load succeeds", failures);
    expect(loaded.imported.result == full_engine::CookedAssetManifestImportResult::Success, "valid file import succeeds", failures);
    expect(loaded.assetCount == 3, "valid file reports asset count", failures);
    expect(loaded.terrainChunkCount == 1, "valid file reports terrain count", failures);
    expect(state.hasManifest(), "valid file retains manifest", failures);
    expect(state.manifest().assets.size() == 3, "retained manifest has assets", failures);
    expect(state.manifest().terrainChunks.size() == 1, "retained manifest has terrain chunks", failures);
    expect(state.latestReadiness().records.empty(), "successful load clears stale readiness", failures);
    expect(state.latestLoadRequests().requests.empty(), "successful load clears stale load request plan", failures);
    expect(state.pendingLoadRequestCount() == 0, "successful load clears pending load queue", failures);
    expect(
        full_engine::terrainManifestFileLoadStatusName(full_engine::TerrainManifestFileLoadStatus::ImportFailed) ==
            std::string("ImportFailed"),
        "file load status names are stable",
        failures);

    std::remove(path);
}

void testReloadAndReplanQueuesMissingHandles(std::vector<std::string>& failures)
{
    const char* path = "terrain_manifest_file_reload_missing_handles.jsonl";
    writeValidManifest(path);

    full_engine::TerrainManifestLoadState state;

    const full_engine::TerrainManifestFileReloadPlanResult reloaded =
        full_engine::reloadTerrainManifestFileAndQueueMissingAssetLoads(path, state, {});

    expect(reloaded.load.status == full_engine::TerrainManifestFileLoadStatus::Success, "reload and replan load succeeds", failures);
    expect(reloaded.readiness.summary.missingHandleCount == 3, "reload and replan records missing handles", failures);
    expect(reloaded.loadRequests.summary.requestCount == 3, "reload and replan creates load requests", failures);
    expect(reloaded.queue.summary.queuedCount == 3, "reload and replan queues missing load requests", failures);
    expect(state.latestReadiness().summary.missingHandleCount == 3, "reload and replan stores readiness", failures);
    expect(state.latestLoadRequests().summary.requestCount == 3, "reload and replan stores load request plan", failures);
    expect(state.pendingLoadRequestCount() == 3, "reload and replan stores pending queue", failures);

    std::remove(path);
}

void testReloadAndReplanSkipsReadyHandles(std::vector<std::string>& failures)
{
    const char* path = "terrain_manifest_file_reload_ready_handles.jsonl";
    writeValidManifest(path);

    full_engine::TerrainManifestLoadState state;
    const full_engine::RendererAssetHandleCatalog readyHandles = handles();

    const full_engine::TerrainManifestFileReloadPlanResult reloaded =
        full_engine::reloadTerrainManifestFileAndQueueMissingAssetLoads(path, state, readyHandles);

    expect(reloaded.load.status == full_engine::TerrainManifestFileLoadStatus::Success, "ready reload load succeeds", failures);
    expect(reloaded.readiness.summary.readyCount == 3, "ready reload records ready handles", failures);
    expect(reloaded.loadRequests.summary.requestCount == 0, "ready reload creates no load requests", failures);
    expect(reloaded.queue.summary.queuedCount == 0, "ready reload queues nothing", failures);
    expect(state.pendingLoadRequestCount() == 0, "ready reload leaves pending queue empty", failures);

    std::remove(path);
}

void testReloadAndReplanFailureSkipsPlanning(std::vector<std::string>& failures)
{
    const char* path = "terrain_manifest_file_reload_missing.jsonl";
    std::remove(path);

    full_engine::TerrainManifestLoadState state;
    seedStaleLoadState(state);

    const full_engine::TerrainManifestFileReloadPlanResult reloaded =
        full_engine::reloadTerrainManifestFileAndQueueMissingAssetLoads(path, state, {});

    expect(reloaded.load.status == full_engine::TerrainManifestFileLoadStatus::ImportFailed, "failed reload reports load failure", failures);
    expect(reloaded.readiness.records.empty(), "failed reload does not plan readiness", failures);
    expect(reloaded.loadRequests.requests.empty(), "failed reload does not plan load requests", failures);
    expect(reloaded.queue.records.empty(), "failed reload does not queue load requests", failures);
    expect(!state.hasManifest(), "failed reload clears manifest", failures);
    expect(state.pendingLoadRequestCount() == 0, "failed reload clears stale pending queue", failures);
}

void testInvalidPathClearsState(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestLoadState state;
    seedStaleLoadState(state);

    const full_engine::TerrainManifestFileLoadResult nullPath =
        full_engine::loadTerrainManifestFileIntoState(nullptr, state);
    expect(nullPath.status == full_engine::TerrainManifestFileLoadStatus::InvalidArgument, "null path is invalid", failures);
    expect(nullPath.imported.result == full_engine::CookedAssetManifestImportResult::InvalidArgument, "null path import detail is invalid", failures);
    expect(!state.hasManifest(), "null path clears manifest", failures);
    expect(state.pendingLoadRequestCount() == 0, "null path clears pending load queue", failures);

    state.setManifest(validManifest());
    const full_engine::TerrainManifestFileLoadResult emptyPath =
        full_engine::loadTerrainManifestFileIntoState("", state);
    expect(emptyPath.status == full_engine::TerrainManifestFileLoadStatus::InvalidArgument, "empty path is invalid", failures);
    expect(emptyPath.imported.result == full_engine::CookedAssetManifestImportResult::InvalidArgument, "empty path import detail is invalid", failures);
    expect(!state.hasManifest(), "empty path clears manifest", failures);
}

void testMissingFileClearsState(std::vector<std::string>& failures)
{
    const char* path = "terrain_manifest_file_load_missing.jsonl";
    std::remove(path);

    full_engine::TerrainManifestLoadState state;
    state.setManifest(validManifest());

    const full_engine::TerrainManifestFileLoadResult loaded =
        full_engine::loadTerrainManifestFileIntoState(path, state);

    expect(loaded.status == full_engine::TerrainManifestFileLoadStatus::ImportFailed, "missing file load fails", failures);
    expect(loaded.imported.result == full_engine::CookedAssetManifestImportResult::IoError, "missing file preserves IO error", failures);
    expect(!state.hasManifest(), "missing file clears manifest", failures);
    expect(loaded.assetCount == 0, "failed missing file has no asset count", failures);
    expect(loaded.terrainChunkCount == 0, "failed missing file has no terrain count", failures);
}

void testMalformedFileClearsState(std::vector<std::string>& failures)
{
    const char* path = "terrain_manifest_file_load_malformed.jsonl";
    writeText(path, "{not json\n");

    full_engine::TerrainManifestLoadState state;
    state.setManifest(validManifest());

    const full_engine::TerrainManifestFileLoadResult loaded =
        full_engine::loadTerrainManifestFileIntoState(path, state);

    expect(loaded.status == full_engine::TerrainManifestFileLoadStatus::ImportFailed, "malformed file load fails", failures);
    expect(loaded.imported.result == full_engine::CookedAssetManifestImportResult::ParseError, "malformed file preserves parse error", failures);
    expect(!state.hasManifest(), "malformed file clears manifest", failures);

    std::remove(path);
}

void testValidationFailureClearsState(std::vector<std::string>& failures)
{
    const char* path = "terrain_manifest_file_load_invalid_manifest.jsonl";
    writeText(path, "{\"record\":\"asset\",\"id\":0,\"kind\":\"Mesh\",\"dependencyCount\":0}\n");

    full_engine::TerrainManifestLoadState state;
    state.setManifest(validManifest());

    const full_engine::TerrainManifestFileLoadResult loaded =
        full_engine::loadTerrainManifestFileIntoState(path, state);

    expect(loaded.status == full_engine::TerrainManifestFileLoadStatus::ImportFailed, "invalid manifest load fails", failures);
    expect(
        loaded.imported.result == full_engine::CookedAssetManifestImportResult::ValidationError,
        "invalid manifest preserves validation error",
        failures);
    expect(
        loaded.imported.validation.result == full_engine::CookedAssetManifestValidationResult::InvalidAssetRecord,
        "invalid manifest preserves validation detail",
        failures);
    expect(!state.hasManifest(), "invalid manifest clears retained state", failures);
    expect(state.latestReadiness().records.empty(), "invalid manifest clears stale readiness", failures);

    std::remove(path);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testValidManifestLoadsIntoState(failures);
    testReloadAndReplanQueuesMissingHandles(failures);
    testReloadAndReplanSkipsReadyHandles(failures);
    testReloadAndReplanFailureSkipsPlanning(failures);
    testInvalidPathClearsState(failures);
    testMissingFileClearsState(failures);
    testMalformedFileClearsState(failures);
    testValidationFailureClearsState(failures);

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
