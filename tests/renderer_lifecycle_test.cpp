#include "full_renderer/Renderer.hpp"

#include "renderer/core/RenderDevice.hpp"
#include "renderer/core/Renderer.hpp"
#include "renderer/scene/ColorGrading.hpp"
#include "renderer/scene/Decal.hpp"
#include "renderer/scene/Fade.hpp"
#include "renderer/scene/Particle.hpp"
#include "renderer/scene/PostPass.hpp"
#include "renderer/scene/Shadow.hpp"
#include "renderer/scene/Weather.hpp"

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

namespace
{
full_renderer::RendererInitDesc validInitDesc()
{
    full_renderer::RendererInitDesc desc;
    desc.backbufferWidth = 640;
    desc.backbufferHeight = 480;
    return desc;
}

full_renderer::FrameDesc validFrameDesc()
{
    full_renderer::FrameDesc desc;
    desc.backbufferWidth = 640;
    desc.backbufferHeight = 480;
    desc.deltaSeconds = 1.0 / 60.0;
    return desc;
}

full_renderer::MeshDesc validMeshDesc()
{
    static const full_renderer::MeshVertex vertices[] = {
        {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    };
    static const std::uint16_t indices[] = {0, 1, 2};

    full_renderer::MeshDesc desc;
    desc.vertices = vertices;
    desc.vertexCount = 3;
    desc.indices = indices;
    desc.indexCount = 3;
    return desc;
}

full_renderer::MaterialDesc validMaterialDesc()
{
    full_renderer::MaterialDesc desc;
    desc.baseColorLinear[0] = 1.0f;
    desc.baseColorLinear[1] = 1.0f;
    desc.baseColorLinear[2] = 1.0f;
    desc.baseColorLinear[3] = 1.0f;
    desc.lit = true;
    return desc;
}

full_renderer::TextureDesc validTextureDesc()
{
    static const std::uint8_t pixels[] = {
        255, 255, 255, 255,
        128, 128, 128, 255,
        64, 64, 64, 255,
        0, 0, 0, 255,
    };

    full_renderer::TextureDesc desc;
    desc.width = 2;
    desc.height = 2;
    desc.format = full_renderer::TextureFormat::Rgba8;
    desc.data = pixels;
    desc.dataSizeBytes = sizeof(pixels);
    return desc;
}

full_renderer::MaterialDesc validTerrainMaterialDesc(const full_renderer::TextureHandle texture)
{
    full_renderer::MaterialDesc desc;
    desc.kind = full_renderer::MaterialKind::TerrainSplat;
    desc.lit = true;
    desc.terrain.uvScale = 0.25f;
    for (std::uint32_t index = 0; index < full_renderer::kMaxTerrainMaterialLayers; ++index)
    {
        desc.terrain.layers[index].albedoTexture = texture;
        desc.terrain.layers[index].fallbackColorLinear[0] = 1.0f;
        desc.terrain.layers[index].fallbackColorLinear[1] = 1.0f;
        desc.terrain.layers[index].fallbackColorLinear[2] = 1.0f;
        desc.terrain.layers[index].fallbackColorLinear[3] = 1.0f;
    }
    return desc;
}

full_renderer::Aabb validTerrainBounds()
{
    full_renderer::Aabb bounds;
    bounds.min[0] = -0.5f;
    bounds.min[1] = -0.5f;
    bounds.min[2] = -0.5f;
    bounds.max[0] = 0.5f;
    bounds.max[1] = 0.5f;
    bounds.max[2] = 0.5f;
    return bounds;
}

void setIdentity(float out[16])
{
    for (int index = 0; index < 16; ++index)
    {
        out[index] = 0.0f;
    }
    out[0] = 1.0f;
    out[5] = 1.0f;
    out[10] = 1.0f;
    out[15] = 1.0f;
}

full_renderer::RenderPacket validPacket(
    const full_renderer::MeshHandle mesh,
    const full_renderer::MaterialHandle material)
{
    static full_renderer::DrawItem drawItem;
    drawItem = {};
    drawItem.mesh = mesh;
    drawItem.material = material;
    setIdentity(drawItem.model);

    full_renderer::RenderPacket packet;
    setIdentity(packet.view.view);
    setIdentity(packet.view.projection);
    packet.directionalLight.directionWorld[0] = 0.0f;
    packet.directionalLight.directionWorld[1] = 1.0f;
    packet.directionalLight.directionWorld[2] = 0.0f;
    packet.directionalLight.colorLinear[0] = 1.0f;
    packet.directionalLight.colorLinear[1] = 1.0f;
    packet.directionalLight.colorLinear[2] = 1.0f;
    packet.directionalLight.intensity = 1.0f;
    packet.drawItems = &drawItem;
    packet.drawItemCount = 1;
    return packet;
}

full_renderer::TerrainChunkHandle createTerrainChunk(
    full_renderer::IRenderer& renderer,
    const full_renderer::MeshHandle mesh,
    const full_renderer::MaterialHandle material)
{
    const full_renderer::TerrainLodDesc lods[] = {
        {mesh, material, 100.0f},
    };
    full_renderer::TerrainChunkDesc desc;
    desc.bounds = validTerrainBounds();
    desc.lods = lods;
    desc.lodCount = 1;
    return renderer.createTerrainChunk(desc);
}

full_renderer::TerrainChunkHandle createTerrainChunkWithSplat(
    full_renderer::IRenderer& renderer,
    const full_renderer::MeshHandle mesh,
    const full_renderer::MaterialHandle material,
    const full_renderer::TextureHandle splatMap)
{
    const full_renderer::TerrainLodDesc lods[] = {
        {mesh, material, 100.0f},
    };
    full_renderer::TerrainChunkDesc desc;
    desc.bounds = validTerrainBounds();
    desc.lods = lods;
    desc.lodCount = 1;
    desc.splatMap = splatMap;
    return renderer.createTerrainChunk(desc);
}

full_renderer::DecalDesc validDecalDesc(const full_renderer::TextureHandle texture = {}) noexcept
{
    full_renderer::DecalDesc decal;
    decal.albedoTexture = texture;
    decal.halfExtentsMeters[0] = 1.0f;
    decal.halfExtentsMeters[1] = 1.0f;
    decal.halfExtentsMeters[2] = 1.0f;
    decal.tintColorLinear[0] = 0.9f;
    decal.tintColorLinear[1] = 0.2f;
    decal.tintColorLinear[2] = 0.1f;
    decal.tintColorLinear[3] = 1.0f;
    decal.opacity = 0.75f;
    return decal;
}

full_renderer::Particle validParticle() noexcept
{
    full_renderer::Particle particle;
    particle.positionWorld[0] = 0.0f;
    particle.positionWorld[1] = 0.0f;
    particle.positionWorld[2] = 0.0f;
    particle.sizeMeters = 0.5f;
    particle.colorLinear[0] = 1.0f;
    particle.colorLinear[1] = 0.8f;
    particle.colorLinear[2] = 0.3f;
    particle.colorLinear[3] = 0.6f;
    return particle;
}

full_renderer::SkeletonDesc validSkeletonDesc(full_renderer::SkeletonJointDesc joints[2])
{
    setIdentity(joints[0].inverseBindPose);
    joints[0].parentIndex = -1;
    setIdentity(joints[1].inverseBindPose);
    joints[1].parentIndex = 0;

    full_renderer::SkeletonDesc desc;
    desc.joints = joints;
    desc.jointCount = 2;
    return desc;
}

full_renderer::SkinnedMeshDesc validSkinnedMeshDesc(
    const full_renderer::SkeletonHandle skeleton,
    full_renderer::SkinnedMeshVertex vertices[3],
    std::uint16_t indices[3])
{
    vertices[0].position[0] = -0.5f;
    vertices[0].position[1] = 0.0f;
    vertices[0].position[2] = 0.0f;
    vertices[1].position[0] = 0.5f;
    vertices[1].position[1] = 0.0f;
    vertices[1].position[2] = 0.0f;
    vertices[2].position[0] = 0.0f;
    vertices[2].position[1] = 1.0f;
    vertices[2].position[2] = 0.0f;
    for (std::uint32_t vertexIndex = 0; vertexIndex < 3; ++vertexIndex)
    {
        vertices[vertexIndex].normal[1] = 1.0f;
        vertices[vertexIndex].jointIndices[0] = 0.0f;
        vertices[vertexIndex].jointIndices[1] = 1.0f;
        vertices[vertexIndex].jointWeights[0] = 0.5f;
        vertices[vertexIndex].jointWeights[1] = 0.5f;
    }

    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;

    full_renderer::SkinnedMeshDesc desc;
    desc.skeleton = skeleton;
    desc.vertices = vertices;
    desc.vertexCount = 3;
    desc.indices = indices;
    desc.indexCount = 3;
    return desc;
}

void recordFadeStats(const full_renderer::scene::FadeRenderState& state, full_renderer::RendererStats& stats);

class FakeRenderDevice final : public full_renderer::core::RenderDevice
{
public:
    enum class AllocationFailureCategory
    {
        Mesh,
        SkinnedMesh,
        Texture,
        Material
    };

    void failNextAllocation(const AllocationFailureCategory category, const bool simulatePartialCreate = false) noexcept
    {
        switch (category)
        {
        case AllocationFailureCategory::Mesh:
            failNextMeshAllocation_ = true;
            failNextMeshAllocationPartial_ = simulatePartialCreate;
            break;
        case AllocationFailureCategory::SkinnedMesh:
            failNextSkinnedMeshAllocation_ = true;
            failNextSkinnedMeshAllocationPartial_ = simulatePartialCreate;
            break;
        case AllocationFailureCategory::Texture:
            failNextTextureAllocation_ = true;
            failNextTextureAllocationPartial_ = simulatePartialCreate;
            break;
        case AllocationFailureCategory::Material:
            failNextMaterialAllocation_ = true;
            failNextMaterialAllocationPartial_ = simulatePartialCreate;
            break;
        }
    }

    void setFallbackResourcesValid(const bool valid) noexcept
    {
        fallbackResourcesValid_ = valid;
        stats_.fallbackResourceValid = valid ? 1U : 0U;
        if (!valid)
        {
            ++stats_.textureAllocationFailureCount;
        }
    }

    void setDecalResourcesValid(const bool valid) noexcept
    {
        decalResourcesValid_ = valid;
    }

    void setParticleResourcesValid(const bool valid) noexcept
    {
        particleResourcesValid_ = valid;
    }

    void setSkinnedPaletteResourceValid(const bool valid) noexcept
    {
        skinnedPaletteResourceValid_ = valid;
    }

    std::uint32_t partialCreateCleanupCount() const noexcept
    {
        return partialCreateCleanupCount_;
    }

    full_renderer::RendererResult initialize(const full_renderer::RendererInitDesc& desc) override
    {
        if (desc.backbufferWidth == 0 || desc.backbufferHeight == 0)
        {
            return full_renderer::RendererResult::InvalidDescriptor;
        }

        initialized_ = true;
        stats_ = {};
        stats_.fallbackResourceValid = fallbackResourcesValid_ ? 1U : 0U;
        return full_renderer::RendererResult::Success;
    }

    void shutdown() noexcept override
    {
        initialized_ = false;
        frameInProgress_ = false;
        meshes_.clear();
        skinnedMeshes_.clear();
        textures_.clear();
        materials_.clear();
        materialDescs_.clear();
        stats_ = {};
        partialCreateCleanupCount_ = 0;
    }

    full_renderer::RendererResult resize(const full_renderer::RendererResizeDesc& desc) override
    {
        if (!initialized_)
        {
            return full_renderer::RendererResult::NotInitialized;
        }

        if (desc.backbufferWidth == 0 || desc.backbufferHeight == 0)
        {
            return full_renderer::RendererResult::InvalidDescriptor;
        }

        return full_renderer::RendererResult::Success;
    }

    full_renderer::MeshHandle createMesh(const full_renderer::MeshDesc& desc) override
    {
        if (!initialized_ || desc.vertexCount == 0 || desc.indexCount == 0)
        {
            return {};
        }

        if (failNextMeshAllocation_)
        {
            failNextMeshAllocation_ = false;
            ++stats_.meshAllocationFailureCount;
            if (failNextMeshAllocationPartial_)
            {
                failNextMeshAllocationPartial_ = false;
                ++partialCreateCleanupCount_;
            }
            return {};
        }

        meshes_.push_back(true);
        stats_.liveMeshes = countLive(meshes_);
        return {static_cast<std::uint32_t>(meshes_.size())};
    }

    void destroyMesh(const full_renderer::MeshHandle handle) noexcept override
    {
        if (!isLiveHandle(handle.id, meshes_))
        {
            recordHandleUse(handle.id, meshes_, false);
            return;
        }

        meshes_[handle.id - 1U] = false;
        stats_.liveMeshes = countLive(meshes_);
    }

    full_renderer::SkinnedMeshHandle createSkinnedMesh(const full_renderer::SkinnedMeshDesc& desc) override
    {
        if (!initialized_ || desc.vertexCount == 0 || desc.indexCount == 0)
        {
            return {};
        }

        if (failNextSkinnedMeshAllocation_)
        {
            failNextSkinnedMeshAllocation_ = false;
            ++stats_.skinnedMeshAllocationFailureCount;
            if (failNextSkinnedMeshAllocationPartial_)
            {
                failNextSkinnedMeshAllocationPartial_ = false;
                ++partialCreateCleanupCount_;
            }
            return {};
        }

        skinnedMeshes_.push_back(true);
        stats_.liveSkinnedMeshes = countLive(skinnedMeshes_);
        return {static_cast<std::uint32_t>(skinnedMeshes_.size())};
    }

    void destroySkinnedMesh(const full_renderer::SkinnedMeshHandle handle) noexcept override
    {
        if (!isLiveHandle(handle.id, skinnedMeshes_))
        {
            recordHandleUse(handle.id, skinnedMeshes_, false);
            return;
        }

        skinnedMeshes_[handle.id - 1U] = false;
        stats_.liveSkinnedMeshes = countLive(skinnedMeshes_);
    }

    full_renderer::TextureHandle createTexture(const full_renderer::TextureDesc& desc) override
    {
        if (!initialized_ ||
            desc.width == 0 ||
            desc.height == 0 ||
            desc.data == nullptr ||
            desc.dataSizeBytes < desc.width * desc.height * 4U)
        {
            return {};
        }

        if (failNextTextureAllocation_)
        {
            failNextTextureAllocation_ = false;
            ++stats_.textureAllocationFailureCount;
            if (failNextTextureAllocationPartial_)
            {
                failNextTextureAllocationPartial_ = false;
                ++partialCreateCleanupCount_;
            }
            return {};
        }

        textures_.push_back(true);
        stats_.liveTextures = countLive(textures_);
        return {static_cast<std::uint32_t>(textures_.size())};
    }

    void destroyTexture(const full_renderer::TextureHandle handle) noexcept override
    {
        if (!isLiveHandle(handle.id, textures_))
        {
            recordHandleUse(handle.id, textures_, false);
            return;
        }

        textures_[handle.id - 1U] = false;
        stats_.liveTextures = countLive(textures_);
    }

    full_renderer::MaterialHandle createMaterial(const full_renderer::MaterialDesc& desc) override
    {
        if (!initialized_)
        {
            return {};
        }

        if (failNextMaterialAllocation_)
        {
            failNextMaterialAllocation_ = false;
            ++stats_.materialAllocationFailureCount;
            if (failNextMaterialAllocationPartial_)
            {
                failNextMaterialAllocationPartial_ = false;
                ++partialCreateCleanupCount_;
            }
            return {};
        }

        materials_.push_back(true);
        materialDescs_.push_back(desc);
        stats_.liveMaterials = countLive(materials_);
        return {static_cast<std::uint32_t>(materials_.size())};
    }

    void destroyMaterial(const full_renderer::MaterialHandle handle) noexcept override
    {
        if (!isLiveHandle(handle.id, materials_))
        {
            recordHandleUse(handle.id, materials_, false);
            return;
        }

        materials_[handle.id - 1U] = false;
        stats_.liveMaterials = countLive(materials_);
    }

    full_renderer::RendererResult beginFrame(const full_renderer::FrameDesc& desc) override
    {
        if (!initialized_)
        {
            return full_renderer::RendererResult::NotInitialized;
        }

        if (frameInProgress_)
        {
            return full_renderer::RendererResult::FrameAlreadyInProgress;
        }

        if (desc.backbufferWidth == 0 || desc.backbufferHeight == 0 || desc.deltaSeconds < 0.0)
        {
            return full_renderer::RendererResult::InvalidDescriptor;
        }

        stats_.submittedDraws = 0;
        stats_.renderedDraws = 0;
        stats_.submittedAnimatedDraws = 0;
        stats_.renderedAnimatedDraws = 0;
        stats_.skippedAnimatedDraws = 0;
        stats_.shadowCasterBatches = 0;
        stats_.shadowPassDraws = 0;
        stats_.shadowStaticMeshReceivers = 0;
        stats_.shadowInstancedMeshReceiverBatches = 0;
        stats_.shadowSkinnedMeshReceivers = 0;
        stats_.shadowSkinnedMeshPassDraws = 0;
        stats_.weatherEnabled = false;
        stats_.weatherWindEnabled = false;
        stats_.weatherWindDirectionWorld[0] = 0.0f;
        stats_.weatherWindDirectionWorld[1] = 0.0f;
        stats_.weatherWindDirectionWorld[2] = 0.0f;
        stats_.weatherWindSpeedMetersPerSecond = 0.0f;
        stats_.weatherPrecipitationEnabled = false;
        stats_.weatherPrecipitationType = full_renderer::PrecipitationType::None;
        stats_.weatherPrecipitationIntensity = 0.0f;
        stats_.weatherPrecipitationUsesParticleBatches = false;
        stats_.weatherWetnessEnabled = false;
        stats_.weatherWetnessAmount = 0.0f;
        stats_.weatherTerrainWetnessDraws = 0;
        stats_.weatherMeshWetnessDraws = 0;
        stats_.weatherFogBlendEnabled = false;
        stats_.weatherFogBlendAmount = 0.0f;
        stats_.weatherEffectiveFogColorLinear[0] = 0.0f;
        stats_.weatherEffectiveFogColorLinear[1] = 0.0f;
        stats_.weatherEffectiveFogColorLinear[2] = 0.0f;
        stats_.weatherEffectiveFogStartMeters = 0.0f;
        stats_.weatherEffectiveFogEndMeters = 0.0f;
        stats_.weatherClampedValueCount = 0;
        stats_.weatherNeutral = true;
        stats_.structureFadeSubmittedCount = 0;
        stats_.structureFadeActiveCount = 0;
        stats_.structureFadeFullyVisibleCount = 0;
        stats_.structureFadePartiallyFadedCount = 0;
        stats_.structureFadeFullyHiddenCount = 0;
        stats_.structureFadeInvalidCount = 0;
        stats_.structureFadeAlphaDraws = 0;
        stats_.structureFadeDitherDraws = 0;
        stats_.structureFadeUnsupportedTargetCount = 0;
        stats_.selectionOutlineEnabled = false;
        stats_.selectionMaskTargetValid = 0;
        stats_.selectedStaticMeshDraws = 0;
        stats_.selectedInstancedBatches = 0;
        stats_.selectedInstances = 0;
        stats_.selectedSkinnedMeshDraws = 0;
        stats_.selectionMaskDraws = 0;
        stats_.selectionOutlineDraws = 0;
        stats_.ssaoEnabled = false;
        stats_.ssaoDepthTargetValid = 0;
        stats_.ssaoOutputTargetValid = 0;
        stats_.ssaoBlurTargetValid = 0;
        stats_.ssaoAoWidth = 0;
        stats_.ssaoAoHeight = 0;
        stats_.ssaoHalfResolution = false;
        stats_.ssaoBlurEnabled = false;
        stats_.ssaoDepthPassDraws = 0;
        stats_.ssaoPassDraws = 0;
        stats_.ssaoCompositeDraws = 0;
        stats_.ssaoBlurPassDraws = 0;
        stats_.ssaoInputDepthValid = 0;
        stats_.ssaoDebugVisualized = false;
        stats_.decalsEnabled = false;
        stats_.decalSubmittedCount = 0;
        stats_.decalActiveCount = 0;
        stats_.decalCulledCount = 0;
        stats_.decalRejectedCount = 0;
        stats_.decalInvalidDescriptorRejectCount = 0;
        stats_.decalMaxCountRejectedCount = 0;
        stats_.decalFallbackColorCount = 0;
        stats_.decalFrustumCullingEnabled = false;
        stats_.decalMaxProjectionDepthMeters = 0.0f;
        stats_.decalProjectionEdgeFadeMeters = 0.0f;
        stats_.decalDebugVolumeCount = 0;
        stats_.decalPassDraws = 0;
        stats_.decalRenderedCount = 0;
        stats_.decalInputDepthValid = 0;
        stats_.decalInputColorValid = 0;
        stats_.decalSceneColorTargetValid = 0;
        stats_.decalSceneDepthTargetValid = 0;
        stats_.decalProjectionDeferred = false;
        stats_.particlesEnabled = false;
        stats_.particleSubmittedBatchCount = 0;
        stats_.particleAcceptedBatchCount = 0;
        stats_.particleRejectedBatchCount = 0;
        stats_.particleCulledBatchCount = 0;
        stats_.particleSubmittedCount = 0;
        stats_.particleAcceptedCount = 0;
        stats_.particleRejectedCount = 0;
        stats_.particleCulledCount = 0;
        stats_.particleSortedBatchCount = 0;
        stats_.particleSortedCount = 0;
        stats_.particleFallbackTextureBatchCount = 0;
        stats_.particleDrawCalls = 0;
        stats_.particleResourceValid = 0;
        stats_.particleCullingEnabled = false;
        stats_.particleSoftParticlesEnabled = false;
        stats_.particleSoftParticlesActive = false;
        stats_.particleSoftParticleDepthInputValid = 0;
        stats_.particleSoftParticleFadeDistanceMeters = 0.0f;
        stats_.colorGradingEnabled = false;
        stats_.colorGradingPassSubmitted = false;
        stats_.colorGradingSceneColorTargetValid = 0;
        stats_.colorGradingResourceValid = 0;
        stats_.colorGradingTonemapEnabled = false;
        stats_.colorGradingTonemapOperator = full_renderer::TonemapOperator::None;
        stats_.colorGradingExposureStops = 0.0f;
        stats_.colorGradingContrast = 1.0f;
        stats_.colorGradingSaturation = 1.0f;
        stats_.colorGradingGamma = 1.0f;
        stats_.colorGradingLutEnabled = false;
        stats_.colorGradingLutActive = false;
        stats_.colorGradingLutSamplingSupported = false;
        stats_.colorGradingLutFallbackCount = 0;
        stats_.colorGradingClampedValueCount = 0;
        stats_.colorGradingDebugMode = full_renderer::ColorGradingDebugMode::None;
        stats_.postViewportWidth = desc.backbufferWidth;
        stats_.postViewportHeight = desc.backbufferHeight;
        stats_.postSceneTargetRequired = false;
        stats_.postSceneTargetActive = false;
        stats_.postSceneTargetReasonMask = 0;
        stats_.postSceneColorTargetValid = 0;
        stats_.postSceneDepthTargetValid = 0;
        stats_.postSceneColorWidth = 0;
        stats_.postSceneColorHeight = 0;
        stats_.postSceneDepthWidth = 0;
        stats_.postSceneDepthHeight = 0;
        stats_.postReadableSceneDepthRequired = false;
        stats_.postFinalPresentSubmitted = 0;
        stats_.postPresentMode = 0;
        stats_.postPassCount = 0;
        stats_.postFullscreenPassCount = 0;
        stats_.postSkippedPassCount = 0;
        stats_.postSkippedPassReasonMask = 0;
        stats_.postInvalidResourceCount = 0;
        stats_.postSceneTargetReconfigured = 0;
        stats_.postSceneTargetRecreateCount = 0;
        stats_.postSceneTargetAllocationFailureCount = 0;
        stats_.postSceneTargetAllocationFailed = 0;
        stats_.shadowRequestedCascadeCount = 0;
        stats_.shadowRequestedMapResolution = 0;
        stats_.shadowResourceReconfigured = 0;
        stats_.shadowResourceAllocationFailed = 0;
        stats_.shadowCascadeCount = 0;
        stats_.shadowCascadeRenderTargetCount = 0;
        for (std::uint32_t cascadeIndex = 0; cascadeIndex < full_renderer::kMaxDirectionalShadowCascades; ++cascadeIndex)
        {
            stats_.shadowCasterBatchesByCascade[cascadeIndex] = 0;
            stats_.shadowStaticMeshCastersByCascade[cascadeIndex] = 0;
            stats_.shadowInstancedMeshCasterBatchesByCascade[cascadeIndex] = 0;
            stats_.shadowSkinnedMeshCastersByCascade[cascadeIndex] = 0;
            stats_.shadowTerrainCasterBatchesByCascade[cascadeIndex] = 0;
            stats_.shadowPassDrawsByCascade[cascadeIndex] = 0;
            stats_.shadowCascadeRenderTargetValid[cascadeIndex] = 0;
        }
        frameInProgress_ = true;
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult submit(const full_renderer::RenderPacket& packet) override
    {
        if (!initialized_)
        {
            return full_renderer::RendererResult::NotInitialized;
        }

        if (!frameInProgress_)
        {
            return full_renderer::RendererResult::FrameNotInProgress;
        }

        for (std::uint32_t index = 0; index < packet.drawItemCount; ++index)
        {
            const full_renderer::DrawItem& item = packet.drawItems[index];
            if (!isLiveHandle(item.mesh.id, meshes_) ||
                !isLiveHandle(item.material.id, materials_))
            {
                recordHandleUse(item.mesh.id, meshes_, true);
                recordHandleUse(item.material.id, materials_, true);
                return full_renderer::RendererResult::InvalidArgument;
            }
            validateMaterialTextureReferences(item.material, true);
        }

        for (std::uint32_t index = 0; index < packet.animatedDrawCount; ++index)
        {
            const full_renderer::AnimatedDrawItem& item = packet.animatedDraws[index];
            if (!isLiveHandle(item.mesh.id, skinnedMeshes_) ||
                !isLiveHandle(item.material.id, materials_) ||
                item.palette.skinningMatrices == nullptr ||
                item.palette.matrixCount == 0 ||
                item.palette.matrixCount > full_renderer::kMaxSkinningJoints)
            {
                recordHandleUse(item.mesh.id, skinnedMeshes_, true);
                recordHandleUse(item.material.id, materials_, true);
                ++stats_.skippedAnimatedDraws;
                return full_renderer::RendererResult::InvalidArgument;
            }
            if (!skinnedPaletteResourceValid_)
            {
                ++stats_.skinnedMeshAllocationFailureCount;
                ++stats_.skippedAnimatedDraws;
                return full_renderer::RendererResult::BackendFailure;
            }
            validateMaterialTextureReferences(item.material, true);
        }

        stats_.submittedDraws += packet.drawItemCount;
        stats_.renderedDraws += packet.drawItemCount;
        stats_.submittedAnimatedDraws += packet.animatedDrawCount;
        stats_.renderedAnimatedDraws += packet.animatedDrawCount;
        for (std::uint32_t index = 0; index < packet.drawItemCount; ++index)
        {
            recordFadeStats(full_renderer::scene::makeFadeRenderState(packet.drawItems[index].fade), stats_);
        }
        for (std::uint32_t index = 0; index < packet.animatedDrawCount; ++index)
        {
            recordFadeStats(full_renderer::scene::makeFadeRenderState(packet.animatedDraws[index].fade), stats_);
        }
        const full_renderer::scene::WeatherRenderPlan weatherPlan =
            full_renderer::scene::makeWeatherRenderPlan(packet.weather, packet.environment);
        stats_.weatherEnabled = weatherPlan.weatherEnabled;
        stats_.weatherWindEnabled = weatherPlan.windEnabled;
        stats_.weatherWindDirectionWorld[0] = weatherPlan.windParams[0];
        stats_.weatherWindDirectionWorld[1] = weatherPlan.windParams[1];
        stats_.weatherWindDirectionWorld[2] = weatherPlan.windParams[2];
        stats_.weatherWindSpeedMetersPerSecond = weatherPlan.windParams[3];
        stats_.weatherPrecipitationEnabled = weatherPlan.precipitationEnabled;
        stats_.weatherPrecipitationType = weatherPlan.precipitationType;
        stats_.weatherPrecipitationIntensity = weatherPlan.precipitationIntensity;
        stats_.weatherPrecipitationUsesParticleBatches = weatherPlan.precipitationUsesParticleBatches;
        stats_.weatherWetnessEnabled = weatherPlan.wetnessEnabled;
        stats_.weatherWetnessAmount = weatherPlan.wetnessAmount;
        stats_.weatherFogBlendEnabled = weatherPlan.fogBlendEnabled;
        stats_.weatherFogBlendAmount = weatherPlan.fogBlendAmount;
        stats_.weatherEffectiveFogColorLinear[0] = weatherPlan.environment.fogColorLinear[0];
        stats_.weatherEffectiveFogColorLinear[1] = weatherPlan.environment.fogColorLinear[1];
        stats_.weatherEffectiveFogColorLinear[2] = weatherPlan.environment.fogColorLinear[2];
        stats_.weatherEffectiveFogStartMeters = weatherPlan.environment.fogStartMeters;
        stats_.weatherEffectiveFogEndMeters = weatherPlan.environment.fogEndMeters;
        stats_.weatherClampedValueCount = weatherPlan.clampedValueCount;
        stats_.weatherNeutral = weatherPlan.neutralState;
        if (weatherPlan.meshWetnessActive)
        {
            stats_.weatherMeshWetnessDraws += packet.drawItemCount + packet.animatedDrawCount;
        }
        bool selectedObjects = false;
        if (packet.selectionOutline.enabled)
        {
            for (std::uint32_t index = 0; index < packet.drawItemCount; ++index)
            {
                if (packet.drawItems[index].selected)
                {
                    ++stats_.selectedStaticMeshDraws;
                    ++stats_.selectionMaskDraws;
                    selectedObjects = true;
                }
            }
            for (std::uint32_t index = 0; index < packet.animatedDrawCount; ++index)
            {
                if (packet.animatedDraws[index].selected)
                {
                    ++stats_.selectedSkinnedMeshDraws;
                    ++stats_.selectionMaskDraws;
                    selectedObjects = true;
                }
            }
            if (selectedObjects)
            {
                stats_.selectionOutlineEnabled = true;
                stats_.selectionMaskTargetValid = 1;
                stats_.selectionOutlineDraws = 1;
            }
        }
        if (packet.ssao.enabled)
        {
            stats_.ssaoEnabled = true;
            stats_.ssaoDepthTargetValid = 1;
            stats_.ssaoOutputTargetValid = 1;
            stats_.ssaoBlurTargetValid = packet.ssao.blurEnabled ? 1U : 0U;
            stats_.ssaoHalfResolution = packet.ssao.halfResolution;
            stats_.ssaoBlurEnabled = packet.ssao.blurEnabled;
            stats_.ssaoAoWidth = packet.ssao.halfResolution ? 320U : 640U;
            stats_.ssaoAoHeight = packet.ssao.halfResolution ? 240U : 480U;
            stats_.ssaoInputDepthValid = 1;
            stats_.ssaoDepthPassDraws += packet.drawItemCount + packet.animatedDrawCount;
            stats_.ssaoPassDraws = 1;
            stats_.ssaoBlurPassDraws = packet.ssao.blurEnabled ? 2U : 0U;
            stats_.ssaoCompositeDraws = 1;
            stats_.ssaoDebugVisualized = packet.ssao.debugVisualize;
        }
        const bool fakeSceneTargetAvailable =
            packet.colorGrading.enabled ||
            packet.ssao.enabled ||
            packet.decals != nullptr ||
            packet.particles != nullptr ||
            packet.selectionOutline.enabled;
        const full_renderer::scene::ColorGradingRenderPlan colorGradingPlan =
            full_renderer::scene::makeColorGradingRenderPlan(
                packet.colorGrading,
                fakeSceneTargetAvailable,
                textureHandleIsLiveOrFallback(packet.colorGrading.lut.texture, true),
                false);
        stats_.colorGradingEnabled = colorGradingPlan.enabled;
        stats_.colorGradingPassSubmitted = colorGradingPlan.passSubmitted;
        stats_.colorGradingSceneColorTargetValid =
            colorGradingPlan.sceneColorTargetAvailable ? 1U : 0U;
        stats_.colorGradingResourceValid = colorGradingPlan.passSubmitted ? 1U : 0U;
        stats_.colorGradingTonemapEnabled = colorGradingPlan.tonemapEnabled;
        stats_.colorGradingTonemapOperator = colorGradingPlan.tonemapOperator;
        stats_.colorGradingExposureStops =
            colorGradingPlan.params[0] > 0.0f ? std::log2(colorGradingPlan.params[0]) : 0.0f;
        stats_.colorGradingContrast = colorGradingPlan.controls[0];
        stats_.colorGradingSaturation = colorGradingPlan.controls[1];
        stats_.colorGradingGamma = colorGradingPlan.controls[2];
        stats_.colorGradingLutEnabled = colorGradingPlan.lutRequested;
        stats_.colorGradingLutActive = colorGradingPlan.lutActive;
        stats_.colorGradingLutSamplingSupported = colorGradingPlan.lutSamplingSupported;
        stats_.colorGradingLutFallbackCount = colorGradingPlan.lutFallback ? 1U : 0U;
        stats_.colorGradingClampedValueCount = colorGradingPlan.clampedValueCount;
        stats_.colorGradingDebugMode = colorGradingPlan.debugMode;
        const full_renderer::scene::DecalRenderPlan decalPlan =
            full_renderer::scene::buildDecalRenderPlan(packet.decals);
        if (decalPlan.enabled)
        {
            stats_.decalsEnabled = true;
            stats_.decalSubmittedCount = decalPlan.submittedCount;
            stats_.decalActiveCount = decalPlan.activeCount;
            stats_.decalCulledCount = decalPlan.culledCount;
            stats_.decalRejectedCount = decalPlan.rejectedCount;
            stats_.decalInvalidDescriptorRejectCount = decalPlan.invalidDescriptorRejectCount;
            stats_.decalMaxCountRejectedCount = decalPlan.maxCountRejectedCount;
            stats_.decalFallbackColorCount = decalPlan.fallbackColorCount;
            stats_.decalFrustumCullingEnabled = decalPlan.frustumCullingEnabled;
            stats_.decalMaxProjectionDepthMeters = decalPlan.maxProjectionDepthMeters;
            stats_.decalProjectionEdgeFadeMeters = decalPlan.projectionEdgeFadeMeters;
            stats_.decalDebugVolumeCount = decalPlan.debugVolumeCount;
            stats_.decalRenderedCount = decalPlan.activeCount;
            stats_.decalPassDraws = decalPlan.activeCount;
            stats_.decalInputDepthValid = decalPlan.activeCount > 0 ? 1U : 0U;
            stats_.decalInputColorValid = decalPlan.activeCount > 0 ? 1U : 0U;
            stats_.decalSceneColorTargetValid = decalPlan.activeCount > 0 ? 1U : 0U;
            stats_.decalSceneDepthTargetValid = decalPlan.activeCount > 0 ? 1U : 0U;
            stats_.decalProjectionDeferred = false;
            for (std::uint32_t decalIndex = 0; decalIndex < decalPlan.activeCount; ++decalIndex)
            {
                const full_renderer::TextureHandle texture = decalPlan.items[decalIndex].desc.albedoTexture;
                if (!textureHandleIsLiveOrFallback(texture, true) && full_renderer::isValid(texture))
                {
                    ++stats_.decalFallbackColorCount;
                }
            }
            const bool needsFallback =
                stats_.decalFallbackColorCount > decalPlan.fallbackColorCount ||
                decalPlan.fallbackColorCount > 0;
            if (decalPlan.activeCount > 0 && (!decalResourcesValid_ || (needsFallback && !fallbackResourcesValid_)))
            {
                stats_.decalRenderedCount = 0;
                stats_.decalPassDraws = 0;
                stats_.decalInputDepthValid = 0;
                stats_.decalInputColorValid = 0;
                stats_.decalSceneColorTargetValid = 0;
                stats_.decalSceneDepthTargetValid = 0;
                stats_.decalProjectionDeferred = true;
                ++stats_.postInvalidResourceCount;
            }
        }
        const full_renderer::scene::ParticleRenderPlan particlePlan =
            full_renderer::scene::buildParticleRenderPlan(packet.particles);
        if (particlePlan.enabled)
        {
            stats_.particlesEnabled = true;
            stats_.particleSubmittedBatchCount = particlePlan.submittedBatchCount;
            stats_.particleAcceptedBatchCount = particlePlan.acceptedBatchCount;
            stats_.particleRejectedBatchCount = particlePlan.rejectedBatchCount;
            stats_.particleCulledBatchCount = particlePlan.culledBatchCount;
            stats_.particleSubmittedCount = particlePlan.submittedParticleCount;
            stats_.particleAcceptedCount = particlePlan.acceptedParticleCount;
            stats_.particleRejectedCount = particlePlan.rejectedParticleCount;
            stats_.particleCulledCount = particlePlan.culledParticleCount;
            stats_.particleSortedBatchCount = particlePlan.sortedBatchCount;
            stats_.particleSortedCount = particlePlan.sortedParticleCount;
            stats_.particleFallbackTextureBatchCount = particlePlan.fallbackTextureBatchCount;
            stats_.particleDrawCalls = particlePlan.drawCallCount;
            stats_.particleResourceValid = particlePlan.acceptedBatchCount > 0 ? 1U : 0U;
            stats_.particleCullingEnabled = particlePlan.frustumCullingEnabled;
            stats_.particleSoftParticlesEnabled = particlePlan.softParticlesEnabled;
            stats_.particleSoftParticleFadeDistanceMeters = particlePlan.softParticleFadeDistanceMeters;
            for (std::uint32_t batchIndex = 0; batchIndex < particlePlan.acceptedBatchCount; ++batchIndex)
            {
                const full_renderer::TextureHandle texture = particlePlan.batches[batchIndex].desc.texture;
                if (!textureHandleIsLiveOrFallback(texture, true) && full_renderer::isValid(texture))
                {
                    ++stats_.particleFallbackTextureBatchCount;
                }
            }
            const bool needsFallback =
                stats_.particleFallbackTextureBatchCount > particlePlan.fallbackTextureBatchCount ||
                particlePlan.fallbackTextureBatchCount > 0;
            if (particlePlan.acceptedBatchCount > 0 && (!particleResourcesValid_ || (needsFallback && !fallbackResourcesValid_)))
            {
                stats_.particleDrawCalls = 0;
                stats_.particleResourceValid = 0;
                ++stats_.postInvalidResourceCount;
            }
        }
        full_renderer::scene::PostPassPlanInput postInput;
        postInput.viewportWidth = 640;
        postInput.viewportHeight = 480;
        postInput.ssaoEnabled = packet.ssao.enabled;
        postInput.ssaoBlurEnabled = packet.ssao.blurEnabled;
        postInput.decalsEnabled = decalPlan.enabled;
        postInput.activeDecals = decalPlan.activeCount > 0;
        postInput.particlesEnabled = particlePlan.enabled;
        postInput.acceptedParticles = particlePlan.acceptedParticleCount > 0;
        postInput.softParticlesEnabled = particlePlan.softParticlesEnabled;
        postInput.selectionOutlineEnabled = packet.selectionOutline.enabled;
        postInput.hasSelectedObjects = selectedObjects;
        postInput.colorGradingEnabled = packet.colorGrading.enabled;
        postInput.sceneTargetAvailable = fakeSceneTargetAvailable;
        postInput.sceneDepthAvailable = fakeSceneTargetAvailable;
        const full_renderer::scene::PostPassPlan postPlan =
            full_renderer::scene::makePostPassPlan(postInput);
        stats_.postViewportWidth = postPlan.viewportWidth;
        stats_.postViewportHeight = postPlan.viewportHeight;
        stats_.postSceneTargetRequired = postPlan.sceneTargetRequired;
        stats_.postSceneTargetActive = fakeSceneTargetAvailable && postPlan.sceneTargetRequired;
        stats_.postSceneTargetReasonMask = postPlan.sceneTargetReasonMask;
        stats_.postSceneColorTargetValid = stats_.postSceneTargetActive ? 1U : 0U;
        stats_.postSceneDepthTargetValid = stats_.postSceneTargetActive ? 1U : 0U;
        stats_.postSceneColorWidth = stats_.postSceneTargetActive ? postPlan.sceneTargetWidth : 0U;
        stats_.postSceneColorHeight = stats_.postSceneTargetActive ? postPlan.sceneTargetHeight : 0U;
        stats_.postSceneDepthWidth = stats_.postSceneTargetActive ? postPlan.sceneTargetWidth : 0U;
        stats_.postSceneDepthHeight = stats_.postSceneTargetActive ? postPlan.sceneTargetHeight : 0U;
        stats_.postReadableSceneDepthRequired = postPlan.readableSceneDepthRequired;
        stats_.postFinalPresentSubmitted = postPlan.finalPresentSubmitted ? 1U : 0U;
        stats_.postPresentMode = static_cast<std::uint32_t>(postPlan.presentMode);
        stats_.postPassCount = postPlan.passCount;
        stats_.postFullscreenPassCount = postPlan.fullscreenPassCount;
        stats_.postSkippedPassCount = postPlan.skippedPassCount;
        stats_.postSkippedPassReasonMask = postPlan.skippedPassReasonMask;
        stats_.postInvalidResourceCount += postPlan.invalidResourceCount;
        if (packet.directionalShadow.enabled)
        {
            stats_.shadowStaticMeshReceivers += packet.drawItemCount;
            stats_.shadowSkinnedMeshReceivers += packet.animatedDrawCount;
        }
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult submitInstanced(
        const full_renderer::RenderPacket& packet,
        const full_renderer::core::InstancedDrawBatch* batches,
        const std::uint32_t batchCount,
        const full_renderer::core::InstancedDrawBatch* shadowBatches,
        const std::uint32_t shadowBatchCount,
        const full_renderer::core::SkinnedShadowDrawBatch* skinnedShadowBatches,
        const std::uint32_t skinnedShadowBatchCount) override
    {
        const full_renderer::RendererResult result = submit(packet);
        if (result != full_renderer::RendererResult::Success)
        {
            return result;
        }

        if (batchCount > 0 && batches == nullptr)
        {
            return full_renderer::RendererResult::InvalidArgument;
        }
        if (shadowBatchCount > 0 && shadowBatches == nullptr)
        {
            return full_renderer::RendererResult::InvalidArgument;
        }
        if (skinnedShadowBatchCount > 0 && skinnedShadowBatches == nullptr)
        {
            return full_renderer::RendererResult::InvalidArgument;
        }

        for (std::uint32_t index = 0; index < batchCount; ++index)
        {
            const full_renderer::core::InstancedDrawBatch& batch = batches[index];
            if (!isLiveHandle(batch.mesh.id, meshes_) ||
                !isLiveHandle(batch.material.id, materials_) ||
                batch.modelMatrices == nullptr ||
                batch.instanceCount == 0)
            {
                recordHandleUse(batch.mesh.id, meshes_, true);
                recordHandleUse(batch.material.id, materials_, true);
                return full_renderer::RendererResult::InvalidArgument;
            }
            validateMaterialTextureReferences(batch.material, true);
            textureHandleIsLiveOrFallback(batch.splatMap, true);

            stats_.submittedDraws += 1;
            stats_.renderedDraws += 1;
            recordFadeStats(full_renderer::scene::makeFadeRenderState(batch.fade), stats_);
            if (packet.directionalShadow.enabled &&
                batch.shadowCasterKind == full_renderer::core::ShadowCasterKind::None)
            {
                ++stats_.shadowInstancedMeshReceiverBatches;
            }
            if (packet.selectionOutline.enabled && batch.selected)
            {
                ++stats_.selectedInstancedBatches;
                stats_.selectedInstances += batch.instanceCount;
                ++stats_.selectionMaskDraws;
                stats_.selectionOutlineEnabled = true;
                stats_.selectionMaskTargetValid = 1;
                stats_.selectionOutlineDraws = 1;
            }
            if (packet.ssao.enabled)
            {
                ++stats_.ssaoDepthPassDraws;
            }
        }

        if (packet.directionalShadow.enabled)
        {
            stats_.terrainShadowsEnabled = shadowBatchCount > 0;
            stats_.shadowRequestedMapResolution =
                full_renderer::scene::clampShadowMapResolution(packet.directionalShadow.mapResolution);
            stats_.shadowRequestedCascadeCount =
                full_renderer::scene::clampShadowCascadeCount(packet.directionalShadow.cascadeCount);
            stats_.shadowMapResolution = stats_.shadowRequestedMapResolution;
            stats_.shadowCasterBatches = shadowBatchCount;
            stats_.shadowCasterBatches += skinnedShadowBatchCount;
            stats_.shadowCascadeCount = stats_.shadowRequestedCascadeCount;
            stats_.shadowCascadeRenderTargetCount = stats_.shadowCascadeCount;
            for (std::uint32_t cascadeIndex = 0; cascadeIndex < stats_.shadowCascadeCount; ++cascadeIndex)
            {
                stats_.shadowCascadeRenderTargetValid[cascadeIndex] = 1;
            }
        }
        for (std::uint32_t index = 0; index < shadowBatchCount; ++index)
        {
            const full_renderer::core::InstancedDrawBatch& batch = shadowBatches[index];
            if (!isLiveHandle(batch.mesh.id, meshes_) ||
                !isLiveHandle(batch.material.id, materials_) ||
                batch.modelMatrices == nullptr ||
                batch.instanceCount == 0)
            {
                recordHandleUse(batch.mesh.id, meshes_, true);
                recordHandleUse(batch.material.id, materials_, true);
                return full_renderer::RendererResult::InvalidArgument;
            }
            validateMaterialTextureReferences(batch.material, true);
            textureHandleIsLiveOrFallback(batch.splatMap, true);

            stats_.shadowPassDraws += 1;
            if (batch.shadowCascadeIndex < full_renderer::kMaxDirectionalShadowCascades)
            {
                ++stats_.shadowCasterBatchesByCascade[batch.shadowCascadeIndex];
                if (batch.shadowCasterKind == full_renderer::core::ShadowCasterKind::StaticMesh)
                {
                    ++stats_.shadowStaticMeshCastersByCascade[batch.shadowCascadeIndex];
                }
                else if (batch.shadowCasterKind == full_renderer::core::ShadowCasterKind::InstancedMesh)
                {
                    ++stats_.shadowInstancedMeshCasterBatchesByCascade[batch.shadowCascadeIndex];
                }
                else if (batch.shadowCasterKind == full_renderer::core::ShadowCasterKind::Terrain)
                {
                    ++stats_.shadowTerrainCasterBatchesByCascade[batch.shadowCascadeIndex];
                }
                ++stats_.shadowPassDrawsByCascade[batch.shadowCascadeIndex];
            }
        }
        for (std::uint32_t index = 0; index < skinnedShadowBatchCount; ++index)
        {
            const full_renderer::core::SkinnedShadowDrawBatch& batch = skinnedShadowBatches[index];
            if (!isLiveHandle(batch.mesh.id, skinnedMeshes_) ||
                !isLiveHandle(batch.material.id, materials_) ||
                batch.model == nullptr ||
                batch.palette.skinningMatrices == nullptr ||
                batch.palette.matrixCount == 0 ||
                batch.palette.matrixCount > full_renderer::kMaxSkinningJoints)
            {
                recordHandleUse(batch.mesh.id, skinnedMeshes_, true);
                recordHandleUse(batch.material.id, materials_, true);
                ++stats_.skippedAnimatedDraws;
                return full_renderer::RendererResult::InvalidArgument;
            }
            if (!skinnedPaletteResourceValid_)
            {
                ++stats_.skinnedMeshAllocationFailureCount;
                ++stats_.skippedAnimatedDraws;
                return full_renderer::RendererResult::BackendFailure;
            }
            validateMaterialTextureReferences(batch.material, true);

            ++stats_.shadowPassDraws;
            ++stats_.shadowSkinnedMeshPassDraws;
            if (batch.shadowCascadeIndex < full_renderer::kMaxDirectionalShadowCascades)
            {
                ++stats_.shadowCasterBatchesByCascade[batch.shadowCascadeIndex];
                ++stats_.shadowSkinnedMeshCastersByCascade[batch.shadowCascadeIndex];
                ++stats_.shadowPassDrawsByCascade[batch.shadowCascadeIndex];
            }
        }

        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult submitDebugLines(
        const full_renderer::RenderViewDesc&,
        const full_renderer::debug::DebugLineVertex* lines,
        const std::uint32_t lineVertexCount) override
    {
        if (!initialized_)
        {
            return full_renderer::RendererResult::NotInitialized;
        }

        if (!frameInProgress_)
        {
            return full_renderer::RendererResult::FrameNotInProgress;
        }

        if (lineVertexCount == 0)
        {
            return full_renderer::RendererResult::Success;
        }

        if (lines == nullptr || (lineVertexCount % 2U) != 0U)
        {
            return full_renderer::RendererResult::InvalidArgument;
        }

        ++debugLineSubmissions_;
        stats_.submittedDraws += 1;
        stats_.renderedDraws += 1;
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererResult endFrame() override
    {
        if (!initialized_)
        {
            return full_renderer::RendererResult::NotInitialized;
        }

        if (!frameInProgress_)
        {
            return full_renderer::RendererResult::FrameNotInProgress;
        }

        ++stats_.frameIndex;
        frameInProgress_ = false;
        return full_renderer::RendererResult::Success;
    }

    full_renderer::RendererStats getStats() const noexcept override
    {
        return stats_;
    }

private:
    static std::uint32_t countLive(const std::vector<bool>& entries)
    {
        std::uint32_t count = 0;
        for (const bool live : entries)
        {
            if (live)
            {
                ++count;
            }
        }
        return count;
    }

    static bool isLiveHandle(const std::uint32_t id, const std::vector<bool>& entries)
    {
        return id != 0 && id <= entries.size() && entries[id - 1U];
    }

    void recordHandleUse(
        const std::uint32_t id,
        const std::vector<bool>& entries,
        const bool submission) noexcept
    {
        if (isLiveHandle(id, entries))
        {
            return;
        }

        if (id == 0)
        {
            ++stats_.invalidHandleUseCount;
        }
        else
        {
            ++stats_.staleHandleUseCount;
        }

        if (submission)
        {
            ++stats_.destroyedHandleSubmissionCount;
        }
    }

    bool textureHandleIsLiveOrFallback(
        const full_renderer::TextureHandle handle,
        const bool submission) noexcept
    {
        if (!full_renderer::isValid(handle))
        {
            return false;
        }

        if (!isLiveHandle(handle.id, textures_))
        {
            recordHandleUse(handle.id, textures_, submission);
            return false;
        }

        return true;
    }

    void validateMaterialTextureReferences(
        const full_renderer::MaterialHandle material,
        const bool submission) noexcept
    {
        if (!isLiveHandle(material.id, materials_) || material.id > materialDescs_.size())
        {
            return;
        }

        const full_renderer::MaterialDesc& desc = materialDescs_[material.id - 1U];
        if (desc.kind != full_renderer::MaterialKind::TerrainSplat)
        {
            return;
        }

        for (const full_renderer::TerrainMaterialLayerDesc& layer : desc.terrain.layers)
        {
            textureHandleIsLiveOrFallback(layer.albedoTexture, submission);
            textureHandleIsLiveOrFallback(layer.normalTexture, submission);
        }
    }

    bool initialized_ = false;
    bool frameInProgress_ = false;
    bool failNextMeshAllocation_ = false;
    bool failNextMeshAllocationPartial_ = false;
    bool failNextSkinnedMeshAllocation_ = false;
    bool failNextSkinnedMeshAllocationPartial_ = false;
    bool failNextTextureAllocation_ = false;
    bool failNextTextureAllocationPartial_ = false;
    bool failNextMaterialAllocation_ = false;
    bool failNextMaterialAllocationPartial_ = false;
    bool fallbackResourcesValid_ = true;
    bool decalResourcesValid_ = true;
    bool particleResourcesValid_ = true;
    bool skinnedPaletteResourceValid_ = true;
    std::vector<bool> meshes_;
    std::vector<bool> skinnedMeshes_;
    std::vector<bool> textures_;
    std::vector<bool> materials_;
    std::vector<full_renderer::MaterialDesc> materialDescs_;
    std::uint32_t partialCreateCleanupCount_ = 0;
    std::uint32_t debugLineSubmissions_ = 0;
    full_renderer::RendererStats stats_;
};

std::unique_ptr<full_renderer::IRenderer> createTestRenderer(FakeRenderDevice** outDevice = nullptr)
{
    auto device = std::make_unique<FakeRenderDevice>();
    if (outDevice != nullptr)
    {
        *outDevice = device.get();
    }
    return std::make_unique<full_renderer::core::Renderer>(
        std::move(device));
}

void expect(const bool condition, const char* message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

void recordFadeStats(const full_renderer::scene::FadeRenderState& state, full_renderer::RendererStats& stats)
{
    if (!state.enabled)
    {
        return;
    }

    ++stats.structureFadeSubmittedCount;
    if (state.fullyVisible)
    {
        ++stats.structureFadeFullyVisibleCount;
    }
    else if (state.fullyHidden)
    {
        ++stats.structureFadeFullyHiddenCount;
    }
    else
    {
        ++stats.structureFadePartiallyFadedCount;
    }
    if (state.active)
    {
        ++stats.structureFadeActiveCount;
        if (state.alphaMode)
        {
            ++stats.structureFadeAlphaDraws;
        }
        else if (state.ditherMode)
        {
            ++stats.structureFadeDitherDraws;
        }
    }
}

void lifecycleSuccess(int& failures)
{
    auto renderer = createTestRenderer();
    expect(renderer != nullptr, "createRenderer returns an instance", failures);
    expect(!renderer->isInitialized(), "new renderer starts uninitialized", failures);

    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);
    expect(renderer->isInitialized(), "renderer reports initialized after init", failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    expect(full_renderer::isValid(mesh), "valid mesh creation succeeds", failures);
    const full_renderer::TextureHandle texture = renderer->createTexture(validTextureDesc());
    expect(full_renderer::isValid(texture), "valid texture creation succeeds", failures);
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());
    expect(full_renderer::isValid(material), "valid material creation succeeds", failures);
    const full_renderer::MaterialHandle terrainMaterial = renderer->createMaterial(validTerrainMaterialDesc(texture));
    expect(full_renderer::isValid(terrainMaterial), "valid terrain material creation succeeds", failures);

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds after init",
        failures);
    expect(renderer->submit(validPacket(mesh, material)) == full_renderer::RendererResult::Success,
        "submit succeeds during frame",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds after beginFrame",
        failures);

    const full_renderer::RendererStats stats = renderer->getStats();
    expect(stats.frameIndex == 1, "stats count ended frames", failures);
    expect(stats.submittedDraws == 1, "stats count submitted draws", failures);
    expect(stats.renderedDraws == 1, "stats count rendered draws", failures);
    expect(stats.liveMeshes == 1, "stats count live meshes", failures);
    expect(stats.liveTextures == 1, "stats count live textures", failures);
    expect(stats.liveMaterials == 2, "stats count live materials", failures);

    full_renderer::RendererResizeDesc resizeDesc;
    resizeDesc.backbufferWidth = 800;
    resizeDesc.backbufferHeight = 600;
    expect(renderer->resize(resizeDesc) == full_renderer::RendererResult::Success,
        "resize succeeds between frames",
        failures);

    renderer->destroyMaterial(material);
    renderer->destroyMaterial(terrainMaterial);
    renderer->destroyTexture(texture);
    renderer->destroyMesh(mesh);
    expect(renderer->getStats().liveMaterials == 0, "destroyMaterial updates stats", failures);
    expect(renderer->getStats().liveTextures == 0, "destroyTexture updates stats", failures);
    expect(renderer->getStats().liveMeshes == 0, "destroyMesh updates stats", failures);

    renderer->shutdown();
    expect(!renderer->isInitialized(), "shutdown clears initialization state", failures);
    renderer->shutdown();
    expect(!renderer->isInitialized(), "shutdown is idempotent", failures);
}

void staleResourceHandlesAreReported(int& failures)
{
    auto renderer = createTestRenderer();
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());
    expect(full_renderer::isValid(mesh), "mesh creation succeeds", failures);
    expect(full_renderer::isValid(material), "material creation succeeds", failures);

    renderer->destroyMesh(mesh);
    renderer->destroyMesh(mesh);
    expect(renderer->getStats().staleHandleUseCount == 1,
        "duplicate mesh destroy is reported as stale handle use",
        failures);

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(validPacket(mesh, material)) == full_renderer::RendererResult::InvalidArgument,
        "submitting a destroyed mesh handle is rejected",
        failures);
    const full_renderer::RendererStats stats = renderer->getStats();
    expect(stats.staleHandleUseCount == 2,
        "stale submission increments stale handle diagnostics",
        failures);
    expect(stats.destroyedHandleSubmissionCount == 1,
        "stale submission increments destroyed-handle submission diagnostics",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame still succeeds after rejected stale submission",
        failures);

    renderer->shutdown();
}

void mockedAllocationFailuresAreReportedAndRecoverable(int& failures)
{
    FakeRenderDevice* fake = nullptr;
    auto renderer = createTestRenderer(&fake);
    expect(fake != nullptr, "test renderer exposes fake backend seam", failures);
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    fake->failNextAllocation(FakeRenderDevice::AllocationFailureCategory::Mesh, true);
    expect(!full_renderer::isValid(renderer->createMesh(validMeshDesc())),
        "mocked mesh allocation failure returns invalid handle",
        failures);
    expect(renderer->getStats().meshAllocationFailureCount == 1,
        "mesh allocation failure is counted",
        failures);
    expect(fake->partialCreateCleanupCount() == 1,
        "partial mesh allocation failure records cleanup",
        failures);
    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    expect(full_renderer::isValid(mesh), "mesh allocation can recover after failure", failures);

    full_renderer::SkeletonJointDesc joints[2] = {};
    const full_renderer::SkeletonHandle skeleton = renderer->createSkeleton(validSkeletonDesc(joints));
    expect(full_renderer::isValid(skeleton), "skeleton creation succeeds", failures);
    full_renderer::SkinnedMeshVertex vertices[3] = {};
    std::uint16_t indices[3] = {};
    fake->failNextAllocation(FakeRenderDevice::AllocationFailureCategory::SkinnedMesh, true);
    expect(!full_renderer::isValid(renderer->createSkinnedMesh(validSkinnedMeshDesc(skeleton, vertices, indices))),
        "mocked skinned mesh allocation failure returns invalid handle",
        failures);
    expect(renderer->getStats().skinnedMeshAllocationFailureCount == 1,
        "skinned mesh allocation failure is counted",
        failures);
    const full_renderer::SkinnedMeshHandle skinnedMesh =
        renderer->createSkinnedMesh(validSkinnedMeshDesc(skeleton, vertices, indices));
    expect(full_renderer::isValid(skinnedMesh), "skinned allocation can recover after failure", failures);

    fake->failNextAllocation(FakeRenderDevice::AllocationFailureCategory::Texture, true);
    expect(!full_renderer::isValid(renderer->createTexture(validTextureDesc())),
        "mocked texture allocation failure returns invalid handle",
        failures);
    expect(renderer->getStats().textureAllocationFailureCount == 1,
        "texture allocation failure is counted",
        failures);
    const full_renderer::TextureHandle texture = renderer->createTexture(validTextureDesc());
    expect(full_renderer::isValid(texture), "texture allocation can recover after failure", failures);

    fake->failNextAllocation(FakeRenderDevice::AllocationFailureCategory::Material, true);
    expect(!full_renderer::isValid(renderer->createMaterial(validMaterialDesc())),
        "mocked material allocation failure returns invalid handle",
        failures);
    expect(renderer->getStats().materialAllocationFailureCount == 1,
        "material allocation failure is counted",
        failures);
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());
    expect(full_renderer::isValid(material), "material allocation can recover after failure", failures);
    expect(renderer->getStats().liveMeshes == 1, "failed allocations do not register extra live meshes", failures);
    expect(renderer->getStats().liveTextures == 1, "failed allocations do not register extra live textures", failures);
    expect(renderer->getStats().liveMaterials == 1, "failed allocations do not register extra live materials", failures);

    renderer->shutdown();
}

void staleDecalTextureReferencesUseFallbackOrSkip(int& failures)
{
    FakeRenderDevice* fake = nullptr;
    auto renderer = createTestRenderer(&fake);
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());
    const full_renderer::TextureHandle texture = renderer->createTexture(validTextureDesc());
    renderer->destroyTexture(texture);

    full_renderer::DecalDesc decal = validDecalDesc(texture);
    full_renderer::DecalSubmitDesc decals;
    decals.enabled = true;
    decals.decals = &decal;
    decals.decalCount = 1;
    full_renderer::RenderPacket packet = validPacket(mesh, material);
    packet.decals = &decals;

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "stale decal texture falls back without failing submission",
        failures);
    full_renderer::RendererStats stats = renderer->getStats();
    expect(stats.decalActiveCount == 1, "stale-texture decal remains an active fallback decal", failures);
    expect(stats.decalRenderedCount == 1, "fallback decal is still rendered when fallback resources are valid", failures);
    expect(stats.decalFallbackColorCount == 1, "stale decal texture increments fallback count", failures);
    expect(stats.staleHandleUseCount >= 1, "stale decal texture increments stale-handle diagnostics", failures);
    expect(stats.destroyedHandleSubmissionCount >= 1,
        "stale decal texture increments destroyed-handle submission diagnostics",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds",
        failures);

