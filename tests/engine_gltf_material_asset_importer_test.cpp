#include "engine/assets/GltfMaterialAssetImporter.hpp"
#include "engine/assets/GltfMaterialSlotAudit.hpp"
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

std::uint32_t countSlotRefs(
    const full_engine::LoadedMaterialAsset& material,
    const full_engine::AssetSourceMaterialTextureSlot slot) noexcept
{
    std::uint32_t count = 0;
    for (std::uint32_t index = 0; index < material.textureRefCount; ++index)
    {
        if (material.textureRefs[index].slot == slot)
        {
            ++count;
        }
    }
    return count;
}

const full_engine::AssetSourceRecord* findTextureSource(
    const full_engine::GltfMaterialAssetImportResult& result,
    const full_engine::AssetId id) noexcept
{
    for (const full_engine::AssetSourceRecord& source : result.sourceRecords)
    {
        if (source.kind == full_engine::AssetKind::Texture &&
            source.id == id)
        {
            return &source;
        }
    }
    return nullptr;
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

void testAllNamedMaterialTextureSlots(std::vector<std::string>& failures)
{
    const full_engine::GltfMaterialAssetImportResult result =
        full_engine::importGltfMaterialAssetSources(
            fixturePath("material_all_slots.gltf"),
            options());

    expect(result.status == full_engine::GltfMaterialAssetImportStatus::Success, "all-slot material extraction succeeds", failures);
    expect(result.materialPayloads.size() == 1, "all-slot extraction emits one material payload", failures);
    expect(result.summary.emittedTextureSourceCount == 3, "all-slot extraction deduplicates texture sources by uri and semantic policy", failures);
    expect(result.summary.emittedMaterialPayloadCount == 1, "all-slot extraction emits one material payload count", failures);
    if (result.materialPayloads.empty())
    {
        return;
    }

    const full_engine::LoadedMaterialAsset& material = result.materialPayloads[0].material;
    expect(material.textureRefCount == 5, "all-slot material payload has five named texture refs", failures);
    expect(countSlotRefs(material, full_engine::AssetSourceMaterialTextureSlot::BaseColor) == 1, "base-color slot ref is present", failures);
    expect(countSlotRefs(material, full_engine::AssetSourceMaterialTextureSlot::Normal) == 1, "normal slot ref is present", failures);
    expect(countSlotRefs(material, full_engine::AssetSourceMaterialTextureSlot::MetallicRoughness) == 1, "metallic-roughness slot ref is present", failures);
    expect(countSlotRefs(material, full_engine::AssetSourceMaterialTextureSlot::Occlusion) == 1, "occlusion slot ref is present", failures);
    expect(countSlotRefs(material, full_engine::AssetSourceMaterialTextureSlot::Emissive) == 1, "emissive slot ref is present", failures);

    for (std::uint32_t index = 0; index < material.textureRefCount; ++index)
    {
        const full_engine::AssetSourceMaterialTextureRef& ref = material.textureRefs[index];
        const full_engine::AssetSourceRecord* const source = findTextureSource(result, ref.id);
        expect(source != nullptr, "each all-slot material ref has a texture source", failures);
        if (source == nullptr)
        {
            continue;
        }

        if (ref.slot == full_engine::AssetSourceMaterialTextureSlot::Normal)
        {
            expect(source->descriptor.texture.semantic == full_engine::AssetSourceTextureSemantic::NormalMap, "normal slot source uses normal-map semantic", failures);
            expect(source->descriptor.texture.colorSpace == full_engine::AssetSourceTextureColorSpace::EncodedNormal, "normal slot source uses encoded-normal color space", failures);
        }
        else if (ref.slot == full_engine::AssetSourceMaterialTextureSlot::MetallicRoughness ||
            ref.slot == full_engine::AssetSourceMaterialTextureSlot::Occlusion)
        {
            expect(source->descriptor.texture.semantic == full_engine::AssetSourceTextureSemantic::LinearData, "linear data slot source uses linear-data semantic", failures);
            expect(source->descriptor.texture.colorSpace == full_engine::AssetSourceTextureColorSpace::Linear, "linear data slot source uses linear color space", failures);
        }
        else
        {
            expect(source->descriptor.texture.semantic == full_engine::AssetSourceTextureSemantic::Color, "color slot source uses color semantic", failures);
            expect(source->descriptor.texture.colorSpace == full_engine::AssetSourceTextureColorSpace::Srgb, "color slot source uses srgb color space", failures);
        }
    }
}

void testAllNamedMaterialTextureSlotAudit(std::vector<std::string>& failures)
{
    const full_engine::GltfMaterialAssetImportResult result =
        full_engine::importGltfMaterialAssetSources(
            fixturePath("material_all_slots.gltf"),
            options());
    std::vector<full_engine::LoadedAssetPayload> texturePayloads;
    std::vector<full_engine::AssetId> resolvedTextureIds;
    for (const full_engine::AssetSourceRecord& source : result.sourceRecords)
    {
        if (source.kind != full_engine::AssetKind::Texture)
        {
            continue;
        }
        const full_engine::LoadedTextureImageImportResult texture =
            full_engine::importLoadedTexturePayloadFromImageFile(source);
        if (texture.status == full_engine::LoadedTextureImageImportStatus::Success)
        {
            texturePayloads.push_back(texture.payload);
            resolvedTextureIds.push_back(source.id);
        }
    }

    const full_engine::GltfMaterialSlotAudit audit =
        full_engine::auditGltfMaterialSlots(
            fixturePath("material_all_slots.gltf"),
            options(),
            &result,
            texturePayloads.data(),
            texturePayloads.size(),
            resolvedTextureIds.data(),
            resolvedTextureIds.size());

    expect(audit.importStatus == full_engine::GltfMaterialAssetImportStatus::Success, "all-slot audit keeps import status", failures);
    expect(audit.records.size() == 1, "all-slot audit reports one material", failures);
    for (std::size_t slotIndex = 0; slotIndex < full_engine::kGltfMaterialSlotAuditSlotCount; ++slotIndex)
    {
        expect(audit.slots[slotIndex].rawTextureKeyCount == 1, "all-slot audit sees raw texture key", failures);
        expect(audit.slots[slotIndex].extractedRefCount == 1, "all-slot audit sees extracted ref", failures);
        expect(audit.slots[slotIndex].emittedTextureSourceCount == 1, "all-slot audit sees emitted texture source by slot", failures);
        expect(audit.slots[slotIndex].importedTexturePayloadCount == 1, "all-slot audit sees imported payload by slot", failures);
        expect(audit.slots[slotIndex].resolvedTextureRefCount == 1, "all-slot audit sees resolved texture ref by slot", failures);
    }
    expect(audit.slots[0].shaderActiveRefCount == 1, "base-color audit is shader-active", failures);
    expect(audit.slots[1].shaderActiveRefCount == 1, "normal audit is shader-active", failures);
    expect(audit.slots[2].shaderActiveRefCount == 0, "metallic-roughness audit is not shader-active yet", failures);
    expect(std::string(full_engine::gltfMaterialSlotAuditSlotName(0)) == "BaseColor", "audit slot name helper covers base color", failures);
    expect(std::string(full_engine::gltfMaterialSlotAuditSlotName(99)) == "Unknown", "audit slot name helper covers unknown", failures);
}

void testMissingTextureSlotAudit(std::vector<std::string>& failures)
{
    const full_engine::GltfMaterialSlotAudit audit =
        full_engine::auditGltfMaterialSlots(
            fixturePath("material_missing_image.gltf"),
            options());

    expect(audit.importStatus == full_engine::GltfMaterialAssetImportStatus::Success, "missing-image audit preserves extraction status", failures);
    expect(!audit.records.empty(), "missing-image audit reports raw material record", failures);
    expect(audit.slots[0].rawTextureKeyCount == 1, "missing-image audit sees authored base-color key", failures);
    expect(audit.slots[0].extractedRefCount == 0, "missing-image audit reports no extracted base-color ref", failures);
    expect(audit.slots[0].importedTexturePayloadCount == 0, "missing-image audit reports no imported base-color payload", failures);
    expect(audit.slots[0].resolvedTextureRefCount == 0, "missing-image audit reports no resolved base-color ref", failures);
}

void testPreserveMaterialIndexIds(std::vector<std::string>& failures)
{
    full_engine::GltfMaterialAssetImportOptions preserve = options();
    preserve.materialIdMode = full_engine::GltfMaterialAssetIdMode::PreserveGltfMaterialIndex;
    const full_engine::GltfMaterialAssetImportResult result =
        full_engine::importGltfMaterialAssetSources(
            fixturePath("material_base_color.gltf"),
            preserve);

    expect(result.status == full_engine::GltfMaterialAssetImportStatus::Success, "preserve-index extraction succeeds", failures);
    expect(result.records.size() == 1, "preserve-index extraction reports one record", failures);
    expect(result.records[0].materialIndex == 0, "preserve-index test fixture uses material zero", failures);
    expect(result.records[0].materialId == asset(1000), "preserve-index mode keeps material zero id", failures);
    expect(!result.materialPayloads.empty() && result.materialPayloads[0].material.id == asset(1000), "preserve-index payload keeps material zero id", failures);
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
    testAllNamedMaterialTextureSlots(failures);
    testAllNamedMaterialTextureSlotAudit(failures);
    testMissingTextureSlotAudit(failures);
    testPreserveMaterialIndexIds(failures);
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
