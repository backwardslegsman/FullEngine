#include "engine/renderer_integration/TerrainManifestAssetLoadRequests.hpp"

namespace full_engine
{
namespace
{
bool isSupportedLoadKind(const AssetKind kind) noexcept
{
    return kind == AssetKind::Mesh ||
        kind == AssetKind::Material ||
        kind == AssetKind::Texture;
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

TerrainManifestAssetLoadQueueRecord TerrainManifestAssetLoadRequestQueue::push(
    const TerrainManifestAssetLoadRequest& request)
{
    TerrainManifestAssetLoadQueueRecord record;
    record.request = request;

    if (!isValid(request.id) || !isSupportedLoadKind(request.kind))
    {
        record.result = TerrainManifestAssetLoadQueueResult::InvalidArgument;
        return record;
    }

    if (contains(request.id, request.kind))
    {
        record.result = TerrainManifestAssetLoadQueueResult::AlreadyQueued;
        return record;
    }

    requests_.push_back(request);
    record.result = TerrainManifestAssetLoadQueueResult::Queued;
    return record;
}

TerrainManifestAssetLoadQueuePushResult TerrainManifestAssetLoadRequestQueue::pushPlan(
    const TerrainManifestAssetLoadRequestPlan& plan)
{
    TerrainManifestAssetLoadQueuePushResult result;
    result.records.reserve(plan.requests.size());

    for (const TerrainManifestAssetLoadRequest& request : plan.requests)
    {
        TerrainManifestAssetLoadQueueRecord record = push(request);
        switch (record.result)
        {
        case TerrainManifestAssetLoadQueueResult::Queued:
            ++result.summary.queuedCount;
            break;
        case TerrainManifestAssetLoadQueueResult::AlreadyQueued:
            ++result.summary.alreadyQueuedCount;
            break;
        case TerrainManifestAssetLoadQueueResult::InvalidArgument:
            ++result.summary.invalidArgumentCount;
            break;
        }
        result.records.push_back(record);
    }

    return result;
}

bool TerrainManifestAssetLoadRequestQueue::contains(const AssetId id, const AssetKind kind) const noexcept
{
    for (const TerrainManifestAssetLoadRequest& request : requests_)
    {
        if (request.id == id && request.kind == kind)
        {
            return true;
        }
    }

    return false;
}

std::size_t TerrainManifestAssetLoadRequestQueue::requestCount() const noexcept
{
    return requests_.size();
}

const std::vector<TerrainManifestAssetLoadRequest>& TerrainManifestAssetLoadRequestQueue::requests() const noexcept
{
    return requests_;
}

TerrainManifestAssetLoadRequestSummary TerrainManifestAssetLoadRequestQueue::summary() const noexcept
{
    TerrainManifestAssetLoadRequestSummary result;
    for (const TerrainManifestAssetLoadRequest& request : requests_)
    {
        incrementSummary(result, request.kind);
    }
    return result;
}

void TerrainManifestAssetLoadRequestQueue::clear() noexcept
{
    requests_.clear();
}
} // namespace full_engine
