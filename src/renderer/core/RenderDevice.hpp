#pragma once

#include "full_renderer/Renderer.hpp"
#include "renderer/debug/TerrainDebug.hpp"

namespace full_renderer::core
{
enum class ShadowCasterKind
{
    None,
    Terrain,
    StaticMesh,
    InstancedMesh,
    SkinnedMesh,
};

struct InstancedDrawBatch
{
    MeshHandle mesh;
    MaterialHandle material;
    TextureHandle splatMap;
    Aabb bounds;
    const float* modelMatrices = nullptr;
    std::uint32_t instanceCount = 0;
    bool selected = false;
    FadeDesc fade;
    std::uint32_t lodIndex = kInvalidTerrainLodIndex;
    std::uint32_t shadowCascadeIndex = kInvalidShadowCascadeIndex;
    ShadowCasterKind shadowCasterKind = ShadowCasterKind::None;
};

struct SkinnedShadowDrawBatch
{
    SkinnedMeshHandle mesh;
    MaterialHandle material;
    std::uint32_t sectionIndex = 0;
    Aabb bounds;
    const float* model = nullptr;
    SkinningPaletteDesc palette;
    std::uint32_t shadowCascadeIndex = kInvalidShadowCascadeIndex;
};

class RenderDevice
{
public:
    virtual ~RenderDevice() = default;

    virtual RendererResult initialize(const RendererInitDesc& desc) = 0;
    virtual void shutdown() noexcept = 0;
    virtual RendererResult resize(const RendererResizeDesc& desc) = 0;
    virtual MeshHandle createMesh(const MeshDesc& desc) = 0;
    virtual void destroyMesh(MeshHandle handle) noexcept = 0;
    virtual SkinnedMeshHandle createSkinnedMesh(const SkinnedMeshDesc& desc) = 0;
    virtual void destroySkinnedMesh(SkinnedMeshHandle handle) noexcept = 0;
    virtual TextureHandle createTexture(const TextureDesc& desc) = 0;
    virtual void destroyTexture(TextureHandle handle) noexcept = 0;
    virtual MaterialHandle createMaterial(const MaterialDesc& desc) = 0;
    virtual void destroyMaterial(MaterialHandle handle) noexcept = 0;
    virtual RendererResult beginFrame(const FrameDesc& desc) = 0;
    virtual RendererResult submit(const RenderPacket& packet) = 0;
    virtual RendererResult submitInstanced(
        const RenderPacket& packet,
        const InstancedDrawBatch* batches,
        std::uint32_t batchCount,
        const InstancedDrawBatch* shadowBatches,
        std::uint32_t shadowBatchCount,
        const SkinnedShadowDrawBatch* skinnedShadowBatches,
        std::uint32_t skinnedShadowBatchCount) = 0;
    virtual RendererResult submitDebugLines(
        const RenderViewDesc& view,
        const debug::DebugLineVertex* lines,
        std::uint32_t lineVertexCount) = 0;
    virtual RendererResult endFrame() = 0;
    virtual RendererStats getStats() const noexcept = 0;
};
} // namespace full_renderer::core
