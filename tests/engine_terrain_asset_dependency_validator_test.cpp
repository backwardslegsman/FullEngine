#include "engine/assets/TerrainAssetDependencyValidator.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return full_engine::AssetId{value};
}

full_engine::AssetRecord makeAssetRecord(const std::uint64_t value, const full_engine::AssetKind kind)
{
    full_engine::AssetRecord record;
    record.id = asset(value);
    record.kind = kind;
    return record;
}

full_engine::TerrainChunkAssetDesc makeTerrainDesc(
    const full_engine::ChunkId& id = {1, 0, 0},
    const std::uint32_t lodCount = 1)
{
    full_engine::TerrainChunkAssetDesc desc;
    desc.id = id;
    desc.lodCount = lodCount;
    for (std::uint32_t index = 0; index < lodCount && index < full_engine::kMaxTerrainAssetLodLevels; ++index)
    {
        desc.lods[index].mesh = asset(100 + index);
        desc.lods[index].material = asset(200 + index);
        desc.lods[index].maxDistanceMeters = static_cast<float>((index + 1) * 100);
    }
    return desc;
}

full_engine::AssetCatalog makeMatchingAssetCatalog(const std::uint32_t lodCount)
{
    full_engine::AssetCatalog catalog;
    for (std::uint32_t index = 0; index < lodCount && index < full_engine::kMaxTerrainAssetLodLevels; ++index)
    {
        catalog.addAsset(makeAssetRecord(100 + index, full_engine::AssetKind::Mesh));
        catalog.addAsset(makeAssetRecord(200 + index, full_engine::AssetKind::Material));
    }
    catalog.addAsset(makeAssetRecord(300, full_engine::AssetKind::Texture));
    return catalog;
}

void expectResult(
    const full_engine::TerrainAssetDependencyValidation& validation,
    const full_engine::TerrainAssetDependencyValidationResult expected,
    const char* message,
    std::vector<std::string>& failures)
{
    expect(validation.result == expected, message, failures);
}

void testValidDependencies(std::vector<std::string>& failures)
{
    full_engine::TerrainChunkAssetDesc noSplat = makeTerrainDesc({1, 0, 0}, 2);
    full_engine::TerrainChunkAssetDesc withSplat = noSplat;
    withSplat.splatMap = asset(300);
    const full_engine::AssetCatalog catalog = makeMatchingAssetCatalog(2);

    const full_engine::TerrainAssetDependencyValidation noSplatValidation =
        full_engine::validateTerrainAssetDependencies(noSplat, catalog);
    const full_engine::TerrainAssetDependencyValidation splatValidation =
        full_engine::validateTerrainAssetDependencies(withSplat, catalog);

    expectResult(
        noSplatValidation,
        full_engine::TerrainAssetDependencyValidationResult::Success,
        "default splat succeeds without texture dependency",
        failures);
    expectResult(
        splatValidation,
        full_engine::TerrainAssetDependencyValidationResult::Success,
        "valid splat texture dependency succeeds",
        failures);
    expect(
        splatValidation.terrainValidation == full_engine::TerrainAssetValidationResult::Success,
        "successful dependency validation preserves success terrain validation",
        failures);
}

void testInvalidTerrainDescriptor(std::vector<std::string>& failures)
{
    full_engine::TerrainChunkAssetDesc desc = makeTerrainDesc({2, 0, 0}, 1);
    desc.lods[0].mesh = {};
    const full_engine::AssetCatalog catalog = makeMatchingAssetCatalog(1);

    const full_engine::TerrainAssetDependencyValidation validation =
        full_engine::validateTerrainAssetDependencies(desc, catalog);

    expectResult(
        validation,
        full_engine::TerrainAssetDependencyValidationResult::InvalidTerrainAssets,
        "invalid terrain assets are reported before dependency lookup",
        failures);
    expect(
        validation.terrainValidation == full_engine::TerrainAssetValidationResult::InvalidMeshAsset,
        "invalid terrain validation detail is preserved",
        failures);
}

