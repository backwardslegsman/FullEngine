#pragma once

#include "full_renderer/Renderer.hpp"
#include "renderer/core/RenderDevice.hpp"
#include "renderer/debug/TerrainDebug.hpp"

#include <cstdint>
#include <vector>

namespace full_renderer::terrain
{
class TerrainSystem
{
public:
    TerrainChunkHandle createChunk(const TerrainChunkDesc& desc);
    RendererResult updateChunk(TerrainChunkHandle handle, const TerrainChunkDesc& desc);
    void destroyChunk(TerrainChunkHandle handle) noexcept;
    void shutdown() noexcept;

    RendererResult buildDrawItems(
        const RenderViewDesc& view,
        const TerrainSubmitDesc& submit,
        std::vector<DrawItem>& outDrawItems);
    RendererResult buildDrawBatches(
        const RenderViewDesc& view,
        const TerrainSubmitDesc& submit,
        std::vector<float>& outModelMatrices,
        std::vector<core::InstancedDrawBatch>& outBatches);
    RendererResult buildShadowCasterBatches(
        const RenderViewDesc& cameraView,
        const TerrainSubmitDesc& submit,
        const DirectionalLightDesc& light,
        const DirectionalShadowDesc& shadow,
        std::vector<float>& outModelMatrices,
        std::vector<core::InstancedDrawBatch>& outBatches);

    TerrainStats getStats() const noexcept;
    std::uint32_t copyDebugInfo(TerrainChunkDebugInfo* outItems, std::uint32_t maxItems) const noexcept;
    std::uint32_t copyBatchDebugInfo(TerrainBatchDebugInfo* outItems, std::uint32_t maxItems) const noexcept;
    std::uint32_t copyShadowCasterDebugInfo(TerrainChunkDebugInfo* outItems, std::uint32_t maxItems) const noexcept;

private:
    struct PendingBatch
    {
        MeshHandle mesh;
        MaterialHandle material;
        TextureHandle splatMap;
        std::uint32_t lodIndex = kInvalidTerrainLodIndex;
        std::uint32_t cascadeIndex = kInvalidShadowCascadeIndex;
        Aabb bounds;
        std::vector<float> matrices;
    };

    struct ChunkRecord
    {
        std::uint32_t generation = 0;
        bool active = false;
        Aabb bounds;
        TextureHandle splatMap;
        std::vector<TerrainLodDesc> lods;
    };

    ChunkRecord* find(TerrainChunkHandle handle) noexcept;
    const ChunkRecord* find(TerrainChunkHandle handle) const noexcept;
    void updateLiveChunkCount() noexcept;
    void resetFrameStats(std::uint32_t submittedChunks) noexcept;
    void resetPendingBatches() noexcept;
    void recordInvalidSubmittedHandle(TerrainChunkHandle handle) noexcept;
    void writeResidencyStats() noexcept;
    void clearPendingResidencyChurn() noexcept;

    std::vector<ChunkRecord> chunks_;
    std::vector<PendingBatch> pendingBatches_;
    std::uint32_t pendingBatchCount_ = 0;
    std::uint32_t chunksCreatedSinceLastSubmit_ = 0;
    std::uint32_t chunksDestroyedSinceLastSubmit_ = 0;
    std::uint32_t chunksUpdatedSinceLastSubmit_ = 0;
    std::uint32_t chunkSlotsReusedSinceLastSubmit_ = 0;
    std::uint32_t totalChunkCreateCount_ = 0;
    std::uint32_t totalChunkDestroyCount_ = 0;
    std::uint32_t totalChunkUpdateCount_ = 0;
    std::uint32_t totalChunkSlotReuseCount_ = 0;
    TerrainStats stats_;
    debug::TerrainDebugSnapshot debugSnapshot_;
    debug::TerrainDebugSnapshot shadowCasterDebugSnapshot_;
    debug::TerrainBatchDebugSnapshot batchDebugSnapshot_;
};
} // namespace full_renderer::terrain
