#include "engine/assets/LoadedAssetPayload.hpp"

#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* const message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

full_engine::LoadedMeshVertex vertex(const float x, const float y, const float z) noexcept
{
    full_engine::LoadedMeshVertex result;
    result.position[0] = x;
    result.position[1] = y;
    result.position[2] = z;
    result.normal[0] = 0.0f;
    result.normal[1] = 1.0f;
    result.normal[2] = 0.0f;
    result.colorLinear[0] = 1.0f;
    result.colorLinear[1] = 0.75f;
    result.colorLinear[2] = 0.5f;
    result.colorLinear[3] = 1.0f;
    return result;
}

full_engine::LoadedMeshAsset meshAsset()
{
    full_engine::LoadedMeshAsset mesh;
    mesh.id = asset(10);
    mesh.vertices = {
        vertex(0.0f, 0.0f, 0.0f),
        vertex(1.0f, 0.0f, 0.0f),
        vertex(0.0f, 1.0f, 0.0f)};
    mesh.indices = {0, 1, 2};
    mesh.localBounds.min[0] = 0.0f;
    mesh.localBounds.min[1] = 0.0f;
    mesh.localBounds.min[2] = 0.0f;
    mesh.localBounds.max[0] = 1.0f;
    mesh.localBounds.max[1] = 1.0f;
    mesh.localBounds.max[2] = 0.0f;
    return mesh;
}

full_engine::LoadedTextureAsset textureAsset()
{
    full_engine::LoadedTextureAsset texture;
    texture.id = asset(20);
    texture.width = 4;
    texture.height = 2;
    texture.mipCount = 1;
    texture.format = full_engine::AssetSourceTextureFormat::Rgba8;
    texture.semantic = full_engine::AssetSourceTextureSemantic::Color;
    texture.colorSpace = full_engine::AssetSourceTextureColorSpace::Srgb;
    texture.bytes.assign(texture.width * texture.height * 4, 255);
    return texture;
}

full_engine::LoadedMaterialAsset materialAsset()
{
    full_engine::LoadedMaterialAsset material;
    material.id = asset(30);
    material.model = full_engine::AssetSourceMaterialModel::TerrainSplat;
    material.alphaMode = full_engine::AssetSourceMaterialAlphaMode::Opaque;
    material.textureRefs[0] = asset(20);
    material.textureRefs[1] = asset(21);
    material.textureRefCount = 2;
    return material;
}

void testValidPayloads(std::vector<std::string>& failures)
{
    expect(
        full_engine::validateLoadedMeshAsset(meshAsset()) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "valid mesh payload passes",
        failures);
    expect(
        full_engine::validateLoadedTextureAsset(textureAsset()) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "valid texture payload passes",
        failures);
    expect(
        full_engine::validateLoadedMaterialAsset(materialAsset()) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "valid material payload passes",
        failures);
}

void testDefaultIdsFail(std::vector<std::string>& failures)
{
    full_engine::LoadedMeshAsset mesh = meshAsset();
    mesh.id = {};
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAssetId,
        "mesh payload rejects default id",
        failures);

    full_engine::LoadedTextureAsset texture = textureAsset();
    texture.id = {};
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAssetId,
        "texture payload rejects default id",
        failures);

    full_engine::LoadedMaterialAsset material = materialAsset();
    material.id = {};
    expect(
        full_engine::validateLoadedMaterialAsset(material) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAssetId,
        "material payload rejects default id",
        failures);
}

void testInvalidMeshPayloads(std::vector<std::string>& failures)
{
    full_engine::LoadedMeshAsset mesh = meshAsset();
    mesh.vertices.clear();
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshVertices,
        "mesh payload rejects empty vertices",
        failures);

    mesh = meshAsset();
    mesh.indices.clear();
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshIndices,
        "mesh payload rejects empty indices",
        failures);

    mesh = meshAsset();
    mesh.indices.push_back(0);
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshIndices,
        "mesh payload rejects non-triangle indices",
        failures);

    mesh = meshAsset();
    mesh.indices[2] = 9;
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshIndices,
        "mesh payload rejects out-of-range index",
        failures);

    mesh = meshAsset();
    mesh.vertices[0].position[1] = std::nanf("");
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshVertexData,
        "mesh payload rejects non-finite position",
        failures);

    mesh = meshAsset();
    mesh.vertices[0].normal[1] = 0.0f;
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshVertexData,
        "mesh payload rejects zero normal",
        failures);

    mesh = meshAsset();
    mesh.vertices[0].colorLinear[0] = 2.0f;
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshVertexData,
        "mesh payload rejects out-of-range color",
        failures);

    mesh = meshAsset();
    mesh.localBounds.min[0] = 2.0f;
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshBounds,
        "mesh payload rejects inverted bounds",
        failures);

    mesh = meshAsset();
    mesh.localBounds.max[2] = std::nanf("");
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshBounds,
        "mesh payload rejects non-finite bounds",
        failures);
}

