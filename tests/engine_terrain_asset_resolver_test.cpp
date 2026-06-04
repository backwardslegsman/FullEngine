#include "engine/renderer_integration/TerrainAssetResolver.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
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

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return full_engine::AssetId{value};
}

full_renderer::MeshHandle mesh(const std::uint32_t id) noexcept
{
    return full_renderer::MeshHandle{id};
}

full_renderer::MaterialHandle material(const std::uint32_t id) noexcept
{
    return full_renderer::MaterialHandle{id};
}

full_renderer::TextureHandle texture(const std::uint32_t id) noexcept
{
    return full_renderer::TextureHandle{id};
}

full_renderer::SkeletonHandle skeleton(const std::uint32_t id) noexcept
{
    return full_renderer::SkeletonHandle{id};
}

full_renderer::SkinnedMeshHandle skinnedMesh(const std::uint32_t id) noexcept
{
    return full_renderer::SkinnedMeshHandle{id};
}

full_engine::TerrainChunkAssetDesc makeAssetDesc(
    const full_engine::ChunkId& id,
    const std::uint32_t lodCount)
{
    full_engine::TerrainChunkAssetDesc desc;
    desc.id = id;
    desc.lodCount = lodCount;
    for (std::uint32_t index = 0; index < lodCount && index < full_engine::kMaxTerrainAssetLodLevels; ++index)
    {
        desc.lods[index].mesh = asset(100 + index);
        desc.lods[index].material = asset(200 + index);
        desc.lods[index].maxDistanceMeters = static_cast<float>((index + 1) * 75);
    }
    return desc;
}

full_engine::RendererAssetHandleCatalog makeHandleCatalog(const std::uint32_t lodCount)
{
    full_engine::RendererAssetHandleCatalog handles;
    for (std::uint32_t index = 0; index < lodCount && index < full_engine::kMaxTerrainAssetLodLevels; ++index)
    {
        handles.addMeshHandle(asset(100 + index), mesh(1000 + index));
        handles.addMaterialHandle(asset(200 + index), material(2000 + index));
    }
    handles.addTextureHandle(asset(300), texture(3000));
    return handles;
}

void expectDefaultResources(
    const full_engine::TerrainChunkResourceDesc& resources,
    const char* message,
    std::vector<std::string>& failures)
{
    expect(resources.id == full_engine::ChunkId{}, message, failures);
    expect(resources.lodCount == 0, "failed resolve leaves lod count default", failures);
    expect(resources.splatMap.id == 0, "failed resolve leaves splat default", failures);
}

