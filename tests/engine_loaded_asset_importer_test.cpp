#include "engine/assets/LoadedAssetImporter.hpp"
#include "engine/renderer_integration/LoadedAssetUploadExecutor.hpp"

#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef FULL_RENDERER_TEST_FIXTURE_DIR
#define FULL_RENDERER_TEST_FIXTURE_DIR "."
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
    return std::string(FULL_RENDERER_TEST_FIXTURE_DIR) + "/" + name;
}

void writeFile(const char* const path, const char* const contents)
{
    std::ofstream output(path, std::ios::out | std::ios::trunc);
    output << contents;
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

full_engine::AssetSourceDescriptor textureDescriptor()
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.texture.width = 2;
    descriptor.texture.height = 2;
    descriptor.texture.mipCount = 1;
    descriptor.texture.format = full_engine::AssetSourceTextureFormat::Rgba8;
    descriptor.texture.semantic = full_engine::AssetSourceTextureSemantic::Color;
    descriptor.texture.colorSpace = full_engine::AssetSourceTextureColorSpace::Srgb;
    return descriptor;
}

full_engine::AssetSourceDescriptor materialDescriptor()
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.material.model = full_engine::AssetSourceMaterialModel::Basic;
    descriptor.material.alphaMode = full_engine::AssetSourceMaterialAlphaMode::Opaque;
    descriptor.material.textureRefs[0] = {full_engine::AssetSourceMaterialTextureSlot::BaseColor, asset(20)};
    descriptor.material.textureRefCount = 1;
    return descriptor;
}

full_engine::AssetSourceRecord source(
    const std::uint64_t id,
    const full_engine::AssetKind kind,
    const std::string& uri,
    const full_engine::AssetSourceDescriptor& descriptor)
{
    full_engine::AssetSourceRecord record;
    record.id = asset(id);
    record.kind = kind;
    record.uri = uri;
    record.descriptor = descriptor;
    return record;
}

class FakeRenderer final : public full_renderer::IRenderer
{
public:
    int meshCreateCalls = 0;
    int textureCreateCalls = 0;
    full_renderer::MeshHandle nextMesh = {1001};
    full_renderer::TextureHandle nextTexture = {2001};

    full_renderer::RendererResult initialize(const full_renderer::RendererInitDesc&) override
    {
        return full_renderer::RendererResult::Success;
    }

    void shutdown() noexcept override {}

    bool isInitialized() const noexcept override
    {
        return true;
    }

    full_renderer::MeshHandle createMesh(const full_renderer::MeshDesc&) override
    {
        ++meshCreateCalls;
        return nextMesh;
    }

    void destroyMesh(full_renderer::MeshHandle) noexcept override {}

    full_renderer::SkeletonHandle createSkeleton(const full_renderer::SkeletonDesc&) override
    {
        return {};
    }

    void destroySkeleton(full_renderer::SkeletonHandle) noexcept override {}

    full_renderer::SkinnedMeshHandle createSkinnedMesh(const full_renderer::SkinnedMeshDesc&) override
    {
        return {};
    }

    void destroySkinnedMesh(full_renderer::SkinnedMeshHandle) noexcept override {}

    full_renderer::TextureHandle createTexture(const full_renderer::TextureDesc&) override
    {
        ++textureCreateCalls;
        return nextTexture;
    }

    void destroyTexture(full_renderer::TextureHandle) noexcept override {}

