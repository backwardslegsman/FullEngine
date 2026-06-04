#include "engine/assets/AssimpLoadedAssetImporter.hpp"
#include "engine/renderer_integration/LoadedAssetUploadPlan.hpp"

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

full_engine::AssetSourceDescriptor skeletonDescriptor(const std::uint32_t jointCount = 2)
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.skeleton.jointCount = jointCount;
    return descriptor;
}

full_engine::AssetSourceDescriptor skinnedMeshDescriptor(
    const std::uint32_t vertexCount = 3,
    const std::uint32_t indexCount = 3,
    const std::uint64_t skeletonAssetId = 7,
    const float maxX = 1.0f)
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.skinnedMesh.vertexCount = vertexCount;
    descriptor.skinnedMesh.indexCount = indexCount;
    descriptor.skinnedMesh.skeletonAssetId = asset(skeletonAssetId);
    descriptor.skinnedMesh.localBounds.min[0] = 0.0f;
    descriptor.skinnedMesh.localBounds.min[1] = 0.0f;
    descriptor.skinnedMesh.localBounds.min[2] = 0.0f;
    descriptor.skinnedMesh.localBounds.max[0] = maxX;
    descriptor.skinnedMesh.localBounds.max[1] = 1.0f;
    descriptor.skinnedMesh.localBounds.max[2] = 0.0f;
    return descriptor;
}

full_engine::AssetSourceDescriptor animationClipDescriptor(
    const std::uint32_t trackCount = 2,
    const float durationSeconds = 1.0f,
    const float ticksPerSecond = 1000.0f,
    const std::uint64_t skeletonAssetId = 7,
    const float metadataTolerance = 0.01f)
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.animationClip.skeletonAssetId = asset(skeletonAssetId);
    descriptor.animationClip.trackCount = trackCount;
    descriptor.animationClip.durationSeconds = durationSeconds;
    descriptor.animationClip.ticksPerSecond = ticksPerSecond;
    descriptor.animationClip.metadataTolerance = metadataTolerance;
    return descriptor;
}

full_engine::AssetSourceRecord skeletonSource(const std::string& uri)
{
    full_engine::AssetSourceRecord record;
    record.id = asset(7);
    record.kind = full_engine::AssetKind::Skeleton;
    record.uri = uri;
    record.descriptor = skeletonDescriptor();
    return record;
}

full_engine::AssetSourceRecord skinnedMeshSource(const std::string& uri)
{
    full_engine::AssetSourceRecord record;
    record.id = asset(8);
    record.kind = full_engine::AssetKind::SkinnedMesh;
    record.uri = uri;
    record.descriptor = skinnedMeshDescriptor();
    return record;
}

full_engine::AssetSourceRecord animationClipSource(const std::string& uri)
{
    full_engine::AssetSourceRecord record;
    record.id = asset(9);
    record.kind = full_engine::AssetKind::AnimationClip;
    record.uri = uri;
    record.descriptor = animationClipDescriptor();
    return record;
}

