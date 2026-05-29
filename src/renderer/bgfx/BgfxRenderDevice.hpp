#pragma once

#include "renderer/core/RenderDevice.hpp"
#include "renderer/scene/ColorGrading.hpp"
#include "renderer/scene/Decal.hpp"
#include "renderer/scene/Fade.hpp"
#include "renderer/scene/Particle.hpp"
#include "renderer/scene/PostPass.hpp"
#include "renderer/scene/Weather.hpp"

#if defined(FULL_RENDERER_ENABLE_BGFX) && FULL_RENDERER_ENABLE_BGFX
#include <bgfx/bgfx.h>
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace full_renderer::bgfx_backend
{
struct ShadowPreviewTextureInfo
{
#if defined(FULL_RENDERER_ENABLE_BGFX) && FULL_RENDERER_ENABLE_BGFX
    bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
#endif
    std::uint32_t resolution = 0;
    std::uint32_t cascadeCount = 0;
    bool valid = false;
};

class BgfxRenderDevice final : public core::RenderDevice
{
public:
    BgfxRenderDevice() = default;
    ~BgfxRenderDevice() override;

    BgfxRenderDevice(const BgfxRenderDevice&) = delete;
    BgfxRenderDevice& operator=(const BgfxRenderDevice&) = delete;
    BgfxRenderDevice(BgfxRenderDevice&&) = delete;
    BgfxRenderDevice& operator=(BgfxRenderDevice&&) = delete;

    RendererResult initialize(const RendererInitDesc& desc) override;
    void shutdown() noexcept override;
    RendererResult resize(const RendererResizeDesc& desc) override;
    MeshHandle createMesh(const MeshDesc& desc) override;
    void destroyMesh(MeshHandle handle) noexcept override;
    SkinnedMeshHandle createSkinnedMesh(const SkinnedMeshDesc& desc) override;
    void destroySkinnedMesh(SkinnedMeshHandle handle) noexcept override;
    TextureHandle createTexture(const TextureDesc& desc) override;
    void destroyTexture(TextureHandle handle) noexcept override;
    MaterialHandle createMaterial(const MaterialDesc& desc) override;
    void destroyMaterial(MaterialHandle handle) noexcept override;
    RendererResult beginFrame(const FrameDesc& desc) override;
    RendererResult submit(const RenderPacket& packet) override;
    RendererResult submitInstanced(
        const RenderPacket& packet,
        const core::InstancedDrawBatch* batches,
        std::uint32_t batchCount,
        const core::InstancedDrawBatch* shadowBatches,
        std::uint32_t shadowBatchCount,
        const core::SkinnedShadowDrawBatch* skinnedShadowBatches,
        std::uint32_t skinnedShadowBatchCount) override;
    RendererResult submitDebugLines(
        const RenderViewDesc& view,
        const debug::DebugLineVertex* lines,
        std::uint32_t lineVertexCount) override;
    RendererResult endFrame() override;
    RendererStats getStats() const noexcept override;
    ShadowPreviewTextureInfo getShadowPreviewTexture(std::uint32_t cascadeIndex) const noexcept;

private:
    struct MaterialResource
    {
        MaterialDesc desc;
        std::uint64_t estimatedBytes = 0;
        bool active = false;
    };

#if defined(FULL_RENDERER_ENABLE_BGFX) && FULL_RENDERER_ENABLE_BGFX
    struct MeshResource
    {
        bgfx::VertexBufferHandle vertexBuffer = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle indexBuffer = BGFX_INVALID_HANDLE;
        std::uint64_t estimatedBytes = 0;
        bool active = false;
    };

    struct SkinnedMeshResource
    {
        bgfx::VertexBufferHandle vertexBuffer = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle indexBuffer = BGFX_INVALID_HANDLE;
        std::uint64_t estimatedBytes = 0;
        bool active = false;
    };
#else
    struct MeshResource
    {
        std::uint64_t estimatedBytes = 0;
        bool active = false;
    };
    struct SkinnedMeshResource
    {
        std::uint64_t estimatedBytes = 0;
        bool active = false;
    };
#endif

#if defined(FULL_RENDERER_ENABLE_BGFX) && FULL_RENDERER_ENABLE_BGFX
    struct TextureResource
    {
        bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
        TextureSemantic semantic = TextureSemantic::Color;
        TextureColorSpace colorSpace = TextureColorSpace::Srgb;
        std::uint32_t mipCount = 1;
        bool compressed = false;
        std::uint64_t estimatedBytes = 0;
        bool active = false;
    };

    struct ShadowCascadeResource
    {
        bgfx::TextureHandle colorTexture = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle depthTexture = BGFX_INVALID_HANDLE;
        bgfx::FrameBufferHandle frameBuffer = BGFX_INVALID_HANDLE;
    };

    struct SelectionMaskResource
    {
        bgfx::TextureHandle colorTexture = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle depthTexture = BGFX_INVALID_HANDLE;
        bgfx::FrameBufferHandle frameBuffer = BGFX_INVALID_HANDLE;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };

    struct SsaoResource
    {
        bgfx::TextureHandle depthColorTexture = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle depthTexture = BGFX_INVALID_HANDLE;
        bgfx::FrameBufferHandle depthFrameBuffer = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle aoTexture = BGFX_INVALID_HANDLE;
        bgfx::FrameBufferHandle aoFrameBuffer = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle blurTempTexture = BGFX_INVALID_HANDLE;
        bgfx::FrameBufferHandle blurTempFrameBuffer = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle blurredTexture = BGFX_INVALID_HANDLE;
        bgfx::FrameBufferHandle blurredFrameBuffer = BGFX_INVALID_HANDLE;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t aoWidth = 0;
        std::uint32_t aoHeight = 0;
    };

    struct SceneTargetResource
    {
        bgfx::TextureHandle colorTexture = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle depthTexture = BGFX_INVALID_HANDLE;
        bgfx::FrameBufferHandle frameBuffer = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle depthColorTexture = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle depthCaptureDepthTexture = BGFX_INVALID_HANDLE;
        bgfx::FrameBufferHandle depthCaptureFrameBuffer = BGFX_INVALID_HANDLE;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };
#else
    struct TextureResource
    {
        TextureSemantic semantic = TextureSemantic::Color;
        TextureColorSpace colorSpace = TextureColorSpace::Srgb;
        std::uint32_t mipCount = 1;
        bool compressed = false;
        std::uint64_t estimatedBytes = 0;
        bool active = false;
    };
#endif

    enum class ResourceHandleState : std::uint8_t
    {
        Live,
        Invalid,
        Stale
    };

    MeshResource* findMesh(MeshHandle handle) noexcept;
    const MeshResource* findMesh(MeshHandle handle) const noexcept;
    SkinnedMeshResource* findSkinnedMesh(SkinnedMeshHandle handle) noexcept;
    const SkinnedMeshResource* findSkinnedMesh(SkinnedMeshHandle handle) const noexcept;
    TextureResource* findTexture(TextureHandle handle) noexcept;
    const TextureResource* findTexture(TextureHandle handle) const noexcept;
    MaterialResource* findMaterial(MaterialHandle handle) noexcept;
    const MaterialResource* findMaterial(MaterialHandle handle) const noexcept;
    ResourceHandleState classifyMeshHandle(MeshHandle handle) const noexcept;
    ResourceHandleState classifySkinnedMeshHandle(SkinnedMeshHandle handle) const noexcept;
    ResourceHandleState classifyTextureHandle(TextureHandle handle) const noexcept;
    ResourceHandleState classifyMaterialHandle(MaterialHandle handle) const noexcept;
    void recordHandleUse(ResourceHandleState state, bool submission) noexcept;
    bool resolveMeshMaterial(
        MeshHandle meshHandle,
        MaterialHandle materialHandle,
        const MeshResource*& mesh,
        const MaterialResource*& material,
        bool submission) noexcept;
    bool resolveSkinnedMeshMaterial(
        SkinnedMeshHandle meshHandle,
        MaterialHandle materialHandle,
        const SkinnedMeshResource*& mesh,
        const MaterialResource*& material,
        bool submission) noexcept;
    const TextureResource* resolveTexture(TextureHandle handle, bool submission) noexcept;
    void destroyAllResources() noexcept;
    void updateLiveResourceStats() noexcept;
#if defined(FULL_RENDERER_ENABLE_BGFX) && FULL_RENDERER_ENABLE_BGFX
    bool ensureShadowResources(std::uint32_t resolution);
    bool ensureShadowResources(std::uint32_t resolution, std::uint32_t cascadeCount);
    void destroyShadowResources() noexcept;
    bool hasValidShadowResources(std::uint32_t cascadeCount) const noexcept;
    bool ensureSelectionMaskResource(std::uint32_t width, std::uint32_t height);
    void destroySelectionMaskResource() noexcept;
    bool ensureSsaoResource(
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t aoWidth,
        std::uint32_t aoHeight);
    void destroySsaoResource() noexcept;
    bool ensureSceneTargetResource(std::uint32_t width, std::uint32_t height);
    void destroySceneTargetResource() noexcept;
    bool hasValidSceneTargetResource() const noexcept;
    bool prepareCsmForwardState(
        const RenderPacket& packet,
        float shadowViewProjMatrices[kMaxDirectionalShadowCascades][16],
        float cascadeSplits[4],
        std::uint32_t& outActiveCascadeCount,
        bool& outShadowsEnabled) const noexcept;
    void bindCsmForwardState(
        const RenderPacket& packet,
        const float shadowViewProjMatrices[kMaxDirectionalShadowCascades][16],
        const float cascadeSplits[4],
        std::uint32_t activeCascadeCount,
        bool shadowsEnabled) const noexcept;
    void bindEnvironmentState(const EnvironmentDesc& environment) const noexcept;
    void bindWeatherState(const scene::WeatherRenderPlan& weatherPlan) const noexcept;
    void bindFadeState(const scene::FadeRenderState& fadeState, const MaterialDesc& material) const noexcept;
    void applyWeatherStats(const scene::WeatherRenderPlan& weatherPlan) noexcept;
    void applyColorGradingStats(const scene::ColorGradingRenderPlan& plan) noexcept;
    scene::PostPassPlan makePostPassPlanForPacket(
        const RenderPacket& packet,
        const scene::DecalRenderPlan& decalPlan,
        const scene::ParticleRenderPlan& particlePlan,
        bool hasSelectedObjects,
        bool sceneTargetAvailable,
        bool sceneDepthAvailable) const noexcept;
    void applyPostPassStats(const scene::PostPassPlan& plan) noexcept;
    RendererResult submitSky(const EnvironmentDesc& environment);
#endif
    RendererResult submitSsao(
        const RenderPacket& packet,
        const core::InstancedDrawBatch* batches,
        std::uint32_t batchCount);
    RendererResult submitSceneDepthCapture(
        const RenderPacket& packet,
        const core::InstancedDrawBatch* batches,
        std::uint32_t batchCount);
    RendererResult submitDecals(
        const RenderPacket& packet,
        const core::InstancedDrawBatch* batches,
        std::uint32_t batchCount);
    RendererResult submitParticles(
        const RenderPacket& packet,
        const core::InstancedDrawBatch* batches,
        std::uint32_t batchCount);
    RendererResult submitScenePresent(const RenderPacket& packet);
    bool hasSelectedObjects(
        const RenderPacket& packet,
        const core::InstancedDrawBatch* batches,
        std::uint32_t batchCount) const noexcept;
    RendererResult submitSelectionOutline(
        const RenderPacket& packet,
        const core::InstancedDrawBatch* batches,
        std::uint32_t batchCount);

    bool initialized_ = false;
    bool frameInProgress_ = false;
    std::uint32_t backbufferWidth_ = 0;
    std::uint32_t backbufferHeight_ = 0;
    std::string shaderBinaryDirectory_;
    std::vector<MeshResource> meshes_;
    std::vector<SkinnedMeshResource> skinnedMeshes_;
    std::vector<TextureResource> textures_;
    std::vector<MaterialResource> materials_;
    RendererStats stats_;

#if defined(FULL_RENDERER_ENABLE_BGFX) && FULL_RENDERER_ENABLE_BGFX
    bgfx::VertexLayout meshVertexLayout_;
    bgfx::VertexLayout skinnedMeshVertexLayout_;
    bgfx::VertexLayout particleVertexLayout_;
    bgfx::VertexLayout debugLineVertexLayout_;
    bgfx::ProgramHandle forwardProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle instancedForwardProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle skinnedForwardProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle skyProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle terrainSplatProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle debugLineProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle terrainShadowProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle skinnedShadowProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle selectionMaskProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle selectionMaskInstancedProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle selectionMaskSkinnedProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle outlineCompositeProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle ssaoDepthProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle ssaoDepthInstancedProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle ssaoDepthSkinnedProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle ssaoProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle ssaoBlurProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle ssaoCompositeProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle decalProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle scenePresentProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle colorGradingProgram_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle particleProgram_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle lightDirIntensityUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle lightColorUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle materialColorUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle terrainLayerColorUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle terrainParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle shadowViewProjUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle shadowParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle shadowFilterParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle cascadeSplitsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle environmentColorsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle fogParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle weatherParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle weatherWindParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle weatherPrecipitationParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle fadeParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle skinningPaletteUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle outlineColorUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle outlineParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle selectionMaskSampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ssaoDepthParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ssaoParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ssaoDepthSampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ssaoSampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle decalInvViewUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle decalInvProjUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle decalWorldToLocalUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle decalColorOpacityUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle decalDepthParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle decalDepthSampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle decalAlbedoSampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sceneColorSampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle colorGradingParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle colorGradingControlsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle colorGradingLiftUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle colorGradingGainUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle particleSampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle particleDepthSampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle particleParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle layerSamplers_[kMaxTerrainMaterialLayers] = {};
    bgfx::UniformHandle normalSamplers_[kMaxTerrainMaterialLayers] = {};
    bgfx::UniformHandle splatSampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle shadowMapSamplers_[kMaxDirectionalShadowCascades] = {};
    bgfx::TextureHandle fallbackWhiteTexture_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle fallbackSplatTexture_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle fallbackNormalTexture_ = BGFX_INVALID_HANDLE;
    ShadowCascadeResource shadowCascadeResources_[kMaxDirectionalShadowCascades];
    SelectionMaskResource selectionMaskResource_;
    SsaoResource ssaoResource_;
    SceneTargetResource sceneTargetResource_;
    bool sceneTargetActiveThisFrame_ = false;
    bool sceneDepthCapturedThisFrame_ = false;
    bool forceSceneTargetForSubmit_ = false;
    bool sceneTargetResourceReconfiguredThisFrame_ = false;
    bool sceneTargetResourceAllocationFailedThisFrame_ = false;
    std::uint32_t shadowMapResolution_ = 0;
    std::uint32_t shadowCascadeResourceCount_ = 0;
    std::uint32_t shadowResourceRecreateCount_ = 0;
    std::uint32_t shadowResourceAllocationFailureCount_ = 0;
    std::uint32_t sceneTargetResourceRecreateCount_ = 0;
    std::uint32_t sceneTargetResourceAllocationFailureCount_ = 0;
    std::uint32_t meshAllocationFailureCount_ = 0;
    std::uint32_t skinnedMeshAllocationFailureCount_ = 0;
    std::uint32_t textureAllocationFailureCount_ = 0;
    std::uint32_t selectionMaskResourceRecreateCount_ = 0;
    std::uint32_t selectionMaskAllocationFailureCount_ = 0;
    std::uint32_t ssaoResourceRecreateCount_ = 0;
    std::uint32_t ssaoAllocationFailureCount_ = 0;
#endif
    std::uint32_t invalidHandleUseCount_ = 0;
    std::uint32_t staleHandleUseCount_ = 0;
    std::uint32_t destroyedHandleSubmissionCount_ = 0;
    std::uint64_t shaderVariantMaskThisFrame_ = 0;
    bool suppressPostScenePasses_ = false;
};
} // namespace full_renderer::bgfx_backend
