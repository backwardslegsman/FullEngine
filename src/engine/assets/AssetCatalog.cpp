#include "engine/assets/AssetCatalog.hpp"

namespace full_engine
{
bool isValid(const AssetId id) noexcept
{
    return id.value != 0;
}

bool operator==(const AssetId lhs, const AssetId rhs) noexcept
{
    return lhs.value == rhs.value;
}

bool operator<(const AssetId lhs, const AssetId rhs) noexcept
{
    return lhs.value < rhs.value;
}

namespace
{
bool isValidKind(const AssetKind kind) noexcept
{
    switch (kind)
    {
    case AssetKind::Mesh:
    case AssetKind::Material:
    case AssetKind::Texture:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        return true;
    case AssetKind::Unknown:
        return false;
    }
    return false;
}
} // namespace

AssetRecordValidationResult validateAssetRecord(const AssetRecord& record)
{
    if (!isValid(record.id))
    {
        return AssetRecordValidationResult::InvalidAssetId;
    }

    if (!isValidKind(record.kind))
    {
        return AssetRecordValidationResult::InvalidKind;
    }

    if (record.dependencyCount > kMaxAssetDependencies)
    {
        return AssetRecordValidationResult::InvalidDependencyCount;
    }

    for (std::uint32_t index = 0; index < record.dependencyCount; ++index)
    {
        const AssetDependencyRef& dependency = record.dependencies[index];
        if (!isValid(dependency.id))
        {
            return AssetRecordValidationResult::InvalidDependencyId;
        }

        if (!isValidKind(dependency.kind))
        {
            return AssetRecordValidationResult::InvalidDependencyKind;
        }
    }

    return AssetRecordValidationResult::Success;
}

AssetCatalogResult AssetCatalog::addAsset(const AssetRecord& record)
{
    if (validateAssetRecord(record) != AssetRecordValidationResult::Success)
    {
        return AssetCatalogResult::InvalidArgument;
    }

    const auto inserted = assets_.emplace(record.id, record);
    return inserted.second ? AssetCatalogResult::Success : AssetCatalogResult::AlreadyExists;
}

AssetCatalogResult AssetCatalog::updateAsset(const AssetRecord& record)
{
    if (validateAssetRecord(record) != AssetRecordValidationResult::Success)
    {
        return AssetCatalogResult::InvalidArgument;
    }

    auto found = assets_.find(record.id);
    if (found == assets_.end())
    {
        return AssetCatalogResult::NotFound;
    }

    found->second = record;
    return AssetCatalogResult::Success;
}

AssetCatalogResult AssetCatalog::removeAsset(const AssetId id)
{
    return assets_.erase(id) > 0 ? AssetCatalogResult::Success : AssetCatalogResult::NotFound;
}

const AssetRecord* AssetCatalog::findAsset(const AssetId id) const
{
    const auto found = assets_.find(id);
    return found == assets_.end() ? nullptr : &found->second;
}

bool AssetCatalog::contains(const AssetId id) const
{
    return assets_.find(id) != assets_.end();
}

std::size_t AssetCatalog::assetCount() const noexcept
{
    return assets_.size();
}

void AssetCatalog::clear() noexcept
{
    assets_.clear();
}
} // namespace full_engine
