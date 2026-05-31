#include "engine/assets/TerrainAssetCatalog.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
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

full_engine::TerrainChunkAssetDesc makeDesc(
    const full_engine::ChunkId& id,
    const std::uint32_t lodCount)
{
    full_engine::TerrainChunkAssetDesc desc;
    desc.id = id;
    desc.lodCount = lodCount;
    desc.splatMap = {};
    for (std::uint32_t index = 0; index < full_engine::kMaxTerrainAssetLodLevels; ++index)
    {
        desc.lods[index].mesh = asset(100 + index);
        desc.lods[index].material = asset(200 + index);
        desc.lods[index].maxDistanceMeters = 50.0f * static_cast<float>(index + 1);
    }
    return desc;
}

bool sameDescCore(const full_engine::TerrainChunkAssetDesc& lhs, const full_engine::TerrainChunkAssetDesc& rhs)
{
    if (!(lhs.id == rhs.id) || lhs.lodCount != rhs.lodCount || !(lhs.splatMap == rhs.splatMap))
    {
        return false;
    }
    for (std::uint32_t index = 0; index < lhs.lodCount; ++index)
    {
        if (!(lhs.lods[index].mesh == rhs.lods[index].mesh) ||
            !(lhs.lods[index].material == rhs.lods[index].material) ||
            lhs.lods[index].maxDistanceMeters != rhs.lods[index].maxDistanceMeters)
        {
            return false;
        }
    }
    return true;
}

void testAssetIdSemantics(std::vector<std::string>& failures)
{
    expect(!full_engine::isValid(full_engine::AssetId{}), "default asset id is invalid", failures);
    expect(full_engine::isValid(asset(1)), "nonzero asset id is valid", failures);
    expect(asset(2) == asset(2), "asset id equality works", failures);
    expect(asset(1) < asset(2), "asset id ordering works", failures);
}

void testValidDescriptors(std::vector<std::string>& failures)
{
    const full_engine::TerrainChunkAssetDesc oneLod = makeDesc({1, 0, 0}, 1);
    full_engine::TerrainChunkAssetDesc multiLod = makeDesc({2, 0, 0}, full_engine::kMaxTerrainAssetLodLevels);
    multiLod.splatMap = {};

    expect(
        full_engine::validateTerrainChunkAssets(oneLod) == full_engine::TerrainAssetValidationResult::Success,
        "one-LOD asset desc validates",
        failures);
    expect(
        full_engine::validateTerrainChunkAssets(multiLod) == full_engine::TerrainAssetValidationResult::Success,
        "multi-LOD asset desc validates",
        failures);
}

void testInvalidLodCount(std::vector<std::string>& failures)
{
    full_engine::TerrainChunkAssetDesc zero = makeDesc({3, 0, 0}, 1);
    zero.lodCount = 0;

    full_engine::TerrainChunkAssetDesc tooMany = makeDesc({4, 0, 0}, full_engine::kMaxTerrainAssetLodLevels);
    tooMany.lodCount = full_engine::kMaxTerrainAssetLodLevels + 1;

    expect(
        full_engine::validateTerrainChunkAssets(zero) == full_engine::TerrainAssetValidationResult::InvalidLodCount,
        "zero LOD count is rejected",
        failures);
    expect(
        full_engine::validateTerrainChunkAssets(tooMany) == full_engine::TerrainAssetValidationResult::InvalidLodCount,
        "too-large LOD count is rejected",
        failures);
}

void testInvalidLodFields(std::vector<std::string>& failures)
{
    full_engine::TerrainChunkAssetDesc invalidMesh = makeDesc({5, 0, 0}, 1);
    invalidMesh.lods[0].mesh = {};
    full_engine::TerrainChunkAssetDesc invalidMaterial = makeDesc({6, 0, 0}, 1);
    invalidMaterial.lods[0].material = {};
    full_engine::TerrainChunkAssetDesc negativeDistance = makeDesc({7, 0, 0}, 1);
    negativeDistance.lods[0].maxDistanceMeters = -1.0f;
    full_engine::TerrainChunkAssetDesc infinityDistance = makeDesc({8, 0, 0}, 1);
    infinityDistance.lods[0].maxDistanceMeters = std::numeric_limits<float>::infinity();
    full_engine::TerrainChunkAssetDesc nanDistance = makeDesc({9, 0, 0}, 1);
    nanDistance.lods[0].maxDistanceMeters = std::numeric_limits<float>::quiet_NaN();
    full_engine::TerrainChunkAssetDesc unsorted = makeDesc({10, 0, 0}, 2);
    unsorted.lods[0].maxDistanceMeters = 100.0f;
    unsorted.lods[1].maxDistanceMeters = 50.0f;

    expect(
        full_engine::validateTerrainChunkAssets(invalidMesh) == full_engine::TerrainAssetValidationResult::InvalidMeshAsset,
        "invalid mesh asset is rejected",
        failures);
    expect(
        full_engine::validateTerrainChunkAssets(invalidMaterial) == full_engine::TerrainAssetValidationResult::InvalidMaterialAsset,
        "invalid material asset is rejected",
        failures);
    expect(
        full_engine::validateTerrainChunkAssets(negativeDistance) == full_engine::TerrainAssetValidationResult::InvalidDistance,
        "negative LOD distance is rejected",
        failures);
    expect(
        full_engine::validateTerrainChunkAssets(infinityDistance) == full_engine::TerrainAssetValidationResult::InvalidDistance,
        "infinite LOD distance is rejected",
        failures);
    expect(
        full_engine::validateTerrainChunkAssets(nanDistance) == full_engine::TerrainAssetValidationResult::InvalidDistance,
        "NaN LOD distance is rejected",
        failures);
    expect(
        full_engine::validateTerrainChunkAssets(unsorted) == full_engine::TerrainAssetValidationResult::UnsortedDistance,
        "unsorted LOD distances are rejected",
        failures);
}

