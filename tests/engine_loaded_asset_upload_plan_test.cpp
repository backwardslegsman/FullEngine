#include "engine/renderer_integration/LoadedAssetUploadPlan.hpp"

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

full_engine::LoadedTextureAsset textureAsset(
    const full_engine::AssetSourceTextureSemantic semantic =
        full_engine::AssetSourceTextureSemantic::Color,
    const full_engine::AssetSourceTextureColorSpace colorSpace =
        full_engine::AssetSourceTextureColorSpace::Srgb)
{
    full_engine::LoadedTextureAsset texture;
    texture.id = asset(20);
    texture.width = 4;
    texture.height = 2;
    texture.mipCount = 1;
    texture.format = full_engine::AssetSourceTextureFormat::Rgba8;
    texture.semantic = semantic;
    texture.colorSpace = colorSpace;
    texture.bytes.assign(texture.width * texture.height * 4, 255);
    return texture;
}

full_engine::LoadedMaterialAsset materialAsset()
{
    full_engine::LoadedMaterialAsset material;
    material.id = asset(30);
    material.model = full_engine::AssetSourceMaterialModel::TerrainSplat;
    material.alphaMode = full_engine::AssetSourceMaterialAlphaMode::AlphaTest;
    material.textureRefs[0] = {full_engine::AssetSourceMaterialTextureSlot::BaseColor, asset(20)};
    material.textureRefs[1] = {full_engine::AssetSourceMaterialTextureSlot::Normal, asset(21)};
    material.textureRefCount = 2;
    return material;
}

void setIdentity(float (&matrix)[16]) noexcept
{
    for (float& value : matrix)
    {
        value = 0.0f;
    }
    matrix[0] = 1.0f;
    matrix[5] = 1.0f;
    matrix[10] = 1.0f;
    matrix[15] = 1.0f;
}

full_engine::LoadedSkeletonJoint joint(const std::int32_t parentIndex)
{
    full_engine::LoadedSkeletonJoint result;
    result.parentIndex = parentIndex;
    setIdentity(result.inverseBindPose);
    setIdentity(result.referenceTransform);
    return result;
}

full_engine::LoadedSkeletonAsset skeletonAsset()
{
    full_engine::LoadedSkeletonAsset skeleton;
    skeleton.id = asset(40);
    skeleton.joints = {joint(-1), joint(0)};
    return skeleton;
}

full_engine::LoadedSkinnedMeshVertex skinnedVertex(const float x, const float y, const float z) noexcept
{
    full_engine::LoadedSkinnedMeshVertex result;
    result.position[0] = x;
    result.position[1] = y;
    result.position[2] = z;
    result.normal[0] = 0.0f;
    result.normal[1] = 1.0f;
    result.normal[2] = 0.0f;
    result.uv0[0] = x;
    result.uv0[1] = y;
    result.colorLinear[0] = 0.25f;
    result.colorLinear[1] = 0.5f;
    result.colorLinear[2] = 0.75f;
    result.colorLinear[3] = 1.0f;
    result.jointIndices[0] = 0;
    result.jointIndices[1] = 1;
    result.jointWeights[0] = 0.75f;
    result.jointWeights[1] = 0.25f;
    return result;
}

full_engine::LoadedSkinnedMeshAsset skinnedMeshAsset()
{
    full_engine::LoadedSkinnedMeshAsset mesh;
    mesh.id = asset(50);
    mesh.skeletonAssetId = asset(40);
    mesh.vertices = {
        skinnedVertex(0.0f, 0.0f, 0.0f),
        skinnedVertex(1.0f, 0.0f, 0.0f),
        skinnedVertex(0.0f, 1.0f, 0.0f)};
    mesh.indices = {0, 1, 2};
    mesh.localBounds.min[0] = 0.0f;
    mesh.localBounds.min[1] = 0.0f;
    mesh.localBounds.min[2] = 0.0f;
    mesh.localBounds.max[0] = 1.0f;
    mesh.localBounds.max[1] = 1.0f;
    mesh.localBounds.max[2] = 0.0f;
    return mesh;
}

full_engine::LoadedAssetPayload meshPayload()
{
    full_engine::LoadedAssetPayload payload;
    payload.kind = full_engine::AssetKind::Mesh;
    payload.mesh = meshAsset();
    return payload;
}

full_engine::LoadedAssetPayload skeletonPayload()
{
    full_engine::LoadedAssetPayload payload;
    payload.kind = full_engine::AssetKind::Skeleton;
    payload.skeleton = skeletonAsset();
    return payload;
}