full_engine::AssimpLoadedAssetImportOptions allowMissingUv0()
{
    full_engine::AssimpLoadedAssetImportOptions options;
    options.defaultMissingUv0ToZero = true;
    return options;
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
            meshSource(fixturePath("static_triangle.gltf")),
            allowMissingUv0());
    expect(result.status == full_engine::AssimpLoadedAssetImportStatus::Success, "valid gltf mesh imports", failures);
    expect(result.payload.kind == full_engine::AssetKind::Mesh, "valid gltf imports mesh payload", failures);
    expect(result.payload.mesh.id == asset(42), "imported mesh preserves asset id", failures);
    expect(result.payload.mesh.vertices.size() == 3, "imported mesh has three vertices", failures);
    expect(result.payload.mesh.indices.size() == 3, "imported mesh has three indices", failures);
    expect(result.payload.mesh.indices[0] == 0 && result.payload.mesh.indices[1] == 1 && result.payload.mesh.indices[2] == 2, "imported mesh preserves indices", failures);
    expect(result.payload.mesh.localBounds.max[0] == 1.0f && result.payload.mesh.localBounds.max[1] == 1.0f, "imported mesh computes bounds", failures);
    expect(result.payload.mesh.vertices[0].normal[2] == 1.0f, "imported mesh copies normals", failures);
    expect(result.payload.mesh.vertices[0].uv0[0] == 0.0f && result.payload.mesh.vertices[0].uv0[1] == 0.0f, "missing UV0 can default to zero", failures);
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
        full_engine::importLoadedAssetPayloadWithAssimp(source, allowMissingUv0());
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
    options.defaultMissingUv0ToZero = true;
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
            meshSource(fixturePath("vertex_colors.gltf")),
            allowMissingUv0());
    expect(result.status == full_engine::AssimpLoadedAssetImportStatus::Success, "vertex-color gltf imports", failures);
    expect(result.payload.mesh.vertices[0].colorLinear[0] == 1.0f && result.payload.mesh.vertices[0].colorLinear[1] == 0.0f, "first vertex color copied", failures);
    expect(result.payload.mesh.vertices[1].colorLinear[1] == 1.0f && result.payload.mesh.vertices[1].colorLinear[0] == 0.0f, "second vertex color copied", failures);
    expect(result.payload.mesh.vertices[2].colorLinear[2] == 1.0f && result.payload.mesh.vertices[2].colorLinear[3] == 0.5f, "third vertex color copied", failures);
}

void testSkeletalImport(std::vector<std::string>& failures)
{
    const std::string path = fixturePath("skinned_triangle.gltf");

    const full_engine::AssimpLoadedAssetImportResult skeleton =
        full_engine::importLoadedAssetPayloadWithAssimp(skeletonSource(path));
    expect(skeleton.status == full_engine::AssimpLoadedAssetImportStatus::Success, "valid gltf skeleton imports", failures);
    expect(skeleton.payload.kind == full_engine::AssetKind::Skeleton, "skeleton import sets payload kind", failures);
    expect(skeleton.payload.skeleton.id == asset(7), "skeleton import preserves asset id", failures);
    expect(skeleton.payload.skeleton.joints.size() == 2, "skeleton import copies joint count", failures);
    expect(skeleton.payload.skeleton.joints[0].name == "Root", "skeleton import copies root name", failures);
    expect(skeleton.payload.skeleton.joints[0].parentIndex == -1, "skeleton import preserves root parent", failures);
    expect(skeleton.payload.skeleton.joints[1].name == "Child", "skeleton import copies child name", failures);
    expect(skeleton.payload.skeleton.joints[1].parentIndex == 0, "skeleton import preserves child parent", failures);
    expect(skeleton.payload.skeleton.joints[0].inverseBindPose[0] == 1.0f, "skeleton import copies inverse bind matrix", failures);
    expect(
        full_engine::validateLoadedAssetPayload(skeleton.payload) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "imported skeleton payload validates",
        failures);

    const full_engine::AssimpLoadedAssetImportResult skinned =
        full_engine::importLoadedAssetPayloadWithAssimp(skinnedMeshSource(path));
    expect(skinned.status == full_engine::AssimpLoadedAssetImportStatus::Success, "valid gltf skinned mesh imports", failures);
    expect(skinned.payload.kind == full_engine::AssetKind::SkinnedMesh, "skinned import sets payload kind", failures);
    expect(skinned.payload.skinnedMesh.id == asset(8), "skinned import preserves mesh asset id", failures);
    expect(skinned.payload.skinnedMesh.skeletonAssetId == asset(7), "skinned import preserves skeleton asset id", failures);
    expect(skinned.payload.skinnedMesh.vertices.size() == 3, "skinned import copies vertices", failures);
    expect(skinned.payload.skinnedMesh.indices.size() == 3, "skinned import copies indices", failures);
    expect(skinned.payload.skinnedMesh.vertices[1].jointIndices[0] == 1, "skinned import maps child joint index", failures);
    expect(skinned.payload.skinnedMesh.vertices[2].jointWeights[0] == 0.5f, "skinned import copies first blended weight", failures);
    expect(skinned.payload.skinnedMesh.vertices[2].jointWeights[1] == 0.5f, "skinned import copies second blended weight", failures);
    expect(skinned.payload.skinnedMesh.vertices[1].uv0[0] == 1.0f, "skinned import copies uv0", failures);
    expect(skinned.payload.skinnedMesh.localBounds.max[0] == 1.0f, "skinned import computes bounds", failures);
    expect(
        full_engine::validateLoadedAssetPayload(skinned.payload) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "imported skinned payload validates",
        failures);

    const full_engine::LoadedAssetPayload payloads[] = {skeleton.payload, skinned.payload};
    const full_engine::LoadedAssetUploadPlan plan =
        full_engine::buildLoadedAssetUploadPlan(payloads, 2);
    expect(plan.summary.plannedCount == 2, "imported skeletal payloads plan upload work", failures);
    expect(plan.records[0].kind == full_engine::AssetKind::Skeleton, "upload plan keeps skeleton record", failures);
    expect(plan.records[1].kind == full_engine::AssetKind::SkinnedMesh, "upload plan keeps skinned mesh record", failures);
}

