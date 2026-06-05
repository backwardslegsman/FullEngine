#include "full_renderer/Animation.hpp"
#include "full_renderer/Renderer.hpp"
#include "full_renderer/Terrain.hpp"
#include "renderer/animation/AnimationSystem.hpp"
#include "renderer/resources/AssetContracts.hpp"
#include "renderer/scene/MaterialPolicy.hpp"
#include "renderer/terrain/TerrainSystem.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

namespace
{
void expect(const bool condition, const char* message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

void identity(float out[16]) noexcept
{
    for (int index = 0; index < 16; ++index)
    {
        out[index] = 0.0f;
    }
    out[0] = 1.0f;
    out[5] = 1.0f;
    out[10] = 1.0f;
    out[15] = 1.0f;
}

full_renderer::MeshDesc makeMesh(
    full_renderer::MeshVertex vertices[3],
    std::uint16_t indices[3]) noexcept
{
    vertices[0].position[0] = 0.0f;
    vertices[0].position[1] = 0.0f;
    vertices[0].position[2] = 0.0f;
    vertices[1].position[0] = 1.0f;
    vertices[1].position[1] = 0.0f;
    vertices[1].position[2] = 0.0f;
    vertices[2].position[0] = 0.0f;
    vertices[2].position[1] = 1.0f;
    vertices[2].position[2] = 0.0f;

    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;

    full_renderer::MeshDesc desc;
    desc.vertices = vertices;
    desc.vertexCount = 3;
    desc.indices = indices;
    desc.indexCount = 3;
    return desc;
}

full_renderer::TextureDesc makeTextureDesc() noexcept
{
    static const std::uint8_t pixels[] = {
        255, 255, 255, 255,
        128, 128, 128, 255,
        64, 64, 64, 255,
        0, 0, 0, 255,
    };

    full_renderer::TextureDesc desc;
    desc.width = 2;
    desc.height = 2;
    desc.format = full_renderer::TextureFormat::Rgba8;
    desc.data = pixels;
    desc.dataSizeBytes = sizeof(pixels);
    return desc;
}

full_renderer::SkeletonDesc makeSkeleton(full_renderer::SkeletonJointDesc joints[2]) noexcept
{
    identity(joints[0].inverseBindPose);
    joints[0].parentIndex = -1;
    identity(joints[1].inverseBindPose);
    joints[1].parentIndex = 0;

    full_renderer::SkeletonDesc desc;
    desc.joints = joints;
    desc.jointCount = 2;
    return desc;
}

full_renderer::SkinnedMeshDesc makeSkinnedMesh(
    const full_renderer::SkeletonHandle skeleton,
    full_renderer::SkinnedMeshVertex vertices[3],
    std::uint16_t indices[3]) noexcept
{
    vertices[0].position[0] = 0.0f;
    vertices[0].position[1] = 0.0f;
    vertices[0].position[2] = 0.0f;
    vertices[1].position[0] = 1.0f;
    vertices[1].position[1] = 0.0f;
    vertices[1].position[2] = 0.0f;
    vertices[2].position[0] = 0.0f;
    vertices[2].position[1] = 1.0f;
    vertices[2].position[2] = 0.0f;

    for (std::uint32_t index = 0; index < 3U; ++index)
    {
        vertices[index].jointIndices[0] = 0.0f;
        vertices[index].jointIndices[1] = 1.0f;
        vertices[index].jointIndices[2] = 0.0f;
        vertices[index].jointIndices[3] = 0.0f;
        vertices[index].jointWeights[0] = 0.5f;
        vertices[index].jointWeights[1] = 0.5f;
        vertices[index].jointWeights[2] = 0.0f;
        vertices[index].jointWeights[3] = 0.0f;
    }

    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;

    full_renderer::SkinnedMeshDesc desc;
    desc.skeleton = skeleton;
    desc.vertices = vertices;
    desc.vertexCount = 3;
    desc.indices = indices;
    desc.indexCount = 3;
    return desc;
}

void textureContractValidation(int& failures)
{
    full_renderer::TextureDesc desc = makeTextureDesc();
    expect(
        full_renderer::resources::validateTextureAssetContract(desc) == full_renderer::RendererResult::Success,
        "default color texture contract is accepted",
        failures);

    desc.semantic = full_renderer::TextureSemantic::NormalMap;
    desc.colorSpace = full_renderer::TextureColorSpace::EncodedNormal;
    expect(
        full_renderer::resources::validateTextureAssetContract(desc) == full_renderer::RendererResult::Success,
        "normal map encoded-normal contract is accepted",
        failures);

    desc.colorSpace = full_renderer::TextureColorSpace::Srgb;
    expect(
        full_renderer::resources::validateTextureAssetContract(desc) == full_renderer::RendererResult::InvalidDescriptor,
        "normal map with sRGB color space is rejected",
        failures);

    desc.semantic = full_renderer::TextureSemantic::TerrainSplat;
    desc.colorSpace = full_renderer::TextureColorSpace::Linear;
    expect(
        full_renderer::resources::validateTextureAssetContract(desc) == full_renderer::RendererResult::Success,
        "terrain splat linear contract is accepted",
        failures);

    desc.mipCount = 2;
    expect(
        full_renderer::resources::validateTextureAssetContract(desc) == full_renderer::RendererResult::InvalidDescriptor,
        "multi-mip runtime upload is rejected until packed assets exist",
        failures);
    desc.mipCount = 1;

    desc.compressed = true;
    expect(
        full_renderer::resources::validateTextureAssetContract(desc) == full_renderer::RendererResult::InvalidDescriptor,
        "compressed runtime upload is rejected",
        failures);
    desc.compressed = false;

    desc.semantic = static_cast<full_renderer::TextureSemantic>(255);
    expect(
        !full_renderer::resources::isValidTextureSemantic(desc.semantic),
        "unknown texture semantic is invalid",
        failures);
}

void meshContractValidation(int& failures)
{
    full_renderer::MeshVertex vertices[3] = {};
    std::uint16_t indices[3] = {};
    full_renderer::MeshDesc desc = makeMesh(vertices, indices);

    expect(
        full_renderer::resources::validateMeshAssetContract(desc) == full_renderer::RendererResult::Success,
        "valid mesh asset contract is accepted",
        failures);

    full_renderer::Aabb bounds;
    expect(full_renderer::resources::computeMeshLocalBounds(desc, bounds), "mesh bounds compute succeeds", failures);
    expect(bounds.min[0] == 0.0f && bounds.max[0] == 1.0f, "mesh bounds capture x extent", failures);
    expect(bounds.min[1] == 0.0f && bounds.max[1] == 1.0f, "mesh bounds capture y extent", failures);

    vertices[0].position[0] = NAN;
    expect(
        full_renderer::resources::validateMeshAssetContract(desc) == full_renderer::RendererResult::InvalidDescriptor,
        "non-finite mesh position is rejected",
        failures);
    vertices[0].position[0] = 0.0f;

    vertices[0].normal[1] = 0.0f;
    expect(
        full_renderer::resources::validateMeshAssetContract(desc) == full_renderer::RendererResult::InvalidDescriptor,
        "zero mesh normal is rejected",
        failures);
    vertices[0].normal[1] = 1.0f;

    vertices[0].tangent[0] = 0.0f;
    expect(
        full_renderer::resources::validateMeshAssetContract(desc) == full_renderer::RendererResult::InvalidDescriptor,
        "zero mesh tangent is rejected",
        failures);
    vertices[0].tangent[0] = 1.0f;

    vertices[0].tangent[3] = 0.0f;
    expect(
        full_renderer::resources::validateMeshAssetContract(desc) == full_renderer::RendererResult::InvalidDescriptor,
        "invalid mesh tangent handedness is rejected",
        failures);
    vertices[0].tangent[3] = 1.0f;

    vertices[2].position[0] = 2.0f;
    vertices[2].position[1] = 0.0f;
    expect(full_renderer::resources::meshHasDegenerateTriangles(desc), "degenerate triangle is detected", failures);
    expect(
        full_renderer::resources::validateMeshAssetContract(desc) == full_renderer::RendererResult::InvalidDescriptor,
        "degenerate triangle is rejected",
        failures);
}

void materialContractValidation(int& failures)
{
    full_renderer::MaterialDesc desc;
    expect(full_renderer::scene::isValidMaterialPolicyDesc(desc), "default material policy is valid", failures);

    desc.alphaMode = full_renderer::MaterialAlphaMode::AlphaTest;
    desc.alphaCutoff = 0.5f;
    expect(
        full_renderer::scene::renderBucketForMaterial(desc) == full_renderer::scene::MaterialRenderBucket::AlphaTest,
        "alpha-test material maps to alpha-test bucket",
        failures);

    desc.kind = full_renderer::MaterialKind::TerrainSplat;
    expect(
        !full_renderer::scene::isValidMaterialPolicyDesc(desc),
        "terrain splat material rejects alpha-test/alpha-blend policy",
        failures);

    desc.kind = full_renderer::MaterialKind::Basic;
    desc.alphaCutoff = NAN;
    expect(!full_renderer::scene::isValidMaterialPolicyDesc(desc), "non-finite alpha cutoff is rejected", failures);
}

void skeletonAndSkinnedContractValidation(int& failures)
{
    full_renderer::animation::AnimationSystem system;
    full_renderer::SkeletonJointDesc joints[2] = {};
    const full_renderer::SkeletonHandle skeleton = system.createSkeleton(makeSkeleton(joints));
    expect(full_renderer::isValid(skeleton), "valid skeleton contract creates metadata", failures);

    full_renderer::SkinnedMeshVertex vertices[3] = {};
    std::uint16_t indices[3] = {};
    full_renderer::SkinnedMeshDesc desc = makeSkinnedMesh(skeleton, vertices, indices);
    expect(system.validateSkinnedMeshDesc(desc), "valid skinned mesh contract is accepted", failures);

    full_renderer::SkinnedMeshSectionDesc section;
    section.firstIndex = 0;
    section.indexCount = 3;
    desc.sections = &section;
    desc.sectionCount = 1;
    expect(system.validateSkinnedMeshDesc(desc), "valid skinned mesh section contract is accepted", failures);

    section.indexCount = 2;
    expect(!system.validateSkinnedMeshDesc(desc), "non-triangle skinned mesh section is rejected", failures);
    section.indexCount = 3;
    desc.sections = nullptr;
    expect(!system.validateSkinnedMeshDesc(desc), "section count with null section pointer is rejected", failures);
    desc.sections = &section;
    desc.sectionCount = 0;

    vertices[0].colorLinear[0] = 2.0f;
    expect(!system.validateSkinnedMeshDesc(desc), "skinned vertex color outside [0,1] is rejected", failures);
    vertices[0].colorLinear[0] = 1.0f;

    vertices[0].normal[1] = 0.0f;
    expect(!system.validateSkinnedMeshDesc(desc), "skinned zero normal is rejected", failures);
    vertices[0].normal[1] = 1.0f;

    vertices[0].tangent[1] = std::numeric_limits<float>::infinity();
    expect(!system.validateSkinnedMeshDesc(desc), "non-finite skinned tangent is rejected", failures);
    vertices[0].tangent[1] = 0.0f;

    vertices[0].tangent[3] = 0.0f;
    expect(!system.validateSkinnedMeshDesc(desc), "invalid skinned tangent handedness is rejected", failures);
    vertices[0].tangent[3] = 1.0f;

    vertices[0].uv0[0] = std::numeric_limits<float>::infinity();
    expect(!system.validateSkinnedMeshDesc(desc), "non-finite skinned UV0 is rejected", failures);
    vertices[0].uv0[0] = 0.0f;

    vertices[0].jointIndices[0] = 0.25f;
    expect(!system.validateSkinnedMeshDesc(desc), "non-integer joint index is rejected", failures);
}

void terrainContractValidation(int& failures)
{
    full_renderer::terrain::TerrainSystem system;

    full_renderer::TerrainLodDesc lod;
    lod.mesh = full_renderer::MeshHandle{1};
    lod.material = full_renderer::MaterialHandle{2};
    lod.maxDistanceMeters = 100.0f;

    full_renderer::TerrainChunkDesc desc;
    desc.bounds.min[0] = -1.0f;
    desc.bounds.min[1] = 0.0f;
    desc.bounds.min[2] = -1.0f;
    desc.bounds.max[0] = 1.0f;
    desc.bounds.max[1] = 1.0f;
    desc.bounds.max[2] = 1.0f;
    desc.lods = &lod;
    desc.lodCount = 1;

    const full_renderer::TerrainChunkHandle chunk = system.createChunk(desc);
    expect(full_renderer::isValid(chunk), "valid terrain chunk descriptor is accepted", failures);

    desc.bounds.max[0] = -2.0f;
    expect(!full_renderer::isValid(system.createChunk(desc)), "invalid terrain bounds are rejected", failures);
    desc.bounds.max[0] = 1.0f;

    lod.maxDistanceMeters = NAN;
    expect(!full_renderer::isValid(system.createChunk(desc)), "non-finite terrain LOD distance is rejected", failures);
}
} // namespace

int main()
{
    int failures = 0;
    textureContractValidation(failures);
    meshContractValidation(failures);
    materialContractValidation(failures);
    skeletonAndSkinnedContractValidation(failures);
    terrainContractValidation(failures);
    return failures == 0 ? 0 : 1;
}
