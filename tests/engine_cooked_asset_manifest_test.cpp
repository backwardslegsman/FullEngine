#include "engine/assets/CookedAssetManifest.hpp"

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

full_engine::ChunkId chunk(const int x, const int y = 0, const int z = 0) noexcept
{
    return full_engine::ChunkId{x, y, z};
}

full_engine::AssetRecord makeAsset(
    const std::uint64_t id,
    const full_engine::AssetKind kind)
{
    full_engine::AssetRecord record;
    record.id = asset(id);
    record.kind = kind;
    return record;
}

full_engine::TerrainChunkAssetDesc makeTerrain(
    const full_engine::ChunkId id,
    const std::uint64_t meshId = 1,
    const std::uint64_t materialId = 2,
    const std::uint64_t splatId = 3)
{
    full_engine::TerrainChunkAssetDesc desc;
    desc.id = id;
    desc.lodCount = 1;
    desc.lods[0].mesh = asset(meshId);
    desc.lods[0].material = asset(materialId);
    desc.lods[0].maxDistanceMeters = 100.0f;
    desc.splatMap = asset(splatId);
    return desc;
}

full_engine::CookedAssetManifest makeValidManifest()
{
    full_engine::CookedAssetManifest manifest;
    manifest.assets.push_back(makeAsset(1, full_engine::AssetKind::Mesh));
    manifest.assets.push_back(makeAsset(2, full_engine::AssetKind::Material));
    manifest.assets.push_back(makeAsset(3, full_engine::AssetKind::Texture));
    manifest.terrainChunks.push_back(makeTerrain(chunk(0)));
    return manifest;
}

void testEmptyManifest(std::vector<std::string>& failures)
{
    const full_engine::CookedAssetManifest manifest;

    const full_engine::CookedAssetManifestValidation validation =
        full_engine::validateCookedAssetManifest(manifest);
    expect(
        validation.result == full_engine::CookedAssetManifestValidationResult::Success,
        "empty manifest validates",
        failures);

    const full_engine::CookedAssetManifestBuildResult build =
        full_engine::buildCatalogsFromCookedAssetManifest(manifest);
    expect(
        build.validation.result == full_engine::CookedAssetManifestValidationResult::Success,
        "empty manifest build succeeds",
        failures);
    expect(build.catalogs.assets.assetCount() == 0, "empty manifest builds empty asset catalog", failures);
    expect(
        build.catalogs.terrainAssets.assetCount() == 0,
        "empty manifest builds empty terrain asset catalog",
        failures);
}

void testValidManifestBuildsCatalogs(std::vector<std::string>& failures)
{
    full_engine::CookedAssetManifest manifest = makeValidManifest();
    manifest.assets.push_back(makeAsset(4, full_engine::AssetKind::Mesh));
    manifest.assets.push_back(makeAsset(5, full_engine::AssetKind::Material));
    manifest.terrainChunks.push_back(makeTerrain(chunk(1), 4, 5, 0));

    const full_engine::CookedAssetManifestBuildResult build =
        full_engine::buildCatalogsFromCookedAssetManifest(manifest);

    expect(
        build.validation.result == full_engine::CookedAssetManifestValidationResult::Success,
        "valid manifest build succeeds",
        failures);
    expect(build.catalogs.assets.assetCount() == 5, "valid manifest builds all generic assets", failures);
    expect(
        build.catalogs.terrainAssets.assetCount() == 2,
        "valid manifest builds all terrain asset descriptors",
        failures);
    expect(build.catalogs.assets.contains(asset(1)), "built asset catalog contains mesh asset", failures);
    expect(
        build.catalogs.terrainAssets.contains(chunk(1)),
        "built terrain catalog contains second chunk descriptor",
        failures);
}

void testDuplicateAssetIdsAreRejected(std::vector<std::string>& failures)
{
    full_engine::CookedAssetManifest manifest = makeValidManifest();
    manifest.assets.push_back(makeAsset(1, full_engine::AssetKind::Mesh));

    const full_engine::CookedAssetManifestValidation validation =
        full_engine::validateCookedAssetManifest(manifest);

    expect(
        validation.result == full_engine::CookedAssetManifestValidationResult::DuplicateAssetId,
        "duplicate asset id is rejected",
        failures);
    expect(validation.assetIndex == 3, "duplicate asset id reports second asset index", failures);
}

void testDuplicateTerrainChunksAreRejected(std::vector<std::string>& failures)
{
    full_engine::CookedAssetManifest manifest = makeValidManifest();
    manifest.terrainChunks.push_back(makeTerrain(chunk(0)));

    const full_engine::CookedAssetManifestValidation validation =
        full_engine::validateCookedAssetManifest(manifest);

    expect(
        validation.result == full_engine::CookedAssetManifestValidationResult::DuplicateTerrainChunk,
        "duplicate terrain chunk is rejected",
        failures);
    expect(validation.terrainChunkIndex == 1, "duplicate terrain chunk reports second descriptor index", failures);
}