void testSkeletalUvPolicy(std::vector<std::string>& failures)
{
    const full_engine::AssimpLoadedAssetImportResult strict =
        full_engine::importLoadedAssetPayloadWithAssimp(
            skinnedMeshSource(fixturePath("skinned_triangle_missing_uv.gltf")));
    expect(strict.status == full_engine::AssimpLoadedAssetImportStatus::UnsupportedScene, "missing skinned uv fails by default", failures);

    const full_engine::AssimpLoadedAssetImportResult fallback =
        full_engine::importLoadedAssetPayloadWithAssimp(
            skinnedMeshSource(fixturePath("skinned_triangle_missing_uv.gltf")),
            allowMissingUv0());
    expect(fallback.status == full_engine::AssimpLoadedAssetImportStatus::Success, "missing skinned uv can default to zero", failures);
    expect(fallback.payload.skinnedMesh.vertices[1].uv0[0] == 0.0f, "defaulted skinned uv0 is zero", failures);
}

void testAnimationImport(std::vector<std::string>& failures)
{
    const full_engine::AssimpLoadedAssetImportResult result =
        full_engine::importLoadedAssetPayloadWithAssimp(
            animationClipSource(fixturePath("skinned_triangle_animation.gltf")));

    expect(result.status == full_engine::AssimpLoadedAssetImportStatus::Success, "valid gltf animation clip imports", failures);
    expect(result.payload.kind == full_engine::AssetKind::AnimationClip, "animation import sets payload kind", failures);
    expect(result.payload.animationClip.id == asset(9), "animation import preserves clip asset id", failures);
    expect(result.payload.animationClip.skeletonAssetId == asset(7), "animation import preserves skeleton reference", failures);
    expect(result.payload.animationClip.tracks.size() == 2, "animation import maps two joint tracks", failures);
    expect(result.payload.animationClip.tracks[0].jointIndex == 0, "animation import sorts root track by joint index", failures);
    expect(result.payload.animationClip.tracks[1].jointIndex == 1, "animation import sorts child track by joint index", failures);
    expect(result.payload.animationClip.tracks[0].translations.size() == 2, "animation import copies translation keys", failures);
    expect(result.payload.animationClip.tracks[0].rotations.size() == 2, "animation import copies rotation keys", failures);
    expect(result.payload.animationClip.tracks[0].scales.size() == 2, "animation import copies scale keys", failures);
    expect(result.payload.animationClip.tracks[0].translations[1].value[1] == 1.0f, "animation import copies translated value", failures);
    expect(result.payload.animationClip.tracks[0].rotations[1].value[2] > 0.7f, "animation import stores xyzw quaternion", failures);
    expect(
        full_engine::validateLoadedAssetPayload(result.payload) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "imported animation payload validates",
        failures);
}

