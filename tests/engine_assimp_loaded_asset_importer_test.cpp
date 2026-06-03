#include "engine/assets/AssimpLoadedAssetImporter.hpp"

#include <cstdlib>
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

full_engine::AssetSourceDescriptor meshDescriptor()
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.mesh.vertexCount = 3;
    descriptor.mesh.indexCount = 3;
    descriptor.mesh.localBounds.min[0] = 0.0f;
    descriptor.mesh.localBounds.min[1] = 0.0f;
    descriptor.mesh.localBounds.min[2] = 0.0f;
    descriptor.mesh.localBounds.max[0] = 1.0f;
    descriptor.mesh.localBounds.max[1] = 1.0f;
    descriptor.mesh.localBounds.max[2] = 0.0f;
    return descriptor;
}

full_engine::AssetSourceRecord meshSource(const std::string& uri)
{
    full_engine::AssetSourceRecord record;
    record.id = asset(42);
    record.kind = full_engine::AssetKind::Mesh;
    record.uri = uri;
    record.descriptor = meshDescriptor();
    return record;
}

full_engine::AssetSourceRecord textureSource()
{
    full_engine::AssetSourceRecord record;
    record.id = asset(99);
    record.kind = full_engine::AssetKind::Texture;
    record.uri = fixturePath("static_triangle.gltf");
    record.descriptor.texture.width = 1;
    record.descriptor.texture.height = 1;
    record.descriptor.texture.mipCount = 1;
    record.descriptor.texture.format = full_engine::AssetSourceTextureFormat::Rgba8;
    record.descriptor.texture.semantic = full_engine::AssetSourceTextureSemantic::Color;
    record.descriptor.texture.colorSpace = full_engine::AssetSourceTextureColorSpace::Srgb;
    return record;
}

void testValidStaticMeshImport(std::vector<std::string>& failures)
{
    const full_engine::AssimpLoadedAssetImportResult result =
        full_engine::importLoadedAssetPayloadWithAssimp(
            meshSource(fixturePath("static_triangle.gltf")));
    expect(result.status == full_engine::AssimpLoadedAssetImportStatus::Success, "valid gltf mesh imports", failures);
    expect(result.payload.kind == full_engine::AssetKind::Mesh, "valid gltf imports mesh payload", failures);
    expect(result.payload.mesh.id == asset(42), "imported mesh preserves asset id", failures);
    expect(result.payload.mesh.vertices.size() == 3, "imported mesh has three vertices", failures);
    expect(result.payload.mesh.indices.size() == 3, "imported mesh has three indices", failures);
    expect(result.payload.mesh.indices[0] == 0 && result.payload.mesh.indices[1] == 1 && result.payload.mesh.indices[2] == 2, "imported mesh preserves indices", failures);
    expect(result.payload.mesh.localBounds.max[0] == 1.0f && result.payload.mesh.localBounds.max[1] == 1.0f, "imported mesh computes bounds", failures);
    expect(result.payload.mesh.vertices[0].normal[2] == 1.0f, "imported mesh copies normals", failures);
    expect(result.payload.mesh.vertices[0].colorLinear[0] == 1.0f && result.payload.mesh.vertices[0].colorLinear[3] == 1.0f, "imported mesh defaults vertex colors", failures);
    expect(
        full_engine::validateLoadedAssetPayload(result.payload) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "imported gltf payload validates",
        failures);
}

void testFailures(std::vector<std::string>& failures)
{
    full_engine::AssetSourceRecord invalid = meshSource(fixturePath("static_triangle.gltf"));
    invalid.id = {};
    const full_engine::AssimpLoadedAssetImportResult invalidSource =
        full_engine::importLoadedAssetPayloadWithAssimp(invalid);
    expect(invalidSource.status == full_engine::AssimpLoadedAssetImportStatus::SourceValidationFailed, "invalid source is rejected", failures);

    const full_engine::AssimpLoadedAssetImportResult unsupported =
        full_engine::importLoadedAssetPayloadWithAssimp(textureSource());
    expect(unsupported.status == full_engine::AssimpLoadedAssetImportStatus::UnsupportedKind, "unsupported kind is rejected", failures);

    const full_engine::AssimpLoadedAssetImportResult missing =
        full_engine::importLoadedAssetPayloadWithAssimp(meshSource(fixturePath("missing.gltf")));
    expect(missing.status == full_engine::AssimpLoadedAssetImportStatus::IoError, "missing gltf reports io error", failures);

    const full_engine::AssimpLoadedAssetImportResult malformed =
        full_engine::importLoadedAssetPayloadWithAssimp(meshSource(fixturePath("malformed.gltf")));
    expect(malformed.status == full_engine::AssimpLoadedAssetImportStatus::ParseError, "malformed gltf reports parse error", failures);

    full_engine::AssetSourceRecord mismatch = meshSource(fixturePath("static_triangle.gltf"));
    mismatch.descriptor.mesh.vertexCount = 4;
    const full_engine::AssimpLoadedAssetImportResult descriptorMismatch =
        full_engine::importLoadedAssetPayloadWithAssimp(mismatch);
    expect(descriptorMismatch.status == full_engine::AssimpLoadedAssetImportStatus::DescriptorMismatch, "descriptor mismatch is reported", failures);
}

void testStatusNames(std::vector<std::string>& failures)
{
    expect(full_engine::assimpLoadedAssetImportStatusName(full_engine::AssimpLoadedAssetImportStatus::Success)[0] != '\0', "success status has name", failures);
    expect(full_engine::assimpLoadedAssetImportStatusName(full_engine::AssimpLoadedAssetImportStatus::InvalidArgument)[0] != '\0', "invalid argument status has name", failures);
    expect(full_engine::assimpLoadedAssetImportStatusName(full_engine::AssimpLoadedAssetImportStatus::SourceValidationFailed)[0] != '\0', "source validation status has name", failures);
    expect(full_engine::assimpLoadedAssetImportStatusName(full_engine::AssimpLoadedAssetImportStatus::IoError)[0] != '\0', "io status has name", failures);
    expect(full_engine::assimpLoadedAssetImportStatusName(full_engine::AssimpLoadedAssetImportStatus::ParseError)[0] != '\0', "parse status has name", failures);
    expect(full_engine::assimpLoadedAssetImportStatusName(full_engine::AssimpLoadedAssetImportStatus::UnsupportedScene)[0] != '\0', "unsupported scene status has name", failures);
    expect(full_engine::assimpLoadedAssetImportStatusName(full_engine::AssimpLoadedAssetImportStatus::DescriptorMismatch)[0] != '\0', "descriptor mismatch status has name", failures);
    expect(full_engine::assimpLoadedAssetImportStatusName(full_engine::AssimpLoadedAssetImportStatus::PayloadValidationFailed)[0] != '\0', "payload validation status has name", failures);
    expect(full_engine::assimpLoadedAssetImportStatusName(full_engine::AssimpLoadedAssetImportStatus::UnsupportedKind)[0] != '\0', "unsupported kind status has name", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testValidStaticMeshImport(failures);
    testFailures(failures);
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
