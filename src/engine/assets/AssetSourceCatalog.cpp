#include "engine/assets/AssetSourceCatalog.hpp"

namespace full_engine
{
namespace
{
bool isLoadableAssetSourceKind(const AssetKind kind) noexcept
{
    return kind == AssetKind::Mesh ||
        kind == AssetKind::Material ||
        kind == AssetKind::Texture ||
        kind == AssetKind::Skeleton ||
        kind == AssetKind::SkinnedMesh;
}
} // namespace

const char* assetSourceCatalogResultName(const AssetSourceCatalogResult result) noexcept
{
    switch (result)
    {
    case AssetSourceCatalogResult::Success:
        return "Success";
    case AssetSourceCatalogResult::AlreadyExists:
        return "AlreadyExists";
    case AssetSourceCatalogResult::NotFound:
        return "NotFound";
    case AssetSourceCatalogResult::InvalidArgument:
        return "InvalidArgument";
    }

    return "Unknown";
}

const char* assetSourceRecordValidationResultName(
    const AssetSourceRecordValidationResult result) noexcept
{
    switch (result)
    {
    case AssetSourceRecordValidationResult::Success:
        return "Success";
    case AssetSourceRecordValidationResult::InvalidAssetId:
        return "InvalidAssetId";
    case AssetSourceRecordValidationResult::InvalidKind:
        return "InvalidKind";
    case AssetSourceRecordValidationResult::EmptyUri:
        return "EmptyUri";
    case AssetSourceRecordValidationResult::InvalidDescriptor:
        return "InvalidDescriptor";
    }

    return "Unknown";
}

AssetSourceRecordValidationResult validateAssetSourceRecord(const AssetSourceRecord& record)
{
    if (!isValid(record.id))
    {
        return AssetSourceRecordValidationResult::InvalidAssetId;
    }

    if (!isLoadableAssetSourceKind(record.kind))
    {
        return AssetSourceRecordValidationResult::InvalidKind;
    }

    if (record.uri.empty())
    {
        return AssetSourceRecordValidationResult::EmptyUri;
    }

    if (validateAssetSourceDescriptor(record.kind, record.descriptor) !=
        AssetSourceDescriptorValidationResult::Success)
    {
        return AssetSourceRecordValidationResult::InvalidDescriptor;
    }

    return AssetSourceRecordValidationResult::Success;
}

AssetSourceCatalogResult AssetSourceCatalog::addSource(const AssetSourceRecord& record)
{
    if (validateAssetSourceRecord(record) != AssetSourceRecordValidationResult::Success)
    {
        return AssetSourceCatalogResult::InvalidArgument;
    }

    const auto inserted = sources_.emplace(record.id, record);
    return inserted.second ? AssetSourceCatalogResult::Success : AssetSourceCatalogResult::AlreadyExists;
}

AssetSourceCatalogResult AssetSourceCatalog::updateSource(const AssetSourceRecord& record)
{
    if (validateAssetSourceRecord(record) != AssetSourceRecordValidationResult::Success)
    {
        return AssetSourceCatalogResult::InvalidArgument;
    }

    auto found = sources_.find(record.id);
    if (found == sources_.end())
    {
        return AssetSourceCatalogResult::NotFound;
    }

    found->second = record;
    return AssetSourceCatalogResult::Success;
}

AssetSourceCatalogResult AssetSourceCatalog::removeSource(const AssetId id)
{
    if (!isValid(id))
    {
        return AssetSourceCatalogResult::InvalidArgument;
    }

    return sources_.erase(id) > 0 ? AssetSourceCatalogResult::Success : AssetSourceCatalogResult::NotFound;
}

const AssetSourceRecord* AssetSourceCatalog::findSource(const AssetId id) const
{
    if (!isValid(id))
    {
        return nullptr;
    }

    const auto found = sources_.find(id);
    return found == sources_.end() ? nullptr : &found->second;
}

bool AssetSourceCatalog::contains(const AssetId id) const
{
    return findSource(id) != nullptr;
}

std::size_t AssetSourceCatalog::sourceCount() const noexcept
{
    return sources_.size();
}

std::vector<AssetSourceRecord> AssetSourceCatalog::sources() const
{
    std::vector<AssetSourceRecord> result;
    result.reserve(sources_.size());
    for (const auto& entry : sources_)
    {
        result.push_back(entry.second);
    }
    return result;
}

void AssetSourceCatalog::clear() noexcept
{
    sources_.clear();
}
} // namespace full_engine
