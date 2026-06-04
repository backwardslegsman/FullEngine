#include "engine/assets/LoadedTextureImageImporter.hpp"
#include "engine/renderer_integration/LoadedAssetUploadExecutor.hpp"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#ifndef FULL_RENDERER_TEST_IMAGE_FIXTURE_DIR
#define FULL_RENDERER_TEST_IMAGE_FIXTURE_DIR "."
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

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

std::string fixturePath(const char* const name)
{
    return std::string(FULL_RENDERER_TEST_IMAGE_FIXTURE_DIR) + "/" + name;
}

std::string malformedFixture()
{
    return fixturePath("malformed.png");
}

full_engine::AssetSourceRecord textureSource(
    const std::string& uri,
    const std::uint32_t width,
    const std::uint32_t height,
    const full_engine::AssetSourceTextureSemantic semantic = full_engine::AssetSourceTextureSemantic::Color,
    const full_engine::AssetSourceTextureColorSpace colorSpace = full_engine::AssetSourceTextureColorSpace::Srgb)
{
    full_engine::AssetSourceRecord record;
    record.id = asset(700);
    record.kind = full_engine::AssetKind::Texture;
    record.uri = uri;
    record.descriptor.texture.width = width;
    record.descriptor.texture.height = height;
    record.descriptor.texture.mipCount = 1;
    record.descriptor.texture.format = full_engine::AssetSourceTextureFormat::Rgba8;
    record.descriptor.texture.semantic = semantic;
    record.descriptor.texture.colorSpace = colorSpace;
    return record;
}

full_engine::AssetSourceRecord meshSource(const std::string& uri)
{
    full_engine::AssetSourceRecord record;
    record.id = asset(701);
    record.kind = full_engine::AssetKind::Mesh;
    record.uri = uri;
    record.descriptor.mesh.vertexCount = 3;
    record.descriptor.mesh.indexCount = 3;
    record.descriptor.mesh.localBounds.max[0] = 1.0f;
    record.descriptor.mesh.localBounds.max[1] = 1.0f;
    return record;
}

class FakeRenderer final : public full_renderer::IRenderer
{
public:
    full_renderer::TextureHandle nextTexture = {901};
    int textureCreateCalls = 0;
    std::uint32_t lastTextureWidth = 0;

    full_renderer::RendererResult initialize(const full_renderer::RendererInitDesc&) override { return full_renderer::RendererResult::Success; }
    void shutdown() noexcept override {}
    bool isInitialized() const noexcept override { return true; }
    full_renderer::MeshHandle createMesh(const full_renderer::MeshDesc&) override { return {}; }
    void destroyMesh(full_renderer::MeshHandle) noexcept override {}
    full_renderer::SkeletonHandle createSkeleton(const full_renderer::SkeletonDesc&) override { return {}; }
    void destroySkeleton(full_renderer::SkeletonHandle) noexcept override {}
    full_renderer::SkinnedMeshHandle createSkinnedMesh(const full_renderer::SkinnedMeshDesc&) override { return {}; }
    void destroySkinnedMesh(full_renderer::SkinnedMeshHandle) noexcept override {}
    full_renderer::TextureHandle createTexture(const full_renderer::TextureDesc& desc) override
    {
        ++textureCreateCalls;
        lastTextureWidth = desc.width;
        return nextTexture;
    }
    void destroyTexture(full_renderer::TextureHandle) noexcept override {}
    full_renderer::MaterialHandle createMaterial(const full_renderer::MaterialDesc&) override { return {}; }
    void destroyMaterial(full_renderer::MaterialHandle) noexcept override {}
    full_renderer::TerrainChunkHandle createTerrainChunk(const full_renderer::TerrainChunkDesc&) override { return {}; }
    full_renderer::RendererResult updateTerrainChunk(full_renderer::TerrainChunkHandle, const full_renderer::TerrainChunkDesc&) override { return full_renderer::RendererResult::Success; }
    void destroyTerrainChunk(full_renderer::TerrainChunkHandle) noexcept override {}
    full_renderer::RendererResult resize(const full_renderer::RendererResizeDesc&) override { return full_renderer::RendererResult::Success; }
    full_renderer::RendererResult beginFrame(const full_renderer::FrameDesc&) override { return full_renderer::RendererResult::Success; }
    full_renderer::RendererResult submit(const full_renderer::RenderPacket&) override { return full_renderer::RendererResult::Success; }
    full_renderer::RendererResult endFrame() override { return full_renderer::RendererResult::Success; }
    full_renderer::RendererStats getStats() const noexcept override { return {}; }
    full_renderer::TerrainStats getTerrainStats() const noexcept override { return {}; }
    std::uint32_t copyTerrainDebugInfo(full_renderer::TerrainChunkDebugInfo*, std::uint32_t) const noexcept override { return 0; }
    std::uint32_t copyTerrainBatchDebugInfo(full_renderer::TerrainBatchDebugInfo*, std::uint32_t) const noexcept override { return 0; }
    std::uint32_t copyTerrainShadowCasterDebugInfo(full_renderer::TerrainChunkDebugInfo*, std::uint32_t) const noexcept override { return 0; }
};