    fake->setFallbackResourcesValid(false);
    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds with invalid fallback resources",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "stale decal texture skips safely when fallback resources fail",
        failures);
    stats = renderer->getStats();
    expect(stats.decalRenderedCount == 0, "decal pass skips without fallback resources", failures);
    expect(stats.decalProjectionDeferred, "decal projection is reported deferred after resource failure", failures);
    expect(stats.postInvalidResourceCount > 0, "decal resource failure updates invalid-resource diagnostics", failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds after decal skip",
        failures);

    renderer->shutdown();
}

void staleParticleTextureReferencesUseFallbackOrSkip(int& failures)
{
    FakeRenderDevice* fake = nullptr;
    auto renderer = createTestRenderer(&fake);
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());
    const full_renderer::TextureHandle texture = renderer->createTexture(validTextureDesc());
    renderer->destroyTexture(texture);

    full_renderer::Particle particle = validParticle();
    full_renderer::ParticleBatchDesc batch;
    batch.particles = &particle;
    batch.particleCount = 1;
    batch.texture = texture;
    full_renderer::ParticleSubmitDesc particles;
    particles.enabled = true;
    particles.batches = &batch;
    particles.batchCount = 1;
    particles.cullAgainstViewFrustum = false;
    full_renderer::RenderPacket packet = validPacket(mesh, material);
    packet.particles = &particles;

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "stale particle texture falls back without failing submission",
        failures);
    full_renderer::RendererStats stats = renderer->getStats();
    expect(stats.particleAcceptedBatchCount == 1, "particle batch remains accepted with fallback texture", failures);
    expect(stats.particleDrawCalls == 1, "particle fallback batch still draws with valid fallback resources", failures);
    expect(stats.particleFallbackTextureBatchCount == 1,
        "stale particle texture increments fallback batch count",
        failures);
    expect(stats.staleHandleUseCount >= 1, "stale particle texture increments stale-handle diagnostics", failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds",
        failures);

    fake->setFallbackResourcesValid(false);
    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds with invalid fallback resources",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "stale particle texture skips safely when fallback resources fail",
        failures);
    stats = renderer->getStats();
    expect(stats.particleDrawCalls == 0, "particle pass skips without fallback resources", failures);
    expect(stats.particleResourceValid == 0, "particle resource status reports invalid fallback resources", failures);
    expect(stats.postInvalidResourceCount > 0, "particle resource failure updates invalid-resource diagnostics", failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds after particle skip",
        failures);

    renderer->shutdown();
}