void testHandleCatalogMutations(std::vector<std::string>& failures)
{
    full_engine::RendererAssetHandleCatalog handles;

    expect(handles.meshHandleCount() == 0, "handle catalog starts with no mesh handles", failures);
    expect(
        handles.addMeshHandle(asset(1), mesh(10)) == full_engine::RendererAssetHandleCatalogResult::Success,
        "mesh handle add succeeds",
        failures);
    expect(
        handles.addMeshHandle(asset(1), mesh(11)) == full_engine::RendererAssetHandleCatalogResult::AlreadyExists,
        "duplicate mesh handle add is rejected",
        failures);
    expect(
        handles.updateMeshHandle(asset(1), mesh(12)) == full_engine::RendererAssetHandleCatalogResult::Success,
        "mesh handle update succeeds",
        failures);
    const full_renderer::MeshHandle* foundMesh = handles.findMeshHandle(asset(1));
    expect(foundMesh != nullptr && foundMesh->id == 12, "updated mesh handle can be found", failures);
    expect(
        handles.removeMeshHandle(asset(1)) == full_engine::RendererAssetHandleCatalogResult::Success,
        "mesh handle remove succeeds",
        failures);
    expect(handles.findMeshHandle(asset(1)) == nullptr, "removed mesh handle is missing", failures);

    expect(
        handles.addMaterialHandle({}, material(20)) == full_engine::RendererAssetHandleCatalogResult::InvalidArgument,
        "invalid material asset id is rejected",
        failures);
    expect(
        handles.addMaterialHandle(asset(2), {}) == full_engine::RendererAssetHandleCatalogResult::InvalidArgument,
        "invalid material handle is rejected",
        failures);
    expect(
        handles.updateTextureHandle(asset(3), texture(30)) == full_engine::RendererAssetHandleCatalogResult::NotFound,
        "missing texture update returns not found",
        failures);
    expect(
        handles.removeTextureHandle({}) == full_engine::RendererAssetHandleCatalogResult::InvalidArgument,
        "invalid texture remove id is rejected",
        failures);
    expect(
        handles.addSkeletonHandle(asset(4), skeleton(40)) == full_engine::RendererAssetHandleCatalogResult::Success,
        "skeleton handle add succeeds",
        failures);
    expect(
        handles.addSkeletonHandle(asset(4), skeleton(41)) == full_engine::RendererAssetHandleCatalogResult::AlreadyExists,
        "duplicate skeleton handle add is rejected",
        failures);
    expect(
        handles.updateSkeletonHandle(asset(4), skeleton(42)) == full_engine::RendererAssetHandleCatalogResult::Success,
        "skeleton handle update succeeds",
        failures);
    const full_renderer::SkeletonHandle* foundSkeleton = handles.findSkeletonHandle(asset(4));
    expect(foundSkeleton != nullptr && foundSkeleton->id == 42, "updated skeleton handle can be found", failures);
    expect(
        handles.removeSkeletonHandle(asset(4)) == full_engine::RendererAssetHandleCatalogResult::Success,
        "skeleton handle remove succeeds",
        failures);
    expect(handles.findSkeletonHandle(asset(4)) == nullptr, "removed skeleton handle is missing", failures);
    expect(
        handles.addSkinnedMeshHandle({}, skinnedMesh(50)) == full_engine::RendererAssetHandleCatalogResult::InvalidArgument,
        "invalid skinned mesh asset id is rejected",
        failures);
    expect(
        handles.addSkinnedMeshHandle(asset(5), {}) == full_engine::RendererAssetHandleCatalogResult::InvalidArgument,
        "invalid skinned mesh handle is rejected",
        failures);
    expect(
        handles.updateSkinnedMeshHandle(asset(5), skinnedMesh(51)) == full_engine::RendererAssetHandleCatalogResult::NotFound,
        "missing skinned mesh update returns not found",
        failures);

    handles.addMaterialHandle(asset(2), material(20));
    handles.addTextureHandle(asset(3), texture(30));
    handles.addSkeletonHandle(asset(4), skeleton(40));
    handles.addSkinnedMeshHandle(asset(5), skinnedMesh(50));
    expect(handles.materialHandleCount() == 1, "material handle count tracks add", failures);
    expect(handles.textureHandleCount() == 1, "texture handle count tracks add", failures);
    expect(handles.skeletonHandleCount() == 1, "skeleton handle count tracks add", failures);
    expect(handles.skinnedMeshHandleCount() == 1, "skinned mesh handle count tracks add", failures);
    handles.clear();
    expect(handles.materialHandleCount() == 0, "clear removes material handles", failures);
    expect(handles.textureHandleCount() == 0, "clear removes texture handles", failures);
    expect(handles.skeletonHandleCount() == 0, "clear removes skeleton handles", failures);
    expect(handles.skinnedMeshHandleCount() == 0, "clear removes skinned mesh handles", failures);
}

void testValidOneLodResolve(std::vector<std::string>& failures)
{
    const full_engine::TerrainChunkAssetDesc desc = makeAssetDesc({1, 0, 0}, 1);
    const full_engine::RendererAssetHandleCatalog handles = makeHandleCatalog(1);

    const full_engine::TerrainAssetResolveResult resolved = full_engine::resolveTerrainChunkResources(desc, handles);

    expect(resolved.status == full_engine::TerrainAssetResolveStatus::Success, "one-LOD resolve succeeds", failures);
    expect(resolved.assetValidation == full_engine::TerrainAssetValidationResult::Success, "resolve records successful validation", failures);
    expect(resolved.resources.id == desc.id, "resolved resource preserves chunk id", failures);
    expect(resolved.resources.lodCount == 1, "resolved resource preserves one lod count", failures);
    expect(resolved.resources.lods[0].mesh.id == 1000, "resolved mesh handle is copied", failures);
    expect(resolved.resources.lods[0].material.id == 2000, "resolved material handle is copied", failures);
    expect(resolved.resources.lods[0].maxDistanceMeters == desc.lods[0].maxDistanceMeters, "resolved distance is copied", failures);
    expect(resolved.resources.splatMap.id == 0, "default splat asset resolves to fallback texture", failures);
    expect(
        full_engine::validateTerrainChunkResources(resolved.resources) == full_engine::TerrainResourceValidationResult::Success,
        "resolved resource descriptor validates",
        failures);
}

void testValidMultiLodAndSplatResolve(std::vector<std::string>& failures)
{
    full_engine::TerrainChunkAssetDesc desc = makeAssetDesc({2, 0, 0}, full_engine::kMaxTerrainAssetLodLevels);
    desc.splatMap = asset(300);
    const full_engine::RendererAssetHandleCatalog handles = makeHandleCatalog(full_engine::kMaxTerrainAssetLodLevels);

    const full_engine::TerrainAssetResolveResult resolved = full_engine::resolveTerrainChunkResources(desc, handles);

    expect(resolved.status == full_engine::TerrainAssetResolveStatus::Success, "multi-LOD resolve succeeds", failures);
    expect(resolved.resources.lodCount == full_engine::kMaxTerrainAssetLodLevels, "multi-LOD count is copied", failures);
    for (std::uint32_t index = 0; index < desc.lodCount; ++index)
    {
        expect(resolved.resources.lods[index].mesh.id == 1000 + index, "multi-LOD mesh handle is copied", failures);
        expect(resolved.resources.lods[index].material.id == 2000 + index, "multi-LOD material handle is copied", failures);
        expect(
            resolved.resources.lods[index].maxDistanceMeters == desc.lods[index].maxDistanceMeters,
            "multi-LOD distance is copied",
            failures);
    }
    expect(resolved.resources.splatMap.id == 3000, "valid splat asset resolves to texture handle", failures);
}

