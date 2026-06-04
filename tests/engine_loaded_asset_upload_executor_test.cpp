#include "engine/renderer_integration/LoadedAssetUploadExecutor.hpp"

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

full_engine::LoadedAssetPayload meshPayload(const std::uint64_t id = 10)
{
    full_engine::LoadedMeshAsset mesh;
    mesh.id = asset(id);
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

    full_engine::LoadedAssetPayload payload;
    payload.kind = full_engine::AssetKind::Mesh;
    payload.mesh = mesh;
    return payload;
}

full_engine::LoadedAssetPayload texturePayload(const std::uint64_t id = 20)
{
    full_engine::LoadedTextureAsset texture;
    texture.id = asset(id);
    texture.width = 4;
    texture.height = 2;
    texture.mipCount = 1;
    texture.format = full_engine::AssetSourceTextureFormat::Rgba8;
    texture.semantic = full_engine::AssetSourceTextureSemantic::Color;
    texture.colorSpace = full_engine::AssetSourceTextureColorSpace::Srgb;
    texture.bytes.assign(texture.width * texture.height * 4, 255);

    full_engine::LoadedAssetPayload payload;
    payload.kind = full_engine::AssetKind::Texture;
    payload.texture = texture;
    return payload;
}

full_engine::LoadedAssetPayload materialPayload(const std::uint64_t id = 30)
{
    full_engine::LoadedMaterialAsset material;
    material.id = asset(id);
    material.model = full_engine::AssetSourceMaterialModel::TerrainSplat;
    material.alphaMode = full_engine::AssetSourceMaterialAlphaMode::Opaque;
    material.textureRefs[0] = {full_engine::AssetSourceMaterialTextureSlot::BaseColor, asset(20)};
    material.textureRefCount = 1;

    full_engine::LoadedAssetPayload payload;
    payload.kind = full_engine::AssetKind::Material;
    payload.material = material;
    return payload;
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

full_engine::LoadedAssetPayload skeletonPayload(const std::uint64_t id = 40)
{
    full_engine::LoadedSkeletonAsset skeleton;
    skeleton.id = asset(id);
    skeleton.joints = {joint(-1), joint(0)};

    full_engine::LoadedAssetPayload payload;
    payload.kind = full_engine::AssetKind::Skeleton;
    payload.skeleton = skeleton;
    return payload;
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
    result.colorLinear[0] = 1.0f;
    result.colorLinear[1] = 0.5f;
    result.colorLinear[2] = 0.25f;
    result.colorLinear[3] = 1.0f;
    result.jointIndices[0] = 0;
    result.jointIndices[1] = 1;
    result.jointWeights[0] = 0.5f;
    result.jointWeights[1] = 0.5f;
    return result;
}

full_engine::LoadedAssetPayload skinnedMeshPayload(
    const std::uint64_t id = 50,
    const std::uint64_t skeletonId = 40)
{
    full_engine::LoadedSkinnedMeshAsset mesh;
    mesh.id = asset(id);
    mesh.skeletonAssetId = asset(skeletonId);
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
    mesh.sections.push_back({asset(30), 0, 3});

    full_engine::LoadedAssetPayload payload;
    payload.kind = full_engine::AssetKind::SkinnedMesh;
    payload.skinnedMesh = mesh;
    return payload;
}

full_engine::LoadedAssetUploadPlan uploadPlan(
    const std::vector<full_engine::LoadedAssetPayload>& payloads)
{
    return full_engine::buildLoadedAssetUploadPlan(payloads.data(), payloads.size());
}

class FakeRenderer final : public full_renderer::IRenderer
{
public:
    full_renderer::MeshHandle nextMesh = {101};
    full_renderer::TextureHandle nextTexture = {201};
    full_renderer::MaterialHandle nextMaterial = {301};
    full_renderer::SkeletonHandle nextSkeleton = {401};
    full_renderer::SkinnedMeshHandle nextSkinnedMesh = {501};
    int meshCreateCalls = 0;
    int textureCreateCalls = 0;
    int materialCreateCalls = 0;
    int skeletonCreateCalls = 0;
    int skinnedMeshCreateCalls = 0;
    std::uint32_t lastMeshVertexCount = 0;
    std::uint32_t lastTextureWidth = 0;
    std::uint32_t lastSkeletonJointCount = 0;
    full_renderer::SkeletonHandle lastSkinnedMeshSkeleton = {};
    std::uint32_t lastSkinnedMeshSectionCount = 0;
    full_renderer::MaterialDesc lastMaterial = {};

    full_renderer::RendererResult initialize(const full_renderer::RendererInitDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    void shutdown() noexcept override {}

    bool isInitialized() const noexcept override
    {
        return true;
    }

    full_renderer::MeshHandle createMesh(const full_renderer::MeshDesc& desc) override
    {
        ++meshCreateCalls;
        lastMeshVertexCount = desc.vertexCount;
        return nextMesh;
    }

    void destroyMesh(full_renderer::MeshHandle) noexcept override {}

    full_renderer::SkeletonHandle createSkeleton(const full_renderer::SkeletonDesc& desc) override
    {
        ++skeletonCreateCalls;
        lastSkeletonJointCount = desc.jointCount;
        return nextSkeleton;
    }

    void destroySkeleton(full_renderer::SkeletonHandle) noexcept override {}

    full_renderer::SkinnedMeshHandle createSkinnedMesh(const full_renderer::SkinnedMeshDesc& desc) override
    {
        ++skinnedMeshCreateCalls;
        lastSkinnedMeshSkeleton = desc.skeleton;
        lastSkinnedMeshSectionCount = desc.sectionCount;
        return nextSkinnedMesh;
    }

    void destroySkinnedMesh(full_renderer::SkinnedMeshHandle) noexcept override {}

    full_renderer::TextureHandle createTexture(const full_renderer::TextureDesc& desc) override
    {
        ++textureCreateCalls;
        lastTextureWidth = desc.width;
        return nextTexture;
    }

    void destroyTexture(full_renderer::TextureHandle) noexcept override {}

    full_renderer::MaterialHandle createMaterial(const full_renderer::MaterialDesc& desc) override
    {
        ++materialCreateCalls;
        lastMaterial = desc;
        return nextMaterial;
    }

    void destroyMaterial(full_renderer::MaterialHandle) noexcept override {}

    full_renderer::TerrainChunkHandle createTerrainChunk(const full_renderer::TerrainChunkDesc&) override
    {
        return {};
    }

    full_renderer::RendererResult updateTerrainChunk(
        full_renderer::TerrainChunkHandle,
        const full_renderer::TerrainChunkDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    void destroyTerrainChunk(full_renderer::TerrainChunkHandle) noexcept override {}

    full_renderer::RendererResult resize(const full_renderer::RendererResizeDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult beginFrame(const full_renderer::FrameDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult submit(const full_renderer::RenderPacket&) override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult endFrame() override
    {
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererStats getStats() const noexcept override
    {
        return {};
    }

    full_renderer::TerrainStats getTerrainStats() const noexcept override
    {
        return {};
    }

    std::uint32_t copyTerrainDebugInfo(full_renderer::TerrainChunkDebugInfo*, std::uint32_t) const noexcept override
    {
        return 0;
    }

    std::uint32_t copyTerrainBatchDebugInfo(full_renderer::TerrainBatchDebugInfo*, std::uint32_t) const noexcept override
    {
        return 0;
    }

    std::uint32_t copyTerrainShadowCasterDebugInfo(
        full_renderer::TerrainChunkDebugInfo*,
        std::uint32_t) const noexcept override
    {
        return 0;
    }
};

void testUploadsMeshAndTexture(std::vector<std::string>& failures)
{
    const std::vector<full_engine::LoadedAssetPayload> payloads = {
        meshPayload(),
        texturePayload()};
    const full_engine::LoadedAssetUploadPlan plan = uploadPlan(payloads);

    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog handles;
    const full_engine::LoadedAssetUploadExecuteResult result =
        full_engine::executeLoadedAssetUploadPlan(renderer, plan, handles);

    expect(result.records.size() == 2, "executor returns one record per plan record", failures);
    expect(result.records[0].status == full_engine::LoadedAssetUploadExecuteStatus::Uploaded, "mesh upload reports uploaded", failures);
    expect(result.records[1].status == full_engine::LoadedAssetUploadExecuteStatus::Uploaded, "texture upload reports uploaded", failures);
    expect(result.summary.uploadedMeshCount == 1, "mesh upload summary increments", failures);
    expect(result.summary.uploadedTextureCount == 1, "texture upload summary increments", failures);
    expect(renderer.meshCreateCalls == 1, "mesh upload calls renderer", failures);
    expect(renderer.textureCreateCalls == 1, "texture upload calls renderer", failures);
    expect(renderer.lastMeshVertexCount == 3, "mesh upload passes descriptor", failures);
    expect(renderer.lastTextureWidth == 4, "texture upload passes descriptor", failures);
    expect(handles.findMeshHandle(asset(10)) != nullptr && handles.findMeshHandle(asset(10))->id == 101, "mesh upload catalogs returned handle", failures);
    expect(handles.findTextureHandle(asset(20)) != nullptr && handles.findTextureHandle(asset(20))->id == 201, "texture upload catalogs returned handle", failures);
}

void testMaterialUploadsAfterTextureResolution(std::vector<std::string>& failures)
{
    const std::vector<full_engine::LoadedAssetPayload> payloads = {materialPayload()};
    const full_engine::LoadedAssetUploadPlan plan = uploadPlan(payloads);

    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog handles;
    (void)handles.addTextureHandle(asset(20), {601});
    const full_engine::LoadedAssetUploadExecuteResult result =
        full_engine::executeLoadedAssetUploadPlan(renderer, plan, handles);

    expect(result.records[0].status == full_engine::LoadedAssetUploadExecuteStatus::Uploaded, "material upload reports uploaded", failures);
    expect(result.summary.uploadedMaterialCount == 1, "material upload summary increments", failures);
    expect(renderer.materialCreateCalls == 1, "material upload calls renderer", failures);
    expect(renderer.lastMaterial.kind == full_renderer::MaterialKind::TerrainSplat, "material upload maps kind", failures);
    expect(renderer.lastMaterial.terrain.layers[0].albedoTexture.id == 601, "material upload resolves texture ref", failures);
    expect(handles.findMaterialHandle(asset(30)) != nullptr && handles.findMaterialHandle(asset(30))->id == 301, "material upload catalogs returned handle", failures);
}

void testMaterialMissingTextureIsRetryable(std::vector<std::string>& failures)
{
    const std::vector<full_engine::LoadedAssetPayload> payloads = {materialPayload()};
    const full_engine::LoadedAssetUploadPlan plan = uploadPlan(payloads);

    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog handles;
    const full_engine::LoadedAssetUploadExecuteResult result =
        full_engine::executeLoadedAssetUploadPlan(renderer, plan, handles);

    expect(result.records[0].status == full_engine::LoadedAssetUploadExecuteStatus::MissingTextureHandle, "missing material texture reports retryable status", failures);
    expect(result.summary.missingTextureHandleCount == 1, "missing material texture summary increments", failures);
    expect(renderer.materialCreateCalls == 0, "missing texture does not call renderer", failures);
    expect(handles.materialHandleCount() == 0, "missing texture does not mutate material catalog", failures);
}

void testUploadsSkeletonAndSkinnedMesh(std::vector<std::string>& failures)
{
    const std::vector<full_engine::LoadedAssetPayload> payloads = {
        skeletonPayload(),
        skinnedMeshPayload()};
    const full_engine::LoadedAssetUploadPlan plan = uploadPlan(payloads);

    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog handles;
    const full_engine::LoadedAssetUploadExecuteResult result =
        full_engine::executeLoadedAssetUploadPlan(renderer, plan, handles);

    expect(result.records.size() == 2, "skeleton executor returns one record per plan record", failures);
    expect(result.records[0].status == full_engine::LoadedAssetUploadExecuteStatus::Uploaded, "skeleton upload reports uploaded", failures);
    expect(result.records[1].status == full_engine::LoadedAssetUploadExecuteStatus::Uploaded, "skinned mesh upload reports uploaded", failures);
    expect(result.summary.uploadedSkeletonCount == 1, "skeleton upload summary increments", failures);
    expect(result.summary.uploadedSkinnedMeshCount == 1, "skinned mesh upload summary increments", failures);
    expect(renderer.skeletonCreateCalls == 1, "skeleton upload calls renderer", failures);
    expect(renderer.skinnedMeshCreateCalls == 1, "skinned mesh upload calls renderer", failures);
    expect(renderer.lastSkeletonJointCount == 2, "skeleton upload passes descriptor", failures);
    expect(renderer.lastSkinnedMeshSkeleton.id == 401, "skinned mesh upload resolves skeleton handle", failures);
    expect(renderer.lastSkinnedMeshSectionCount == 1, "skinned mesh upload passes section descriptors", failures);
    expect(handles.findSkeletonHandle(asset(40)) != nullptr && handles.findSkeletonHandle(asset(40))->id == 401, "skeleton upload catalogs returned handle", failures);
    expect(handles.findSkinnedMeshHandle(asset(50)) != nullptr && handles.findSkinnedMeshHandle(asset(50))->id == 501, "skinned mesh upload catalogs returned handle", failures);
}

void testSkinnedMeshMissingSkeletonIsRetryable(std::vector<std::string>& failures)
{
    const std::vector<full_engine::LoadedAssetPayload> payloads = {skinnedMeshPayload()};
    const full_engine::LoadedAssetUploadPlan plan = uploadPlan(payloads);

    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog handles;
    const full_engine::LoadedAssetUploadExecuteResult result =
        full_engine::executeLoadedAssetUploadPlan(renderer, plan, handles);

    expect(result.records[0].status == full_engine::LoadedAssetUploadExecuteStatus::MissingSkeletonHandle, "missing skeleton reports retryable status", failures);
    expect(result.summary.missingSkeletonHandleCount == 1, "missing skeleton summary increments", failures);
    expect(renderer.skinnedMeshCreateCalls == 0, "missing skeleton does not call renderer", failures);
    expect(handles.skinnedMeshHandleCount() == 0, "missing skeleton does not mutate skinned mesh catalog", failures);
}

void testExistingMappingsSkipUpload(std::vector<std::string>& failures)
{
    const std::vector<full_engine::LoadedAssetPayload> payloads = {
        meshPayload(),
        texturePayload(),
        skeletonPayload(),
        skinnedMeshPayload()};
    const full_engine::LoadedAssetUploadPlan plan = uploadPlan(payloads);

    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog handles;
    (void)handles.addMeshHandle(asset(10), {501});
    (void)handles.addTextureHandle(asset(20), {601});
    (void)handles.addMaterialHandle(asset(30), {701});
    (void)handles.addSkeletonHandle(asset(40), {801});
    (void)handles.addSkinnedMeshHandle(asset(50), {901});

    const full_engine::LoadedAssetUploadExecuteResult result =
        full_engine::executeLoadedAssetUploadPlan(renderer, plan, handles);

    expect(result.records[0].status == full_engine::LoadedAssetUploadExecuteStatus::AlreadyMapped, "existing mesh mapping skips upload", failures);
    expect(result.records[1].status == full_engine::LoadedAssetUploadExecuteStatus::AlreadyMapped, "existing texture mapping skips upload", failures);
    expect(result.records[2].status == full_engine::LoadedAssetUploadExecuteStatus::AlreadyMapped, "existing skeleton mapping skips upload", failures);
    expect(result.records[3].status == full_engine::LoadedAssetUploadExecuteStatus::AlreadyMapped, "existing skinned mesh mapping skips upload", failures);
    expect(result.summary.alreadyMappedCount == 4, "already mapped summary increments", failures);
    expect(renderer.meshCreateCalls == 0 && renderer.textureCreateCalls == 0, "existing mappings do not call mesh/texture renderer", failures);
    expect(renderer.skeletonCreateCalls == 0 && renderer.skinnedMeshCreateCalls == 0, "existing mappings do not call skeleton renderer", failures);
    expect(handles.findMeshHandle(asset(10))->id == 501, "existing mesh mapping is preserved", failures);
    expect(handles.findTextureHandle(asset(20))->id == 601, "existing texture mapping is preserved", failures);
    expect(handles.findSkeletonHandle(asset(40))->id == 801, "existing skeleton mapping is preserved", failures);
    expect(handles.findSkinnedMeshHandle(asset(50))->id == 901, "existing skinned mesh mapping is preserved", failures);
}

void testRendererFailureDoesNotMutateCatalog(std::vector<std::string>& failures)
{
    const std::vector<full_engine::LoadedAssetPayload> payloads = {
        meshPayload(),
        texturePayload(),
        skeletonPayload()};
    const full_engine::LoadedAssetUploadPlan plan = uploadPlan(payloads);

    FakeRenderer renderer;
    renderer.nextMesh = {};
    renderer.nextTexture = {};
    renderer.nextSkeleton = {};
    full_engine::RendererAssetHandleCatalog handles;
    const full_engine::LoadedAssetUploadExecuteResult result =
        full_engine::executeLoadedAssetUploadPlan(renderer, plan, handles);

    expect(result.records[0].status == full_engine::LoadedAssetUploadExecuteStatus::RendererFailed, "invalid mesh handle reports renderer failure", failures);
    expect(result.records[1].status == full_engine::LoadedAssetUploadExecuteStatus::RendererFailed, "invalid texture handle reports renderer failure", failures);
    expect(result.records[2].status == full_engine::LoadedAssetUploadExecuteStatus::RendererFailed, "invalid skeleton handle reports renderer failure", failures);
    expect(result.summary.rendererFailedCount == 3, "renderer failure summary increments", failures);
    expect(handles.meshHandleCount() == 0 && handles.textureHandleCount() == 0, "renderer failure does not mutate mesh/texture catalog", failures);
    expect(handles.skeletonHandleCount() == 0, "renderer failure does not mutate skeleton catalog", failures);
}

void testSkinnedMeshRendererFailureDoesNotMutateCatalog(std::vector<std::string>& failures)
{
    const std::vector<full_engine::LoadedAssetPayload> payloads = {skinnedMeshPayload()};
    const full_engine::LoadedAssetUploadPlan plan = uploadPlan(payloads);

    FakeRenderer renderer;
    renderer.nextSkinnedMesh = {};
    full_engine::RendererAssetHandleCatalog handles;
    (void)handles.addSkeletonHandle(asset(40), {808});
    const full_engine::LoadedAssetUploadExecuteResult result =
        full_engine::executeLoadedAssetUploadPlan(renderer, plan, handles);

    expect(result.records[0].status == full_engine::LoadedAssetUploadExecuteStatus::RendererFailed, "invalid skinned mesh handle reports renderer failure", failures);
    expect(result.summary.rendererFailedCount == 1, "skinned renderer failure summary increments", failures);
    expect(renderer.skinnedMeshCreateCalls == 1, "skinned renderer failure still attempts upload", failures);
    expect(handles.skinnedMeshHandleCount() == 0, "skinned renderer failure does not mutate skinned mesh catalog", failures);
    expect(handles.findSkeletonHandle(asset(40)) != nullptr, "skinned renderer failure preserves skeleton mapping", failures);
}

void testCatalogRejectedPreservesExistingMappings(std::vector<std::string>& failures)
{
    const std::vector<full_engine::LoadedAssetPayload> payloads = {meshPayload()};
    full_engine::LoadedAssetUploadPlan plan = uploadPlan(payloads);
    plan.records[0].id = {};

    FakeRenderer renderer;
    renderer.nextMesh = {909};
    full_engine::RendererAssetHandleCatalog handles;
    (void)handles.addMeshHandle(asset(77), {707});

    const full_engine::LoadedAssetUploadExecuteResult result =
        full_engine::executeLoadedAssetUploadPlan(renderer, plan, handles);

    expect(result.records[0].status == full_engine::LoadedAssetUploadExecuteStatus::CatalogRejected, "catalog rejection reports rejected", failures);
    expect(result.records[0].catalogResult == full_engine::RendererAssetHandleCatalogResult::InvalidArgument, "catalog rejection records catalog result", failures);
    expect(result.summary.catalogRejectedCount == 1, "catalog rejection summary increments", failures);
    expect(renderer.meshCreateCalls == 1, "catalog rejection happens after renderer upload", failures);
    expect(handles.meshHandleCount() == 1, "catalog rejection preserves existing mappings", failures);
    expect(handles.findMeshHandle(asset(77)) != nullptr && handles.findMeshHandle(asset(77))->id == 707, "existing mapping remains unchanged", failures);
}

void testSkippedUnplannedAndUnsupportedKind(std::vector<std::string>& failures)
{
    full_engine::LoadedAssetUploadPlan plan;

    full_engine::LoadedAssetUploadRecord invalid;
    invalid.id = asset(1);
    invalid.kind = full_engine::AssetKind::Mesh;
    invalid.status = full_engine::LoadedAssetUploadStatus::InvalidPayload;
    plan.records.push_back(invalid);

    full_engine::LoadedAssetUploadRecord unsupported;
    unsupported.id = asset(2);
    unsupported.kind = full_engine::AssetKind::Shader;
    unsupported.status = full_engine::LoadedAssetUploadStatus::Planned;
    plan.records.push_back(unsupported);

    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog handles;
    const full_engine::LoadedAssetUploadExecuteResult result =
        full_engine::executeLoadedAssetUploadPlan(renderer, plan, handles);

    expect(result.records.size() == 2, "skipped test preserves record count", failures);
    expect(result.records[0].status == full_engine::LoadedAssetUploadExecuteStatus::SkippedUnplanned, "invalid source record skips unplanned", failures);
    expect(result.records[1].status == full_engine::LoadedAssetUploadExecuteStatus::UnsupportedKind, "planned unsupported kind reports unsupported", failures);
    expect(result.summary.skippedUnplannedCount == 1, "skipped unplanned summary increments", failures);
    expect(result.summary.unsupportedKindCount == 1, "unsupported kind summary increments", failures);
    expect(renderer.meshCreateCalls == 0 && renderer.textureCreateCalls == 0, "skipped records do not call renderer", failures);
}

void testMixedOrderAndStatusNames(std::vector<std::string>& failures)
{
    const std::vector<full_engine::LoadedAssetPayload> payloads = {
        texturePayload(1),
        skeletonPayload(4),
        skinnedMeshPayload(5, 4),
        materialPayload(2),
        meshPayload(3)};
    const full_engine::LoadedAssetUploadPlan plan = uploadPlan(payloads);

    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog handles;
    (void)handles.addTextureHandle(asset(20), {401});
    const full_engine::LoadedAssetUploadExecuteResult result =
        full_engine::executeLoadedAssetUploadPlan(renderer, plan, handles);

    expect(result.records[0].id == asset(1), "mixed executor preserves first record order", failures);
    expect(result.records[1].id == asset(4), "mixed executor preserves second record order", failures);
    expect(result.records[2].id == asset(5), "mixed executor preserves third record order", failures);
    expect(result.records[3].id == asset(2), "mixed executor preserves fourth record order", failures);
    expect(result.records[4].id == asset(3), "mixed executor preserves fifth record order", failures);
    expect(result.summary.uploadedTextureCount == 1, "mixed executor counts texture upload", failures);
    expect(result.summary.uploadedSkeletonCount == 1, "mixed executor counts skeleton upload", failures);
    expect(result.summary.uploadedSkinnedMeshCount == 1, "mixed executor counts skinned mesh upload", failures);
    expect(result.summary.uploadedMaterialCount == 1, "mixed executor counts material upload", failures);
    expect(result.summary.uploadedMeshCount == 1, "mixed executor counts mesh upload", failures);

    const full_engine::LoadedAssetUploadExecuteStatus statuses[] = {
        full_engine::LoadedAssetUploadExecuteStatus::Uploaded,
        full_engine::LoadedAssetUploadExecuteStatus::AlreadyMapped,
        full_engine::LoadedAssetUploadExecuteStatus::MissingTextureHandle,
        full_engine::LoadedAssetUploadExecuteStatus::MissingSkeletonHandle,
        full_engine::LoadedAssetUploadExecuteStatus::SkippedUnplanned,
        full_engine::LoadedAssetUploadExecuteStatus::RendererFailed,
        full_engine::LoadedAssetUploadExecuteStatus::CatalogRejected,
        full_engine::LoadedAssetUploadExecuteStatus::UnsupportedKind};
    for (const full_engine::LoadedAssetUploadExecuteStatus status : statuses)
    {
        expect(
            std::string(full_engine::loadedAssetUploadExecuteStatusName(status)) != "Unknown",
            "upload executor status has stable name",
            failures);
    }
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testUploadsMeshAndTexture(failures);
    testMaterialUploadsAfterTextureResolution(failures);
    testMaterialMissingTextureIsRetryable(failures);
    testUploadsSkeletonAndSkinnedMesh(failures);
    testSkinnedMeshMissingSkeletonIsRetryable(failures);
    testExistingMappingsSkipUpload(failures);
    testRendererFailureDoesNotMutateCatalog(failures);
    testSkinnedMeshRendererFailureDoesNotMutateCatalog(failures);
    testCatalogRejectedPreservesExistingMappings(failures);
    testSkippedUnplannedAndUnsupportedKind(failures);
    testMixedOrderAndStatusNames(failures);

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
