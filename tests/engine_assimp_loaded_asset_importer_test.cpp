#include "engine/assets/AssimpLoadedAssetImporter.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdint>
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

full_engine::AssetSourceDescriptor meshDescriptor(
    const std::uint32_t vertexCount,
    const std::uint32_t indexCount,
    const float minX,
    const float minY,
    const float minZ,
    const float maxX,
    const float maxY,
    const float maxZ)
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.mesh.vertexCount = vertexCount;
    descriptor.mesh.indexCount = indexCount;
    descriptor.mesh.localBounds.min[0] = minX;
    descriptor.mesh.localBounds.min[1] = minY;
    descriptor.mesh.localBounds.min[2] = minZ;
    descriptor.mesh.localBounds.max[0] = maxX;
    descriptor.mesh.localBounds.max[1] = maxY;
    descriptor.mesh.localBounds.max[2] = maxZ;
    return descriptor;
}

full_engine::AssetSourceDescriptor meshDescriptor()
{
    return meshDescriptor(3, 3, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
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

void writeFloat(std::ofstream& output, const float value)
{
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void writeU16(std::ofstream& output, const std::uint16_t value)
{
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

std::string makeTooManyVerticesGltf()
{
    constexpr std::uint32_t kMeshCount = 21846;
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "full_renderer_assimp_importer_tests";
    std::filesystem::create_directories(directory);
    const std::filesystem::path binPath = directory / "too_many_meshes.bin";
    const std::filesystem::path gltfPath = directory / "too_many_vertices.gltf";

    {
        std::ofstream binary(binPath, std::ios::binary | std::ios::trunc);
        for (const float value : {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f})
        {
            writeFloat(binary, value);
        }
        for (std::uint32_t index = 0; index < 3; ++index)
        {
            writeFloat(binary, 0.0f);
            writeFloat(binary, 0.0f);
            writeFloat(binary, 1.0f);
        }
        writeU16(binary, 0);
        writeU16(binary, 1);
        writeU16(binary, 2);
    }

    constexpr std::uint32_t positionBytes = 3U * 3U * sizeof(float);
    constexpr std::uint32_t normalBytes = 3U * 3U * sizeof(float);
    const std::uint32_t indexOffset = positionBytes + normalBytes;
    const std::uint32_t byteLength = indexOffset + 3U * sizeof(std::uint16_t);

    {
        std::ofstream gltf(gltfPath, std::ios::trunc);
        gltf <<
            "{\n"
            "  \"asset\": { \"version\": \"2.0\" },\n"
            "  \"scene\": 0,\n"
            "  \"scenes\": [{ \"nodes\": [";
        for (std::uint32_t index = 0; index < kMeshCount; ++index)
        {
            if (index > 0)
            {
                gltf << ", ";
            }
            gltf << index;
        }
        gltf <<
            "] }],\n"
            "  \"nodes\": [";
        for (std::uint32_t index = 0; index < kMeshCount; ++index)
        {
            if (index > 0)
            {
                gltf << ", ";
            }
            gltf << "{ \"mesh\": " << index << " }";
        }
        gltf <<
            "],\n"
            "  \"meshes\": [";
        for (std::uint32_t index = 0; index < kMeshCount; ++index)
        {
            if (index > 0)
            {
                gltf << ", ";
            }
            gltf << "{ \"primitives\": [{ \"attributes\": { \"POSITION\": 0, \"NORMAL\": 1 }, \"indices\": 2, \"mode\": 4 }] }";
        }
        gltf <<
            "],\n"
            "  \"buffers\": [{ \"uri\": \"" << binPath.filename().string() << "\", \"byteLength\": " << byteLength << " }],\n"
            "  \"bufferViews\": [\n"
            "    { \"buffer\": 0, \"byteOffset\": 0, \"byteLength\": " << positionBytes << ", \"target\": 34962 },\n"
            "    { \"buffer\": 0, \"byteOffset\": " << positionBytes << ", \"byteLength\": " << normalBytes << ", \"target\": 34962 },\n"
            "    { \"buffer\": 0, \"byteOffset\": " << indexOffset << ", \"byteLength\": 6, \"target\": 34963 }\n"
            "  ],\n"
            "  \"accessors\": [\n"
            "    { \"bufferView\": 0, \"byteOffset\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\", \"min\": [0.0, 0.0, 0.0], \"max\": [1.0, 1.0, 0.0] },\n"
            "    { \"bufferView\": 1, \"byteOffset\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\" },\n"
            "    { \"bufferView\": 2, \"byteOffset\": 0, \"componentType\": 5123, \"count\": 3, \"type\": \"SCALAR\" }\n"
            "  ]\n"
            "}\n";
    }

    return gltfPath.string();
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

void testMultiMeshImport(std::vector<std::string>& failures)
{
    full_engine::AssetSourceRecord source = meshSource(fixturePath("multi_mesh_static.gltf"));
    source.descriptor = meshDescriptor(6, 6, 0.0f, 0.0f, 0.0f, 3.0f, 1.0f, 0.0f);
    const full_engine::AssimpLoadedAssetImportResult result =
        full_engine::importLoadedAssetPayloadWithAssimp(source);
    expect(result.status == full_engine::AssimpLoadedAssetImportStatus::Success, "multi-mesh gltf imports", failures);
    expect(result.payload.mesh.vertices.size() == 6, "multi-mesh import combines vertices", failures);
    expect(result.payload.mesh.indices.size() == 6, "multi-mesh import combines indices", failures);
    expect(result.payload.mesh.indices[0] == 0 && result.payload.mesh.indices[1] == 1 && result.payload.mesh.indices[2] == 2, "multi-mesh preserves first mesh indices", failures);
    expect(result.payload.mesh.indices[3] == 3 && result.payload.mesh.indices[4] == 4 && result.payload.mesh.indices[5] == 5, "multi-mesh offsets second mesh indices", failures);
    expect(result.payload.mesh.localBounds.max[0] == 3.0f && result.payload.mesh.localBounds.max[1] == 1.0f, "multi-mesh computes aggregate bounds", failures);
    expect(result.payload.mesh.vertices[3].position[0] == 2.0f, "multi-mesh preserves source mesh order", failures);
}

void testGeneratedNormals(std::vector<std::string>& failures)
{
    const full_engine::AssimpLoadedAssetImportResult withoutNormals =
        full_engine::importLoadedAssetPayloadWithAssimp(
            meshSource(fixturePath("no_normals.gltf")));
    expect(withoutNormals.status == full_engine::AssimpLoadedAssetImportStatus::UnsupportedScene, "missing normals fail by default", failures);

    full_engine::AssimpLoadedAssetImportOptions options;
    options.generateMissingNormals = true;
    const full_engine::AssimpLoadedAssetImportResult generated =
        full_engine::importLoadedAssetPayloadWithAssimp(
            meshSource(fixturePath("no_normals.gltf")),
            options);
    expect(generated.status == full_engine::AssimpLoadedAssetImportStatus::Success, "missing normals can be generated", failures);
    expect(generated.payload.mesh.vertices.size() == 3, "generated-normal import keeps vertices", failures);
    expect(generated.payload.mesh.vertices[0].normal[2] > 0.0f, "generated normals point along triangle normal", failures);
}

void testVertexColors(std::vector<std::string>& failures)
{
    const full_engine::AssimpLoadedAssetImportResult result =
        full_engine::importLoadedAssetPayloadWithAssimp(
            meshSource(fixturePath("vertex_colors.gltf")));
    expect(result.status == full_engine::AssimpLoadedAssetImportStatus::Success, "vertex-color gltf imports", failures);
    expect(result.payload.mesh.vertices[0].colorLinear[0] == 1.0f && result.payload.mesh.vertices[0].colorLinear[1] == 0.0f, "first vertex color copied", failures);
    expect(result.payload.mesh.vertices[1].colorLinear[1] == 1.0f && result.payload.mesh.vertices[1].colorLinear[0] == 0.0f, "second vertex color copied", failures);
    expect(result.payload.mesh.vertices[2].colorLinear[2] == 1.0f && result.payload.mesh.vertices[2].colorLinear[3] == 0.5f, "third vertex color copied", failures);
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
    expect(descriptorMismatch.status == full_engine::AssimpLoadedAssetImportStatus::DescriptorMismatch, "descriptor vertex-count mismatch is reported", failures);

    full_engine::AssetSourceRecord indexMismatch = meshSource(fixturePath("static_triangle.gltf"));
    indexMismatch.descriptor.mesh.indexCount = 6;
    const full_engine::AssimpLoadedAssetImportResult descriptorIndexMismatch =
        full_engine::importLoadedAssetPayloadWithAssimp(indexMismatch);
    expect(descriptorIndexMismatch.status == full_engine::AssimpLoadedAssetImportStatus::DescriptorMismatch, "descriptor index-count mismatch is reported", failures);

    full_engine::AssetSourceRecord boundsMismatch = meshSource(fixturePath("static_triangle.gltf"));
    boundsMismatch.descriptor.mesh.localBounds.max[0] = 2.0f;
    const full_engine::AssimpLoadedAssetImportResult descriptorBoundsMismatch =
        full_engine::importLoadedAssetPayloadWithAssimp(boundsMismatch);
    expect(descriptorBoundsMismatch.status == full_engine::AssimpLoadedAssetImportStatus::DescriptorMismatch, "descriptor bounds mismatch is reported", failures);
}

void testUnsupportedScenes(std::vector<std::string>& failures)
{
    const full_engine::AssimpLoadedAssetImportResult empty =
        full_engine::importLoadedAssetPayloadWithAssimp(
            meshSource(fixturePath("empty_scene.gltf")));
    expect(empty.status == full_engine::AssimpLoadedAssetImportStatus::UnsupportedScene, "empty scene is unsupported", failures);

    const full_engine::AssimpLoadedAssetImportResult line =
        full_engine::importLoadedAssetPayloadWithAssimp(
            meshSource(fixturePath("line_primitive.gltf")));
    expect(line.status == full_engine::AssimpLoadedAssetImportStatus::UnsupportedScene, "non-triangle primitive is unsupported", failures);

    full_engine::AssetSourceRecord tooMany = meshSource(makeTooManyVerticesGltf());
    tooMany.descriptor = meshDescriptor(65538, 65538, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
    const full_engine::AssimpLoadedAssetImportResult tooManyVertices =
        full_engine::importLoadedAssetPayloadWithAssimp(tooMany);
    expect(tooManyVertices.status == full_engine::AssimpLoadedAssetImportStatus::UnsupportedScene, "too many aggregate vertices are unsupported", failures);
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
    testMultiMeshImport(failures);
    testGeneratedNormals(failures);
    testVertexColors(failures);
    testFailures(failures);
    testUnsupportedScenes(failures);
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
