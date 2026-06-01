#include "engine/renderer_integration/TerrainManifestAssetLoadCompletionInbox.hpp"

namespace full_engine
{
namespace
{
bool isSupportedInboxKind(const AssetKind kind) noexcept
{
    return kind == AssetKind::Mesh ||
        kind == AssetKind::Material ||
        kind == AssetKind::Texture;
}

bool completionHasValidInboxHandle(const TerrainManifestAssetLoadJobCompletion& completion) noexcept
{
    switch (completion.request.kind)
    {
    case AssetKind::Mesh:
        return full_renderer::isValid(completion.output.mesh);
    case AssetKind::Material:
        return full_renderer::isValid(completion.output.material);
    case AssetKind::Texture:
        return full_renderer::isValid(completion.output.texture);
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        return false;
    }

    return false;
}

std::size_t findCompletionIndex(
    const std::vector<TerrainManifestAssetLoadJobCompletion>& completions,
    const AssetId id,
    const AssetKind kind) noexcept
{
    for (std::size_t index = 0; index < completions.size(); ++index)
    {
        const TerrainManifestAssetLoadJobCompletion& completion = completions[index];
        if (completion.request.id == id && completion.request.kind == kind)
        {
            return index;
        }
    }

    return completions.size();
}

void incrementSummary(
    TerrainManifestAssetLoadCompletionInboxSummary& summary,
    const TerrainManifestAssetLoadCompletionInboxStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadCompletionInboxStatus::Published:
        ++summary.publishedCount;
        break;
    case TerrainManifestAssetLoadCompletionInboxStatus::AlreadyPublished:
        ++summary.alreadyPublishedCount;
        break;
    case TerrainManifestAssetLoadCompletionInboxStatus::InvalidRequest:
        ++summary.invalidRequestCount;
        break;
    case TerrainManifestAssetLoadCompletionInboxStatus::MissingHandle:
        ++summary.missingHandleCount;
        break;
    }
}
} // namespace

const char* terrainManifestAssetLoadCompletionInboxStatusName(
    const TerrainManifestAssetLoadCompletionInboxStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadCompletionInboxStatus::Published:
        return "Published";
    case TerrainManifestAssetLoadCompletionInboxStatus::AlreadyPublished:
        return "AlreadyPublished";
    case TerrainManifestAssetLoadCompletionInboxStatus::InvalidRequest:
        return "InvalidRequest";
    case TerrainManifestAssetLoadCompletionInboxStatus::MissingHandle:
        return "MissingHandle";
    }

    return "Unknown";
}

const char* terrainManifestAssetLoadCompletionInboxRemoveStatusName(
    const TerrainManifestAssetLoadCompletionInboxRemoveStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadCompletionInboxRemoveStatus::Removed:
        return "Removed";
    case TerrainManifestAssetLoadCompletionInboxRemoveStatus::NotFound:
        return "NotFound";
    case TerrainManifestAssetLoadCompletionInboxRemoveStatus::InvalidArgument:
        return "InvalidArgument";
    }

    return "Unknown";
}

const char* terrainManifestAssetLoadCompletionInboxReplaceStatusName(
    const TerrainManifestAssetLoadCompletionInboxReplaceStatus status) noexcept
{
    switch (status)
    {
    case TerrainManifestAssetLoadCompletionInboxReplaceStatus::Replaced:
        return "Replaced";
    case TerrainManifestAssetLoadCompletionInboxReplaceStatus::Published:
        return "Published";
    case TerrainManifestAssetLoadCompletionInboxReplaceStatus::InvalidRequest:
        return "InvalidRequest";
    case TerrainManifestAssetLoadCompletionInboxReplaceStatus::MissingHandle:
        return "MissingHandle";
    }

    return "Unknown";
}

TerrainManifestAssetLoadCompletionInboxRecord TerrainManifestAssetLoadCompletionInbox::publish(
    const TerrainManifestAssetLoadJobCompletion& completion)
{
    TerrainManifestAssetLoadCompletionInboxRecord record;
    record.completion = completion;

    const TerrainManifestAssetLoadRequest& request = completion.request;
    if (!isValid(request.id) || !isSupportedInboxKind(request.kind))
    {
        record.status = TerrainManifestAssetLoadCompletionInboxStatus::InvalidRequest;
    }
    else if (completion.output.status != TerrainManifestAssetLoadCallbackStatus::Loaded ||
        !completionHasValidInboxHandle(completion))
    {
        record.status = TerrainManifestAssetLoadCompletionInboxStatus::MissingHandle;
    }
    else if (contains(request.id, request.kind))
    {
        record.status = TerrainManifestAssetLoadCompletionInboxStatus::AlreadyPublished;
    }
    else
    {
        completions_.push_back(completion);
        record.status = TerrainManifestAssetLoadCompletionInboxStatus::Published;
    }

    return record;
}

