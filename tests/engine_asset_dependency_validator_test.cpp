#include "engine/assets/AssetDependencyValidator.hpp"

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
    const std::uint64_t id,
    const full_engine::AssetKind kind)
{
    full_engine::AssetRecord record;
    record.id = asset(id);
    record.kind = kind;
    return record;
}

full_engine::AssetRecord makeMaterialWithDependencies()
{
    full_engine::AssetRecord record = makeRecord(10, full_engine::AssetKind::Material);
    record.dependencyCount = 2;
    record.dependencies[0].id = asset(20);
    record.dependencies[0].kind = full_engine::AssetKind::Texture;
    record.dependencies[1].id = asset(30);
    record.dependencies[1].kind = full_engine::AssetKind::Shader;
    return record;
}

full_engine::AssetCatalog makeCatalogWithDependencies()
{
    full_engine::AssetCatalog catalog;
    (void)catalog.addAsset(makeRecord(20, full_engine::AssetKind::Texture));
    (void)catalog.addAsset(makeRecord(30, full_engine::AssetKind::Shader));
    return catalog;
}

void testValidDependenciesSucceed(std::vector<std::string>& failures)
{
    const full_engine::AssetCatalog catalog = makeCatalogWithDependencies();
    const full_engine::AssetDependencyValidation validation =
        full_engine::validateAssetDependencies(makeMaterialWithDependencies(), catalog);

    expect(
        validation.result == full_engine::AssetDependencyValidationResult::Success,
        "valid dependencies succeed",
        failures);
    expect(
        validation.dependencyIndex == full_engine::AssetDependencyValidation::invalidDependencyIndex,
        "successful validation leaves dependency index unset",
        failures);
}

void testZeroDependenciesSucceed(std::vector<std::string>& failures)
{
    const full_engine::AssetCatalog catalog;
    const full_engine::AssetDependencyValidation validation =
        full_engine::validateAssetDependencies(makeRecord(1, full_engine::AssetKind::Mesh), catalog);

    expect(
        validation.result == full_engine::AssetDependencyValidationResult::Success,
        "record with zero dependencies succeeds",
        failures);
}

void testInvalidRecordIsReported(std::vector<std::string>& failures)
{
    full_engine::AssetRecord record = makeMaterialWithDependencies();
    record.dependencyCount = full_engine::kMaxAssetDependencies + 1;

    const full_engine::AssetDependencyValidation validation =
        full_engine::validateAssetDependencies(record, makeCatalogWithDependencies());

    expect(
        validation.result == full_engine::AssetDependencyValidationResult::InvalidAssetRecord,
        "invalid source record is reported",
        failures);
    expect(
        validation.recordValidation == full_engine::AssetRecordValidationResult::InvalidDependencyCount,
        "invalid source record preserves nested validation detail",
        failures);
}

void testMissingDependencyIsReported(std::vector<std::string>& failures)
{
    full_engine::AssetCatalog catalog;
    (void)catalog.addAsset(makeRecord(20, full_engine::AssetKind::Texture));

    const full_engine::AssetDependencyValidation validation =
        full_engine::validateAssetDependencies(makeMaterialWithDependencies(), catalog);

    expect(
        validation.result == full_engine::AssetDependencyValidationResult::MissingDependency,
        "missing dependency is reported",
        failures);
    expect(validation.dependencyIndex == 1, "missing dependency reports dependency index", failures);
}

void testWrongDependencyKindIsReported(std::vector<std::string>& failures)
{
    full_engine::AssetCatalog catalog;
    (void)catalog.addAsset(makeRecord(20, full_engine::AssetKind::Texture));
    (void)catalog.addAsset(makeRecord(30, full_engine::AssetKind::Texture));

    const full_engine::AssetDependencyValidation validation =
        full_engine::validateAssetDependencies(makeMaterialWithDependencies(), catalog);

    expect(
        validation.result == full_engine::AssetDependencyValidationResult::WrongDependencyKind,
        "wrong dependency kind is reported",
        failures);
    expect(validation.dependencyIndex == 1, "wrong dependency kind reports dependency index", failures);
}

void testInactiveDependenciesAreIgnored(std::vector<std::string>& failures)
{
    full_engine::AssetRecord record = makeRecord(40, full_engine::AssetKind::Material);
    record.dependencyCount = 1;
    record.dependencies[0].id = asset(20);
    record.dependencies[0].kind = full_engine::AssetKind::Texture;
    record.dependencies[1].id = {};
    record.dependencies[1].kind = full_engine::AssetKind::Unknown;

    full_engine::AssetCatalog catalog;
    (void)catalog.addAsset(makeRecord(20, full_engine::AssetKind::Texture));

    const full_engine::AssetDependencyValidation validation =
        full_engine::validateAssetDependencies(record, catalog);

    expect(
        validation.result == full_engine::AssetDependencyValidationResult::Success,
        "inactive dependency slots are ignored",
        failures);
}

void testValidationDoesNotMutateCatalog(std::vector<std::string>& failures)
{
    full_engine::AssetCatalog catalog = makeCatalogWithDependencies();
    const std::size_t countBefore = catalog.assetCount();

    const full_engine::AssetDependencyValidation validation =
        full_engine::validateAssetDependencies(makeMaterialWithDependencies(), catalog);

    expect(
        validation.result == full_engine::AssetDependencyValidationResult::Success,
        "validation before mutation check succeeds",
        failures);
    expect(catalog.assetCount() == countBefore, "dependency validation does not mutate catalog", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testValidDependenciesSucceed(failures);
    testZeroDependenciesSucceed(failures);
    testInvalidRecordIsReported(failures);
    testMissingDependencyIsReported(failures);
    testWrongDependencyKindIsReported(failures);
    testInactiveDependenciesAreIgnored(failures);
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