full_engine::LoadedAssetPayload skinnedMeshPayload()
{
    full_engine::LoadedAssetPayload payload;
    payload.kind = full_engine::AssetKind::SkinnedMesh;
    payload.skinnedMesh = skinnedMeshAsset();
    return payload;
}

full_engine::LoadedAssetPayload texturePayload(
    const full_engine::AssetSourceTextureSemantic semantic =
        full_engine::AssetSourceTextureSemantic::Color,
    const full_engine::AssetSourceTextureColorSpace colorSpace =
        full_engine::AssetSourceTextureColorSpace::Srgb)
{
    full_engine::LoadedAssetPayload payload;
    payload.kind = full_engine::AssetKind::Texture;
    payload.texture = textureAsset(semantic, colorSpace);
    return payload;
}

full_engine::LoadedAssetPayload materialPayload()
{
    full_engine::LoadedAssetPayload payload;
    payload.kind = full_engine::AssetKind::Material;
    payload.material = materialAsset();
    return payload;
}

void testValidPayloadsPlanUploadWork(std::vector<std::string>& failures)
{
    const std::vector<full_engine::LoadedAssetPayload> payloads = {
        meshPayload(),
        texturePayload(),
        materialPayload(),
        skeletonPayload(),
        skinnedMeshPayload()};

    const full_engine::LoadedAssetUploadPlan plan =
        full_engine::buildLoadedAssetUploadPlan(payloads.data(), payloads.size());

    expect(plan.records.size() == 5, "upload plan returns one record per payload", failures);
    expect(plan.summary.plannedCount == 5, "upload plan counts planned payloads", failures);
    expect(plan.summary.invalidPayloadCount == 0, "upload plan has no invalid payloads", failures);

    const full_engine::LoadedAssetUploadRecord& mesh = plan.records[0];
    expect(mesh.status == full_engine::LoadedAssetUploadStatus::Planned, "mesh upload work is planned", failures);
    expect(mesh.kind == full_engine::AssetKind::Mesh, "mesh upload record keeps kind", failures);
    expect(mesh.id == asset(10), "mesh upload record keeps asset id", failures);
    expect(mesh.mesh.vertices.size() == 3, "mesh upload work copies vertex count", failures);
    expect(mesh.mesh.indices.size() == 3, "mesh upload work copies index count", failures);
    expect(mesh.mesh.desc.vertices == mesh.mesh.vertices.data(), "mesh descriptor points into owned vertices", failures);
    expect(mesh.mesh.desc.indices == mesh.mesh.indices.data(), "mesh descriptor points into owned indices", failures);
    expect(mesh.mesh.desc.vertexCount == 3, "mesh descriptor copies vertex count", failures);
    expect(mesh.mesh.desc.indexCount == 3, "mesh descriptor copies index count", failures);
    expect(mesh.mesh.vertices[1].position[0] == 1.0f, "mesh upload work copies vertex position", failures);
    expect(mesh.mesh.vertices[0].colorLinear[2] == 0.5f, "mesh upload work copies vertex color", failures);

    const full_engine::LoadedAssetUploadRecord& texture = plan.records[1];
    expect(texture.status == full_engine::LoadedAssetUploadStatus::Planned, "texture upload work is planned", failures);
    expect(texture.texture.desc.width == 4, "texture descriptor copies width", failures);
    expect(texture.texture.desc.height == 2, "texture descriptor copies height", failures);
    expect(texture.texture.desc.format == full_renderer::TextureFormat::Rgba8, "texture descriptor maps format", failures);
    expect(texture.texture.desc.semantic == full_renderer::TextureSemantic::Color, "texture descriptor maps semantic", failures);
    expect(texture.texture.desc.colorSpace == full_renderer::TextureColorSpace::Srgb, "texture descriptor maps color space", failures);
    expect(texture.texture.desc.mipCount == 1, "texture descriptor copies mip count", failures);
    expect(texture.texture.desc.compressed == false, "texture descriptor is uncompressed", failures);
    expect(texture.texture.desc.data == texture.texture.bytes.data(), "texture descriptor points into owned bytes", failures);
    expect(texture.texture.desc.dataSizeBytes == texture.texture.bytes.size(), "texture descriptor copies byte count", failures);

    const full_engine::LoadedAssetUploadRecord& material = plan.records[2];
    expect(material.status == full_engine::LoadedAssetUploadStatus::Planned, "material upload expectation is planned", failures);
    expect(material.material.kind == full_renderer::MaterialKind::TerrainSplat, "material upload maps kind", failures);
    expect(material.material.alphaMode == full_renderer::MaterialAlphaMode::AlphaTest, "material upload maps alpha mode", failures);
    expect(material.material.textureRefs.size() == 2, "material upload copies texture ref count", failures);
    expect(
        material.material.textureRefs[0].slot == full_engine::AssetSourceMaterialTextureSlot::BaseColor &&
            material.material.textureRefs[0].id == asset(20),
        "material upload preserves named texture refs",
        failures);

    const full_engine::LoadedAssetUploadRecord& skeleton = plan.records[3];
    expect(skeleton.status == full_engine::LoadedAssetUploadStatus::Planned, "skeleton upload work is planned", failures);
    expect(skeleton.skeleton.joints.size() == 2, "skeleton upload work copies joint count", failures);
    expect(skeleton.skeleton.desc.joints == skeleton.skeleton.joints.data(), "skeleton descriptor points into owned joints", failures);
    expect(skeleton.skeleton.desc.jointCount == 2, "skeleton descriptor copies joint count", failures);
    expect(skeleton.skeleton.joints[1].parentIndex == 0, "skeleton upload copies parent index", failures);
    expect(skeleton.skeleton.joints[0].inverseBindPose[0] == 1.0f, "skeleton upload copies inverse bind matrix", failures);

    const full_engine::LoadedAssetUploadRecord& skinned = plan.records[4];
    expect(skinned.status == full_engine::LoadedAssetUploadStatus::Planned, "skinned mesh upload work is planned", failures);
    expect(skinned.skinnedMesh.skeletonAssetId == asset(40), "skinned upload preserves skeleton asset id", failures);
    expect(skinned.skinnedMesh.vertices.size() == 3, "skinned upload work copies vertex count", failures);
    expect(skinned.skinnedMesh.indices.size() == 3, "skinned upload work copies index count", failures);
    expect(skinned.skinnedMesh.desc.vertices == skinned.skinnedMesh.vertices.data(), "skinned descriptor points into owned vertices", failures);
    expect(skinned.skinnedMesh.desc.indices == skinned.skinnedMesh.indices.data(), "skinned descriptor points into owned indices", failures);
    expect(skinned.skinnedMesh.vertices[0].jointIndices[1] == 1.0f, "skinned upload copies joint indices as renderer floats", failures);
    expect(skinned.skinnedMesh.vertices[0].jointWeights[1] == 0.25f, "skinned upload copies joint weights", failures);
}

