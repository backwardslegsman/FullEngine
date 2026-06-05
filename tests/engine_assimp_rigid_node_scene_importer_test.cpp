#include "engine/assets/AssimpRigidNodeSceneImporter.hpp"
#include "engine/assets/LoadedAnimationSampler.hpp"
#include "engine/renderer_integration/LoadedAssetUploadExecutor.hpp"
#include "engine/renderer_integration/LoadedMeshUploadDiagnostics.hpp"
#include "engine/renderer_integration/RigidNodeAttachmentDrawBuilder.hpp"

#include <cstdlib>
#include <cstdint>
#include <fstream>
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

std::string fixturePath(const char* const name)
{
    return std::string(FULL_RENDERER_TEST_GLTF_FIXTURE_DIR) + "/" + name;
}

full_engine::AssimpRigidNodeSceneImportOptions validOptions(const char* const name)
{
    full_engine::AssimpRigidNodeSceneImportOptions options;
    options.path = fixturePath(name);
    options.skeletonAssetId = asset(100);
    options.animationClipAssetId = asset(101);
    options.firstMeshAssetId = asset(200);
    options.assimp.allowMissingRotationKeys = false;
    options.assimp.generateMissingTangents = false;
    return options;
}

full_engine::LoadedAssetPayload meshPayload(const full_engine::LoadedMeshAsset& mesh)
{
    full_engine::LoadedAssetPayload payload;
    payload.kind = full_engine::AssetKind::Mesh;
    payload.mesh = mesh;
    return payload;
}

class FakeRenderer final : public full_renderer::IRenderer
{
public:
    full_renderer::MeshHandle nextMesh = {501};
    int meshCreateCalls = 0;

