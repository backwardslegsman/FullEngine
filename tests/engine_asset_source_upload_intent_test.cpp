#include "engine/renderer_integration/AssetSourceUploadIntent.hpp"

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

full_engine::AssetSourceDescriptor meshDescriptor()
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.mesh.vertexCount = 4;
    descriptor.mesh.indexCount = 6;
    descriptor.mesh.localBounds.min[0] = -1.0f;
    descriptor.mesh.localBounds.min[1] = -2.0f;
    descriptor.mesh.localBounds.min[2] = -3.0f;
    descriptor.mesh.localBounds.max[0] = 1.0f;
    descriptor.mesh.localBounds.max[1] = 2.0f;
    descriptor.mesh.localBounds.max[2] = 3.0f;
    return descriptor;
}

full_engine::AssetSourceDescriptor textureDescriptor(
    const full_engine::AssetSourceTextureSemantic semantic =
        full_engine::AssetSourceTextureSemantic::TerrainSplat,
    const full_engine::AssetSourceTextureColorSpace colorSpace =
        full_engine::AssetSourceTextureColorSpace::Linear,
    const std::uint32_t mipCount = 1)
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.texture.width = 8;
    descriptor.texture.height = 4;
    descriptor.texture.mipCount = mipCount;
    descriptor.texture.format = full_engine::AssetSourceTextureFormat::Rgba8;
    descriptor.texture.semantic = semantic;
    descriptor.texture.colorSpace = colorSpace;
    return descriptor;
}

full_engine::AssetSourceDescriptor materialDescriptor()
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.material.model = full_engine::AssetSourceMaterialModel::TerrainSplat;
    descriptor.material.alphaMode = full_engine::AssetSourceMaterialAlphaMode::AlphaTest;
    descriptor.material.textureRefs[0] = {full_engine::AssetSourceMaterialTextureSlot::BaseColor, asset(30)};
    descriptor.material.textureRefs[1] = {full_engine::AssetSourceMaterialTextureSlot::Normal, asset(31)};
    descriptor.material.textureRefCount = 2;
    return descriptor;
}

full_engine::AssetSourceDescriptor skeletonDescriptor()
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.skeleton.jointCount = 3;
    return descriptor;
}

full_engine::AssetSourceDescriptor skinnedMeshDescriptor()
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.skinnedMesh.vertexCount = 6;
    descriptor.skinnedMesh.indexCount = 6;
    descriptor.skinnedMesh.skeletonAssetId = asset(40);
    descriptor.skinnedMesh.localBounds.min[0] = -1.0f;
    descriptor.skinnedMesh.localBounds.min[1] = 0.0f;
    descriptor.skinnedMesh.localBounds.min[2] = -1.0f;
    descriptor.skinnedMesh.localBounds.max[0] = 1.0f;
    descriptor.skinnedMesh.localBounds.max[1] = 2.0f;
    descriptor.skinnedMesh.localBounds.max[2] = 1.0f;
    return descriptor;
}

full_engine::AssetSourceRecord source(
    const std::uint64_t id,
    const full_engine::AssetKind kind,
    const char* const uri,
    const full_engine::AssetSourceDescriptor& descriptor)
{
    full_engine::AssetSourceRecord record;
    record.id = asset(id);
    record.kind = kind;
    record.uri = uri;
    record.descriptor = descriptor;
    return record;
}

full_engine::TerrainManifestAssetLoadRequest request(
    const std::uint64_t id,
    const full_engine::AssetKind kind) noexcept
{
    full_engine::TerrainManifestAssetLoadRequest result;
    result.id = asset(id);
    result.kind = kind;
    return result;
}

