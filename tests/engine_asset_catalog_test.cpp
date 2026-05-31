#include "engine/assets/AssetCatalog.hpp"

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

full_engine::AssetRecord makeRecord(
    const full_engine::AssetId id,
    const full_engine::AssetKind kind = full_engine::AssetKind::Mesh)
{
    full_engine::AssetRecord record;
    record.id = id;
    record.kind = kind;
    return record;
}

full_engine::AssetRecord makeRecordWithDependencies(const full_engine::AssetId id)
{
    full_engine::AssetRecord record = makeRecord(id, full_engine::AssetKind::Material);
    record.dependencyCount = 2;
    record.dependencies[0].id = asset(100);
    record.dependencies[0].kind = full_engine::AssetKind::Texture;
    record.dependencies[1].id = asset(101);
    record.dependencies[1].kind = full_engine::AssetKind::Shader;
    return record;
}

bool sameRecordCore(const full_engine::AssetRecord& lhs, const full_engine::AssetRecord& rhs)
{
    if (!(lhs.id == rhs.id) || lhs.kind != rhs.kind || lhs.dependencyCount != rhs.dependencyCount)
    {
        return false;
    }

    for (std::uint32_t index = 0; index < lhs.dependencyCount; ++index)
    {
        if (!(lhs.dependencies[index].id == rhs.dependencies[index].id) ||
            lhs.dependencies[index].kind != rhs.dependencies[index].kind)
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

void testValidRecords(std::vector<std::string>& failures)
{
    expect(
        full_engine::validateAssetRecord(makeRecord(asset(1))) ==
            full_engine::AssetRecordValidationResult::Success,
        "record with zero dependencies validates",
        failures);
    expect(
        full_engine::validateAssetRecord(makeRecordWithDependencies(asset(2))) ==
            full_engine::AssetRecordValidationResult::Success,
        "record with multiple dependencies validates",
        failures);
}

void testInvalidRecords(std::vector<std::string>& failures)
{
    full_engine::AssetRecord invalidId = makeRecord(asset(1));
    invalidId.id = {};

    full_engine::AssetRecord invalidKind = makeRecord(asset(2));
    invalidKind.kind = full_engine::AssetKind::Unknown;

    full_engine::AssetRecord tooManyDependencies = makeRecord(asset(3));
    tooManyDependencies.dependencyCount = full_engine::kMaxAssetDependencies + 1;

    full_engine::AssetRecord invalidDependencyId = makeRecordWithDependencies(asset(4));
    invalidDependencyId.dependencies[1].id = {};

    full_engine::AssetRecord invalidDependencyKind = makeRecordWithDependencies(asset(5));
    invalidDependencyKind.dependencies[0].kind = full_engine::AssetKind::Unknown;

    expect(
        full_engine::validateAssetRecord(invalidId) ==
            full_engine::AssetRecordValidationResult::InvalidAssetId,
        "invalid asset id is rejected",
        failures);
    expect(
        full_engine::validateAssetRecord(invalidKind) ==
            full_engine::AssetRecordValidationResult::InvalidKind,
        "invalid asset kind is rejected",
        failures);
    expect(
        full_engine::validateAssetRecord(tooManyDependencies) ==
            full_engine::AssetRecordValidationResult::InvalidDependencyCount,
        "too many dependencies is rejected",
        failures);
    expect(
        full_engine::validateAssetRecord(invalidDependencyId) ==
            full_engine::AssetRecordValidationResult::InvalidDependencyId,
        "invalid dependency id is rejected",
        failures);
    expect(
        full_engine::validateAssetRecord(invalidDependencyKind) ==
            full_engine::AssetRecordValidationResult::InvalidDependencyKind,
        "invalid dependency kind is rejected",
        failures);
}

void testAddFindAndDuplicate(std::vector<std::string>& failures)
{
    full_engine::AssetCatalog catalog;
    const full_engine::AssetRecord record = makeRecordWithDependencies(asset(10));

    expect(catalog.assetCount() == 0, "asset catalog starts empty", failures);
    expect(catalog.addAsset(record) == full_engine::AssetCatalogResult::Success, "asset add succeeds", failures);
    expect(catalog.addAsset(record) == full_engine::AssetCatalogResult::AlreadyExists, "duplicate asset add is rejected", failures);
    expect(catalog.assetCount() == 1, "duplicate asset add preserves count", failures);
    expect(catalog.contains(record.id), "asset catalog contains added id", failures);

    const full_engine::AssetRecord* found = catalog.findAsset(record.id);
    expect(found != nullptr, "added asset can be found", failures);
    if (found != nullptr)
    {
        expect(sameRecordCore(*found, record), "found asset matches added record", failures);
    }
}

void testUpdateRemoveAndClear(std::vector<std::string>& failures)
{
    full_engine::AssetCatalog catalog;
    const full_engine::AssetRecord original = makeRecord(asset(20), full_engine::AssetKind::Mesh);
    const full_engine::AssetRecord replacement = makeRecordWithDependencies(asset(20));

    expect(catalog.updateAsset(original) == full_engine::AssetCatalogResult::NotFound, "missing update returns not found", failures);
    expect(catalog.removeAsset(original.id) == full_engine::AssetCatalogResult::NotFound, "missing remove returns not found", failures);
    expect(catalog.addAsset(original) == full_engine::AssetCatalogResult::Success, "asset add before update succeeds", failures);
    expect(catalog.updateAsset(replacement) == full_engine::AssetCatalogResult::Success, "asset update succeeds", failures);

    const full_engine::AssetRecord* found = catalog.findAsset(original.id);
    expect(found != nullptr, "updated asset can be found", failures);
    if (found != nullptr)
    {
        expect(sameRecordCore(*found, replacement), "updated asset is replaced", failures);
    }

    expect(catalog.removeAsset(original.id) == full_engine::AssetCatalogResult::Success, "asset remove succeeds", failures);
    expect(!catalog.contains(original.id), "asset remove clears lookup", failures);

    expect(catalog.addAsset(makeRecord(asset(21))) == full_engine::AssetCatalogResult::Success, "first add before clear succeeds", failures);
    expect(catalog.addAsset(makeRecord(asset(22), full_engine::AssetKind::Texture)) == full_engine::AssetCatalogResult::Success, "second add before clear succeeds", failures);
    catalog.clear();
    expect(catalog.assetCount() == 0, "asset clear removes all records", failures);
}

void testInvalidMutationsDoNotChangeCatalog(std::vector<std::string>& failures)
{
    full_engine::AssetCatalog catalog;
    const full_engine::AssetRecord valid = makeRecord(asset(30));
    full_engine::AssetRecord invalidAdd = makeRecord(asset(31));
    invalidAdd.kind = full_engine::AssetKind::Unknown;
    full_engine::AssetRecord invalidUpdate = makeRecord(asset(30));
    invalidUpdate.dependencyCount = 1;
    invalidUpdate.dependencies[0].id = {};

    expect(catalog.addAsset(valid) == full_engine::AssetCatalogResult::Success, "valid asset add succeeds", failures);
    expect(catalog.addAsset(invalidAdd) == full_engine::AssetCatalogResult::InvalidArgument, "invalid asset add is rejected", failures);
    expect(catalog.assetCount() == 1, "invalid asset add preserves count", failures);
    expect(catalog.updateAsset(invalidUpdate) == full_engine::AssetCatalogResult::InvalidArgument, "invalid asset update is rejected", failures);
    expect(catalog.assetCount() == 1, "invalid asset update preserves count", failures);

    const full_engine::AssetRecord* found = catalog.findAsset(valid.id);
    expect(found != nullptr, "valid asset remains after invalid update", failures);
    if (found != nullptr)
    {
        expect(sameRecordCore(*found, valid), "invalid update preserves existing record", failures);
    }
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testAssetIdSemantics(failures);
    testValidRecords(failures);
    testInvalidRecords(failures);
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