void testAnimationSingleTrackImport(std::vector<std::string>& failures)
{
    full_engine::AssetSourceRecord source =
        animationClipSource(fixturePath("skinned_triangle_animation_missing_rotation.gltf"));
    source.descriptor = animationClipDescriptor(1, 1.0f, 1000.0f);
    const full_engine::AssimpLoadedAssetImportResult fallback =
        full_engine::importLoadedAssetPayloadWithAssimp(source);
    expect(fallback.status == full_engine::AssimpLoadedAssetImportStatus::Success, "single-track gltf animation imports", failures);
    expect(fallback.payload.animationClip.tracks.size() == 1, "single-track animation maps one joint track", failures);
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
        full_engine::importLoadedAssetPayloadWithAssimp(meshSource(fixturePath("malformed.gltf")), allowMissingUv0());
    expect(malformed.status == full_engine::AssimpLoadedAssetImportStatus::ParseError, "malformed gltf reports parse error", failures);

    full_engine::AssetSourceRecord mismatch = meshSource(fixturePath("static_triangle.gltf"));
    mismatch.descriptor.mesh.vertexCount = 4;
    const full_engine::AssimpLoadedAssetImportResult descriptorMismatch =
        full_engine::importLoadedAssetPayloadWithAssimp(mismatch, allowMissingUv0());
    expect(descriptorMismatch.status == full_engine::AssimpLoadedAssetImportStatus::DescriptorMismatch, "descriptor vertex-count mismatch is reported", failures);

    full_engine::AssetSourceRecord indexMismatch = meshSource(fixturePath("static_triangle.gltf"));
    indexMismatch.descriptor.mesh.indexCount = 6;
    const full_engine::AssimpLoadedAssetImportResult descriptorIndexMismatch =
        full_engine::importLoadedAssetPayloadWithAssimp(indexMismatch, allowMissingUv0());
    expect(descriptorIndexMismatch.status == full_engine::AssimpLoadedAssetImportStatus::DescriptorMismatch, "descriptor index-count mismatch is reported", failures);

    full_engine::AssetSourceRecord boundsMismatch = meshSource(fixturePath("static_triangle.gltf"));
    boundsMismatch.descriptor.mesh.localBounds.max[0] = 2.0f;
    const full_engine::AssimpLoadedAssetImportResult descriptorBoundsMismatch =
        full_engine::importLoadedAssetPayloadWithAssimp(boundsMismatch, allowMissingUv0());
    expect(descriptorBoundsMismatch.status == full_engine::AssimpLoadedAssetImportStatus::DescriptorMismatch, "descriptor bounds mismatch is reported", failures);

    full_engine::AssetSourceRecord skeletonMismatch = skeletonSource(fixturePath("skinned_triangle.gltf"));
    skeletonMismatch.descriptor = skeletonDescriptor(3);
    const full_engine::AssimpLoadedAssetImportResult skeletonDescriptorMismatch =
        full_engine::importLoadedAssetPayloadWithAssimp(skeletonMismatch);
    expect(skeletonDescriptorMismatch.status == full_engine::AssimpLoadedAssetImportStatus::DescriptorMismatch, "skeleton joint-count mismatch is reported", failures);

    full_engine::AssetSourceRecord skinnedVertexMismatch = skinnedMeshSource(fixturePath("skinned_triangle.gltf"));
    skinnedVertexMismatch.descriptor = skinnedMeshDescriptor(4, 3, 7);
    const full_engine::AssimpLoadedAssetImportResult skinnedVertexDescriptorMismatch =
        full_engine::importLoadedAssetPayloadWithAssimp(skinnedVertexMismatch);
    expect(skinnedVertexDescriptorMismatch.status == full_engine::AssimpLoadedAssetImportStatus::DescriptorMismatch, "skinned vertex-count mismatch is reported", failures);

    full_engine::AssetSourceRecord skinnedBoundsMismatch = skinnedMeshSource(fixturePath("skinned_triangle.gltf"));
    skinnedBoundsMismatch.descriptor = skinnedMeshDescriptor(3, 3, 7, 2.0f);
    const full_engine::AssimpLoadedAssetImportResult skinnedBoundsDescriptorMismatch =
        full_engine::importLoadedAssetPayloadWithAssimp(skinnedBoundsMismatch);
    expect(skinnedBoundsDescriptorMismatch.status == full_engine::AssimpLoadedAssetImportStatus::DescriptorMismatch, "skinned bounds mismatch is reported", failures);

    full_engine::AssetSourceRecord animationTrackMismatch = animationClipSource(fixturePath("skinned_triangle_animation.gltf"));
    animationTrackMismatch.descriptor = animationClipDescriptor(1);
    const full_engine::AssimpLoadedAssetImportResult animationTrackDescriptorMismatch =
        full_engine::importLoadedAssetPayloadWithAssimp(animationTrackMismatch);
    expect(animationTrackDescriptorMismatch.status == full_engine::AssimpLoadedAssetImportStatus::DescriptorMismatch, "animation track-count mismatch is reported", failures);

    full_engine::AssetSourceRecord animationDurationMismatch = animationClipSource(fixturePath("skinned_triangle_animation.gltf"));
    animationDurationMismatch.descriptor = animationClipDescriptor(2, 2.0f, 1000.0f, 7, 0.001f);
    const full_engine::AssimpLoadedAssetImportResult animationDurationDescriptorMismatch =
        full_engine::importLoadedAssetPayloadWithAssimp(animationDurationMismatch);
    expect(animationDurationDescriptorMismatch.status == full_engine::AssimpLoadedAssetImportStatus::DescriptorMismatch, "animation duration mismatch is reported", failures);

}