void testResolveFromAssetCatalog(std::vector<std::string>& failures)
{
    full_engine::TerrainAssetCatalog assets;
    const full_engine::TerrainChunkAssetDesc desc = makeAssetDesc({3, 0, 0}, 1);
    assets.addChunkAssets(desc);

    const full_engine::RendererAssetHandleCatalog handles = makeHandleCatalog(1);
    const full_engine::TerrainAssetResolveResult missing =
        full_engine::resolveTerrainChunkResources(assets, {99, 0, 0}, handles);
    const full_engine::TerrainAssetResolveResult resolved =
        full_engine::resolveTerrainChunkResources(assets, desc.id, handles);

    expect(missing.status == full_engine::TerrainAssetResolveStatus::MissingChunkAssets, "missing asset catalog chunk is reported", failures);
    expectDefaultResources(missing.resources, "missing asset catalog chunk leaves resources default", failures);
    expect(resolved.status == full_engine::TerrainAssetResolveStatus::Success, "asset catalog chunk resolves", failures);
    expect(resolved.resources.id == desc.id, "asset catalog resolve preserves chunk id", failures);
}

void testInvalidAssetDescriptor(std::vector<std::string>& failures)
{
    full_engine::TerrainChunkAssetDesc desc = makeAssetDesc({4, 0, 0}, 1);
    desc.lods[0].mesh = {};
    const full_engine::RendererAssetHandleCatalog handles = makeHandleCatalog(1);

    const full_engine::TerrainAssetResolveResult resolved = full_engine::resolveTerrainChunkResources(desc, handles);

    expect(resolved.status == full_engine::TerrainAssetResolveStatus::InvalidChunkAssets, "invalid asset descriptor is reported", failures);
    expect(
        resolved.assetValidation == full_engine::TerrainAssetValidationResult::InvalidMeshAsset,
        "invalid asset descriptor preserves validation detail",
        failures);
    expectDefaultResources(resolved.resources, "invalid asset descriptor leaves resources default", failures);
}

void testMissingHandleMappings(std::vector<std::string>& failures)
{
    const full_engine::TerrainChunkAssetDesc desc = makeAssetDesc({5, 0, 0}, 1);

    full_engine::RendererAssetHandleCatalog missingMesh;
    missingMesh.addMaterialHandle(asset(200), material(2000));
    const full_engine::TerrainAssetResolveResult meshResult =
        full_engine::resolveTerrainChunkResources(desc, missingMesh);
    expect(meshResult.status == full_engine::TerrainAssetResolveStatus::MissingMeshHandle, "missing mesh mapping is reported", failures);
    expectDefaultResources(meshResult.resources, "missing mesh mapping leaves resources default", failures);

    full_engine::RendererAssetHandleCatalog missingMaterial;
    missingMaterial.addMeshHandle(asset(100), mesh(1000));
    const full_engine::TerrainAssetResolveResult materialResult =
        full_engine::resolveTerrainChunkResources(desc, missingMaterial);
    expect(
        materialResult.status == full_engine::TerrainAssetResolveStatus::MissingMaterialHandle,
        "missing material mapping is reported",
        failures);
    expectDefaultResources(materialResult.resources, "missing material mapping leaves resources default", failures);

    full_engine::TerrainChunkAssetDesc splatDesc = desc;
    splatDesc.splatMap = asset(300);
    full_engine::RendererAssetHandleCatalog missingSplat = makeHandleCatalog(1);
    missingSplat.removeTextureHandle(asset(300));
    const full_engine::TerrainAssetResolveResult splatResult =
        full_engine::resolveTerrainChunkResources(splatDesc, missingSplat);
    expect(
        splatResult.status == full_engine::TerrainAssetResolveStatus::MissingSplatMapHandle,
        "missing splat mapping is reported",
        failures);
    expectDefaultResources(splatResult.resources, "missing splat mapping leaves resources default", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testHandleCatalogMutations(failures);
    testValidOneLodResolve(failures);
    testValidMultiLodAndSplatResolve(failures);
    testResolveFromAssetCatalog(failures);
    testInvalidAssetDescriptor(failures);
    testMissingHandleMappings(failures);

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