void staleMaterialTextureReferencesAreReported(int& failures)
{
    auto renderer = createTestRenderer();
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::TextureHandle albedo = renderer->createTexture(validTextureDesc());
    const full_renderer::TextureHandle normal = renderer->createTexture(validTextureDesc());
    const full_renderer::TextureHandle splat = renderer->createTexture(validTextureDesc());
    full_renderer::MaterialDesc terrainMaterialDesc = validTerrainMaterialDesc({});
    terrainMaterialDesc.terrain.layers[0].albedoTexture = albedo;
    terrainMaterialDesc.terrain.layers[0].normalTexture = normal;
    const full_renderer::MaterialHandle terrainMaterial = renderer->createMaterial(terrainMaterialDesc);
    const full_renderer::TerrainChunkHandle terrainChunk =
        createTerrainChunkWithSplat(*renderer, mesh, terrainMaterial, splat);
    expect(full_renderer::isValid(terrainChunk), "terrain chunk with texture references is created", failures);

    renderer->destroyTexture(albedo);
    renderer->destroyTexture(normal);
    renderer->destroyTexture(splat);

    full_renderer::TerrainSubmitDesc terrainSubmit;
    terrainSubmit.chunks = &terrainChunk;
    terrainSubmit.chunkCount = 1;
    terrainSubmit.cameraPositionWorld[2] = -2.0f;

    full_renderer::RenderPacket packet = validPacket(mesh, terrainMaterial);
    packet.terrain = &terrainSubmit;

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "stale terrain material textures fall back without failing submission",
        failures);
    const full_renderer::RendererStats stats = renderer->getStats();
    expect(stats.staleHandleUseCount >= 3,
        "stale albedo, normal, and splat texture references are reported",
        failures);
    expect(stats.destroyedHandleSubmissionCount >= 3,
        "stale material texture references increment submission diagnostics",
        failures);
    expect(stats.renderedDraws >= 1, "fallback material texture submission still renders", failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds",
        failures);

    renderer->shutdown();
}