void testMissingDependencies(std::vector<std::string>& failures)
{
    const full_engine::TerrainChunkAssetDesc desc = makeTerrainDesc({3, 0, 0}, 1);

    full_engine::AssetCatalog missingMesh;
    missingMesh.addAsset(makeAssetRecord(200, full_engine::AssetKind::Material));
    expectResult(
        full_engine::validateTerrainAssetDependencies(desc, missingMesh),
        full_engine::TerrainAssetDependencyValidationResult::MissingMeshAsset,
        "missing mesh asset is reported",
        failures);

    full_engine::AssetCatalog missingMaterial;
    missingMaterial.addAsset(makeAssetRecord(100, full_engine::AssetKind::Mesh));
    expectResult(
        full_engine::validateTerrainAssetDependencies(desc, missingMaterial),
        full_engine::TerrainAssetDependencyValidationResult::MissingMaterialAsset,
        "missing material asset is reported",
        failures);

    full_engine::TerrainChunkAssetDesc splatDesc = desc;
    splatDesc.splatMap = asset(300);
    full_engine::AssetCatalog missingSplat = makeMatchingAssetCatalog(1);
    missingSplat.removeAsset(asset(300));
    expectResult(
        full_engine::validateTerrainAssetDependencies(splatDesc, missingSplat),
        full_engine::TerrainAssetDependencyValidationResult::MissingSplatMapAsset,
        "missing splat texture asset is reported",
        failures);
}

void testWrongDependencyKinds(std::vector<std::string>& failures)
{
    const full_engine::TerrainChunkAssetDesc desc = makeTerrainDesc({4, 0, 0}, 1);

    full_engine::AssetCatalog wrongMesh = makeMatchingAssetCatalog(1);
    wrongMesh.updateAsset(makeAssetRecord(100, full_engine::AssetKind::Texture));
    expectResult(
        full_engine::validateTerrainAssetDependencies(desc, wrongMesh),
        full_engine::TerrainAssetDependencyValidationResult::WrongMeshAssetKind,
        "wrong mesh asset kind is reported",
        failures);

    full_engine::AssetCatalog wrongMaterial = makeMatchingAssetCatalog(1);
    wrongMaterial.updateAsset(makeAssetRecord(200, full_engine::AssetKind::Shader));
    expectResult(
        full_engine::validateTerrainAssetDependencies(desc, wrongMaterial),
        full_engine::TerrainAssetDependencyValidationResult::WrongMaterialAssetKind,
        "wrong material asset kind is reported",
        failures);

    full_engine::TerrainChunkAssetDesc splatDesc = desc;
    splatDesc.splatMap = asset(300);
    full_engine::AssetCatalog wrongSplat = makeMatchingAssetCatalog(1);
    wrongSplat.updateAsset(makeAssetRecord(300, full_engine::AssetKind::Material));
    expectResult(
        full_engine::validateTerrainAssetDependencies(splatDesc, wrongSplat),
        full_engine::TerrainAssetDependencyValidationResult::WrongSplatMapAssetKind,
        "wrong splat asset kind is reported",
        failures);
}

void testInactiveLodsIgnored(std::vector<std::string>& failures)
{
    full_engine::TerrainChunkAssetDesc desc = makeTerrainDesc({5, 0, 0}, 1);
    desc.lods[1].mesh = asset(999);
    desc.lods[1].material = asset(998);
    desc.lods[1].maxDistanceMeters = -100.0f;
    const full_engine::AssetCatalog catalog = makeMatchingAssetCatalog(1);

    expectResult(
        full_engine::validateTerrainAssetDependencies(desc, catalog),
        full_engine::TerrainAssetDependencyValidationResult::Success,
        "inactive LOD dependencies are ignored",
        failures);
}

void testValidationDoesNotMutateCatalog(std::vector<std::string>& failures)
{
    const full_engine::TerrainChunkAssetDesc desc = makeTerrainDesc({6, 0, 0}, 1);
    full_engine::AssetCatalog catalog = makeMatchingAssetCatalog(1);
    const std::size_t beforeCount = catalog.assetCount();

    const full_engine::TerrainAssetDependencyValidation validation =
        full_engine::validateTerrainAssetDependencies(desc, catalog);

    expectResult(
        validation,
        full_engine::TerrainAssetDependencyValidationResult::Success,
        "dependency validation succeeds before mutation check",
        failures);
    expect(catalog.assetCount() == beforeCount, "dependency validation does not mutate asset catalog", failures);
    expect(catalog.contains(asset(100)), "dependency validation preserves mesh record", failures);
    expect(catalog.contains(asset(200)), "dependency validation preserves material record", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testValidDependencies(failures);
    testInvalidTerrainDescriptor(failures);
    testMissingDependencies(failures);
    testWrongDependencyKinds(failures);
    testInactiveLodsIgnored(failures);
    testValidationDoesNotMutateCatalog(failures);

    if (!failures.empty())
    {
        for (const std::string& failure : failures)
        {
            std::cerr << failure << '\n';
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