    full_renderer::MaterialHandle createMaterial(const full_renderer::MaterialDesc&) override
    {
        return {};
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

void testValidFixtureImports(std::vector<std::string>& failures)
{
    const full_engine::LoadedAssetImportResult mesh =
        full_engine::importLoadedAssetPayloadFromDevFile(source(
            10,
            full_engine::AssetKind::Mesh,
            fixturePath("dev_triangle.fmeshdev"),
            meshDescriptor()));
    expect(mesh.status == full_engine::LoadedAssetImportStatus::Success, "mesh fixture imports", failures);
    expect(mesh.payload.kind == full_engine::AssetKind::Mesh, "mesh import sets payload kind", failures);
    expect(mesh.payload.mesh.vertices.size() == 3, "mesh import copies vertex count", failures);
    expect(mesh.payload.mesh.indices.size() == 3, "mesh import copies index count", failures);
    expect(mesh.payload.mesh.vertices[1].position[0] == 1.0f, "mesh import copies vertex data", failures);

    const full_engine::LoadedAssetImportResult texture =
        full_engine::importLoadedAssetPayloadFromDevFile(source(
            20,
            full_engine::AssetKind::Texture,
            fixturePath("dev_checker_2x2.ftexdev"),
            textureDescriptor()));
    expect(texture.status == full_engine::LoadedAssetImportStatus::Success, "texture fixture imports", failures);
    expect(texture.payload.kind == full_engine::AssetKind::Texture, "texture import sets payload kind", failures);
    expect(texture.payload.texture.bytes.size() == 16, "texture import copies byte count", failures);
    expect(texture.payload.texture.bytes[1] == 0, "texture import copies byte data", failures);
    expect(texture.payload.texture.bytes[15] == 255, "texture import copies final byte", failures);

    const full_engine::LoadedAssetImportResult material =
        full_engine::importLoadedAssetPayloadFromDevFile(source(
            30,
            full_engine::AssetKind::Material,
            fixturePath("dev_basic.fmatdev"),
            materialDescriptor()));
    expect(material.status == full_engine::LoadedAssetImportStatus::Success, "material fixture imports", failures);
    expect(material.payload.kind == full_engine::AssetKind::Material, "material import sets payload kind", failures);
    expect(material.payload.material.textureRefCount == 1, "material import copies texture ref count", failures);
    expect(
        material.payload.material.textureRefs[0].slot == full_engine::AssetSourceMaterialTextureSlot::BaseColor &&
            material.payload.material.textureRefs[0].id == asset(20),
        "material import copies named texture asset ref",
        failures);
}

void testFailureStatuses(std::vector<std::string>& failures)
{
    full_engine::AssetSourceRecord invalid = source(
        10,
        full_engine::AssetKind::Mesh,
        fixturePath("dev_triangle.fmeshdev"),
        meshDescriptor());
    invalid.id = {};
    expect(
        full_engine::importLoadedAssetPayloadFromDevFile(invalid).status ==
            full_engine::LoadedAssetImportStatus::SourceValidationFailed,
        "invalid source reports source validation failure",
        failures);

    expect(
        full_engine::importLoadedAssetPayloadFromDevFile(source(
            10,
            full_engine::AssetKind::Mesh,
            fixturePath("missing.fmeshdev"),
            meshDescriptor())).status == full_engine::LoadedAssetImportStatus::IoError,
        "missing source reports io error",
        failures);

    expect(
        full_engine::importLoadedAssetPayloadFromDevFile(source(
            10,
            full_engine::AssetKind::Shader,
            fixturePath("dev_triangle.fmeshdev"),
            meshDescriptor())).status == full_engine::LoadedAssetImportStatus::UnsupportedKind,
        "unsupported source kind reports unsupported",
        failures);

    writeFile("loaded_asset_importer_malformed.fmeshdev", "fmeshdev 1\nbogus\n");
    expect(
        full_engine::importLoadedAssetPayloadFromDevFile(source(
            10,
            full_engine::AssetKind::Mesh,
            "loaded_asset_importer_malformed.fmeshdev",
            meshDescriptor())).status == full_engine::LoadedAssetImportStatus::ParseError,
        "malformed source reports parse error",
        failures);
}

void testDescriptorAndPayloadFailures(std::vector<std::string>& failures)
{
    full_engine::AssetSourceDescriptor mismatched = meshDescriptor();
    mismatched.mesh.vertexCount = 4;
    expect(
        full_engine::importLoadedAssetPayloadFromDevFile(source(
            10,
            full_engine::AssetKind::Mesh,
            fixturePath("dev_triangle.fmeshdev"),
            mismatched)).status == full_engine::LoadedAssetImportStatus::DescriptorMismatch,
        "descriptor mismatch is reported",
        failures);

    writeFile(
        "loaded_asset_importer_bad_payload.fmeshdev",
        "fmeshdev 1\n"
        "counts 3 3\n"
        "bounds 0 0 0 1 1 0\n"
        "v 0 0 0 0 0 0 1 0 0 1\n"
        "v 1 0 0 0 1 0 0 1 0 1\n"
        "v 0 1 0 0 1 0 0 0 1 1\n"
        "i 0 1 2\n");
    const full_engine::LoadedAssetImportResult badPayload =
        full_engine::importLoadedAssetPayloadFromDevFile(source(
            10,
            full_engine::AssetKind::Mesh,
            "loaded_asset_importer_bad_payload.fmeshdev",
            meshDescriptor()));
    expect(
        badPayload.status == full_engine::LoadedAssetImportStatus::PayloadValidationFailed,
        "payload validation failure is reported",
        failures);
    expect(
        badPayload.payloadValidation ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshVertexData,
        "payload validation detail is preserved",
        failures);
}

void testImportToUploadExecutorFlow(std::vector<std::string>& failures)
{
    const full_engine::LoadedAssetImportResult mesh =
        full_engine::importLoadedAssetPayloadFromDevFile(source(
            10,
            full_engine::AssetKind::Mesh,
            fixturePath("dev_triangle.fmeshdev"),
            meshDescriptor()));
    const full_engine::LoadedAssetImportResult texture =
        full_engine::importLoadedAssetPayloadFromDevFile(source(
            20,
            full_engine::AssetKind::Texture,
            fixturePath("dev_checker_2x2.ftexdev"),
            textureDescriptor()));
    const std::vector<full_engine::LoadedAssetPayload> payloads = {
        mesh.payload,
        texture.payload};

    const full_engine::LoadedAssetUploadPlan uploadPlan =
        full_engine::buildLoadedAssetUploadPlan(payloads.data(), payloads.size());
    expect(uploadPlan.summary.plannedCount == 2, "imported payloads plan upload work", failures);

    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog handles;
    const full_engine::LoadedAssetUploadExecuteResult upload =
        full_engine::executeLoadedAssetUploadPlan(renderer, uploadPlan, handles);

    expect(upload.summary.uploadedMeshCount == 1, "imported mesh uploads", failures);
    expect(upload.summary.uploadedTextureCount == 1, "imported texture uploads", failures);
    expect(renderer.meshCreateCalls == 1, "end-to-end flow calls mesh upload", failures);
    expect(renderer.textureCreateCalls == 1, "end-to-end flow calls texture upload", failures);
    expect(handles.findMeshHandle(asset(10)) != nullptr, "end-to-end flow catalogs mesh handle", failures);
    expect(handles.findTextureHandle(asset(20)) != nullptr, "end-to-end flow catalogs texture handle", failures);
}

void testStatusNames(std::vector<std::string>& failures)
{
    const full_engine::LoadedAssetImportStatus statuses[] = {
        full_engine::LoadedAssetImportStatus::Success,
        full_engine::LoadedAssetImportStatus::InvalidArgument,
        full_engine::LoadedAssetImportStatus::SourceValidationFailed,
        full_engine::LoadedAssetImportStatus::IoError,
        full_engine::LoadedAssetImportStatus::ParseError,
        full_engine::LoadedAssetImportStatus::DescriptorMismatch,
        full_engine::LoadedAssetImportStatus::PayloadValidationFailed,
        full_engine::LoadedAssetImportStatus::UnsupportedKind};
    for (const full_engine::LoadedAssetImportStatus status : statuses)
    {
        expect(
            std::string(full_engine::loadedAssetImportStatusName(status)) != "Unknown",
            "import status has stable name",
            failures);
    }
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testValidFixtureImports(failures);
    testFailureStatuses(failures);
    testDescriptorAndPayloadFailures(failures);
    testImportToUploadExecutorFlow(failures);
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