void skinnedPaletteAndStaleSkinnedResourcesAreRejected(int& failures)
{
    auto renderer = createTestRenderer();
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());
    full_renderer::SkeletonJointDesc joints[2] = {};
    const full_renderer::SkeletonHandle skeleton = renderer->createSkeleton(validSkeletonDesc(joints));
    full_renderer::SkinnedMeshVertex vertices[3] = {};
    std::uint16_t indices[3] = {};
    const full_renderer::SkinnedMeshHandle skinnedMesh =
        renderer->createSkinnedMesh(validSkinnedMeshDesc(skeleton, vertices, indices));
    expect(full_renderer::isValid(skinnedMesh), "skinned mesh creation succeeds", failures);

    float paletteMatrices[2][16] = {};
    setIdentity(paletteMatrices[0]);
    setIdentity(paletteMatrices[1]);

    full_renderer::AnimatedDrawItem animatedDraw;
    animatedDraw.mesh = skinnedMesh;
    animatedDraw.material = material;
    setIdentity(animatedDraw.model);
    animatedDraw.bounds = validTerrainBounds();
    animatedDraw.palette.skinningMatrices = &paletteMatrices[0][0];
    animatedDraw.palette.matrixCount = 2;

    full_renderer::RenderPacket packet = validPacket(mesh, material);
    packet.animatedDraws = &animatedDraw;
    packet.animatedDrawCount = 1;

    full_renderer::AnimatedDrawItem missingPalette = animatedDraw;
    missingPalette.palette.skinningMatrices = nullptr;
    packet.animatedDraws = &missingPalette;
    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::InvalidArgument,
        "missing skinned palette is rejected before backend submission",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds after missing palette rejection",
        failures);

    renderer->destroySkinnedMesh(skinnedMesh);
    packet.animatedDraws = &animatedDraw;
    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds after skinned mesh destroy",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::InvalidArgument,
        "stale skinned mesh submission is rejected",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds after stale skinned mesh rejection",
        failures);

    renderer->shutdown();
}

