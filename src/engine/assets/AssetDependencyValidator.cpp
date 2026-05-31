#include "engine/assets/AssetDependencyValidator.hpp"

namespace full_engine
{
namespace
{
AssetDependencyValidation invalidRecordValidation(const AssetRecordValidationResult recordValidation) noexcept
{
    AssetDependencyValidation validation;
    validation.result = AssetDependencyValidationResult::InvalidAssetRecord;
    validation.recordValidation = recordValidation;
    return validation;
}

AssetDependencyValidation dependencyValidation(
    const AssetDependencyValidationResult result,
    const std::uint32_t index) noexcept
{
    AssetDependencyValidation validation;
    validation.result = result;
    validation.dependencyIndex = index;
    return validation;
}
} // namespace

AssetDependencyValidation validateAssetDependencies(const AssetRecord& record, const AssetCatalog& catalog)
{
    const AssetRecordValidationResult recordValidation = validateAssetRecord(record);
    if (recordValidation != AssetRecordValidationResult::Success)
    {
        return invalidRecordValidation(recordValidation);
    }

    for (std::uint32_t index = 0; index < record.dependencyCount; ++index)
    {
        const AssetDependencyRef& dependency = record.dependencies[index];
        const AssetRecord* dependencyRecord = catalog.findAsset(dependency.id);
        if (dependencyRecord == nullptr)
        {
            return dependencyValidation(AssetDependencyValidationResult::MissingDependency, index);
        }

        if (dependencyRecord->kind != dependency.kind)
        {
            return dependencyValidation(AssetDependencyValidationResult::WrongDependencyKind, index);
        }
    }

    return {};
}
} // namespace full_engine
