#pragma once

#include "full_renderer/Renderer.hpp"

#include <memory>

namespace full_renderer::bgfx_backend
{
struct ShadowPreviewTextureInfo;
}

namespace full_renderer::core
{
class RenderDevice;
} // namespace full_renderer::core

namespace full_renderer::terrain
{
class TerrainSystem;
}

namespace full_renderer::animation
{
class AnimationSystem;
}

namespace full_renderer::core
{

class Renderer final : public IRenderer
{
public:
    explicit Renderer(std::unique_ptr<RenderDevice> device) noexcept;
    ~Renderer() override;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    RendererResult initialize(const RendererInitDesc& desc) override;
    void shutdown() noexcept override;
    bool isInitialized() const noexcept override;
    RendererResult resize(const RendererResizeDesc& desc) override;
    MeshHandle createMesh(const MeshDesc& desc) override;
    void destroyMesh(MeshHandle handle) noexcept override;
    SkeletonHandle createSkeleton(const SkeletonDesc& desc) override;
    void destroySkeleton(SkeletonHandle handle) noexcept override;
    SkinnedMeshHandle createSkinnedMesh(const SkinnedMeshDesc& desc) override;
    void destroySkinnedMesh(SkinnedMeshHandle handle) noexcept override;
    TextureHandle createTexture(const TextureDesc& desc) override;
    void destroyTexture(TextureHandle handle) noexcept override;
    MaterialHandle createMaterial(const MaterialDesc& desc) override;
    void destroyMaterial(MaterialHandle handle) noexcept override;
    TerrainChunkHandle createTerrainChunk(const TerrainChunkDesc& desc) override;
    RendererResult updateTerrainChunk(TerrainChunkHandle handle, const TerrainChunkDesc& desc) override;
    void destroyTerrainChunk(TerrainChunkHandle handle) noexcept override;
    RendererResult beginFrame(const FrameDesc& desc) override;
    RendererResult submit(const RenderPacket& packet) override;
    RendererResult endFrame() override;
    RendererStats getStats() const noexcept override;
    TerrainStats getTerrainStats() const noexcept override;
    std::uint32_t copyTerrainDebugInfo(
        TerrainChunkDebugInfo* outItems,
        std::uint32_t maxItems) const noexcept override;
    std::uint32_t copyTerrainBatchDebugInfo(
        TerrainBatchDebugInfo* outItems,
        std::uint32_t maxItems) const noexcept override;
    std::uint32_t copyTerrainShadowCasterDebugInfo(
        TerrainChunkDebugInfo* outItems,
        std::uint32_t maxItems) const noexcept override;
    bgfx_backend::ShadowPreviewTextureInfo getShadowPreviewTexture(std::uint32_t cascadeIndex) const noexcept;

private:
    std::unique_ptr<RenderDevice> device_;
    std::unique_ptr<full_renderer::terrain::TerrainSystem> terrain_;
    std::unique_ptr<full_renderer::animation::AnimationSystem> animation_;
    CullingCategoryStats lastStaticMeshCullingStats_;
    CullingCategoryStats lastInstancedMeshCullingStats_;
    CullingCategoryStats lastSkinnedMeshCullingStats_;
    FrameBudgetStats frameBudgetStats_;
    std::uint32_t lastAnimationDebugLineVertices_ = 0;
    bool initialized_ = false;
    bool frameInProgress_ = false;
};
} // namespace full_renderer::core