void mockedOptionalResourceFailuresSkipPassesSafely(int& failures)
{
    FakeRenderDevice* fake = nullptr;
    auto renderer = createTestRenderer(&fake);
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());

    full_renderer::DecalDesc decal = validDecalDesc();
    full_renderer::DecalSubmitDesc decals;
    decals.enabled = true;
    decals.decals = &decal;
    decals.decalCount = 1;
    fake->setDecalResourcesValid(false);
    full_renderer::RenderPacket packet = validPacket(mesh, material);
    packet.decals = &decals;
    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "mocked decal resource allocation failure skips optional pass",
        failures);
    expect(renderer->getStats().decalRenderedCount == 0, "decal draw count is zero after mocked resource failure", failures);
    expect(renderer->getStats().postInvalidResourceCount > 0, "decal resource failure is diagnosed", failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds after decal resource failure",
        failures);
    fake->setDecalResourcesValid(true);

    full_renderer::Particle particle = validParticle();
    full_renderer::ParticleBatchDesc batch;
    batch.particles = &particle;
    batch.particleCount = 1;
    full_renderer::ParticleSubmitDesc particles;
    particles.enabled = true;
    particles.batches = &batch;
    particles.batchCount = 1;
    particles.cullAgainstViewFrustum = false;
    fake->setParticleResourcesValid(false);
    packet = validPacket(mesh, material);
    packet.particles = &particles;
    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "mocked particle resource allocation failure skips optional pass",
        failures);
    expect(renderer->getStats().particleDrawCalls == 0,
        "particle draw count is zero after mocked resource failure",
        failures);
    expect(renderer->getStats().postInvalidResourceCount > 0, "particle resource failure is diagnosed", failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds after particle resource failure",
        failures);

    full_renderer::SkeletonJointDesc joints[2] = {};
    const full_renderer::SkeletonHandle skeleton = renderer->createSkeleton(validSkeletonDesc(joints));
    full_renderer::SkinnedMeshVertex vertices[3] = {};
    std::uint16_t indices[3] = {};
    const full_renderer::SkinnedMeshHandle skinnedMesh =
        renderer->createSkinnedMesh(validSkinnedMeshDesc(skeleton, vertices, indices));
    float paletteMatrices[2][16] = {};
    setIdentity(paletteMatrices[0]);
    setIdentity(paletteMatrices[1]);
    full_renderer::AnimatedDrawItem animatedDraw;
    animatedDraw.mesh = skinnedMesh;
    animatedDraw.material = material;
    setIdentity(animatedDraw.model);
    animatedDraw.bounds = validTerrainBounds();
    animatedDraw.palette.skinningMatrices = &paletteMatrices[0][0];
    animatedDraw.palette.matrixCount = 2;
    fake->setSkinnedPaletteResourceValid(false);
    packet = validPacket(mesh, material);
    packet.animatedDraws = &animatedDraw;
    packet.animatedDrawCount = 1;
    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::BackendFailure,
        "mocked skinned palette resource failure fails required skinned draw safely",
        failures);
    expect(renderer->getStats().skinnedMeshAllocationFailureCount > 0,
        "skinned palette resource failure updates skinned resource diagnostics",
        failures);
    expect(renderer->getStats().skippedAnimatedDraws > 0,
        "skinned palette resource failure skips animated draw",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds after skinned palette resource failure",
        failures);

    renderer->shutdown();
}