void testInvalidGenericAssetReportsNestedDetail(std::vector<std::string>& failures)
{
    full_engine::CookedAssetManifest manifest = makeValidManifest();
    manifest.assets[1].kind = full_engine::AssetKind::Unknown;

    const full_engine::CookedAssetManifestBuildResult build =
        full_engine::buildCatalogsFromCookedAssetManifest(manifest);

    expect(
        build.validation.result == full_engine::CookedAssetManifestValidationResult::InvalidAssetRecord,
        "invalid asset record is rejected",
        failures);
    expect(build.validation.assetIndex == 1, "invalid asset record reports asset index", failures);
    expect(
        build.validation.assetValidation == full_engine::AssetRecordValidationResult::InvalidKind,
        "invalid asset record preserves nested detail",
        failures);
    expect(build.catalogs.assets.assetCount() == 0, "failed build leaves asset catalog empty", failures);
    expect(
        build.catalogs.terrainAssets.assetCount() == 0,
        "failed build leaves terrain asset catalog empty",
        failures);
}

void testInvalidTerrainAssetsReportNestedDetail(std::vector<std::string>& failures)
{
    full_engine::CookedAssetManifest manifest = makeValidManifest();
    manifest.terrainChunks[0].lodCount = 0;

    const full_engine::CookedAssetManifestValidation validation =
        full_engine::validateCookedAssetManifest(manifest);

    expect(
        validation.result == full_engine::CookedAssetManifestValidationResult::InvalidTerrainAssets,
        "invalid terrain assets are rejected",
        failures);
    expect(validation.terrainChunkIndex == 0, "invalid terrain assets report terrain index", failures);
    expect(
        validation.terrainValidation == full_engine::TerrainAssetValidationResult::InvalidLodCount,
        "invalid terrain assets preserve nested detail",
        failures);
}

void testMissingTerrainDependenciesAreRejected(std::vector<std::string>& failures)
{
    full_engine::CookedAssetManifest manifest = makeValidManifest();
    manifest.assets.erase(manifest.assets.begin());

    const full_engine::CookedAssetManifestValidation validation =
        full_engine::validateCookedAssetManifest(manifest);

    expect(
        validation.result == full_engine::CookedAssetManifestValidationResult::InvalidTerrainDependencies,
        "missing terrain dependency is rejected",
        failures);
    expect(validation.terrainChunkIndex == 0, "missing dependency reports terrain index", failures);
    expect(
        validation.terrainDependencyValidation ==
            full_engine::TerrainAssetDependencyValidationResult::MissingMeshAsset,
        "missing dependency preserves nested dependency detail",
        failures);
}

void testWrongKindTerrainDependenciesAreRejected(std::vector<std::string>& failures)
{
    full_engine::CookedAssetManifest manifest = makeValidManifest();
    manifest.assets[0].kind = full_engine::AssetKind::Texture;

    const full_engine::CookedAssetManifestValidation validation =
        full_engine::validateCookedAssetManifest(manifest);

    expect(
        validation.result == full_engine::CookedAssetManifestValidationResult::InvalidTerrainDependencies,
        "wrong-kind terrain dependency is rejected",
        failures);
    expect(
        validation.terrainDependencyValidation ==
            full_engine::TerrainAssetDependencyValidationResult::WrongMeshAssetKind,
        "wrong-kind dependency preserves nested dependency detail",
        failures);
}

void testFirstFailureIsDeterministic(std::vector<std::string>& failures)
{
    full_engine::CookedAssetManifest manifest = makeValidManifest();
    manifest.assets[2].id = {};
    manifest.terrainChunks.push_back(makeTerrain(chunk(0)));

    const full_engine::CookedAssetManifestValidation validation =
        full_engine::validateCookedAssetManifest(manifest);

    expect(
        validation.result == full_engine::CookedAssetManifestValidationResult::InvalidAssetRecord,
        "asset failures are reported before later terrain failures",
        failures);
    expect(validation.assetIndex == 2, "first failure reports deterministic asset index", failures);
    expect(
        validation.terrainChunkIndex == full_engine::CookedAssetManifestValidation::invalidIndex,
        "asset failure leaves terrain index unset",
        failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testEmptyManifest(failures);
    testValidManifestBuildsCatalogs(failures);
    testDuplicateAssetIdsAreRejected(failures);
    testDuplicateTerrainChunksAreRejected(failures);
    testInvalidGenericAssetReportsNestedDetail(failures);
    testInvalidTerrainAssetsReportNestedDetail(failures);
    testMissingTerrainDependenciesAreRejected(failures);
    testWrongKindTerrainDependenciesAreRejected(failures);
    testFirstFailureIsDeterministic(failures);

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
