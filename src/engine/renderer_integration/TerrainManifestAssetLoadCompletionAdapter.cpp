#include "engine/renderer_integration/TerrainManifestAssetLoadCompletionAdapter.hpp"

namespace full_engine
{
TerrainManifestAssetLoadWorkerCompletionPublishResult publishTerrainManifestAssetLoadWorkerCompletion(
    TerrainManifestAssetLoadCompletionInbox& inbox,
    const TerrainManifestAssetLoadJobCompletion& completion)
{
    TerrainManifestAssetLoadWorkerCompletionPublishResult result;
    TerrainManifestAssetLoadCompletionInboxRecord record = inbox.publish(completion);
    result.publish.records.push_back(record);
    switch (record.status)
    {
    case TerrainManifestAssetLoadCompletionInboxStatus::Published:
        ++result.publish.summary.publishedCount;
        break;
    case TerrainManifestAssetLoadCompletionInboxStatus::AlreadyPublished:
        ++result.publish.summary.alreadyPublishedCount;
        break;
    case TerrainManifestAssetLoadCompletionInboxStatus::InvalidRequest:
        ++result.publish.summary.invalidRequestCount;
        break;
    case TerrainManifestAssetLoadCompletionInboxStatus::MissingHandle:
        ++result.publish.summary.missingHandleCount;
        break;
    }
    result.pendingCompletionCount = inbox.completionCount();
    return result;
}

TerrainManifestAssetLoadWorkerCompletionPublishResult publishTerrainManifestAssetLoadWorkerCompletions(
    TerrainManifestAssetLoadCompletionInbox& inbox,
    const TerrainManifestAssetLoadJobCompletion* const completions,
    const std::size_t completionCount)
{
    TerrainManifestAssetLoadWorkerCompletionPublishResult result;
    result.publish = inbox.publish(completions, completionCount);
    result.pendingCompletionCount = inbox.completionCount();
    return result;
}

TerrainManifestAssetLoadWorkerCompletionReplaceResult replaceTerrainManifestAssetLoadWorkerCompletion(
    TerrainManifestAssetLoadCompletionInbox& inbox,
    const TerrainManifestAssetLoadJobCompletion& completion)
{
    TerrainManifestAssetLoadWorkerCompletionReplaceResult result;
    result.replace = inbox.replace(completion);
    result.pendingCompletionCount = inbox.completionCount();
    return result;
}

TerrainManifestAssetLoadWorkerCompletionRemoveResult removeTerrainManifestAssetLoadWorkerCompletion(
    TerrainManifestAssetLoadCompletionInbox& inbox,
    const AssetId id,
    const AssetKind kind)
{
    TerrainManifestAssetLoadWorkerCompletionRemoveResult result;
    result.remove = inbox.remove(id, kind);
    result.pendingCompletionCount = inbox.completionCount();
    return result;
}
} // namespace full_engine