void invalidDimensionsAreRejected(int& failures)
{
    auto renderer = createTestRenderer();

    full_renderer::RendererInitDesc badInit = validInitDesc();
    badInit.backbufferWidth = 0;
    expect(renderer->initialize(badInit) == full_renderer::RendererResult::InvalidDescriptor,
        "zero init width is rejected",
        failures);
    expect(!renderer->isInitialized(), "invalid init does not initialize renderer", failures);

    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid init still succeeds after invalid attempt",
        failures);

    full_renderer::FrameDesc badFrame = validFrameDesc();
    badFrame.backbufferHeight = 0;
    expect(renderer->beginFrame(badFrame) == full_renderer::RendererResult::InvalidDescriptor,
        "zero frame height is rejected",
        failures);

    badFrame = validFrameDesc();
    badFrame.deltaSeconds = -1.0;
    expect(renderer->beginFrame(badFrame) == full_renderer::RendererResult::InvalidDescriptor,
        "negative frame time is rejected",
        failures);

    full_renderer::RendererResizeDesc badResize;
    badResize.backbufferWidth = 0;
    badResize.backbufferHeight = 480;
    expect(renderer->resize(badResize) == full_renderer::RendererResult::InvalidDescriptor,
        "zero resize width is rejected",
        failures);

    renderer->shutdown();
}

void resourceDescriptorsAreValidated(int& failures)
{
    auto renderer = createTestRenderer();

    expect(!full_renderer::isValid(renderer->createMesh(validMeshDesc())),
        "mesh creation before initialization is rejected",
        failures);
    expect(!full_renderer::isValid(renderer->createMaterial(validMaterialDesc())),
        "material creation before initialization is rejected",
        failures);
    expect(!full_renderer::isValid(renderer->createTexture(validTextureDesc())),
        "texture creation before initialization is rejected",
        failures);

    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    full_renderer::MeshDesc badMesh = validMeshDesc();
    badMesh.vertices = nullptr;
    expect(!full_renderer::isValid(renderer->createMesh(badMesh)),
        "null mesh vertices are rejected",
        failures);

    badMesh = validMeshDesc();
    badMesh.indexCount = 2;
    expect(!full_renderer::isValid(renderer->createMesh(badMesh)),
        "non-triangle index count is rejected",
        failures);

    full_renderer::MaterialDesc badMaterial = validMaterialDesc();
    badMaterial.baseColorLinear[0] = 2.0f;
    expect(!full_renderer::isValid(renderer->createMaterial(badMaterial)),
        "out-of-range material color is rejected",
        failures);

    full_renderer::TextureDesc badTexture = validTextureDesc();
    badTexture.data = nullptr;
    expect(!full_renderer::isValid(renderer->createTexture(badTexture)),
        "null texture data is rejected",
        failures);

    badTexture = validTextureDesc();
    badTexture.dataSizeBytes = 1;
    expect(!full_renderer::isValid(renderer->createTexture(badTexture)),
        "short texture data is rejected",
        failures);

    const full_renderer::TextureHandle texture = renderer->createTexture(validTextureDesc());
    expect(full_renderer::isValid(texture), "valid texture creation succeeds", failures);

    full_renderer::MaterialDesc fallbackTerrainMaterial = validTerrainMaterialDesc({});
    expect(full_renderer::isValid(renderer->createMaterial(fallbackTerrainMaterial)),
        "terrain material accepts missing layer textures for fallback colors",
        failures);

    full_renderer::MaterialDesc terrainMaterial = validTerrainMaterialDesc(texture);
    terrainMaterial.terrain.uvScale = 0.0f;
    expect(!full_renderer::isValid(renderer->createMaterial(terrainMaterial)),
        "zero terrain material UV scale is rejected",
        failures);

    terrainMaterial = validTerrainMaterialDesc(texture);
    terrainMaterial.terrain.normalMapStrength = -0.01f;
    expect(!full_renderer::isValid(renderer->createMaterial(terrainMaterial)),
        "negative terrain normal-map strength is rejected",
        failures);

    terrainMaterial = validTerrainMaterialDesc(texture);
    terrainMaterial.terrain.normalMapStrength = 1.01f;
    expect(!full_renderer::isValid(renderer->createMaterial(terrainMaterial)),
        "out-of-range terrain normal-map strength is rejected",
        failures);

    terrainMaterial = validTerrainMaterialDesc(texture);
    terrainMaterial.terrain.layers[0].normalTexture = texture;
    terrainMaterial.terrain.normalMapStrength = 0.5f;
    expect(full_renderer::isValid(renderer->createMaterial(terrainMaterial)),
        "terrain material accepts valid layer normal textures",
        failures);

    terrainMaterial = validTerrainMaterialDesc(texture);
    terrainMaterial.terrain.layers[0].fallbackColorLinear[0] = 2.0f;
    expect(!full_renderer::isValid(renderer->createMaterial(terrainMaterial)),
        "out-of-range terrain layer fallback color is rejected",
        failures);

    renderer->shutdown();
}

void invalidLifecycleTransitionsAreRejected(int& failures)
{
    auto renderer = createTestRenderer();

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::NotInitialized,
        "beginFrame before initialize is rejected",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::NotInitialized,
        "endFrame before initialize is rejected",
        failures);
    expect(renderer->resize({640, 480}) == full_renderer::RendererResult::NotInitialized,
        "resize before initialize is rejected",
        failures);

    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::AlreadyInitialized,
        "double initialization is rejected",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::FrameNotInProgress,
        "endFrame without an active frame is rejected",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());

    expect(renderer->submit(validPacket(mesh, material)) == full_renderer::RendererResult::FrameNotInProgress,
        "submit before beginFrame is rejected",
        failures);
    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->resize({800, 600}) == full_renderer::RendererResult::FrameAlreadyInProgress,
        "resize during an active frame is rejected",
        failures);
    expect(renderer->beginFrame(validFrameDesc()) ==
            full_renderer::RendererResult::FrameAlreadyInProgress,
        "nested beginFrame is rejected",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame clears active frame",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::FrameNotInProgress,
        "second endFrame is rejected",
        failures);

    renderer->shutdown();
}

void invalidSubmitDataIsRejected(int& failures)
{
    auto renderer = createTestRenderer();
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);

    full_renderer::RenderPacket packet = validPacket(mesh, material);
    packet.drawItems = nullptr;
    expect(renderer->submit(packet) == full_renderer::RendererResult::InvalidArgument,
        "null draw item pointer with non-zero count is rejected",
        failures);

    packet = validPacket({999}, material);
    expect(renderer->submit(packet) == full_renderer::RendererResult::InvalidArgument,
        "invalid mesh handle is rejected",
        failures);

    packet = validPacket(mesh, {999});
    expect(renderer->submit(packet) == full_renderer::RendererResult::InvalidArgument,
        "invalid material handle is rejected",
        failures);

    packet = validPacket(mesh, material);
    packet.directionalLight.directionWorld[1] = 0.0f;
    expect(renderer->submit(packet) == full_renderer::RendererResult::InvalidArgument,
        "zero light direction is rejected",
        failures);

    packet = validPacket(mesh, material);
    packet.selectionOutline.enabled = true;
    packet.selectionOutline.thicknessPixels = -1.0f;
    expect(renderer->submit(packet) == full_renderer::RendererResult::InvalidArgument,
        "negative outline thickness is rejected",
        failures);

    packet = validPacket(mesh, material);
    packet.selectionOutline.enabled = true;
    packet.selectionOutline.colorLinear[3] = 2.0f;
    expect(renderer->submit(packet) == full_renderer::RendererResult::InvalidArgument,
        "out-of-range outline color is rejected",
        failures);

    packet = validPacket(mesh, material);
    packet.ssao.enabled = true;
    packet.ssao.radiusMeters = -1.0f;
    expect(renderer->submit(packet) == full_renderer::RendererResult::InvalidArgument,
        "invalid SSAO radius is rejected",
        failures);

    packet = validPacket(mesh, material);
    packet.ssao.enabled = true;
    packet.ssao.blurRadiusPixels = -1.0f;
    expect(renderer->submit(packet) == full_renderer::RendererResult::InvalidArgument,
        "invalid SSAO blur radius is rejected",
        failures);

    packet = validPacket(mesh, material);
    packet.colorGrading.enabled = true;
    packet.colorGrading.gamma = std::numeric_limits<float>::quiet_NaN();
    expect(renderer->submit(packet) == full_renderer::RendererResult::InvalidArgument,
        "invalid color grading gamma is rejected",
        failures);

    full_renderer::DrawItem fadedDraw;
    fadedDraw.mesh = mesh;
    fadedDraw.material = material;
    setIdentity(fadedDraw.model);
    fadedDraw.fade.enabled = true;
    fadedDraw.fade.visibility = std::numeric_limits<float>::quiet_NaN();
    packet = validPacket(mesh, material);
    packet.drawItems = &fadedDraw;
    packet.drawItemCount = 1;
    expect(renderer->submit(packet) == full_renderer::RendererResult::InvalidArgument,
        "invalid fade visibility is rejected",
        failures);

    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds",
        failures);
    expect(renderer->submit(validPacket(mesh, material)) == full_renderer::RendererResult::FrameNotInProgress,
        "submit after endFrame is rejected",
        failures);

    renderer->shutdown();
}

