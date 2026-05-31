#include "engine/assets/CookedAssetManifest.hpp"

namespace full_engine
{
namespace
{
CookedAssetManifestValidation successValidation() noexcept
{
    return {};
}

CookedAssetManifestValidation invalidAssetRecordValidation(
    const std::size_t index,
    const AssetRecordValidationResult assetValidation) noexcept
{
    CookedAssetManifestValidation validation;
    validation.result = CookedAssetManifestValidationResult::InvalidAssetRecord;
    validation.assetIndex = index;
    validation.assetValidation = assetValidation;
    return validation;
}

CookedAssetManifestValidation duplicateAssetValidation(const std::size_t index) noexcept
{
    CookedAssetManifestValidation validation;
    validation.result = CookedAssetManifestValidationResult::DuplicateAssetId;
    validation.assetIndex = index;
    return validation;
}

CookedAssetManifestValidation invalidAssetDependencyValidation(
    const std::size_t index,
    const AssetDependencyValidation& dependencyValidation) noexcept
{
    CookedAssetManifestValidation validation;
    validation.result = CookedAssetManifestValidationResult::InvalidAssetDependencies;
    validation.assetIndex = index;
    validation.assetValidation = dependencyValidation.recordValidation;
    validation.assetDependencyValidation = dependencyValidation.result;
    validation.assetDependencyIndex = dependencyValidation.dependencyIndex;
    return validation;
}

CookedAssetManifestValidation invalidTerrainValidation(
    const std::size_t index,
    const TerrainAssetValidationResult terrainValidation) noexcept
{
    CookedAssetManifestValidation validation;
    validation.result = CookedAssetManifestValidationResult::InvalidTerrainAssets;
    validation.terrainChunkIndex = index;
    validation.terrainValidation = terrainValidation;
    return validation;
}

CookedAssetManifestValidation duplicateTerrainValidation(const std::size_t index) noexcept
{
    CookedAssetManifestValidation validation;
    validation.result = CookedAssetManifestValidationResult::DuplicateTerrainChunk;
    validation.terrainChunkIndex = index;
    return validation;
}

CookedAssetManifestValidation invalidTerrainDependencyValidation(
    const std::size_t index,
    const TerrainAssetDependencyValidation& dependencyValidation) noexcept
{
    CookedAssetManifestValidation validation;
    validation.result = CookedAssetManifestValidationResult::InvalidTerrainDependencies;
    validation.terrainChunkIndex = index;
    validation.terrainValidation = dependencyValidation.terrainValidation;
    validation.terrainDependencyValidation = dependencyValidation.result;
    return validation;
}
} // namespace

CookedAssetManifestValidation validateCookedAssetManifest(const CookedAssetManifest& manifest)
{
    AssetCatalog assetCatalog;
    for (std::size_t index = 0; index < manifest.assets.size(); ++index)
    {
        const AssetRecord& record = manifest.assets[index];
        const AssetRecordValidationResult recordValidation = validateAssetRecord(record);
        if (recordValidation != AssetRecordValidationResult::Success)
        {
            return invalidAssetRecordValidation(index, recordValidation);
        }

        const AssetCatalogResult addResult = assetCatalog.addAsset(record);
        if (addResult == AssetCatalogResult::AlreadyExists)
        {
            return duplicateAssetValidation(index);
        }
    }

    for (std::size_t index = 0; index < manifest.assets.size(); ++index)
    {
        const AssetDependencyValidation dependencyValidation =
            validateAssetDependencies(manifest.assets[index], assetCatalog);
        if (dependencyValidation.result != AssetDependencyValidationResult::Success)
        {
            return invalidAssetDependencyValidation(index, dependencyValidation);
        }
    }

    TerrainAssetCatalog terrainCatalog;
    for (std::size_t index = 0; index < manifest.terrainChunks.size(); ++index)
    {
        const TerrainChunkAssetDesc& desc = manifest.terrainChunks[index];
        const TerrainAssetValidationResult terrainValidation = validateTerrainChunkAssets(desc);
        if (terrainValidation != TerrainAssetValidationResult::Success)
        {
            return invalidTerrainValidation(index, terrainValidation);
        }

        if (terrainCatalog.contains(desc.id))
        {
            return duplicateTerrainValidation(index);
        }

        const TerrainAssetDependencyValidation dependencyValidation =
            validateTerrainAssetDependencies(desc, assetCatalog);
        if (dependencyValidation.result != TerrainAssetDependencyValidationResult::Success)
        {
            return invalidTerrainDependencyValidation(index, dependencyValidation);
        }

        (void)terrainCatalog.addChunkAssets(desc);
    }

    return successValidation();
}

CookedAssetManifestBuildResult buildCatalogsFromCookedAssetManifest(const CookedAssetManifest& manifest)
{
    CookedAssetManifestBuildResult result;
    result.validation = validateCookedAssetManifest(manifest);
    if (result.validation.result != CookedAssetManifestValidationResult::Success)
    {
        return result;
    }

    for (const AssetRecord& record : manifest.assets)
    {
        (void)result.catalogs.assets.addAsset(record);
    }

    for (const TerrainChunkAssetDesc& desc : manifest.terrainChunks)
    {
        (void)result.catalogs.terrainAssets.addChunkAssets(desc);
    }

    return result;
}
} // namespace full_engine
