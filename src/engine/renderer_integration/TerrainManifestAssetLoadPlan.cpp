#include "engine/renderer_integration/TerrainManifestAssetLoadPlan.hpp"

namespace full_engine
{
namespace
{
AssetKind toAssetKind(const TerrainManifestAssetHandleKind kind) noexcept
{
    switch (kind)
    {
    case TerrainManifestAssetHandleKind::Mesh:
        return AssetKind::Mesh;
    case TerrainManifestAssetHandleKind::Material:
        return AssetKind::Material;
    case TerrainManifestAssetHandleKind::Texture:
        return AssetKind::Texture;
    }

    return AssetKind::Unknown;
}

void incrementSummary(TerrainManifestAssetLoadRequestSummary& summary, const AssetKind kind) noexcept
{
    ++summary.requestCount;
    switch (kind)
    {
    case AssetKind::Mesh:
        ++summary.meshRequestCount;
        break;
    case AssetKind::Material:
        ++summary.materialRequestCount;
        break;
    case AssetKind::Texture:
        ++summary.textureRequestCount;
        break;
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        break;
    }
}
} // namespace

TerrainManifestAssetLoadRequestPlan buildTerrainManifestAssetLoadRequestPlan(
    const TerrainManifestAssetReadinessPlan& readiness)
{
    TerrainManifestAssetLoadRequestPlan plan;
    plan.requests.reserve(readiness.summary.missingHandleCount);

    for (const TerrainManifestAssetReadinessRecord& record : readiness.records)
    {
        if (record.status != TerrainManifestAssetReadinessStatus::MissingHandle)
        {
            continue;
        }

        TerrainManifestAssetLoadRequest request;
        request.id = record.id;
        request.kind = toAssetKind(record.kind);
        incrementSummary(plan.summary, request.kind);
        plan.requests.push_back(request);
    }

    return plan;
}
} // namespace full_engine