void testValidRgbaPngImport(std::vector<std::string>& failures)
{
    const std::string path = fixturePath("rgba_2x2.png");
    const full_engine::LoadedTextureImageImportResult result =
        full_engine::importLoadedTexturePayloadFromImageFile(textureSource(path, 2, 2));

    expect(result.status == full_engine::LoadedTextureImageImportStatus::Success, "rgba png imports", failures);
    expect(result.payload.kind == full_engine::AssetKind::Texture, "rgba png imports texture payload", failures);
    expect(result.payload.texture.id == asset(700), "rgba png preserves asset id", failures);
    expect(result.payload.texture.width == 2 && result.payload.texture.height == 2, "rgba png dimensions match", failures);
    expect(result.payload.texture.mipCount == 1, "rgba png imports one mip", failures);
    expect(result.payload.texture.format == full_engine::AssetSourceTextureFormat::Rgba8, "rgba png imports rgba8", failures);
    expect(result.payload.texture.semantic == full_engine::AssetSourceTextureSemantic::Color, "rgba png copies semantic", failures);
    expect(result.payload.texture.colorSpace == full_engine::AssetSourceTextureColorSpace::Srgb, "rgba png copies color space", failures);
    expect(result.payload.texture.bytes.size() == 16, "rgba png byte count matches", failures);
    expect(full_engine::validateLoadedAssetPayload(result.payload) == full_engine::LoadedAssetPayloadValidationResult::Success, "rgba png payload validates", failures);
}

void testRgbExpandsToRgba(std::vector<std::string>& failures)
{
    const std::string path = fixturePath("rgb_2x1.png");
    const full_engine::LoadedTextureImageImportResult result =
        full_engine::importLoadedTexturePayloadFromImageFile(textureSource(path, 2, 1));

    expect(result.status == full_engine::LoadedTextureImageImportStatus::Success, "rgb png imports", failures);
    expect(result.payload.texture.bytes.size() == 8, "rgb png expands to rgba byte count", failures);
    expect(result.payload.texture.bytes[0] == 10 && result.payload.texture.bytes[1] == 20 && result.payload.texture.bytes[2] == 30 && result.payload.texture.bytes[3] == 255, "first rgb pixel expands with opaque alpha", failures);
    expect(result.payload.texture.bytes[4] == 40 && result.payload.texture.bytes[5] == 50 && result.payload.texture.bytes[6] == 60 && result.payload.texture.bytes[7] == 255, "second rgb pixel expands with opaque alpha", failures);
}