void testAddFindAndDuplicate(std::vector<std::string>& failures)
{
    full_engine::TerrainAssetCatalog catalog;
    const full_engine::TerrainChunkAssetDesc desc = makeDesc({11, 0, 0}, 2);

    expect(catalog.assetCount() == 0, "default terrain asset catalog is empty", failures);
    expect(catalog.addChunkAssets(desc) == full_engine::TerrainAssetCatalogResult::Success, "asset add succeeds", failures);
    expect(catalog.addChunkAssets(desc) == full_engine::TerrainAssetCatalogResult::AlreadyExists, "duplicate asset add is rejected", failures);
    expect(catalog.contains(desc.id), "asset catalog contains added chunk", failures);
    expect(catalog.assetCount() == 1, "asset catalog count remains one after duplicate", failures);

    const full_engine::TerrainChunkAssetDesc* found = catalog.findChunkAssets(desc.id);
    expect(found != nullptr, "added asset desc can be found", failures);
    if (found != nullptr)
    {
        expect(sameDescCore(*found, desc), "found asset desc matches added desc", failures);
    }
}

void testUpdateRemoveAndClear(std::vector<std::string>& failures)
{
    full_engine::TerrainAssetCatalog catalog;
    full_engine::TerrainChunkAssetDesc original = makeDesc({12, 0, 0}, 1);
    full_engine::TerrainChunkAssetDesc replacement = makeDesc({12, 0, 0}, 2);
    replacement.splatMap = asset(500);

    expect(catalog.updateChunkAssets(original) == full_engine::TerrainAssetCatalogResult::NotFound, "missing asset update is not found", failures);
    expect(catalog.removeChunkAssets(original.id) == full_engine::TerrainAssetCatalogResult::NotFound, "missing asset remove is not found", failures);
    expect(catalog.addChunkAssets(original) == full_engine::TerrainAssetCatalogResult::Success, "asset add before update succeeds", failures);
    expect(catalog.updateChunkAssets(replacement) == full_engine::TerrainAssetCatalogResult::Success, "asset update succeeds", failures);

    const full_engine::TerrainChunkAssetDesc* found = catalog.findChunkAssets(original.id);
    expect(found != nullptr, "updated asset desc can be found", failures);
    if (found != nullptr)
    {
        expect(sameDescCore(*found, replacement), "updated asset desc is replaced", failures);
    }

    expect(catalog.removeChunkAssets(original.id) == full_engine::TerrainAssetCatalogResult::Success, "asset remove succeeds", failures);
    expect(!catalog.contains(original.id), "asset remove clears lookup", failures);

    expect(catalog.addChunkAssets(makeDesc({13, 0, 0}, 1)) == full_engine::TerrainAssetCatalogResult::Success, "first asset add before clear succeeds", failures);
    expect(catalog.addChunkAssets(makeDesc({14, 0, 0}, 1)) == full_engine::TerrainAssetCatalogResult::Success, "second asset add before clear succeeds", failures);
    catalog.clear();
    expect(catalog.assetCount() == 0, "asset clear removes all descriptors", failures);
}

void testInvalidMutationsDoNotChangeCatalog(std::vector<std::string>& failures)
{
    full_engine::TerrainAssetCatalog catalog;
    full_engine::TerrainChunkAssetDesc valid = makeDesc({15, 0, 0}, 1);
    full_engine::TerrainChunkAssetDesc invalidAdd = makeDesc({16, 0, 0}, 1);
    invalidAdd.lods[0].mesh = {};
    full_engine::TerrainChunkAssetDesc invalidUpdate = makeDesc({15, 0, 0}, 1);
    invalidUpdate.lods[0].material = {};

    expect(catalog.addChunkAssets(valid) == full_engine::TerrainAssetCatalogResult::Success, "valid asset add succeeds", failures);
    expect(catalog.addChunkAssets(invalidAdd) == full_engine::TerrainAssetCatalogResult::InvalidArgument, "invalid asset add is rejected", failures);
    expect(catalog.assetCount() == 1, "invalid asset add does not change count", failures);
    expect(catalog.updateChunkAssets(invalidUpdate) == full_engine::TerrainAssetCatalogResult::InvalidArgument, "invalid asset update is rejected", failures);
    expect(catalog.assetCount() == 1, "invalid asset update does not change count", failures);

    const full_engine::TerrainChunkAssetDesc* found = catalog.findChunkAssets(valid.id);
    expect(found != nullptr, "valid asset desc remains after invalid update", failures);
    if (found != nullptr)
    {
        expect(sameDescCore(*found, valid), "invalid asset update preserves existing desc", failures);
    }
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testAssetIdSemantics(failures);
    testValidDescriptors(failures);
    testInvalidLodCount(failures);
    testInvalidLodFields(failures);
    testAddFindAndDuplicate(failures);
    testUpdateRemoveAndClear(failures);
    testInvalidMutationsDoNotChangeCatalog(failures);

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