void testInvalidPayloadsReportInvalid(std::vector<std::string>& failures)
{
    full_engine::LoadedAssetPayload mesh = meshPayload();
    mesh.mesh.id = {};

    full_engine::LoadedAssetPayload skeleton = skeletonPayload();
    skeleton.skeleton.joints.clear();

    full_engine::LoadedAssetPayload skinned = skinnedMeshPayload();
    skinned.skinnedMesh.skeletonAssetId = {};

    const full_engine::LoadedAssetPayload payloads[] = {mesh, skeleton, skinned};
    const full_engine::LoadedAssetUploadPlan plan =
        full_engine::buildLoadedAssetUploadPlan(payloads, 3);

    expect(plan.records.size() == 3, "invalid payloads still produce diagnostic records", failures);
    expect(plan.records[0].status == full_engine::LoadedAssetUploadStatus::InvalidPayload, "invalid payload reports invalid payload", failures);
    expect(plan.records[1].status == full_engine::LoadedAssetUploadStatus::InvalidPayload, "invalid skeleton reports invalid payload", failures);
    expect(plan.records[2].status == full_engine::LoadedAssetUploadStatus::InvalidPayload, "invalid skinned mesh reports invalid payload", failures);
    expect(plan.summary.invalidPayloadCount == 3, "invalid payload summary is counted", failures);
}

void testUnsupportedKindReportsUnsupported(std::vector<std::string>& failures)
{
    full_engine::LoadedAssetPayload payload;
    payload.kind = full_engine::AssetKind::TerrainChunk;

    const full_engine::LoadedAssetUploadPlan plan =
        full_engine::buildLoadedAssetUploadPlan(&payload, 1);

    expect(plan.records.size() == 1, "unsupported kind produces diagnostic record", failures);
    expect(plan.records[0].status == full_engine::LoadedAssetUploadStatus::UnsupportedKind, "unsupported kind reports unsupported", failures);
    expect(plan.summary.unsupportedKindCount == 1, "unsupported kind summary is counted", failures);
}