void testUnsupportedScenes(std::vector<std::string>& failures)
{
    const full_engine::AssimpLoadedAssetImportResult empty =
        full_engine::importLoadedAssetPayloadWithAssimp(
            meshSource(fixturePath("empty_scene.gltf")));
    expect(empty.status == full_engine::AssimpLoadedAssetImportStatus::UnsupportedScene, "empty scene is unsupported", failures);

    const full_engine::AssimpLoadedAssetImportResult line =
        full_engine::importLoadedAssetPayloadWithAssimp(
            meshSource(fixturePath("line_primitive.gltf")),
            allowMissingUv0());
    expect(line.status == full_engine::AssimpLoadedAssetImportStatus::UnsupportedScene, "non-triangle primitive is unsupported", failures);

    full_engine::AssetSourceRecord tooMany = meshSource(makeTooManyVerticesGltf());
    tooMany.descriptor = meshDescriptor(65538, 65538, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
    const full_engine::AssimpLoadedAssetImportResult tooManyVertices =
        full_engine::importLoadedAssetPayloadWithAssimp(tooMany);
    expect(tooManyVertices.status == full_engine::AssimpLoadedAssetImportStatus::UnsupportedScene, "too many aggregate vertices are unsupported", failures);

    const full_engine::AssimpLoadedAssetImportResult missingWeights =
        full_engine::importLoadedAssetPayloadWithAssimp(
            skinnedMeshSource(fixturePath("skinned_triangle_missing_weights.gltf")));
    expect(missingWeights.status == full_engine::AssimpLoadedAssetImportStatus::UnsupportedScene, "skinned mesh without weights is unsupported", failures);
}

void testWolfSkeletalAnimationSmoke(std::vector<std::string>& failures)
{
    const std::string path = std::string(FULL_RENDERER_TEST_GLTF_FIXTURE_DIR) +
        "/../33-gltf-wolf/gltf/Wolf-Blender-2.82a.gltf";

    full_engine::AssetSourceRecord skeleton = skeletonSource(path);
    skeleton.descriptor = skeletonDescriptor(49);
    const full_engine::AssimpLoadedAssetImportResult skeletonResult =
        full_engine::importLoadedAssetPayloadWithAssimp(skeleton);
    expect(skeletonResult.status == full_engine::AssimpLoadedAssetImportStatus::Success, "wolf gltf imports skeleton structurally", failures);

    full_engine::AssetSourceRecord clip = animationClipSource(path);
    clip.descriptor = animationClipDescriptor(49, 0.6666667f, 1000.0f, 7, 0.01f);
    const full_engine::AssimpLoadedAssetImportResult clipResult =
        full_engine::importLoadedAssetPayloadWithAssimp(clip);
    expect(clipResult.status == full_engine::AssimpLoadedAssetImportStatus::Success, "wolf gltf imports first animation structurally", failures);
    expect(clipResult.payload.animationClip.tracks.size() == 49, "wolf animation maps one track per joint", failures);
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
    testSkeletalImport(failures);
    testSkeletalUvPolicy(failures);
    testAnimationImport(failures);
    testAnimationSingleTrackImport(failures);
    testFailures(failures);
    testUnsupportedScenes(failures);
    testWolfSkeletalAnimationSmoke(failures);
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