    full_renderer::RendererResult initialize(const full_renderer::RendererInitDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    void shutdown() noexcept override {}
    bool isInitialized() const noexcept override { return true; }

    full_renderer::MeshHandle createMesh(const full_renderer::MeshDesc&) override
    {
        return {nextMesh.id + static_cast<std::uint16_t>(meshCreateCalls++)};
    }

    void destroyMesh(full_renderer::MeshHandle) noexcept override {}
    full_renderer::SkeletonHandle createSkeleton(const full_renderer::SkeletonDesc&) override { return {}; }
    void destroySkeleton(full_renderer::SkeletonHandle) noexcept override {}
    full_renderer::SkinnedMeshHandle createSkinnedMesh(const full_renderer::SkinnedMeshDesc&) override { return {}; }
    void destroySkinnedMesh(full_renderer::SkinnedMeshHandle) noexcept override {}
    full_renderer::TextureHandle createTexture(const full_renderer::TextureDesc&) override { return {}; }
    void destroyTexture(full_renderer::TextureHandle) noexcept override {}
    full_renderer::MaterialHandle createMaterial(const full_renderer::MaterialDesc&) override { return {}; }
    void destroyMaterial(full_renderer::MaterialHandle) noexcept override {}
    full_renderer::TerrainChunkHandle createTerrainChunk(const full_renderer::TerrainChunkDesc&) override { return {}; }
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
    full_renderer::RendererStats getStats() const noexcept override { return {}; }
    full_renderer::TerrainStats getTerrainStats() const noexcept override { return {}; }
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

void testRigidNodeImportAndDrawPath(std::vector<std::string>& failures)
{
    const full_engine::AssimpRigidNodeSceneImportResult import =
        full_engine::importRigidNodeSceneWithAssimp(validOptions("rigid_node_two_meshes.gltf"));

    expect(import.status == full_engine::AssimpRigidNodeSceneImportStatus::Success, "rigid node scene imports", failures);
    expect(import.skeleton.joints.size() == 2, "rigid node import builds two joints", failures);
    expect(import.animationClip.tracks.size() == 2, "rigid node import builds two animation tracks", failures);
    expect(import.attachments.size() == 2, "rigid node import builds two attachments", failures);
    expect(import.summary.importedAttachmentCount == 2, "rigid import summary counts attachments", failures);
    expect(import.meshRecords.size() == 2, "rigid import records mesh refs", failures);
    expect(import.attachments[0].meshAssetId == asset(200), "first attachment uses first mesh asset id", failures);
    expect(import.attachments[1].meshAssetId == asset(201), "second attachment increments mesh asset id", failures);

    std::vector<full_engine::LoadedAssetPayload> payloads;
    for (const full_engine::AssimpRigidNodeMeshAttachment& attachment : import.attachments)
    {
        payloads.push_back(meshPayload(attachment.mesh));
    }
    const full_engine::LoadedAssetUploadPlan uploadPlan =
        full_engine::buildLoadedAssetUploadPlan(payloads.data(), payloads.size());

    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog handles;
    const full_engine::LoadedAssetUploadExecuteResult upload =
        full_engine::executeLoadedAssetUploadPlan(renderer, uploadPlan, handles);
    expect(upload.summary.uploadedMeshCount == 2, "rigid attachment meshes upload", failures);
    expect(handles.meshHandleCount() == 2, "rigid upload catalogs mesh handles", failures);

    const full_engine::LoadedAnimationSampleResult sample =
        full_engine::sampleLoadedAnimationClip(
            import.skeleton,
            import.animationClip,
            0.5f,
            full_engine::LoadedAnimationPlaybackMode::Clamp);
    expect(sample.status == full_engine::LoadedAnimationSampleStatus::Success, "rigid clip samples", failures);

    const full_engine::RigidNodeAttachmentDrawResult draws =
        full_engine::buildRigidNodeAttachmentDraws(
            import.attachments.data(),
            import.attachments.size(),
            handles,
            sample.pose,
            full_renderer::MaterialHandle{901});
    expect(draws.summary.builtCount == 2, "rigid draw builder emits two draws", failures);
    expect(draws.draws.size() == 2, "rigid draw vector has two items", failures);
    expect(draws.draws[0].mesh.id == 501, "first draw resolves first uploaded mesh", failures);
    expect(draws.draws[1].mesh.id == 502, "second draw resolves second uploaded mesh", failures);
    expect(draws.draws[0].model[12] == 0.5f, "first draw uses sampled root translation", failures);
    expect(draws.draws[1].model[12] == 0.5f, "child draw inherits root x translation", failures);
    expect(draws.draws[1].model[13] == 1.0f, "child draw uses sampled child y translation", failures);
    expect(draws.draws[0].bounds.min[0] == 0.5f, "first draw bounds transform by sampled matrix", failures);
}

void testStrictMissingTangentsRejectsFixture(std::vector<std::string>& failures)
{
    full_engine::AssimpRigidNodeSceneImportOptions options = validOptions("rigid_node_two_meshes.gltf");
    options.assimp.generateMissingTangents = false;
    options.assimp.triangulate = true;
    options.path = fixturePath("rigid_node_missing_tangent.gltf");

    const full_engine::AssimpRigidNodeSceneImportResult result =
        full_engine::importRigidNodeSceneWithAssimp(options);
    expect(result.status == full_engine::AssimpRigidNodeSceneImportStatus::UnsupportedScene, "fixture without tangent attachments is unsupported", failures);
    expect(result.summary.missingAttributeCount > 0, "missing tangent reports missing attribute skip", failures);
}

void testDrawBuilderFailures(std::vector<std::string>& failures)
{
    const full_engine::AssimpRigidNodeSceneImportResult import =
        full_engine::importRigidNodeSceneWithAssimp(validOptions("rigid_node_two_meshes.gltf"));
    const full_engine::LoadedAnimationSampleResult sample =
        full_engine::sampleLoadedAnimationClip(import.skeleton, import.animationClip, 0.0f);

    full_engine::RendererAssetHandleCatalog handles;
    const full_engine::RigidNodeAttachmentDrawResult missingMesh =
        full_engine::buildRigidNodeAttachmentDraws(
            import.attachments.data(),
            import.attachments.size(),
            handles,
            sample.pose,
            full_renderer::MaterialHandle{901});
    expect(missingMesh.summary.missingMeshHandleCount == 2, "missing mesh handles are reported", failures);

    (void)handles.addMeshHandle(asset(200), {1});
    (void)handles.addMeshHandle(asset(201), {2});
    full_engine::LoadedAnimationPose badPose = sample.pose;
    badPose.modelMatrices.pop_back();
    const full_engine::RigidNodeAttachmentDrawResult invalidPose =
        full_engine::buildRigidNodeAttachmentDraws(
            import.attachments.data(),
            import.attachments.size(),
            handles,
            badPose,
            full_renderer::MaterialHandle{901});
    expect(invalidPose.summary.invalidPoseCount == 2, "invalid pose blocks all draws", failures);

    const full_engine::RigidNodeAttachmentDrawResult invalidMaterial =
        full_engine::buildRigidNodeAttachmentDraws(
            import.attachments.data(),
            import.attachments.size(),
            handles,
            sample.pose,
            {});
    expect(invalidMaterial.summary.invalidMaterialHandleCount == 2, "invalid material blocks draws", failures);
}

void testUploadContractFilteringKeepsDrawableAttachments(std::vector<std::string>& failures)
{
    full_engine::AssimpRigidNodeSceneImportResult import =
        full_engine::importRigidNodeSceneWithAssimp(validOptions("rigid_node_two_meshes.gltf"));
    expect(import.attachments.size() == 2, "filtering fixture imports two attachments", failures);
    if (import.attachments.size() != 2)
    {
        return;
    }

    import.attachments[1].mesh.vertices[2].position[0] = 2.0f;
    import.attachments[1].mesh.vertices[2].position[1] = 0.0f;
    import.attachments[1].mesh.localBounds.max[0] = 2.0f;

    std::vector<full_engine::LoadedAssetPayload> payloads;
    for (const full_engine::AssimpRigidNodeMeshAttachment& attachment : import.attachments)
    {
        payloads.push_back(meshPayload(attachment.mesh));
    }

    const full_engine::LoadedAssetUploadPlan fullPlan =
        full_engine::buildLoadedAssetUploadPlan(payloads.data(), payloads.size());
    expect(fullPlan.summary.plannedCount == 1, "filtering plan keeps one uploadable attachment", failures);
    expect(fullPlan.summary.unsupportedRendererContractCount == 1, "filtering plan counts renderer contract reject", failures);

    const full_engine::LoadedMeshUploadDiagnostics rejected =
        full_engine::diagnoseLoadedMeshUploadContract(import.attachments[1].mesh);
    expect(rejected.status == full_engine::LoadedMeshUploadDiagnosticStatus::DegenerateTriangles, "filtered attachment reports degenerate triangles", failures);
    expect(rejected.degenerateTriangleCount == 1, "filtered attachment counts degenerate triangle", failures);

    std::vector<full_engine::LoadedAssetPayload> uploadablePayloads;
    std::vector<full_engine::AssimpRigidNodeMeshAttachment> uploadableAttachments;
    for (std::size_t index = 0; index < fullPlan.records.size(); ++index)
    {
        if (fullPlan.records[index].status == full_engine::LoadedAssetUploadStatus::Planned)
        {
            uploadablePayloads.push_back(payloads[index]);
            uploadableAttachments.push_back(import.attachments[index]);
        }
    }

    const full_engine::LoadedAssetUploadPlan executablePlan =
        full_engine::buildLoadedAssetUploadPlan(uploadablePayloads.data(), uploadablePayloads.size());
    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog handles;
    const full_engine::LoadedAssetUploadExecuteResult upload =
        full_engine::executeLoadedAssetUploadPlan(renderer, executablePlan, handles);
    expect(upload.summary.uploadedMeshCount == 1, "filtered upload creates one mesh", failures);

    const full_engine::LoadedAnimationSampleResult sample =
        full_engine::sampleLoadedAnimationClip(import.skeleton, import.animationClip, 0.0f);
    const full_engine::RigidNodeAttachmentDrawResult draws =
        full_engine::buildRigidNodeAttachmentDraws(
            uploadableAttachments.data(),
            uploadableAttachments.size(),
            handles,
            sample.pose,
            full_renderer::MaterialHandle{901});
    expect(draws.summary.builtCount == 1, "filtered draw path emits one retained draw", failures);
}

void testStatusNames(std::vector<std::string>& failures)
{
    expect(std::string(full_engine::assimpRigidNodeSceneImportStatusName(full_engine::AssimpRigidNodeSceneImportStatus::Success)) == "Success", "scene status name covers success", failures);
    expect(std::string(full_engine::assimpRigidNodeSceneImportStatusName(full_engine::AssimpRigidNodeSceneImportStatus::InvalidArgument)) == "InvalidArgument", "scene status name covers invalid argument", failures);
    expect(std::string(full_engine::assimpRigidNodeSceneImportStatusName(full_engine::AssimpRigidNodeSceneImportStatus::IoError)) == "IoError", "scene status name covers io", failures);
    expect(std::string(full_engine::assimpRigidNodeSceneImportStatusName(full_engine::AssimpRigidNodeSceneImportStatus::ParseError)) == "ParseError", "scene status name covers parse", failures);
    expect(std::string(full_engine::assimpRigidNodeSceneImportStatusName(full_engine::AssimpRigidNodeSceneImportStatus::UnsupportedScene)) == "UnsupportedScene", "scene status name covers unsupported", failures);
    expect(std::string(full_engine::assimpRigidNodeSceneImportStatusName(full_engine::AssimpRigidNodeSceneImportStatus::PayloadValidationFailed)) == "PayloadValidationFailed", "scene status name covers payload", failures);

    expect(std::string(full_engine::assimpRigidNodeMeshStatusName(full_engine::AssimpRigidNodeMeshStatus::Imported)) == "Imported", "mesh status name covers imported", failures);
    expect(std::string(full_engine::assimpRigidNodeMeshStatusName(full_engine::AssimpRigidNodeMeshStatus::MissingTangents)) == "MissingTangents", "mesh status name covers tangents", failures);
    expect(std::string(full_engine::rigidNodeAttachmentDrawStatusName(full_engine::RigidNodeAttachmentDrawStatus::Built)) == "Built", "draw status name covers built", failures);
    expect(std::string(full_engine::rigidNodeAttachmentDrawStatusName(full_engine::RigidNodeAttachmentDrawStatus::JointOutOfRange)) == "JointOutOfRange", "draw status name covers joint range", failures);
}

bool optionalFbxEnabled() noexcept
{
    const char* const enabled = std::getenv("FULL_RENDERER_RUN_OPTIONAL_FBX_PROBES");
    return enabled != nullptr && std::string(enabled) == "1";
}

void writeOptionalReport(
    const char* const name,
    const std::string& body)
{
    const char* const reportDir = std::getenv("FULL_RENDERER_OPTIONAL_FBX_PROBE_REPORT_DIR");
    if (reportDir == nullptr || std::string(reportDir).empty())
    {
        return;
    }

    const std::string path = std::string(reportDir) + "/" + name;
    std::ofstream output(path);
    output << body;
}

void testOptionalKnightRigidImport(std::vector<std::string>& failures)
{
    if (!optionalFbxEnabled())
    {
        std::cout << "Optional Knight rigid import skipped; set FULL_RENDERER_RUN_OPTIONAL_FBX_PROBES=1 to enable.\n";
        return;
    }

    full_engine::AssimpRigidNodeSceneImportOptions options;
    options.path = std::string(FULL_RENDERER_TEST_OPTIONAL_ASSET_DIR) +
        "/pkg_e_knight_anim/pkg_e_knight_anim/Exports/FBX/Knight_USD_002.fbx";
    options.skeletonAssetId = asset(9000);
    options.animationClipAssetId = asset(9001);
    options.firstMeshAssetId = asset(9100);
    options.assimp.generateMissingTangents = true;
    options.assimp.generateMissingNormals = true;
    options.assimp.defaultMissingUv0ToZero = true;

    const full_engine::AssimpRigidNodeSceneImportResult result =
        full_engine::importRigidNodeSceneWithAssimp(options);
    writeOptionalReport(
        "Knight_USD.assimp_rigid_node_import.json",
        full_engine::formatAssimpRigidNodeSceneImportResultJson(result));
    if (result.status == full_engine::AssimpRigidNodeSceneImportStatus::IoError)
    {
        std::cout << "Optional Knight rigid import skipped; asset is not present.\n";
        return;
    }

    std::cout << "Optional Knight rigid import status="
              << full_engine::assimpRigidNodeSceneImportStatusName(result.status)
              << " joints=" << result.skeleton.joints.size()
              << " tracks=" << result.animationClip.tracks.size()
              << " attachments=" << result.attachments.size()
              << " skipped=" << result.summary.skippedMeshCount << "\n";
    expect(result.status == full_engine::AssimpRigidNodeSceneImportStatus::Success, "optional Knight rigid import succeeds when enabled and present", failures);
    expect(!result.skeleton.joints.empty(), "optional Knight rigid import has joints", failures);
    expect(!result.animationClip.tracks.empty(), "optional Knight rigid import has animation tracks", failures);
    expect(!result.attachments.empty(), "optional Knight rigid import has mesh attachments", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testRigidNodeImportAndDrawPath(failures);
    testStrictMissingTangentsRejectsFixture(failures);
    testDrawBuilderFailures(failures);
    testUploadContractFilteringKeepsDrawableAttachments(failures);
    testStatusNames(failures);
    testOptionalKnightRigidImport(failures);

    if (!failures.empty())
    {
        for (const std::string& failure : failures)
        {
            std::cerr << failure << "\n";
        }
        return 1;
    }

    return 0;
}