void testUnsupportedRendererContract(std::vector<std::string>& failures)
{
    full_engine::LoadedAssetPayload texture = texturePayload(
        full_engine::AssetSourceTextureSemantic::Color,
        full_engine::AssetSourceTextureColorSpace::Linear);

    expect(
        full_engine::validateLoadedAssetPayload(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "renderer-contract test payload is asset-valid",
        failures);

    const full_engine::LoadedAssetPayload payloads[] = {texture};
    const full_engine::LoadedAssetUploadPlan plan =
        full_engine::buildLoadedAssetUploadPlan(payloads, 1);

    expect(plan.records.size() == 1, "unsupported renderer contract payload produces record", failures);
    expect(
        plan.records[0].status == full_engine::LoadedAssetUploadStatus::UnsupportedRendererContract,
        "asset-valid but renderer-invalid texture reports unsupported contract",
        failures);
    expect(plan.summary.unsupportedRendererContractCount == 1, "unsupported renderer contract summary is counted", failures);
}

void testOrderAndInactiveSlots(std::vector<std::string>& failures)
{
    full_engine::LoadedAssetPayload material = materialPayload();
    material.mesh = {};
    material.texture = {};

    full_engine::LoadedAssetPayload mesh = meshPayload();
    mesh.texture = {};
    mesh.material = {};

    full_engine::LoadedAssetPayload texture = texturePayload(
        full_engine::AssetSourceTextureSemantic::TerrainSplat,
        full_engine::AssetSourceTextureColorSpace::Linear);
    texture.mesh = {};
    texture.material = {};

    full_engine::LoadedAssetPayload skeleton = skeletonPayload();
    skeleton.mesh = {};
    skeleton.texture = {};
    skeleton.material = {};
    skeleton.skinnedMesh = {};

    full_engine::LoadedAssetPayload skinned = skinnedMeshPayload();
    skinned.mesh = {};
    skinned.texture = {};
    skinned.material = {};
    skinned.skeleton = {};

    const std::vector<full_engine::LoadedAssetPayload> payloads = {material, mesh, texture, skeleton, skinned};
    const full_engine::LoadedAssetUploadPlan plan =
        full_engine::buildLoadedAssetUploadPlan(payloads.data(), payloads.size());

    expect(plan.summary.plannedCount == 5, "inactive slots do not affect upload planning", failures);
    expect(plan.records[0].kind == full_engine::AssetKind::Material, "upload plan preserves first payload order", failures);
    expect(plan.records[1].kind == full_engine::AssetKind::Mesh, "upload plan preserves second payload order", failures);
    expect(plan.records[2].kind == full_engine::AssetKind::Texture, "upload plan preserves third payload order", failures);
    expect(plan.records[3].kind == full_engine::AssetKind::Skeleton, "upload plan preserves fourth payload order", failures);
    expect(plan.records[4].kind == full_engine::AssetKind::SkinnedMesh, "upload plan preserves fifth payload order", failures);
    expect(plan.records[2].texture.desc.semantic == full_renderer::TextureSemantic::TerrainSplat, "terrain splat semantic maps in ordered payload", failures);
    expect(plan.records[2].texture.desc.colorSpace == full_renderer::TextureColorSpace::Linear, "linear color space maps in ordered payload", failures);
}

void testNullInputAndStatusNames(std::vector<std::string>& failures)
{
    const full_engine::LoadedAssetUploadPlan empty =
        full_engine::buildLoadedAssetUploadPlan(nullptr, 0);
    const full_engine::LoadedAssetUploadPlan invalid =
        full_engine::buildLoadedAssetUploadPlan(nullptr, 2);

    expect(empty.records.empty(), "null empty upload plan has no records", failures);
    expect(empty.summary.invalidPayloadCount == 0, "null empty upload plan has zero invalid count", failures);
    expect(invalid.records.empty(), "null non-empty upload plan has no records", failures);
    expect(invalid.summary.invalidPayloadCount == 2, "null non-empty upload plan counts invalid payloads", failures);

    const full_engine::LoadedAssetUploadStatus statuses[] = {
        full_engine::LoadedAssetUploadStatus::Planned,
        full_engine::LoadedAssetUploadStatus::InvalidPayload,
        full_engine::LoadedAssetUploadStatus::UnsupportedKind,
        full_engine::LoadedAssetUploadStatus::UnsupportedRendererContract};
    for (const full_engine::LoadedAssetUploadStatus status : statuses)
    {
        expect(
            std::string(full_engine::loadedAssetUploadStatusName(status)) != "Unknown",
            "loaded upload status has stable name",
            failures);
    }
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testValidPayloadsPlanUploadWork(failures);
    testInvalidPayloadsReportInvalid(failures);
    testUnsupportedKindReportsUnsupported(failures);
    testUnsupportedRendererContract(failures);
    testOrderAndInactiveSlots(failures);
    testNullInputAndStatusNames(failures);

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