void terrainInstancedSubmissionReachesBackend(int& failures)
{
    auto renderer = createTestRenderer();
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());
    const full_renderer::TerrainChunkHandle terrainChunk = createTerrainChunk(*renderer, mesh, material);
    expect(full_renderer::isValid(terrainChunk), "terrain chunk creation succeeds", failures);

    const full_renderer::TerrainLodDesc updatedLods[] = {
        {mesh, material, 250.0f},
    };
    full_renderer::TerrainChunkDesc updatedTerrain;
    updatedTerrain.bounds = validTerrainBounds();
    updatedTerrain.lods = updatedLods;
    updatedTerrain.lodCount = 1;
    expect(renderer->updateTerrainChunk(terrainChunk, updatedTerrain) == full_renderer::RendererResult::Success,
        "terrain chunk descriptor update succeeds",
        failures);
    expect(renderer->getTerrainStats().chunksUpdatedSinceLastSubmit == 1,
        "terrain update is visible in terrain stats",
        failures);
    full_renderer::TerrainChunkDesc invalidTerrain = updatedTerrain;
    invalidTerrain.lodCount = 0;
    expect(renderer->updateTerrainChunk(terrainChunk, invalidTerrain) == full_renderer::RendererResult::InvalidDescriptor,
        "invalid terrain chunk descriptor update is rejected",
        failures);
    expect(renderer->updateTerrainChunk({}, updatedTerrain) == full_renderer::RendererResult::InvalidArgument,
        "invalid terrain chunk update handle is rejected",
        failures);

    full_renderer::TerrainSubmitDesc terrainSubmit;
    terrainSubmit.chunks = &terrainChunk;
    terrainSubmit.chunkCount = 1;
    terrainSubmit.cameraPositionWorld[0] = 0.0f;
    terrainSubmit.cameraPositionWorld[1] = 0.0f;
    terrainSubmit.cameraPositionWorld[2] = -3.0f;
    terrainSubmit.debug.captureChunkInfo = true;
    terrainSubmit.debug.captureBatchInfo = true;

    full_renderer::RenderPacket packet = validPacket(mesh, material);
    packet.directionalShadow.enabled = true;
    packet.directionalShadow.mapResolution = 1024;
    packet.directionalShadow.extentMeters = 10.0f;
    packet.directionalShadow.centerWorld[0] = 0.0f;
    packet.directionalShadow.centerWorld[1] = 0.0f;
    packet.directionalShadow.centerWorld[2] = 0.0f;
    packet.terrain = &terrainSubmit;

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->updateTerrainChunk(terrainChunk, updatedTerrain) == full_renderer::RendererResult::FrameAlreadyInProgress,
        "terrain chunk update during active frame is rejected",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "mixed normal and terrain instanced submission succeeds",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds",
        failures);

    const full_renderer::RendererStats renderStats = renderer->getStats();
    expect(renderStats.submittedDraws == 2, "fake backend receives one normal draw plus one terrain batch", failures);
    expect(renderStats.renderedDraws == 2, "fake backend renders one normal draw plus one terrain batch", failures);
    expect(renderStats.shadowCasterBatches == 1, "fake backend receives one shadow caster batch", failures);
    expect(renderStats.shadowPassDraws == 1, "fake backend records one shadow pass draw", failures);
    expect(renderer->getTerrainStats().terrainDraws == 1, "terrain stats count one instanced batch", failures);
    expect(renderer->getTerrainStats().shadowCasterChunks == 1, "terrain stats count one shadow caster chunk", failures);
    expect(renderer->getTerrainStats().chunksUpdatedSinceLastSubmit == 1,
        "terrain submission carries update churn stats",
        failures);
    expect(renderer->getTerrainStats().terrainBatchesByLod[0] == 1,
        "terrain stats count one LOD 0 instanced batch",
        failures);
    full_renderer::TerrainBatchDebugInfo batchDebug;
    expect(renderer->copyTerrainBatchDebugInfo(&batchDebug, 1) == 1,
        "renderer exposes terrain batch debug info",
        failures);
    expect(batchDebug.selectedLod == 0, "batch debug records selected LOD", failures);
    expect(batchDebug.instanceCount == 1, "batch debug records instance count", failures);
    full_renderer::TerrainChunkDebugInfo shadowCasterDebug;
    expect(renderer->copyTerrainShadowCasterDebugInfo(&shadowCasterDebug, 1) == 1,
        "renderer exposes terrain shadow-caster debug info",
        failures);
    expect(shadowCasterDebug.selectedAsShadowCaster,
        "terrain shadow-caster debug info marks caster selection",
        failures);

    renderer->shutdown();
}

void meshAndInstancedShadowCastersReachBackend(int& failures)
{
    auto renderer = createTestRenderer();
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());

    full_renderer::DrawItem meshDraw;
    meshDraw.mesh = mesh;
    meshDraw.material = material;
    setIdentity(meshDraw.model);
    meshDraw.bounds = validTerrainBounds();
    meshDraw.castsShadow = true;

    float instanceMatrices[2][16] = {};
    setIdentity(instanceMatrices[0]);
    setIdentity(instanceMatrices[1]);
    instanceMatrices[1][12] = 1.0f;

    full_renderer::InstancedDrawDesc instancedDraw;
    instancedDraw.mesh = mesh;
    instancedDraw.material = material;
    instancedDraw.modelMatrices = &instanceMatrices[0][0];
    instancedDraw.instanceCount = 2;
    instancedDraw.bounds = validTerrainBounds();
    instancedDraw.castsShadow = true;

    full_renderer::RenderPacket packet = validPacket(mesh, material);
    packet.drawItems = &meshDraw;
    packet.drawItemCount = 1;
    packet.instancedDraws = &instancedDraw;
    packet.instancedDrawCount = 1;
    packet.directionalShadow.enabled = true;
    packet.directionalShadow.mapResolution = 1024;
    packet.directionalShadow.extentMeters = 10.0f;
    packet.directionalShadow.centerWorld[0] = 0.0f;
    packet.directionalShadow.centerWorld[1] = 0.0f;
    packet.directionalShadow.centerWorld[2] = 0.0f;

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "static and instanced mesh shadow caster submission succeeds",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds",
        failures);

    const full_renderer::RendererStats stats = renderer->getStats();
    expect(stats.submittedDraws == 2, "fake backend receives one normal draw plus one instanced batch", failures);
    expect(stats.renderedDraws == 2, "fake backend renders one normal draw plus one instanced batch", failures);
    expect(stats.shadowCasterBatches == 2, "fake backend receives static and instanced shadow batches", failures);
    expect(stats.shadowPassDraws == 2, "fake backend records static and instanced shadow draws", failures);
    expect(stats.shadowStaticMeshCastersByCascade[0] == 1,
        "stats count static mesh shadow caster",
        failures);
    expect(stats.shadowInstancedMeshCasterBatchesByCascade[0] == 1,
        "stats count instanced mesh shadow caster batch",
        failures);
    expect(stats.shadowStaticMeshReceivers == 1,
        "stats count static mesh CSM receiver",
        failures);
    expect(stats.shadowInstancedMeshReceiverBatches == 1,
        "stats count instanced mesh CSM receiver batch",
        failures);

    renderer->shutdown();
}

void skinnedShadowCastersReachBackend(int& failures)
{
    auto renderer = createTestRenderer();
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());
    full_renderer::SkeletonJointDesc joints[2] = {};
    const full_renderer::SkeletonHandle skeleton = renderer->createSkeleton(validSkeletonDesc(joints));
    expect(full_renderer::isValid(skeleton), "skeleton creation succeeds", failures);

    full_renderer::SkinnedMeshVertex vertices[3] = {};
    std::uint16_t indices[3] = {};
    const full_renderer::SkinnedMeshHandle skinnedMesh =
        renderer->createSkinnedMesh(validSkinnedMeshDesc(skeleton, vertices, indices));
    expect(full_renderer::isValid(skinnedMesh), "skinned mesh creation succeeds", failures);

    float paletteMatrices[2][16] = {};
    setIdentity(paletteMatrices[0]);
    setIdentity(paletteMatrices[1]);

    full_renderer::AnimatedDrawItem animatedDraw;
    animatedDraw.mesh = skinnedMesh;
    animatedDraw.material = material;
    setIdentity(animatedDraw.model);
    animatedDraw.bounds = validTerrainBounds();
    animatedDraw.palette.skinningMatrices = &paletteMatrices[0][0];
    animatedDraw.palette.matrixCount = 2;
    animatedDraw.castsShadow = true;

    full_renderer::RenderPacket packet = validPacket(mesh, material);
    packet.animatedDraws = &animatedDraw;
    packet.animatedDrawCount = 1;
    packet.directionalShadow.enabled = true;
    packet.directionalShadow.mapResolution = 1024;
    packet.directionalShadow.extentMeters = 10.0f;
    packet.directionalShadow.centerWorld[0] = 0.0f;
    packet.directionalShadow.centerWorld[1] = 0.0f;
    packet.directionalShadow.centerWorld[2] = 0.0f;

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "skinned mesh shadow caster submission succeeds",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds",
        failures);

    const full_renderer::RendererStats stats = renderer->getStats();
    expect(stats.submittedAnimatedDraws == 1, "stats count submitted animated draw", failures);
    expect(stats.renderedAnimatedDraws == 1, "stats count rendered animated draw", failures);
    expect(stats.shadowCasterBatches == 1, "fake backend receives one skinned shadow caster", failures);
    expect(stats.shadowSkinnedMeshCastersByCascade[0] == 1,
        "stats count skinned shadow caster in cascade 0",
        failures);
    expect(stats.shadowSkinnedMeshPassDraws == 1, "stats count skinned shadow pass draw", failures);
    expect(stats.shadowSkinnedMeshReceivers == 1, "stats count skinned CSM receiver", failures);

    renderer->shutdown();
}

void selectionOutlineSubmissionReachesBackend(int& failures)
{
    auto renderer = createTestRenderer();
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());
    full_renderer::SkeletonJointDesc joints[2] = {};
    const full_renderer::SkeletonHandle skeleton = renderer->createSkeleton(validSkeletonDesc(joints));
    expect(full_renderer::isValid(skeleton), "skeleton creation succeeds", failures);

    full_renderer::SkinnedMeshVertex vertices[3] = {};
    std::uint16_t indices[3] = {};
    const full_renderer::SkinnedMeshHandle skinnedMesh =
        renderer->createSkinnedMesh(validSkinnedMeshDesc(skeleton, vertices, indices));
    expect(full_renderer::isValid(skinnedMesh), "skinned mesh creation succeeds", failures);

    full_renderer::DrawItem meshDraw;
    meshDraw.mesh = mesh;
    meshDraw.material = material;
    setIdentity(meshDraw.model);
    meshDraw.bounds = validTerrainBounds();
    meshDraw.selected = true;

    float instanceMatrices[2][16] = {};
    setIdentity(instanceMatrices[0]);
    setIdentity(instanceMatrices[1]);
    full_renderer::InstancedDrawDesc instancedDraw;
    instancedDraw.mesh = mesh;
    instancedDraw.material = material;
    instancedDraw.modelMatrices = &instanceMatrices[0][0];
    instancedDraw.instanceCount = 2;
    instancedDraw.bounds = validTerrainBounds();
    instancedDraw.selected = true;

    float paletteMatrices[2][16] = {};
    setIdentity(paletteMatrices[0]);
    setIdentity(paletteMatrices[1]);
    full_renderer::AnimatedDrawItem animatedDraw;
    animatedDraw.mesh = skinnedMesh;
    animatedDraw.material = material;
    setIdentity(animatedDraw.model);
    animatedDraw.bounds = validTerrainBounds();
    animatedDraw.palette.skinningMatrices = &paletteMatrices[0][0];
    animatedDraw.palette.matrixCount = 2;
    animatedDraw.selected = true;

    full_renderer::RenderPacket packet = validPacket(mesh, material);
    packet.drawItems = &meshDraw;
    packet.drawItemCount = 1;
    packet.instancedDraws = &instancedDraw;
    packet.instancedDrawCount = 1;
    packet.animatedDraws = &animatedDraw;
    packet.animatedDrawCount = 1;
    packet.selectionOutline.enabled = true;
    packet.selectionOutline.thicknessPixels = 3.0f;

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "selected static, instanced, and skinned submissions succeed",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds",
        failures);

    const full_renderer::RendererStats stats = renderer->getStats();
    expect(stats.selectionOutlineEnabled, "selection outline is enabled in stats", failures);
    expect(stats.selectionMaskTargetValid == 1, "selection mask target is valid in stats", failures);
    expect(stats.selectedStaticMeshDraws == 1, "stats count selected static mesh", failures);
    expect(stats.selectedInstancedBatches == 1, "stats count selected instanced batch", failures);
    expect(stats.selectedInstances == 2, "stats count selected batch instances", failures);
    expect(stats.selectedSkinnedMeshDraws == 1, "stats count selected skinned mesh", failures);
    expect(stats.selectionMaskDraws == 3, "stats count selection mask submissions", failures);
    expect(stats.selectionOutlineDraws == 1, "stats count one outline composite pass", failures);

    renderer->shutdown();
}