void testValidSourcesPlanUploadIntent(std::vector<std::string>& failures)
{
    const std::vector<full_engine::AssetSourceRecord> sources = {
        source(1, full_engine::AssetKind::Mesh, "mesh.bin", meshDescriptor()),
        source(
            2,
            full_engine::AssetKind::Texture,
            "splat.rgba8",
            textureDescriptor()),
        source(3, full_engine::AssetKind::Material, "terrain.material", materialDescriptor()),
    };

    const full_engine::AssetSourceUploadIntentPlan plan =
        full_engine::buildAssetSourceUploadIntentPlan(sources.data(), sources.size());

    expect(plan.records.size() == 3, "upload intent returns one record per source", failures);
    expect(plan.summary.plannedCount == 3, "upload intent counts planned sources", failures);
    expect(plan.records[0].status == full_engine::AssetSourceUploadIntentStatus::Planned, "mesh upload intent planned", failures);
    expect(plan.records[0].mesh.vertexCount == 4, "mesh upload intent copies vertex count", failures);
    expect(plan.records[0].mesh.indexCount == 6, "mesh upload intent copies index count", failures);
    expect(plan.records[0].mesh.localBounds.min[2] == -3.0f, "mesh upload intent copies local bounds", failures);

    expect(plan.records[1].texture.format == full_renderer::TextureFormat::Rgba8, "texture upload intent maps format", failures);
    expect(plan.records[1].texture.semantic == full_renderer::TextureSemantic::TerrainSplat, "texture upload intent maps semantic", failures);
    expect(plan.records[1].texture.colorSpace == full_renderer::TextureColorSpace::Linear, "texture upload intent maps color space", failures);
    expect(plan.records[1].texture.expectedMinimumByteCount == 128, "texture upload intent computes RGBA8 byte count", failures);

    expect(plan.records[2].material.kind == full_renderer::MaterialKind::TerrainSplat, "material upload intent maps kind", failures);
    expect(plan.records[2].material.alphaMode == full_renderer::MaterialAlphaMode::AlphaTest, "material upload intent maps alpha mode", failures);
    expect(plan.records[2].material.textureRefs.size() == 2, "material upload intent copies texture ref count", failures);
    expect(
        plan.records[2].material.textureRefs[0].slot == full_engine::AssetSourceMaterialTextureSlot::BaseColor &&
            plan.records[2].material.textureRefs[0].id == asset(30),
        "material upload intent keeps named texture refs",
        failures);
}

void testTextureEnumMappings(std::vector<std::string>& failures)
{
    const std::vector<full_engine::AssetSourceRecord> sources = {
        source(
            10,
            full_engine::AssetKind::Texture,
            "normal.rgba8",
            textureDescriptor(
                full_engine::AssetSourceTextureSemantic::NormalMap,
                full_engine::AssetSourceTextureColorSpace::EncodedNormal)),
        source(
            11,
            full_engine::AssetKind::Texture,
            "debug.rgba8",
            textureDescriptor(
                full_engine::AssetSourceTextureSemantic::Debug,
                full_engine::AssetSourceTextureColorSpace::Srgb)),
    };

    const full_engine::AssetSourceUploadIntentPlan plan =
        full_engine::buildAssetSourceUploadIntentPlan(sources.data(), sources.size());

    expect(plan.summary.plannedCount == 2, "texture mapping test plans both records", failures);
    expect(plan.records[0].texture.semantic == full_renderer::TextureSemantic::NormalMap, "normal semantic maps to renderer normal map", failures);
    expect(plan.records[0].texture.colorSpace == full_renderer::TextureColorSpace::EncodedNormal, "encoded normal color space maps", failures);
    expect(plan.records[1].texture.semantic == full_renderer::TextureSemantic::Debug, "debug semantic maps to renderer debug", failures);
    expect(plan.records[1].texture.colorSpace == full_renderer::TextureColorSpace::Srgb, "srgb color space maps", failures);
}

