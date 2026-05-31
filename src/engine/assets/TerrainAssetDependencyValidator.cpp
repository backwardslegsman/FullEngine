#include "engine/assets/TerrainAssetDependencyValidator.hpp"

namespace full_engine
{
namespace
{
TerrainAssetDependencyValidation missingOrWrongKind(
    const AssetCatalog& assetCatalog,
    const AssetId id,
    const AssetKind expectedKind,
    const TerrainAssetDependencyValidationResult missingResult,
    const TerrainAssetDependencyValidationResult wrongKindResult)
{
    const AssetRecord* record = assetCatalog.findAsset(id);
    if (record == nullptr)
    {
        return {missingResult, TerrainAssetValidationResult::Success};
    }

    if (record->kind != expectedKind)
    {
        return {wrongKindResult, TerrainAssetValidationResult::Success};
    }

    return {TerrainAssetDependencyValidationResult::Success, TerrainAssetValidationResult::Success};
}
} // namespace

TerrainAssetDependencyValidation validateTerrainAssetDependencies(
    const TerrainChunkAssetDesc& terrainAssets,
    const AssetCatalog& assetCatalog)
{
    const TerrainAssetValidationResult terrainValidation = validateTerrainChunkAssets(terrainAssets);
    if (terrainValidation != TerrainAssetValidationResult::Success)
    {
        return {TerrainAssetDependencyValidationResult::InvalidTerrainAssets, terrainValidation};
    }

    for (std::uint32_t index = 0; index < terrainAssets.lodCount; ++index)
    {
        const TerrainAssetLodRef& lod = terrainAssets.lods[index];

        const TerrainAssetDependencyValidation meshValidation = missingOrWrongKind(
            assetCatalog,
            lod.mesh,
            AssetKind::Mesh,
            TerrainAssetDependencyValidationResult::MissingMeshAsset,
            TerrainAssetDependencyValidationResult::WrongMeshAssetKind);
        if (meshValidation.result != TerrainAssetDependencyValidationResult::Success)
        {
            return meshValidation;
        }

        const TerrainAssetDependencyValidation materialValidation = missingOrWrongKind(
            assetCatalog,
            lod.material,
            AssetKind::Material,
            TerrainAssetDependencyValidationResult::MissingMaterialAsset,
            TerrainAssetDependencyValidationResult::WrongMaterialAssetKind);
        if (materialValidation.result != TerrainAssetDependencyValidationResult::Success)
        {
            return materialValidation;
        }
    }

    if (isValid(terrainAssets.splatMap))
    {
        const TerrainAssetDependencyValidation splatValidation = missingOrWrongKind(
            assetCatalog,
            terrainAssets.splatMap,
            AssetKind::Texture,
            TerrainAssetDependencyValidationResult::MissingSplatMapAsset,
            TerrainAssetDependencyValidationResult::WrongSplatMapAssetKind);
        if (splatValidation.result != TerrainAssetDependencyValidationResult::Success)
        {
            return splatValidation;
        }
    }

    return {TerrainAssetDependencyValidationResult::Success, TerrainAssetValidationResult::Success};
}
} // namespace full_engine