void invalidInstancedSubmitDataIsRejected(int& failures)
{
    auto renderer = createTestRenderer();
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());

    full_renderer::InstancedDrawDesc instancedDraw;
    instancedDraw.mesh = mesh;
    instancedDraw.material = material;
    instancedDraw.instanceCount = 1;

    full_renderer::RenderPacket packet = validPacket(mesh, material);
    packet.instancedDraws = &instancedDraw;
    packet.instancedDrawCount = 1;

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::InvalidArgument,
        "instanced draw with null model matrices is rejected",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds",
        failures);

    renderer->shutdown();
}

void structureFadeSubmissionReachesBackend(int& failures)
{
    auto renderer = createTestRenderer();
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());
    full_renderer::SkeletonJointDesc joints[2] = {};
    const full_renderer::SkeletonHandle skeleton = renderer->createSkeleton(validSkeletonDesc(joints));
    expect(full_renderer::isValid(skeleton), "skeleton creation succeeds", failures);

    full_renderer::SkinnedMeshVertex vertices[3] = {};
    std::uint16_t indices[3] = {};
    const full_renderer::SkinnedMeshHandle skinnedMesh =
        renderer->createSkinnedMesh(validSkinnedMeshDesc(skeleton, vertices, indices));
    expect(full_renderer::isValid(skinnedMesh), "skinned mesh creation succeeds", failures);

    full_renderer::DrawItem meshDraw;
    meshDraw.mesh = mesh;
    meshDraw.material = material;
    setIdentity(meshDraw.model);
    meshDraw.fade.enabled = true;
    meshDraw.fade.mode = full_renderer::FadeMode::Dithered;
    meshDraw.fade.visibility = 0.5f;

    float instanceMatrices[2][16] = {};
    setIdentity(instanceMatrices[0]);
    setIdentity(instanceMatrices[1]);
    full_renderer::InstancedDrawDesc instancedDraw;
    instancedDraw.mesh = mesh;
    instancedDraw.material = material;
    instancedDraw.modelMatrices = &instanceMatrices[0][0];
    instancedDraw.instanceCount = 2;
    instancedDraw.bounds = validTerrainBounds();
    instancedDraw.fade.enabled = true;
    instancedDraw.fade.mode = full_renderer::FadeMode::Alpha;
    instancedDraw.fade.visibility = 0.0f;

    float paletteMatrices[2][16] = {};
    setIdentity(paletteMatrices[0]);
    setIdentity(paletteMatrices[1]);
    full_renderer::AnimatedDrawItem animatedDraw;
    animatedDraw.mesh = skinnedMesh;
    animatedDraw.material = material;
    setIdentity(animatedDraw.model);
    animatedDraw.bounds = validTerrainBounds();
    animatedDraw.palette.skinningMatrices = &paletteMatrices[0][0];
    animatedDraw.palette.matrixCount = 2;
    animatedDraw.fade.enabled = true;
    animatedDraw.fade.mode = full_renderer::FadeMode::Alpha;
    animatedDraw.fade.visibility = 1.0f;

    full_renderer::RenderPacket packet = validPacket(mesh, material);
    packet.drawItems = &meshDraw;
    packet.drawItemCount = 1;
    packet.instancedDraws = &instancedDraw;
    packet.instancedDrawCount = 1;
    packet.animatedDraws = &animatedDraw;
    packet.animatedDrawCount = 1;

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "faded static, instanced, and skinned submissions succeed",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds",
        failures);

    const full_renderer::RendererStats stats = renderer->getStats();
    expect(stats.structureFadeSubmittedCount == 3,
        "stats count submitted fade descriptors",
        failures);
    expect(stats.structureFadeActiveCount == 2,
        "stats count active faded draws and batches",
        failures);
    expect(stats.structureFadeFullyVisibleCount == 1,
        "stats count fully visible enabled fade descriptor",
        failures);
    expect(stats.structureFadePartiallyFadedCount == 1,
        "stats count partially faded descriptor",
        failures);
    expect(stats.structureFadeFullyHiddenCount == 1,
        "stats count fully hidden descriptor",
        failures);
    expect(stats.structureFadeAlphaDraws == 1,
        "stats count active alpha fade draw",
        failures);
    expect(stats.structureFadeDitherDraws == 1,
        "stats count active dither fade draw",
        failures);
    expect(stats.structureFadeUnsupportedTargetCount == 0,
        "object-like fade targets are supported",
        failures);

    renderer->shutdown();
}

void ssaoSubmissionReachesBackend(int& failures)
{
    auto renderer = createTestRenderer();
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());

    full_renderer::DrawItem meshDraw;
    meshDraw.mesh = mesh;
    meshDraw.material = material;
    setIdentity(meshDraw.model);
    meshDraw.bounds = validTerrainBounds();

    float instanceMatrices[2][16] = {};
    setIdentity(instanceMatrices[0]);
    setIdentity(instanceMatrices[1]);
    full_renderer::InstancedDrawDesc instancedDraw;
    instancedDraw.mesh = mesh;
    instancedDraw.material = material;
    instancedDraw.modelMatrices = &instanceMatrices[0][0];
    instancedDraw.instanceCount = 2;
    instancedDraw.bounds = validTerrainBounds();

    full_renderer::RenderPacket packet = validPacket(mesh, material);
    packet.drawItems = &meshDraw;
    packet.drawItemCount = 1;
    packet.instancedDraws = &instancedDraw;
    packet.instancedDrawCount = 1;
    packet.ssao.enabled = true;
    packet.ssao.debugVisualize = true;
    packet.ssao.sampleCount = 8;
    packet.ssao.halfResolution = true;
    packet.ssao.blurEnabled = true;
    packet.ssao.blurRadiusPixels = 1.0f;

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "SSAO-enabled submission succeeds",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds",
        failures);

    const full_renderer::RendererStats stats = renderer->getStats();
    expect(stats.ssaoEnabled, "SSAO is enabled in stats", failures);
    expect(stats.ssaoDepthTargetValid == 1, "SSAO depth target is valid", failures);
    expect(stats.ssaoOutputTargetValid == 1, "SSAO output target is valid", failures);
    expect(stats.ssaoBlurTargetValid == 1, "SSAO blur target is valid", failures);
    expect(stats.ssaoHalfResolution, "SSAO half-resolution mode is reported", failures);
    expect(stats.ssaoAoWidth == 320 && stats.ssaoAoHeight == 240,
        "SSAO half-resolution dimensions are reported",
        failures);
    expect(stats.ssaoBlurEnabled, "SSAO blur mode is reported", failures);
    expect(stats.ssaoInputDepthValid == 1, "SSAO input depth is available", failures);
    expect(stats.ssaoDepthPassDraws == 2, "SSAO depth capture counts mesh and instanced batch", failures);
    expect(stats.ssaoPassDraws == 1, "SSAO records one fullscreen AO pass", failures);
    expect(stats.ssaoBlurPassDraws == 2, "SSAO records horizontal and vertical blur passes", failures);
    expect(stats.ssaoCompositeDraws == 1, "SSAO records one composite pass", failures);
    expect(stats.ssaoDebugVisualized, "SSAO debug visualization state is reported", failures);

    renderer->shutdown();
}

void colorGradingSubmissionReachesBackend(int& failures)
{
    auto renderer = createTestRenderer();
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());

    full_renderer::RenderPacket packet = validPacket(mesh, material);
    packet.colorGrading.enabled = true;
    packet.colorGrading.tonemap.enabled = true;
    packet.colorGrading.tonemap.operatorType = full_renderer::TonemapOperator::Reinhard;
    packet.colorGrading.tonemap.exposureStops = 1.0f;
    packet.colorGrading.contrast = 1.25f;
    packet.colorGrading.saturation = 0.75f;
    packet.colorGrading.gamma = 1.1f;
    packet.colorGrading.lut.enabled = true;
    packet.colorGrading.lut.texture.id = 99;
    packet.colorGrading.lut.strength = 0.5f;
    packet.colorGrading.debugMode = full_renderer::ColorGradingDebugMode::TonemapOnly;

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "color grading enabled submission succeeds",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds",
        failures);

    const full_renderer::RendererStats stats = renderer->getStats();
    expect(stats.colorGradingEnabled, "color grading is enabled in stats", failures);
    expect(stats.colorGradingPassSubmitted, "color grading final pass is submitted", failures);
    expect(stats.colorGradingSceneColorTargetValid == 1,
        "color grading scene color target is valid",
        failures);
    expect(stats.colorGradingResourceValid == 1,
        "color grading resources are valid in fake backend",
        failures);
    expect(stats.colorGradingTonemapEnabled, "color grading tonemap is enabled", failures);
    expect(stats.colorGradingTonemapOperator == full_renderer::TonemapOperator::Reinhard,
        "color grading tonemap operator is reported",
        failures);
    expect(stats.colorGradingExposureStops == 1.0f,
        "color grading exposure is reported",
        failures);
    expect(stats.colorGradingContrast == 1.25f &&
            stats.colorGradingSaturation == 0.75f &&
            stats.colorGradingGamma == 1.1f,
        "color grading controls are reported",
        failures);
    expect(stats.colorGradingLutEnabled,
        "color grading LUT request is reported",
        failures);
    expect(!stats.colorGradingLutActive && stats.colorGradingLutFallbackCount == 1,
        "unsupported LUT sampling falls back in stats",
        failures);
    expect(stats.colorGradingDebugMode == full_renderer::ColorGradingDebugMode::TonemapOnly,
        "color grading debug mode is reported",
        failures);

    renderer->shutdown();
}

void cascadeFrustaDebugSubmissionDoesNotFailFrame(int& failures)
{
    auto renderer = createTestRenderer();
    expect(renderer->initialize(validInitDesc()) == full_renderer::RendererResult::Success,
        "valid initialization succeeds",
        failures);

    const full_renderer::MeshHandle mesh = renderer->createMesh(validMeshDesc());
    const full_renderer::MaterialHandle material = renderer->createMaterial(validMaterialDesc());
    const full_renderer::TerrainChunkHandle terrainChunk = createTerrainChunk(*renderer, mesh, material);
    expect(full_renderer::isValid(terrainChunk), "terrain chunk creation succeeds", failures);

    full_renderer::TerrainSubmitDesc terrainSubmit;
    terrainSubmit.chunks = &terrainChunk;
    terrainSubmit.chunkCount = 1;
    terrainSubmit.cameraPositionWorld[0] = 0.0f;
    terrainSubmit.cameraPositionWorld[1] = 0.0f;
    terrainSubmit.cameraPositionWorld[2] = -3.0f;

    full_renderer::RenderPacket packet = validPacket(mesh, material);
    packet.directionalShadow.enabled = true;
    packet.directionalShadow.mapResolution = 1024;
    packet.directionalShadow.extentMeters = 10.0f;
    packet.directionalShadow.cascadeCount = full_renderer::kMaxDirectionalShadowCascades;
    packet.directionalShadow.cascadeCameraNearMeters = 0.1f;
    packet.directionalShadow.cascadeCameraFarMeters = 100.0f;
    packet.directionalShadow.cascadeShadowDistanceMeters = 60.0f;
    packet.directionalShadow.debugDrawCascadeFrusta = true;
    packet.terrain = &terrainSubmit;

    expect(renderer->beginFrame(validFrameDesc()) == full_renderer::RendererResult::Success,
        "beginFrame succeeds",
        failures);
    expect(renderer->submit(packet) == full_renderer::RendererResult::Success,
        "cascade frusta debug draw does not fail renderer submission",
        failures);
    const full_renderer::RendererStats stats = renderer->getStats();
    expect(stats.shadowCascadeCount == full_renderer::kMaxDirectionalShadowCascades,
        "fake backend records active shadow cascade count",
        failures);
    expect(stats.shadowCascadeRenderTargetCount == full_renderer::kMaxDirectionalShadowCascades,
        "fake backend records per-cascade render targets",
        failures);
    expect(stats.shadowCascadeRenderTargetValid[0] == 1,
        "fake backend marks cascade 0 render target valid",
        failures);
    expect(renderer->endFrame() == full_renderer::RendererResult::Success,
        "endFrame succeeds",
        failures);

    renderer->shutdown();
}
} // namespace

int main()
{
    int failures = 0;

    lifecycleSuccess(failures);
    invalidDimensionsAreRejected(failures);
    staleResourceHandlesAreReported(failures);
    mockedAllocationFailuresAreReportedAndRecoverable(failures);
    staleDecalTextureReferencesUseFallbackOrSkip(failures);
    staleParticleTextureReferencesUseFallbackOrSkip(failures);
    staleMaterialTextureReferencesAreReported(failures);
    skinnedPaletteAndStaleSkinnedResourcesAreRejected(failures);
    mockedOptionalResourceFailuresSkipPassesSafely(failures);
    resourceDescriptorsAreValidated(failures);
    invalidLifecycleTransitionsAreRejected(failures);
    invalidSubmitDataIsRejected(failures);
    terrainInstancedSubmissionReachesBackend(failures);
    meshAndInstancedShadowCastersReachBackend(failures);
    skinnedShadowCastersReachBackend(failures);
    selectionOutlineSubmissionReachesBackend(failures);
    structureFadeSubmissionReachesBackend(failures);
    ssaoSubmissionReachesBackend(failures);
    colorGradingSubmissionReachesBackend(failures);
    invalidInstancedSubmitDataIsRejected(failures);
    cascadeFrustaDebugSubmissionDoesNotFailFrame(failures);

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