TerrainManifestAssetLoadCompletionInboxPublishResult TerrainManifestAssetLoadCompletionInbox::publish(
    const TerrainManifestAssetLoadJobCompletion* const completions,
    const std::size_t completionCount)
{
    TerrainManifestAssetLoadCompletionInboxPublishResult result;
    if (completions == nullptr && completionCount > 0)
    {
        result.summary.invalidRequestCount = completionCount;
        return result;
    }

    result.records.reserve(completionCount);
    for (std::size_t index = 0; index < completionCount; ++index)
    {
        TerrainManifestAssetLoadCompletionInboxRecord record = publish(completions[index]);
        incrementSummary(result.summary, record.status);
        result.records.push_back(record);
    }

    return result;
}

TerrainManifestAssetLoadCompletionInboxRemoveResult TerrainManifestAssetLoadCompletionInbox::remove(
    const AssetId id,
    const AssetKind kind)
{
    TerrainManifestAssetLoadCompletionInboxRemoveResult result;
    result.request.id = id;
    result.request.kind = kind;

    if (!isValid(id) || !isSupportedInboxKind(kind))
    {
        result.status = TerrainManifestAssetLoadCompletionInboxRemoveStatus::InvalidArgument;
        return result;
    }

    const std::size_t index = findCompletionIndex(completions_, id, kind);
    if (index == completions_.size())
    {
        result.status = TerrainManifestAssetLoadCompletionInboxRemoveStatus::NotFound;
        return result;
    }

    const auto offset =
        static_cast<std::vector<TerrainManifestAssetLoadJobCompletion>::difference_type>(index);
    completions_.erase(completions_.begin() + offset);
    result.status = TerrainManifestAssetLoadCompletionInboxRemoveStatus::Removed;
    return result;
}

TerrainManifestAssetLoadCompletionInboxReplaceResult TerrainManifestAssetLoadCompletionInbox::replace(
    const TerrainManifestAssetLoadJobCompletion& completion)
{
    TerrainManifestAssetLoadCompletionInboxReplaceResult result;
    result.completion = completion;

    const TerrainManifestAssetLoadRequest& request = completion.request;
    if (!isValid(request.id) || !isSupportedInboxKind(request.kind))
    {
        result.status = TerrainManifestAssetLoadCompletionInboxReplaceStatus::InvalidRequest;
        return result;
    }

    if (completion.output.status != TerrainManifestAssetLoadCallbackStatus::Loaded ||
        !completionHasValidInboxHandle(completion))
    {
        result.status = TerrainManifestAssetLoadCompletionInboxReplaceStatus::MissingHandle;
        return result;
    }

    const std::size_t index = findCompletionIndex(completions_, request.id, request.kind);
    if (index == completions_.size())
    {
        completions_.push_back(completion);
        result.status = TerrainManifestAssetLoadCompletionInboxReplaceStatus::Published;
    }
    else
    {
        completions_[index] = completion;
        result.status = TerrainManifestAssetLoadCompletionInboxReplaceStatus::Replaced;
    }

    return result;
}

bool TerrainManifestAssetLoadCompletionInbox::contains(
    const AssetId id,
    const AssetKind kind) const noexcept
{
    if (!isValid(id) || !isSupportedInboxKind(kind))
    {
        return false;
    }

    return findCompletionIndex(completions_, id, kind) != completions_.size();
}

std::size_t TerrainManifestAssetLoadCompletionInbox::completionCount() const noexcept
{
    return completions_.size();
}

const std::vector<TerrainManifestAssetLoadJobCompletion>&
TerrainManifestAssetLoadCompletionInbox::completions() const noexcept
{
    return completions_;
}

void TerrainManifestAssetLoadCompletionInbox::clear() noexcept
{
    completions_.clear();
}
} // namespace full_engine
