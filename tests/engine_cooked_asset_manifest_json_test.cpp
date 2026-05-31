#include "engine/assets/CookedAssetManifestJson.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
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

std::string readFile(const char* path)
{
    std::ifstream input(path);
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

void writeText(const char* path, const char* text)
{
    std::ofstream output(path, std::ios::out | std::ios::trunc);
    output << text;
}

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return full_engine::AssetId{value};
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

full_engine::AssetRecord makeMaterialWithDependencies()
{
    full_engine::AssetRecord record = makeAsset(20, full_engine::AssetKind::Material);
    record.dependencyCount = 2;
    record.dependencies[0].id = asset(30);
    record.dependencies[0].kind = full_engine::AssetKind::Texture;
    record.dependencies[1].id = asset(40);
    record.dependencies[1].kind = full_engine::AssetKind::Shader;
    return record;
}

full_engine::TerrainChunkAssetDesc makeTerrain(
    const full_engine::ChunkId& id,
    const std::uint64_t mesh0,
    const std::uint64_t material,
    const std::uint64_t splat)
{
    full_engine::TerrainChunkAssetDesc desc;
    desc.id = id;
    desc.lodCount = 2;
    desc.splatMap = asset(splat);
    desc.lods[0].mesh = asset(mesh0);
    desc.lods[0].material = asset(material);
    desc.lods[0].maxDistanceMeters = 12.5f;
    desc.lods[1].mesh = asset(mesh0 + 1);
    desc.lods[1].material = asset(material);
    desc.lods[1].maxDistanceMeters = 100.0f;
    return desc;
}

full_engine::CookedAssetManifest makeManifest()
{
    full_engine::CookedAssetManifest manifest;
    manifest.assets.push_back(makeAsset(10, full_engine::AssetKind::Mesh));
    manifest.assets.push_back(makeAsset(11, full_engine::AssetKind::Mesh));
    manifest.assets.push_back(makeMaterialWithDependencies());
    manifest.assets.push_back(makeAsset(30, full_engine::AssetKind::Texture));
    manifest.assets.push_back(makeAsset(40, full_engine::AssetKind::Shader));
    manifest.assets.push_back(makeAsset(31, full_engine::AssetKind::Texture));
    manifest.terrainChunks.push_back(makeTerrain({-2, 0, 3}, 10, 20, 31));
    manifest.terrainChunks.push_back(makeTerrain({4, 1, -5}, 10, 20, 0));
    return manifest;
}

void expectAssetEqual(
    const full_engine::AssetRecord& actual,
    const full_engine::AssetRecord& expected,
    const char* label,
    std::vector<std::string>& failures)
{
    expect(actual.id == expected.id, label, failures);
    expect(actual.kind == expected.kind, label, failures);
    expect(actual.dependencyCount == expected.dependencyCount, label, failures);
    for (std::uint32_t index = 0; index < expected.dependencyCount; ++index)
    {
        expect(actual.dependencies[index].id == expected.dependencies[index].id, label, failures);
        expect(actual.dependencies[index].kind == expected.dependencies[index].kind, label, failures);
    }
}

void expectTerrainEqual(
    const full_engine::TerrainChunkAssetDesc& actual,
    const full_engine::TerrainChunkAssetDesc& expected,
    const char* label,
    std::vector<std::string>& failures)
{
    expect(actual.id == expected.id, label, failures);
    expect(actual.lodCount == expected.lodCount, label, failures);
    expect(actual.splatMap == expected.splatMap, label, failures);
    for (std::uint32_t index = 0; index < expected.lodCount; ++index)
    {
        expect(actual.lods[index].mesh == expected.lods[index].mesh, label, failures);
        expect(actual.lods[index].material == expected.lods[index].material, label, failures);
        expect(actual.lods[index].maxDistanceMeters == expected.lods[index].maxDistanceMeters, label, failures);
    }
}

void testEmptyManifestRoundTrips(std::vector<std::string>& failures)
{
    const char* path = "cooked_asset_manifest_empty_test.jsonl";
    std::remove(path);

    const full_engine::CookedAssetManifest manifest;
    const full_engine::CookedAssetManifestExportResult exported =
        full_engine::exportCookedAssetManifestJsonLines(manifest, path);
    const full_engine::CookedAssetManifestImport imported =
        full_engine::importCookedAssetManifestJsonLines(path);

    expect(exported == full_engine::CookedAssetManifestExportResult::Success, "empty manifest export succeeds", failures);
    expect(readFile(path).empty(), "empty manifest writes empty file", failures);
    expect(imported.result == full_engine::CookedAssetManifestImportResult::Success, "empty manifest import succeeds", failures);
    expect(imported.manifest.assets.empty(), "empty manifest imports no assets", failures);
    expect(imported.manifest.terrainChunks.empty(), "empty manifest imports no terrain chunks", failures);
    std::remove(path);
}

void testManifestRoundTripsWithOrder(std::vector<std::string>& failures)
{
    const char* path = "cooked_asset_manifest_round_trip_test.jsonl";
    std::remove(path);

    const full_engine::CookedAssetManifest manifest = makeManifest();
    const full_engine::CookedAssetManifestExportResult exported =
        full_engine::exportCookedAssetManifestJsonLines(manifest, path);
    const full_engine::CookedAssetManifestImport imported =
        full_engine::importCookedAssetManifestJsonLines(path);

    expect(exported == full_engine::CookedAssetManifestExportResult::Success, "manifest export succeeds", failures);
    expect(imported.result == full_engine::CookedAssetManifestImportResult::Success, "manifest import succeeds", failures);
    expect(imported.manifest.assets.size() == manifest.assets.size(), "asset count round-trips", failures);
    expect(imported.manifest.terrainChunks.size() == manifest.terrainChunks.size(), "terrain count round-trips", failures);
    if (imported.manifest.assets.size() == manifest.assets.size())
    {
        expectAssetEqual(imported.manifest.assets[2], manifest.assets[2], "asset dependencies round-trip", failures);
    }
    if (imported.manifest.terrainChunks.size() == manifest.terrainChunks.size())
    {
        expectTerrainEqual(imported.manifest.terrainChunks[0], manifest.terrainChunks[0], "first terrain round-trips", failures);
        expectTerrainEqual(imported.manifest.terrainChunks[1], manifest.terrainChunks[1], "default splat terrain round-trips", failures);
    }
    std::remove(path);
}

void testExportedFieldsAreDeterministic(std::vector<std::string>& failures)
{
    const char* path = "cooked_asset_manifest_fields_test.jsonl";
    std::remove(path);

    const full_engine::CookedAssetManifest manifest = makeManifest();
    const full_engine::CookedAssetManifestExportResult exported =
        full_engine::exportCookedAssetManifestJsonLines(manifest, path);
    const std::string content = readFile(path);

    expect(exported == full_engine::CookedAssetManifestExportResult::Success, "field export succeeds", failures);
    expect(content.find("\"record\":\"asset\"") != std::string::npos, "asset record field is exported", failures);
    expect(content.find("\"kind\":\"Material\"") != std::string::npos, "asset kind is exported", failures);
    expect(content.find("\"dependency0Kind\":\"Texture\"") != std::string::npos, "dependency kind is exported", failures);
    expect(content.find("\"record\":\"terrainChunk\"") != std::string::npos, "terrain record field is exported", failures);
    expect(content.find("\"chunkX\":-2") != std::string::npos, "terrain chunk coordinate is exported", failures);
    expect(content.find("\"lod1MaxDistanceMeters\":100") != std::string::npos, "terrain lod distance is exported", failures);
    expect(content.find("\"splatMap\":0") != std::string::npos, "default splat map is exported as zero", failures);
    std::remove(path);
}

void testInvalidArgumentsAndIo(std::vector<std::string>& failures)
{
    const full_engine::CookedAssetManifest manifest;

    expect(
        full_engine::exportCookedAssetManifestJsonLines(manifest, nullptr) ==
            full_engine::CookedAssetManifestExportResult::InvalidArgument,
        "null export path is invalid",
        failures);
    expect(
        full_engine::exportCookedAssetManifestJsonLines(manifest, "") ==
            full_engine::CookedAssetManifestExportResult::InvalidArgument,
        "empty export path is invalid",
        failures);
    expect(
        full_engine::exportCookedAssetManifestJsonLines(manifest, ".") ==
            full_engine::CookedAssetManifestExportResult::IoError,
        "directory export path is IO error",
        failures);
    expect(
        full_engine::importCookedAssetManifestJsonLines(nullptr).result ==
            full_engine::CookedAssetManifestImportResult::InvalidArgument,
        "null import path is invalid",
        failures);
    expect(
        full_engine::importCookedAssetManifestJsonLines("").result ==
            full_engine::CookedAssetManifestImportResult::InvalidArgument,
        "empty import path is invalid",
        failures);
    expect(
        full_engine::importCookedAssetManifestJsonLines("missing_cooked_asset_manifest.jsonl").result ==
            full_engine::CookedAssetManifestImportResult::IoError,
        "missing import path is IO error",
        failures);
    expect(
        std::string(full_engine::cookedAssetManifestExportResultName(
            full_engine::CookedAssetManifestExportResult::IoError)) == "IoError",
        "export result names are deterministic",
        failures);
    expect(
        std::string(full_engine::cookedAssetManifestImportResultName(
            full_engine::CookedAssetManifestImportResult::ValidationError)) == "ValidationError",
        "import result names are deterministic",
        failures);
    expect(std::string(full_engine::assetKindName(full_engine::AssetKind::SkinnedMesh)) == "SkinnedMesh",
        "asset kind names are deterministic",
        failures);
}

void testUnknownFieldsAreIgnored(std::vector<std::string>& failures)
{
    const char* path = "cooked_asset_manifest_unknown_fields_test.jsonl";
    std::remove(path);
    writeText(
        path,
        "{\"ignored\":true,\"record\":\"asset\",\"id\":1,\"kind\":\"Mesh\",\"dependencyCount\":0}\n"
        "{\"record\":\"asset\",\"id\":2,\"kind\":\"Material\",\"dependencyCount\":0,\"ignored\":\"yes\"}\n"
        "{\"record\":\"terrainChunk\",\"chunkX\":0,\"chunkY\":0,\"chunkZ\":0,\"lodCount\":1,\"lod0Mesh\":1,\"lod0Material\":2,\"lod0MaxDistanceMeters\":10,\"splatMap\":0,\"ignored\":99}\n");

    const full_engine::CookedAssetManifestImport imported =
        full_engine::importCookedAssetManifestJsonLines(path);

    expect(imported.result == full_engine::CookedAssetManifestImportResult::Success, "unknown fields are ignored", failures);
    expect(imported.manifest.assets.size() == 2, "unknown field import keeps assets", failures);
    expect(imported.manifest.terrainChunks.size() == 1, "unknown field import keeps terrain", failures);
    std::remove(path);
}

void testMalformedLinesReturnParseError(std::vector<std::string>& failures)
{
    const char* path = "cooked_asset_manifest_malformed_test.jsonl";
    std::remove(path);

    writeText(path, "{\"record\":\"asset\",\"id\":1,\"kind\":\"Mesh\"}\n");
    full_engine::CookedAssetManifestImport imported =
        full_engine::importCookedAssetManifestJsonLines(path);
    expect(imported.result == full_engine::CookedAssetManifestImportResult::ParseError, "missing field is parse error", failures);
    expect(imported.manifest.assets.empty(), "parse error returns no partial manifest", failures);

    writeText(path, "{\"record\":\"mystery\",\"id\":1}\n");
    expect(
        full_engine::importCookedAssetManifestJsonLines(path).result ==
            full_engine::CookedAssetManifestImportResult::ParseError,
        "unknown record type is parse error",
        failures);

    writeText(path, "{\"record\":\"asset\",\"id\":1,\"kind\":\"Nope\",\"dependencyCount\":0}\n");
    expect(
        full_engine::importCookedAssetManifestJsonLines(path).result ==
            full_engine::CookedAssetManifestImportResult::ParseError,
        "invalid enum string is parse error",
        failures);

    writeText(path, "{\"record\":\"asset\",\"id\":18446744073709551616,\"kind\":\"Mesh\",\"dependencyCount\":0}\n");
    expect(
        full_engine::importCookedAssetManifestJsonLines(path).result ==
            full_engine::CookedAssetManifestImportResult::ParseError,
        "overflow asset id is parse error",
        failures);

    writeText(
        path,
        "{\"record\":\"terrainChunk\",\"chunkX\":2147483648,\"chunkY\":0,\"chunkZ\":0,\"lodCount\":1,\"lod0Mesh\":1,\"lod0Material\":2,\"lod0MaxDistanceMeters\":10,\"splatMap\":0}\n");
    expect(
        full_engine::importCookedAssetManifestJsonLines(path).result ==
            full_engine::CookedAssetManifestImportResult::ParseError,
        "overflow coordinate is parse error",
        failures);

    writeText(
        path,
        "{\"record\":\"terrainChunk\",\"chunkX\":0,\"chunkY\":0,\"chunkZ\":0,\"lodCount\":1,\"lod0Mesh\":1,\"lod0Material\":2,\"lod0MaxDistanceMeters\":nan,\"splatMap\":0}\n");
    expect(
        full_engine::importCookedAssetManifestJsonLines(path).result ==
            full_engine::CookedAssetManifestImportResult::ParseError,
        "invalid float is parse error",
        failures);

    std::remove(path);
}

void testValidationErrorPreservesDetail(std::vector<std::string>& failures)
{
    const char* path = "cooked_asset_manifest_validation_error_test.jsonl";
    std::remove(path);
    writeText(
        path,
        "{\"record\":\"asset\",\"id\":1,\"kind\":\"Unknown\",\"dependencyCount\":0}\n");

    const full_engine::CookedAssetManifestImport imported =
        full_engine::importCookedAssetManifestJsonLines(path);

    expect(imported.result == full_engine::CookedAssetManifestImportResult::ValidationError, "invalid parsed manifest is validation error", failures);
    expect(imported.manifest.assets.empty(), "validation error returns empty manifest", failures);
    expect(
        imported.validation.result == full_engine::CookedAssetManifestValidationResult::InvalidAssetRecord,
        "validation error preserves manifest detail",
        failures);
    expect(
        imported.validation.assetValidation == full_engine::AssetRecordValidationResult::InvalidKind,
        "validation error preserves nested asset detail",
        failures);
    std::remove(path);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testEmptyManifestRoundTrips(failures);
    testManifestRoundTripsWithOrder(failures);
    testExportedFieldsAreDeterministic(failures);
    testInvalidArgumentsAndIo(failures);
    testUnknownFieldsAreIgnored(failures);
    testMalformedLinesReturnParseError(failures);
    testValidationErrorPreservesDetail(failures);

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
