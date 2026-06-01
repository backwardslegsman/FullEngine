#include "engine/renderer_integration/TerrainManifestAssetSourcePlan.hpp"

namespace full_engine
{
namespace
{
bool isLoadableManifestAssetKind(const AssetKind kind) noexcept
{
    return kind == AssetKind::Mesh ||
        kind == AssetKind::Material ||
        kind == AssetKind::Texture;
}

void incrementSummary(
    TerrainManifestAssetSourceRequestSummary& summary,
    const TerrainManifestAssetSourceRequestStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetSourceRequestStatus::Mapped:
        ++summary.mappedCount;
        break;
    case TerrainManifestAssetSourceRequestStatus::MissingSource:
        ++summary.missingSourceCount;
        break;
    case TerrainManifestAssetSourceRequestStatus::InvalidRequest:
        ++summary.invalidRequestCount;
        break;
    }
}

TerrainManifestAssetSourceRequestRecord mapRequestToSource(
    const TerrainManifestAssetLoadRequest& request,
    const AssetSourceCatalog& sources)
{
    TerrainManifestAssetSourceRequestRecord record;
    record.request = request;

    if (!isValid(request.id) || !isLoadableManifestAssetKind(request.kind))
    {
        record.status = TerrainManifestAssetSourceRequestStatus::InvalidRequest;
        return record;
    }

    const AssetSourceRecord* source = sources.findSource(request.id);
    if (source == nullptr || source->kind != request.kind)
    {
        record.status = TerrainManifestAssetSourceRequestStatus::MissingSource;
        return record;
    }

    record.source = *source;
    record.status = TerrainManifestAssetSourceRequestStatus::Mapped;
    return record;
}
} // namespace

const char* terrainManifestAssetSourceRequestStatusName(
    const TerrainManifestAssetSourceRequestStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetSourceRequestStatus::Mapped:
        return "Mapped";
    case TerrainManifestAssetSourceRequestStatus::MissingSource:
        return "MissingSource";
    case TerrainManifestAssetSourceRequestStatus::InvalidRequest:
        return "InvalidRequest";
    }

    return "Unknown";
}

TerrainManifestAssetSourceRequestPlan buildTerrainManifestAssetSourceRequestPlan(
    const TerrainManifestAssetLoadRequest* const requests,
    const std::size_t requestCount,
    const AssetSourceCatalog& sources)
{
    TerrainManifestAssetSourceRequestPlan result;
    if (requests == nullptr && requestCount > 0)
    {
        result.summary.invalidRequestCount = requestCount;
        return result;
    }

    result.records.reserve(requestCount);
    for (std::size_t index = 0; index < requestCount; ++index)
    {
        TerrainManifestAssetSourceRequestRecord record = mapRequestToSource(requests[index], sources);
        incrementSummary(result.summary, record.status);
        result.records.push_back(record);
    }

    return result;
}

TerrainManifestAssetSourceRequestPlan buildTerrainManifestAssetSourceRequestPlan(
    const TerrainManifestAssetLoadRequestPlan& plan,
    const AssetSourceCatalog& sources)
{
    return buildTerrainManifestAssetSourceRequestPlan(
        plan.requests.data(),
        plan.requests.size(),
        sources);
}
} // namespace full_engine