void testInvalidTexturePayloads(std::vector<std::string>& failures)
{
    full_engine::LoadedTextureAsset texture = textureAsset();
    texture.width = 0;
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidTextureDimensions,
        "texture payload rejects zero width",
        failures);

    texture = textureAsset();
    texture.height = 0;
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidTextureDimensions,
        "texture payload rejects zero height",
        failures);

    texture = textureAsset();
    texture.mipCount = 2;
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidTextureMipCount,
        "texture payload rejects non-v1 mip count",
        failures);

    texture = textureAsset();
    texture.format = full_engine::AssetSourceTextureFormat::Unknown;
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidTextureFormat,
        "texture payload rejects unknown format",
        failures);

    texture = textureAsset();
    texture.semantic = full_engine::AssetSourceTextureSemantic::Unknown;
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidTextureSemantic,
        "texture payload rejects unknown semantic",
        failures);

    texture = textureAsset();
    texture.colorSpace = full_engine::AssetSourceTextureColorSpace::Unknown;
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidTextureColorSpace,
        "texture payload rejects unknown color space",
        failures);

    texture = textureAsset();
    texture.bytes.resize(texture.bytes.size() - 1);
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidTextureByteCount,
        "texture payload rejects undersized byte buffer",
        failures);
}

void testInvalidMaterialPayloads(std::vector<std::string>& failures)
{
    full_engine::LoadedMaterialAsset material = materialAsset();
    material.model = full_engine::AssetSourceMaterialModel::Unknown;
    expect(
        full_engine::validateLoadedMaterialAsset(material) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialModel,
        "material payload rejects unknown model",
        failures);

    material = materialAsset();
    material.alphaMode = full_engine::AssetSourceMaterialAlphaMode::Unknown;
    expect(
        full_engine::validateLoadedMaterialAsset(material) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialAlphaMode,
        "material payload rejects unknown alpha mode",
        failures);

    material = materialAsset();
    material.textureRefCount = full_engine::kMaxAssetSourceMaterialTextureRefs + 1;
    expect(
        full_engine::validateLoadedMaterialAsset(material) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialTextureCount,
        "material payload rejects too many texture refs",
        failures);

    material = materialAsset();
    material.textureRefs[1] = {};
    expect(
        full_engine::validateLoadedMaterialAsset(material) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialTextureRef,
        "material payload rejects active default texture ref",
        failures);
}

void testPayloadDispatchAndInactiveSlots(std::vector<std::string>& failures)
{
    full_engine::LoadedAssetPayload meshPayload;
    meshPayload.kind = full_engine::AssetKind::Mesh;
    meshPayload.mesh = meshAsset();
    meshPayload.texture = {};
    meshPayload.material = {};
    expect(
        full_engine::validateLoadedAssetPayload(meshPayload) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "payload dispatch validates active mesh slot only",
        failures);

    full_engine::LoadedAssetPayload texturePayload;
    texturePayload.kind = full_engine::AssetKind::Texture;
    texturePayload.mesh = {};
    texturePayload.texture = textureAsset();
    texturePayload.material = {};
    expect(
        full_engine::validateLoadedAssetPayload(texturePayload) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "payload dispatch validates active texture slot only",
        failures);

    full_engine::LoadedAssetPayload materialPayload;
    materialPayload.kind = full_engine::AssetKind::Material;
    materialPayload.mesh = {};
    materialPayload.texture = {};
    materialPayload.material = materialAsset();
    expect(
        full_engine::validateLoadedAssetPayload(materialPayload) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "payload dispatch validates active material slot only",
        failures);

    full_engine::LoadedAssetPayload invalidPayload;
    invalidPayload.kind = full_engine::AssetKind::TerrainChunk;
    expect(
        full_engine::validateLoadedAssetPayload(invalidPayload) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidKind,
        "payload dispatch rejects unsupported kind",
        failures);
}

void testResultNames(std::vector<std::string>& failures)
{
    const full_engine::LoadedAssetPayloadValidationResult results[] = {
        full_engine::LoadedAssetPayloadValidationResult::Success,
        full_engine::LoadedAssetPayloadValidationResult::InvalidKind,
        full_engine::LoadedAssetPayloadValidationResult::InvalidAssetId,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMeshVertices,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMeshIndices,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMeshVertexData,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMeshBounds,
        full_engine::LoadedAssetPayloadValidationResult::InvalidTextureDimensions,
        full_engine::LoadedAssetPayloadValidationResult::InvalidTextureMipCount,
        full_engine::LoadedAssetPayloadValidationResult::InvalidTextureFormat,
        full_engine::LoadedAssetPayloadValidationResult::InvalidTextureSemantic,
        full_engine::LoadedAssetPayloadValidationResult::InvalidTextureColorSpace,
        full_engine::LoadedAssetPayloadValidationResult::InvalidTextureByteCount,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialModel,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialAlphaMode,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialTextureCount,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialTextureRef};

    for (const full_engine::LoadedAssetPayloadValidationResult result : results)
    {
        expect(
            std::string(full_engine::loadedAssetPayloadValidationResultName(result)) != "Unknown",
            "loaded payload validation result has stable name",
            failures);
    }
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testValidPayloads(failures);
    testDefaultIdsFail(failures);
    testInvalidMeshPayloads(failures);
    testInvalidTexturePayloads(failures);
    testInvalidMaterialPayloads(failures);
    testPayloadDispatchAndInactiveSlots(failures);
    testResultNames(failures);

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