void testInvalidAndUnsupportedSources(std::vector<std::string>& failures)
{
    full_engine::AssetSourceRecord invalid = source(20, full_engine::AssetKind::Mesh, "bad.mesh", meshDescriptor());
    invalid.descriptor.mesh.vertexCount = 0;
    const full_engine::AssetSourceRecord multiMip =
        source(21, full_engine::AssetKind::Texture, "mips.rgba8", textureDescriptor(
            full_engine::AssetSourceTextureSemantic::Color,
            full_engine::AssetSourceTextureColorSpace::Srgb,
            2));
    const full_engine::AssetSourceRecord skeleton =
        source(22, full_engine::AssetKind::Skeleton, "character.skel", skeletonDescriptor());
    const full_engine::AssetSourceRecord skinnedMesh =
        source(23, full_engine::AssetKind::SkinnedMesh, "character.gltf", skinnedMeshDescriptor());
    const std::vector<full_engine::AssetSourceRecord> sources = {invalid, multiMip, skeleton, skinnedMesh};

    const full_engine::AssetSourceUploadIntentPlan plan =
        full_engine::buildAssetSourceUploadIntentPlan(sources.data(), sources.size());

    expect(plan.summary.invalidSourceCount == 1, "upload intent counts invalid source", failures);
    expect(plan.summary.unsupportedRendererContractCount == 3, "upload intent counts unsupported renderer contract", failures);
    expect(plan.records[0].status == full_engine::AssetSourceUploadIntentStatus::InvalidSource, "invalid descriptor reports invalid source", failures);
    expect(
        plan.records[1].status == full_engine::AssetSourceUploadIntentStatus::UnsupportedRendererContract,
        "multi-mip texture reports unsupported renderer contract",
        failures);
    expect(plan.records[1].texture.mipCount == 2, "unsupported texture still copies metadata", failures);
    expect(
        plan.records[2].status == full_engine::AssetSourceUploadIntentStatus::UnsupportedRendererContract,
        "valid skeleton source reports unsupported renderer contract",
        failures);
    expect(
        plan.records[3].status == full_engine::AssetSourceUploadIntentStatus::UnsupportedRendererContract,
        "valid skinned mesh source reports unsupported renderer contract",
        failures);
}

void testSourceRequestPlanOverload(std::vector<std::string>& failures)
{
    full_engine::TerrainManifestAssetSourceRequestPlan sourcePlan;

    full_engine::TerrainManifestAssetSourceRequestRecord mapped;
    mapped.request = request(1, full_engine::AssetKind::Mesh);
    mapped.source = source(1, full_engine::AssetKind::Mesh, "mesh.bin", meshDescriptor());
    mapped.status = full_engine::TerrainManifestAssetSourceRequestStatus::Mapped;
    sourcePlan.records.push_back(mapped);

    full_engine::TerrainManifestAssetSourceRequestRecord missing;
    missing.request = request(2, full_engine::AssetKind::Texture);
    missing.status = full_engine::TerrainManifestAssetSourceRequestStatus::MissingSource;
    sourcePlan.records.push_back(missing);

    full_engine::TerrainManifestAssetSourceRequestRecord invalid;
    invalid.request = request(0, full_engine::AssetKind::Mesh);
    invalid.status = full_engine::TerrainManifestAssetSourceRequestStatus::InvalidRequest;
    sourcePlan.records.push_back(invalid);

    const full_engine::AssetSourceUploadIntentPlan plan =
        full_engine::buildAssetSourceUploadIntentPlan(sourcePlan);

    expect(plan.records.size() == 3, "source request overload preserves record count", failures);
    expect(plan.records[0].status == full_engine::AssetSourceUploadIntentStatus::Planned, "mapped source request plans upload intent", failures);
    expect(plan.records[1].status == full_engine::AssetSourceUploadIntentStatus::SourceNotMapped, "missing source request reports not mapped", failures);
    expect(plan.records[2].status == full_engine::AssetSourceUploadIntentStatus::SourceNotMapped, "invalid source request reports not mapped", failures);
    expect(plan.summary.plannedCount == 1, "source request overload counts planned", failures);
    expect(plan.summary.sourceNotMappedCount == 2, "source request overload counts unmapped", failures);
    expect(plan.records[0].id == asset(1), "source request overload preserves mapped order", failures);
    expect(plan.records[1].id == asset(2), "source request overload preserves missing order", failures);
}

void testNullSourceArray(std::vector<std::string>& failures)
{
    const full_engine::AssetSourceUploadIntentPlan empty =
        full_engine::buildAssetSourceUploadIntentPlan(nullptr, 0);
    const full_engine::AssetSourceUploadIntentPlan invalid =
        full_engine::buildAssetSourceUploadIntentPlan(nullptr, 3);

    expect(empty.records.empty(), "null empty source upload plan has no records", failures);
    expect(empty.summary.invalidSourceCount == 0, "null empty source upload plan has zero invalid count", failures);
    expect(invalid.records.empty(), "null non-empty source upload plan has no records", failures);
    expect(invalid.summary.invalidSourceCount == 3, "null non-empty source upload plan counts invalid sources", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testValidSourcesPlanUploadIntent(failures);
    testTextureEnumMappings(failures);
    testInvalidAndUnsupportedSources(failures);
    testSourceRequestPlanOverload(failures);
    testNullSourceArray(failures);

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
