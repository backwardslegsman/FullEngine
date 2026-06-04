#include "engine/assets/GltfMaterialAssetImporter.hpp"
#include "engine/assets/LoadedTextureImageImporter.hpp"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#ifndef FULL_RENDERER_TEST_GLTF_FIXTURE_DIR
#define FULL_RENDERER_TEST_GLTF_FIXTURE_DIR "."
#endif

namespace
{
void expect(const bool condition, const char* const message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

std::string fixturePath(const char* const name)
{
    return std::string(FULL_RENDERER_TEST_GLTF_FIXTURE_DIR) + "/" + name;
}

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

full_engine::GltfMaterialAssetImportOptions options()
{
    full_engine::GltfMaterialAssetImportOptions result;
    result.firstMaterialId = asset(1000);
    result.firstTextureId = asset(2000);
    return result;
}

void testBaseColorMaterialExtraction(std::vector<std::string>& failures)
{
    const full_engine::GltfMaterialAssetImportResult result =
        full_engine::importGltfMaterialAssetSources(
            fixturePath("material_base_color.gltf"),
            options());

    expect(result.status == full_engine::GltfMaterialAssetImportStatus::Success, "base-color material extraction succeeds", failures);
    expect(result.records.size() == 1, "base-color extraction reports one material record", failures);
    expect(result.records[0].status == full_engine::GltfMaterialAssetImportRecordStatus::Planned, "base-color material is planned", failures);
    expect(result.records[0].materialId == asset(1000), "material id is assigned from base", failures);
    expect(result.records[0].baseColorTextureId == asset(2000), "texture id is assigned from base", failures);
    expect(result.summary.plannedMaterialCount == 1, "planned material count increments", failures);
    expect(result.summary.emittedTextureSourceCount == 1, "one texture source emitted", failures);
    expect(result.summary.emittedMaterialSourceCount == 1, "one material source emitted", failures);
    expect(result.summary.emittedMaterialPayloadCount == 1, "one material payload emitted", failures);
    expect(result.sourceRecords.size() == 2, "texture and material sources emitted", failures);
    expect(result.sourceRecords[0].kind == full_engine::AssetKind::Texture, "texture source emitted before material", failures);
    expect(result.sourceRecords[0].id == asset(2000), "texture source id matches assigned id", failures);
    expect(result.sourceRecords[0].descriptor.texture.width == 2, "texture width comes from image metadata", failures);
    expect(result.sourceRecords[0].descriptor.texture.height == 1, "texture height comes from image metadata", failures);
    expect(result.sourceRecords[0].descriptor.texture.format == full_engine::AssetSourceTextureFormat::Rgba8, "texture source is rgba8 contract", failures);
    expect(result.sourceRecords[0].descriptor.texture.semantic == full_engine::AssetSourceTextureSemantic::Color, "texture semantic defaults to color", failures);
    expect(result.sourceRecords[0].descriptor.texture.colorSpace == full_engine::AssetSourceTextureColorSpace::Srgb, "texture color-space defaults to srgb", failures);
    expect(result.sourceRecords[1].kind == full_engine::AssetKind::Material, "material source emitted after texture", failures);
    expect(result.sourceRecords[1].descriptor.material.alphaMode == full_engine::AssetSourceMaterialAlphaMode::AlphaBlend, "gltf blend alpha maps to alpha blend", failures);
    expect(result.sourceRecords[1].descriptor.material.textureRefCount == 1, "material source references texture", failures);
    expect(
        result.sourceRecords[1].descriptor.material.textureRefs[0].slot == full_engine::AssetSourceMaterialTextureSlot::BaseColor &&
            result.sourceRecords[1].descriptor.material.textureRefs[0].id == asset(2000),
        "material source references assigned base-color texture id",
        failures);
    expect(result.materialPayloads.size() == 1, "material payload emitted", failures);
    expect(result.materialPayloads[0].kind == full_engine::AssetKind::Material, "payload kind is material", failures);
    expect(result.materialPayloads[0].material.alphaMode == full_engine::AssetSourceMaterialAlphaMode::AlphaBlend, "payload alpha mode maps from gltf", failures);
    expect(result.materialPayloads[0].material.textureRefCount == 1, "payload references one texture", failures);
    expect(
        result.materialPayloads[0].material.textureRefs[0].slot == full_engine::AssetSourceMaterialTextureSlot::BaseColor &&
            result.materialPayloads[0].material.textureRefs[0].id == asset(2000),
        "payload references assigned base-color texture id",
        failures);
    expect(full_engine::validateLoadedAssetPayload(result.materialPayloads[0]) == full_engine::LoadedAssetPayloadValidationResult::Success, "extracted material payload validates", failures);

    const full_engine::LoadedTextureImageImportResult texture =
        full_engine::importLoadedTexturePayloadFromImageFile(result.sourceRecords[0]);
    expect(texture.status == full_engine::LoadedTextureImageImportStatus::Success, "extracted texture source imports through image importer", failures);
}

void testMaterialWithoutTexture(std::vector<std::string>& failures)
{
    const full_engine::GltfMaterialAssetImportResult result =
        full_engine::importGltfMaterialAssetSources(
            fixturePath("material_no_texture.gltf"),
            options());

    expect(result.status == full_engine::GltfMaterialAssetImportStatus::Success, "material without texture extracts", failures);
    expect(result.records.size() == 1, "no-texture extraction reports one material record", failures);
    expect(result.records[0].status == full_engine::GltfMaterialAssetImportRecordStatus::NoBaseColorTexture, "no-texture material is diagnosed", failures);
    expect(result.summary.noBaseColorTextureCount == 1, "no-texture count increments", failures);
    expect(result.summary.emittedTextureSourceCount == 0, "no texture source emitted", failures);
    expect(result.summary.emittedMaterialSourceCount == 1, "material source still emitted", failures);
    expect(result.materialPayloads.size() == 1, "material payload emitted without texture", failures);
    expect(result.materialPayloads[0].material.textureRefCount == 0, "material payload has zero texture refs", failures);
    expect(result.sourceRecords.size() == 1 && result.sourceRecords[0].kind == full_engine::AssetKind::Material, "only material source emitted", failures);
}

void testTextureInfoFailureIsDiagnostic(std::vector<std::string>& failures)
{
    const full_engine::GltfMaterialAssetImportResult result =
        full_engine::importGltfMaterialAssetSources(
            fixturePath("material_missing_image.gltf"),
            options());

    expect(result.status == full_engine::GltfMaterialAssetImportStatus::Success, "missing image keeps top-level extraction successful", failures);
    expect(result.records.size() == 1, "missing image reports material record", failures);
    expect(result.records[0].status == full_engine::GltfMaterialAssetImportRecordStatus::TextureInfoFailed, "missing image reports texture info failure", failures);
    expect(result.summary.textureInfoFailedCount == 1, "texture info failure count increments", failures);
    expect(result.sourceRecords.empty(), "missing image emits no source records", failures);
    expect(result.materialPayloads.empty(), "missing image emits no material payload", failures);
}

void testTopLevelFailures(std::vector<std::string>& failures)
{
    full_engine::GltfMaterialAssetImportOptions invalid = options();
    invalid.firstTextureId = {};
    const full_engine::GltfMaterialAssetImportResult invalidOptions =
        full_engine::importGltfMaterialAssetSources(fixturePath("material_base_color.gltf"), invalid);
    expect(invalidOptions.status == full_engine::GltfMaterialAssetImportStatus::InvalidArgument, "invalid options are rejected", failures);

    const full_engine::GltfMaterialAssetImportResult missing =
        full_engine::importGltfMaterialAssetSources(fixturePath("missing_materials.gltf"), options());
    expect(missing.status == full_engine::GltfMaterialAssetImportStatus::IoError, "missing gltf reports io error", failures);

    const full_engine::GltfMaterialAssetImportResult malformed =
        full_engine::importGltfMaterialAssetSources(fixturePath("malformed.gltf"), options());
    expect(malformed.status == full_engine::GltfMaterialAssetImportStatus::ParseError, "malformed gltf reports parse error", failures);

    const full_engine::GltfMaterialAssetImportResult empty =
        full_engine::importGltfMaterialAssetSources(fixturePath("empty_scene.gltf"), options());
    expect(empty.status == full_engine::GltfMaterialAssetImportStatus::UnsupportedScene, "scene without materials is unsupported", failures);
}

void testStatusNames(std::vector<std::string>& failures)
{
    const full_engine::GltfMaterialAssetImportStatus statuses[] = {
        full_engine::GltfMaterialAssetImportStatus::Success,
        full_engine::GltfMaterialAssetImportStatus::InvalidArgument,
        full_engine::GltfMaterialAssetImportStatus::IoError,
        full_engine::GltfMaterialAssetImportStatus::ParseError,
        full_engine::GltfMaterialAssetImportStatus::UnsupportedScene};
    for (const full_engine::GltfMaterialAssetImportStatus status : statuses)
    {
        expect(std::string(full_engine::gltfMaterialAssetImportStatusName(status)) != "Unknown", "top-level status has stable name", failures);
    }

    const full_engine::GltfMaterialAssetImportRecordStatus recordStatuses[] = {
        full_engine::GltfMaterialAssetImportRecordStatus::Planned,
        full_engine::GltfMaterialAssetImportRecordStatus::NoBaseColorTexture,
        full_engine::GltfMaterialAssetImportRecordStatus::TextureInfoFailed,
        full_engine::GltfMaterialAssetImportRecordStatus::SourceValidationFailed,
        full_engine::GltfMaterialAssetImportRecordStatus::PayloadValidationFailed};
    for (const full_engine::GltfMaterialAssetImportRecordStatus status : recordStatuses)
    {
        expect(std::string(full_engine::gltfMaterialAssetImportRecordStatusName(status)) != "Unknown", "record status has stable name", failures);
    }
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testBaseColorMaterialExtraction(failures);
    testMaterialWithoutTexture(failures);
    testTextureInfoFailureIsDiagnostic(failures);
    testTopLevelFailures(failures);
    testStatusNames(failures);

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