void testFailures(std::vector<std::string>& failures)
{
    const std::string path = fixturePath("rgba_2x2.png");

    full_engine::AssetSourceRecord invalid = textureSource(path, 2, 2);
    invalid.id = {};
    const full_engine::LoadedTextureImageImportResult invalidSource =
        full_engine::importLoadedTexturePayloadFromImageFile(invalid);
    expect(invalidSource.status == full_engine::LoadedTextureImageImportStatus::SourceValidationFailed, "invalid source is rejected", failures);

    const full_engine::LoadedTextureImageImportResult unsupported =
        full_engine::importLoadedTexturePayloadFromImageFile(meshSource(path));
    expect(unsupported.status == full_engine::LoadedTextureImageImportStatus::UnsupportedKind, "unsupported source kind is rejected", failures);

    const full_engine::LoadedTextureImageImportResult missing =
        full_engine::importLoadedTexturePayloadFromImageFile(textureSource(path + ".missing", 2, 2));
    expect(missing.status == full_engine::LoadedTextureImageImportStatus::IoError, "missing image reports io error", failures);

    const full_engine::LoadedTextureImageImportResult malformed =
        full_engine::importLoadedTexturePayloadFromImageFile(textureSource(malformedFixture(), 2, 2));
    expect(malformed.status == full_engine::LoadedTextureImageImportStatus::DecodeError, "malformed image reports decode error", failures);

    full_engine::AssetSourceRecord widthMismatch = textureSource(path, 1, 2);
    const full_engine::LoadedTextureImageImportResult badWidth =
        full_engine::importLoadedTexturePayloadFromImageFile(widthMismatch);
    expect(badWidth.status == full_engine::LoadedTextureImageImportStatus::DescriptorMismatch, "width mismatch is reported", failures);

    full_engine::AssetSourceRecord heightMismatch = textureSource(path, 2, 1);
    const full_engine::LoadedTextureImageImportResult badHeight =
        full_engine::importLoadedTexturePayloadFromImageFile(heightMismatch);
    expect(badHeight.status == full_engine::LoadedTextureImageImportStatus::DescriptorMismatch, "height mismatch is reported", failures);

    full_engine::AssetSourceRecord mipMismatch = textureSource(path, 2, 2);
    mipMismatch.descriptor.texture.mipCount = 2;
    const full_engine::LoadedTextureImageImportResult badMip =
        full_engine::importLoadedTexturePayloadFromImageFile(mipMismatch);
    expect(badMip.status == full_engine::LoadedTextureImageImportStatus::DescriptorMismatch, "mip mismatch is reported", failures);

    full_engine::AssetSourceRecord invalidFormat = textureSource(path, 2, 2);
    invalidFormat.descriptor.texture.format = full_engine::AssetSourceTextureFormat::Unknown;
    const full_engine::LoadedTextureImageImportResult badFormat =
        full_engine::importLoadedTexturePayloadFromImageFile(invalidFormat);
    expect(badFormat.status == full_engine::LoadedTextureImageImportStatus::SourceValidationFailed, "invalid texture format fails source validation", failures);
}

void testUploadPlanAndExecutor(std::vector<std::string>& failures)
{
    const std::string path = fixturePath("rgb_2x1.png");
    const full_engine::LoadedTextureImageImportResult imported =
        full_engine::importLoadedTexturePayloadFromImageFile(textureSource(path, 2, 1));
    const full_engine::LoadedAssetUploadPlan plan =
        full_engine::buildLoadedAssetUploadPlan(&imported.payload, 1);

    FakeRenderer renderer;
    full_engine::RendererAssetHandleCatalog handles;
    const full_engine::LoadedAssetUploadExecuteResult executed =
        full_engine::executeLoadedAssetUploadPlan(renderer, plan, handles);

    expect(plan.records.size() == 1 && plan.records[0].status == full_engine::LoadedAssetUploadStatus::Planned, "imported texture builds upload work", failures);
    expect(executed.records.size() == 1 && executed.records[0].status == full_engine::LoadedAssetUploadExecuteStatus::Uploaded, "imported texture upload executes", failures);
    expect(renderer.textureCreateCalls == 1 && renderer.lastTextureWidth == 2, "imported texture upload calls renderer with descriptor", failures);
    expect(handles.findTextureHandle(asset(700)) != nullptr && handles.findTextureHandle(asset(700))->id == 901, "imported texture upload catalogs handle", failures);
}

void testStatusNames(std::vector<std::string>& failures)
{
    const full_engine::LoadedTextureImageImportStatus statuses[] = {
        full_engine::LoadedTextureImageImportStatus::Success,
        full_engine::LoadedTextureImageImportStatus::InvalidArgument,
        full_engine::LoadedTextureImageImportStatus::SourceValidationFailed,
        full_engine::LoadedTextureImageImportStatus::IoError,
        full_engine::LoadedTextureImageImportStatus::DecodeError,
        full_engine::LoadedTextureImageImportStatus::DescriptorMismatch,
        full_engine::LoadedTextureImageImportStatus::PayloadValidationFailed,
        full_engine::LoadedTextureImageImportStatus::UnsupportedKind};
    for (const full_engine::LoadedTextureImageImportStatus status : statuses)
    {
        expect(std::string(full_engine::loadedTextureImageImportStatusName(status)) != "Unknown", "texture image import status has stable name", failures);
    }
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testValidRgbaPngImport(failures);
    testRgbExpandsToRgba(failures);
    testFailures(failures);
    testUploadPlanAndExecutor(failures);
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
