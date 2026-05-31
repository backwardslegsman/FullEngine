#include "engine/renderer_integration/TerrainManifestFileLoad.hpp"

namespace full_engine
{
const char* terrainManifestFileLoadStatusName(const TerrainManifestFileLoadStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestFileLoadStatus::Success:
        return "Success";
    case TerrainManifestFileLoadStatus::InvalidArgument:
        return "InvalidArgument";
    case TerrainManifestFileLoadStatus::ImportFailed:
        return "ImportFailed";
    }

    return "Unknown";
}

TerrainManifestFileLoadResult loadTerrainManifestFileIntoState(
    const char* const path,
    TerrainManifestLoadState& state)
{
    TerrainManifestFileLoadResult result;

    if (path == nullptr || path[0] == '\0')
    {
        state.clearManifest();
        result.status = TerrainManifestFileLoadStatus::InvalidArgument;
        result.imported.result = CookedAssetManifestImportResult::InvalidArgument;
        return result;
    }

    result.imported = importCookedAssetManifestJsonLines(path);
    if (result.imported.result != CookedAssetManifestImportResult::Success)
    {
        state.clearManifest();
        result.status = TerrainManifestFileLoadStatus::ImportFailed;
        return result;
    }

    result.assetCount = result.imported.manifest.assets.size();
    result.terrainChunkCount = result.imported.manifest.terrainChunks.size();
    state.setManifest(result.imported.manifest);
    result.status = TerrainManifestFileLoadStatus::Success;
    return result;
}

TerrainManifestFileReloadPlanResult reloadTerrainManifestFileAndQueueMissingAssetLoads(
    const char* const path,
    TerrainManifestLoadState& state,
    const RendererAssetHandleCatalog& handles)
{
    TerrainManifestFileReloadPlanResult result;
    result.load = loadTerrainManifestFileIntoState(path, state);
    if (result.load.status != TerrainManifestFileLoadStatus::Success)
    {
        return result;
    }

    result.readiness = state.planAssetReadiness(handles);
    result.loadRequests = state.planAssetLoadRequests();
    result.queue = state.queueLatestAssetLoadRequests();
    return result;
}
} // namespace full_engine
