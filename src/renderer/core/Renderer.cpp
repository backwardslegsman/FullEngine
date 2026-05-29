#include "renderer/core/Renderer.hpp"

#include "renderer/bgfx/BgfxRenderDevice.hpp"
#include "renderer/animation/AnimationSystem.hpp"
#include "renderer/core/RenderDevice.hpp"
#include "renderer/debug/FrameBudget.hpp"
#include "renderer/scene/ColorGrading.hpp"
#include "renderer/scene/CullingDiagnostics.hpp"
#include "renderer/debug/TerrainDebug.hpp"
#include "renderer/scene/Decal.hpp"
#include "renderer/scene/Environment.hpp"
#include "renderer/scene/Fade.hpp"
#include "renderer/scene/Frustum.hpp"
#include "renderer/scene/MaterialPolicy.hpp"
#include "renderer/scene/Math.hpp"
#include "renderer/scene/Particle.hpp"
#include "renderer/scene/Shadow.hpp"
#include "renderer/scene/Ssao.hpp"
#include "renderer/scene/Weather.hpp"
#include "renderer/resources/AssetContracts.hpp"
#include "renderer/terrain/TerrainSystem.hpp"

#include <cmath>
#include <chrono>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace full_renderer::core
{
namespace
{
using BudgetClock = std::chrono::steady_clock;

bool hasValidDimensions(const std::uint32_t width, const std::uint32_t height) noexcept
{
    return width > 0 && height > 0;
}

std::uint64_t elapsedMicroseconds(
    const BudgetClock::time_point start,
    const BudgetClock::time_point end) noexcept
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
}

void recordElapsedStage(
    FrameBudgetStats& stats,
    const FrameBudgetStage stage,
    const BudgetClock::time_point start,
    const BudgetClock::time_point end) noexcept
{
    debug::recordFrameBudgetStage(stats, stage, elapsedMicroseconds(start, end));
}

void addFrameBudgetBytes(
    FrameBudgetStats& stats,
    std::uint64_t& categoryBytes,
    const std::uint64_t bytes) noexcept
{
    categoryBytes += bytes;
    stats.totalStagedBytes += bytes;
    if (bytes > 0)
    {
        ++stats.frameAllocationEstimateCount;
    }
}

void applyFrameBudgetSubmissionEstimate(
    FrameBudgetStats& destination,
    const FrameBudgetStats& estimate) noexcept
{
    destination.totalSubmittedRenderables = estimate.totalSubmittedRenderables;
    destination.staticDrawStagedBytes = estimate.staticDrawStagedBytes;
    destination.instanceStagedBytes = estimate.instanceStagedBytes;
    destination.skinnedPaletteStagedBytes = estimate.skinnedPaletteStagedBytes;
    destination.decalStagedBytes = estimate.decalStagedBytes;
    destination.particleStagedBytes = estimate.particleStagedBytes;
    destination.totalStagedBytes = estimate.totalStagedBytes;
    destination.frameAllocationEstimateCount = estimate.frameAllocationEstimateCount;
}

scene::DecalRenderPlan buildCameraCulledDecalPlan(const RenderPacket& packet) noexcept
{
    if (packet.decals == nullptr || !packet.decals->enabled)
    {
        return scene::buildDecalRenderPlan(packet.decals);
    }

    float viewProjection[16] = {};
    scene::multiplyColumnMajor4x4(packet.view.projection, packet.view.view, viewProjection);
    const scene::Frustum frustum = scene::extractFrustumFromViewProjection(viewProjection);
    return scene::buildDecalRenderPlan(packet.decals, &frustum);
}

RendererResult validateInitDesc(const RendererInitDesc& desc) noexcept
{
    if (!hasValidDimensions(desc.backbufferWidth, desc.backbufferHeight))
    {
        return RendererResult::InvalidDescriptor;
    }

    return RendererResult::Success;
}

RendererResult validateFrameDesc(const FrameDesc& desc) noexcept
{
    if (!hasValidDimensions(desc.backbufferWidth, desc.backbufferHeight))
    {
        return RendererResult::InvalidDescriptor;
    }

    if (!std::isfinite(desc.deltaSeconds) || desc.deltaSeconds < 0.0)
    {
        return RendererResult::InvalidDescriptor;
    }

    return RendererResult::Success;
}

RendererResult validateResizeDesc(const RendererResizeDesc& desc) noexcept
{
    if (!hasValidDimensions(desc.backbufferWidth, desc.backbufferHeight))
    {
        return RendererResult::InvalidDescriptor;
    }

    return RendererResult::Success;
}

bool hasFiniteValues(const float* values, const std::size_t count) noexcept
{
    for (std::size_t index = 0; index < count; ++index)
    {
        if (!std::isfinite(values[index]))
        {
            return false;
        }
    }

    return true;
}

bool isUnitRangeColor(const float* values, const std::size_t count) noexcept
{
    for (std::size_t index = 0; index < count; ++index)
    {
        if (!std::isfinite(values[index]) || values[index] < 0.0f || values[index] > 1.0f)
        {
            return false;
        }
    }

    return true;
}

bool isValidSelectionOutlineDesc(const SelectionOutlineDesc& desc) noexcept
{
    if (!desc.enabled)
    {
        return true;
    }

    return isUnitRangeColor(desc.colorLinear, 4) &&
        std::isfinite(desc.thicknessPixels) &&
        desc.thicknessPixels >= 0.0f;
}

bool hasNonZeroLength(const float x, const float y, const float z) noexcept
{
    constexpr float kMinimumLengthSquared = 0.000001f;
    return (x * x + y * y + z * z) > kMinimumLengthSquared;
}

RendererResult validateMeshDesc(const MeshDesc& desc) noexcept
{
    return resources::validateMeshAssetContract(desc);
}

RendererResult validateMaterialDesc(const MaterialDesc& desc) noexcept
{
    if (!scene::isValidMaterialPolicyDesc(desc))
    {
        return RendererResult::InvalidDescriptor;
    }

    if (!isUnitRangeColor(desc.baseColorLinear, 4))
    {
        return RendererResult::InvalidDescriptor;
    }

    if (desc.kind == MaterialKind::TerrainSplat)
    {
        if (!std::isfinite(desc.terrain.uvScale) || desc.terrain.uvScale <= 0.0f)
        {
            return RendererResult::InvalidDescriptor;
        }

        if (!std::isfinite(desc.terrain.normalMapStrength) ||
            desc.terrain.normalMapStrength < 0.0f ||
            desc.terrain.normalMapStrength > 1.0f)
        {
            return RendererResult::InvalidDescriptor;
        }

        for (const TerrainMaterialLayerDesc& layer : desc.terrain.layers)
        {
            if (!isUnitRangeColor(layer.fallbackColorLinear, 4))
            {
                return RendererResult::InvalidDescriptor;
            }
        }
    }

    return RendererResult::Success;
}

RendererResult validateTextureDesc(const TextureDesc& desc) noexcept
{
    return resources::validateTextureAssetContract(desc);
}

RendererResult validateRenderPacket(const RenderPacket& packet) noexcept
{
    if (!hasFiniteValues(packet.view.view, 16) ||
        !hasFiniteValues(packet.view.projection, 16) ||
        !hasFiniteValues(packet.directionalLight.directionWorld, 3) ||
        !hasFiniteValues(packet.directionalLight.colorLinear, 3) ||
        !std::isfinite(packet.directionalLight.intensity) ||
        packet.directionalLight.intensity < 0.0f ||
        !isUnitRangeColor(packet.directionalLight.colorLinear, 3) ||
        !hasNonZeroLength(
            packet.directionalLight.directionWorld[0],
            packet.directionalLight.directionWorld[1],
            packet.directionalLight.directionWorld[2]))
    {
        return RendererResult::InvalidArgument;
    }

    if (!scene::isValidDirectionalShadowDesc(packet.directionalShadow))
    {
        return RendererResult::InvalidArgument;
    }

    if (!scene::isValidEnvironmentDesc(packet.environment))
    {
        return RendererResult::InvalidArgument;
    }

    if (!scene::isValidWeatherDesc(packet.weather))
    {
        return RendererResult::InvalidArgument;
    }

    if (!isValidSelectionOutlineDesc(packet.selectionOutline))
    {
        return RendererResult::InvalidArgument;
    }

    if (!scene::isValidSsaoDesc(packet.ssao))
    {
        return RendererResult::InvalidArgument;
    }

    if (!scene::isValidColorGradingDesc(packet.colorGrading))
    {
        return RendererResult::InvalidArgument;
    }

    if (packet.decals != nullptr && !scene::isValidDecalSubmitDesc(*packet.decals))
    {
        return RendererResult::InvalidArgument;
    }

    if (packet.particles != nullptr && !scene::isValidParticleSubmitDesc(*packet.particles))
    {
        return RendererResult::InvalidArgument;
    }

    if (packet.drawItemCount > 0 && packet.drawItems == nullptr)
    {
        return RendererResult::InvalidArgument;
    }

    if (packet.instancedDrawCount > 0 && packet.instancedDraws == nullptr)
    {
        return RendererResult::InvalidArgument;
    }

    if (packet.animatedDrawCount > 0 && packet.animatedDraws == nullptr)
    {
        return RendererResult::InvalidArgument;
    }

    if (packet.terrain != nullptr)
    {
        if (packet.terrain->chunkCount > 0 && packet.terrain->chunks == nullptr)
        {
            return RendererResult::InvalidArgument;
        }

        if (!hasFiniteValues(packet.terrain->cameraPositionWorld, 3))
        {
            return RendererResult::InvalidArgument;
        }
    }

    for (std::uint32_t index = 0; index < packet.drawItemCount; ++index)
    {
        const DrawItem& item = packet.drawItems[index];
        if (!isValid(item.mesh) ||
            !isValid(item.material) ||
            !hasFiniteValues(item.model, 16) ||
            !scene::isValidFadeDesc(item.fade) ||
            (item.castsShadow && !scene::isValidAabb(item.bounds)))
        {
            return RendererResult::InvalidArgument;
        }
    }

    for (std::uint32_t index = 0; index < packet.instancedDrawCount; ++index)
    {
        const InstancedDrawDesc& item = packet.instancedDraws[index];
        if (!isValid(item.mesh) ||
            !isValid(item.material) ||
            item.modelMatrices == nullptr ||
            item.instanceCount == 0 ||
            !scene::isValidFadeDesc(item.fade) ||
            (item.castsShadow && !scene::isValidAabb(item.bounds)))
        {
            return RendererResult::InvalidArgument;
        }

        for (std::uint32_t instanceIndex = 0; instanceIndex < item.instanceCount; ++instanceIndex)
        {
            if (!hasFiniteValues(item.modelMatrices + instanceIndex * 16U, 16))
            {
                return RendererResult::InvalidArgument;
            }
        }
    }

    return RendererResult::Success;
}

bool buildShadowCascadeSet(
    const DirectionalLightDesc& light,
    const DirectionalShadowDesc& shadow,
    const RenderViewDesc& view,
    scene::DirectionalShadowCascadeSet& outCascadeSet) noexcept
{
    if (scene::clampShadowCascadeCount(shadow.cascadeCount) == 1U)
    {
        scene::DirectionalShadowSplit split;
        if (!scene::buildDirectionalShadowSplit(light, shadow, split))
        {
            return false;
        }
        outCascadeSet.cascadeCount = 1;
        outCascadeSet.splits[0] = split;
        outCascadeSet.splits[0].splitIndex = 0;
        return true;
    }

    return scene::buildDirectionalShadowCascadeSet(light, shadow, view, outCascadeSet) &&
        outCascadeSet.cascadeCount > 0;
}

void appendMatrix(const float matrix[16], std::vector<float>& outMatrices)
{
    outMatrices.insert(outMatrices.end(), matrix, matrix + 16);
}

void appendPublicInstancedBatches(
    const RenderPacket& packet,
    std::vector<InstancedDrawBatch>& outBatches)
{
    outBatches.reserve(outBatches.size() + packet.instancedDrawCount);
    for (std::uint32_t index = 0; index < packet.instancedDrawCount; ++index)
    {
        const InstancedDrawDesc& desc = packet.instancedDraws[index];

        InstancedDrawBatch batch;
        batch.mesh = desc.mesh;
        batch.material = desc.material;
        batch.bounds = desc.bounds;
        batch.modelMatrices = desc.modelMatrices;
        batch.instanceCount = desc.instanceCount;
        batch.selected = desc.selected;
        batch.fade = desc.fade;
        outBatches.push_back(batch);
    }
}

RendererResult appendMeshShadowCasterBatches(
    const RenderPacket& packet,
    const scene::DirectionalShadowCascadeSet& cascadeSet,
    std::vector<float>& outModelMatrices,
    std::vector<InstancedDrawBatch>& outBatches)
{
    if (!packet.directionalShadow.enabled)
    {
        return RendererResult::Success;
    }

    outModelMatrices.reserve(
        outModelMatrices.size() +
        static_cast<std::size_t>(packet.drawItemCount) *
            static_cast<std::size_t>(cascadeSet.cascadeCount) *
            16U);

    for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeSet.cascadeCount; ++cascadeIndex)
    {
        const scene::Frustum lightFrustum =
            scene::extractFrustumFromViewProjection(cascadeSet.splits[cascadeIndex].matrices.viewProjection);

        for (std::uint32_t drawIndex = 0; drawIndex < packet.drawItemCount; ++drawIndex)
        {
            const DrawItem& draw = packet.drawItems[drawIndex];
            if (!draw.castsShadow)
            {
                continue;
            }

            if (!scene::intersects(lightFrustum, draw.bounds))
            {
                continue;
            }

            const std::uint32_t matrixOffset = static_cast<std::uint32_t>(outModelMatrices.size());
            appendMatrix(draw.model, outModelMatrices);

            InstancedDrawBatch batch;
            batch.mesh = draw.mesh;
            batch.material = draw.material;
            batch.bounds = draw.bounds;
            batch.modelMatrices = outModelMatrices.data() + matrixOffset;
            batch.instanceCount = 1;
            batch.shadowCascadeIndex = cascadeIndex;
            batch.shadowCasterKind = ShadowCasterKind::StaticMesh;
            outBatches.push_back(batch);
        }

        for (std::uint32_t batchIndex = 0; batchIndex < packet.instancedDrawCount; ++batchIndex)
        {
            const InstancedDrawDesc& instanced = packet.instancedDraws[batchIndex];
            if (!instanced.castsShadow)
            {
                continue;
            }

            if (!scene::intersects(lightFrustum, instanced.bounds))
            {
                continue;
            }

            InstancedDrawBatch batch;
            batch.mesh = instanced.mesh;
            batch.material = instanced.material;
            batch.bounds = instanced.bounds;
            batch.modelMatrices = instanced.modelMatrices;
            batch.instanceCount = instanced.instanceCount;
            batch.shadowCascadeIndex = cascadeIndex;
            batch.shadowCasterKind = ShadowCasterKind::InstancedMesh;
            outBatches.push_back(batch);
        }
    }

    return RendererResult::Success;
}

RendererResult appendSkinnedShadowCasterBatches(
    const RenderPacket& packet,
    const scene::DirectionalShadowCascadeSet& cascadeSet,
    std::vector<SkinnedShadowDrawBatch>& outBatches)
{
    if (!packet.directionalShadow.enabled)
    {
        return RendererResult::Success;
    }

    outBatches.reserve(
        outBatches.size() +
        static_cast<std::size_t>(packet.animatedDrawCount) *
            static_cast<std::size_t>(cascadeSet.cascadeCount));

    for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeSet.cascadeCount; ++cascadeIndex)
    {
        const scene::Frustum lightFrustum =
            scene::extractFrustumFromViewProjection(cascadeSet.splits[cascadeIndex].matrices.viewProjection);

        for (std::uint32_t drawIndex = 0; drawIndex < packet.animatedDrawCount; ++drawIndex)
        {
            const AnimatedDrawItem& draw = packet.animatedDraws[drawIndex];
            if (!draw.castsShadow)
            {
                continue;
            }

            if (!scene::intersects(lightFrustum, draw.bounds))
            {
                continue;
            }

            SkinnedShadowDrawBatch batch;
            batch.mesh = draw.mesh;
            batch.material = draw.material;
            batch.bounds = draw.bounds;
            batch.model = draw.model;
            batch.palette = draw.palette;
            batch.shadowCascadeIndex = cascadeIndex;
            outBatches.push_back(batch);
        }
    }

    return RendererResult::Success;
}
} // namespace

Renderer::Renderer(std::unique_ptr<RenderDevice> device) noexcept
    : device_(std::move(device))
    , terrain_(std::make_unique<terrain::TerrainSystem>())
    , animation_(std::make_unique<animation::AnimationSystem>())
{
}

Renderer::~Renderer()
{
    shutdown();
}

RendererResult Renderer::initialize(const RendererInitDesc& desc)
{
    if (initialized_)
    {
        return RendererResult::AlreadyInitialized;
    }

    if (device_ == nullptr)
    {
        return RendererResult::BackendFailure;
    }

    const RendererResult validation = validateInitDesc(desc);
    if (validation != RendererResult::Success)
    {
        return validation;
    }

    const RendererResult result = device_->initialize(desc);
    if (result == RendererResult::Success)
    {
        initialized_ = true;
        frameInProgress_ = false;
    }

    return result;
}

void Renderer::shutdown() noexcept
{
    if (device_ == nullptr)
    {
        initialized_ = false;
        frameInProgress_ = false;
        return;
    }

    if (frameInProgress_)
    {
        (void)device_->endFrame();
        frameInProgress_ = false;
    }

    if (initialized_)
    {
        terrain_->shutdown();
        animation_->clear();
        lastStaticMeshCullingStats_ = {};
        lastInstancedMeshCullingStats_ = {};
        lastSkinnedMeshCullingStats_ = {};
        frameBudgetStats_ = {};
        lastAnimationDebugLineVertices_ = 0;
        device_->shutdown();
        initialized_ = false;
    }
}

bool Renderer::isInitialized() const noexcept
{
    return initialized_;
}

RendererResult Renderer::resize(const RendererResizeDesc& desc)
{
    if (!initialized_)
    {
        return RendererResult::NotInitialized;
    }

    if (frameInProgress_)
    {
        return RendererResult::FrameAlreadyInProgress;
    }

    const RendererResult validation = validateResizeDesc(desc);
    if (validation != RendererResult::Success)
    {
        return validation;
    }

    if (device_ == nullptr)
    {
        return RendererResult::BackendFailure;
    }

    return device_->resize(desc);
}

MeshHandle Renderer::createMesh(const MeshDesc& desc)
{
    if (!initialized_ || device_ == nullptr)
    {
        return {};
    }

    if (validateMeshDesc(desc) != RendererResult::Success)
    {
        return {};
    }

    return device_->createMesh(desc);
}

void Renderer::destroyMesh(const MeshHandle handle) noexcept
{
    if (!initialized_ || frameInProgress_ || device_ == nullptr || !isValid(handle))
    {
        return;
    }

    device_->destroyMesh(handle);
}

SkeletonHandle Renderer::createSkeleton(const SkeletonDesc& desc)
{
    if (!initialized_ || frameInProgress_ || animation_ == nullptr)
    {
        return {};
    }

    return animation_->createSkeleton(desc);
}

void Renderer::destroySkeleton(const SkeletonHandle handle) noexcept
{
    if (!initialized_ || frameInProgress_ || animation_ == nullptr || !isValid(handle))
    {
        return;
    }

    animation_->destroySkeleton(handle);
}

SkinnedMeshHandle Renderer::createSkinnedMesh(const SkinnedMeshDesc& desc)
{
    if (!initialized_ || frameInProgress_ || device_ == nullptr || animation_ == nullptr)
    {
        return {};
    }

    if (!animation_->validateSkinnedMeshDesc(desc))
    {
        return {};
    }

    const SkinnedMeshHandle handle = device_->createSkinnedMesh(desc);
    if (isValid(handle))
    {
        animation_->registerSkinnedMesh(handle, desc);
    }
    return handle;
}

void Renderer::destroySkinnedMesh(const SkinnedMeshHandle handle) noexcept
{
    if (!initialized_ || frameInProgress_ || device_ == nullptr || animation_ == nullptr || !isValid(handle))
    {
        return;
    }

    animation_->unregisterSkinnedMesh(handle);
    device_->destroySkinnedMesh(handle);
}

TextureHandle Renderer::createTexture(const TextureDesc& desc)
{
    if (!initialized_ || device_ == nullptr)
    {
        return {};
    }

    if (validateTextureDesc(desc) != RendererResult::Success)
    {
        return {};
    }

    return device_->createTexture(desc);
}

void Renderer::destroyTexture(const TextureHandle handle) noexcept
{
    if (!initialized_ || frameInProgress_ || device_ == nullptr || !isValid(handle))
    {
        return;
    }

    device_->destroyTexture(handle);
}

MaterialHandle Renderer::createMaterial(const MaterialDesc& desc)
{
    if (!initialized_ || device_ == nullptr)
    {
        return {};
    }

    if (validateMaterialDesc(desc) != RendererResult::Success)
    {
        return {};
    }

    return device_->createMaterial(desc);
}

void Renderer::destroyMaterial(const MaterialHandle handle) noexcept
{
    if (!initialized_ || frameInProgress_ || device_ == nullptr || !isValid(handle))
    {
        return;
    }

    device_->destroyMaterial(handle);
}

TerrainChunkHandle Renderer::createTerrainChunk(const TerrainChunkDesc& desc)
{
    if (!initialized_ || frameInProgress_)
    {
        return {};
    }

    return terrain_->createChunk(desc);
}

RendererResult Renderer::updateTerrainChunk(const TerrainChunkHandle handle, const TerrainChunkDesc& desc)
{
    if (!initialized_)
    {
        return RendererResult::NotInitialized;
    }

    if (frameInProgress_)
    {
        return RendererResult::FrameAlreadyInProgress;
    }

    if (!isValid(handle))
    {
        return RendererResult::InvalidArgument;
    }

    return terrain_->updateChunk(handle, desc);
}

void Renderer::destroyTerrainChunk(const TerrainChunkHandle handle) noexcept
{
    if (!initialized_ || frameInProgress_)
    {
        return;
    }

    terrain_->destroyChunk(handle);
}

RendererResult Renderer::beginFrame(const FrameDesc& desc)
{
    const BudgetClock::time_point stageStart = BudgetClock::now();
    if (!initialized_)
    {
        return RendererResult::NotInitialized;
    }

    if (frameInProgress_)
    {
        return RendererResult::FrameAlreadyInProgress;
    }

    const RendererResult validation = validateFrameDesc(desc);
    if (validation != RendererResult::Success)
    {
        return validation;
    }

    if (device_ == nullptr)
    {
        return RendererResult::BackendFailure;
    }

    const RendererResult result = device_->beginFrame(desc);
    if (result == RendererResult::Success)
    {
        frameBudgetStats_ = {};
        recordElapsedStage(
            frameBudgetStats_,
            FrameBudgetStage::BeginFrame,
            stageStart,
            BudgetClock::now());
        frameInProgress_ = true;
    }

    return result;
}

RendererResult Renderer::submit(const RenderPacket& packet)
{
    if (!initialized_)
    {
        return RendererResult::NotInitialized;
    }

    if (!frameInProgress_)
    {
        return RendererResult::FrameNotInProgress;
    }

    const BudgetClock::time_point validationStart = BudgetClock::now();
    const RendererResult validation = validateRenderPacket(packet);
    if (validation != RendererResult::Success)
    {
        recordElapsedStage(
            frameBudgetStats_,
            FrameBudgetStage::SubmitValidation,
            validationStart,
            BudgetClock::now());
        return validation;
    }

    if (animation_ == nullptr || !animation_->validateAnimatedDraws(packet.animatedDraws, packet.animatedDrawCount))
    {
        recordElapsedStage(
            frameBudgetStats_,
            FrameBudgetStage::SubmitValidation,
            validationStart,
            BudgetClock::now());
        return RendererResult::InvalidArgument;
    }
    recordElapsedStage(
        frameBudgetStats_,
        FrameBudgetStage::SubmitValidation,
        validationStart,
        BudgetClock::now());
    lastAnimationDebugLineVertices_ = 0;

    const BudgetClock::time_point budgetEstimateStart = BudgetClock::now();
    const FrameBudgetStats submissionEstimate = debug::estimateFrameBudgetSubmission(packet);
    applyFrameBudgetSubmissionEstimate(frameBudgetStats_, submissionEstimate);
    recordElapsedStage(
        frameBudgetStats_,
        FrameBudgetStage::StaticMeshPlanning,
        budgetEstimateStart,
        BudgetClock::now());
    const BudgetClock::time_point estimatePoint = BudgetClock::now();
    recordElapsedStage(frameBudgetStats_, FrameBudgetStage::SkinnedPlanning, estimatePoint, estimatePoint);
    recordElapsedStage(frameBudgetStats_, FrameBudgetStage::DecalPlanning, estimatePoint, estimatePoint);
    recordElapsedStage(frameBudgetStats_, FrameBudgetStage::ParticlePlanning, estimatePoint, estimatePoint);
    recordElapsedStage(frameBudgetStats_, FrameBudgetStage::SelectionOutlinePlanning, estimatePoint, estimatePoint);

    const BudgetClock::time_point cullingStart = BudgetClock::now();
    const scene::SharedCullingDiagnosticsPlan cullingDiagnostics =
        scene::buildSharedCullingDiagnosticsPlan(packet);
    lastStaticMeshCullingStats_ = cullingDiagnostics.staticMeshes;
    lastInstancedMeshCullingStats_ = cullingDiagnostics.instancedMeshes;
    lastSkinnedMeshCullingStats_ = cullingDiagnostics.skinnedMeshes;
    recordElapsedStage(
        frameBudgetStats_,
        FrameBudgetStage::CullingDiagnostics,
        cullingStart,
        BudgetClock::now());

    if (device_ == nullptr)
    {
        return RendererResult::BackendFailure;
    }

    TerrainSubmitDesc terrainSubmit;
    const bool hasTerrain = packet.terrain != nullptr;
    if (hasTerrain)
    {
        terrainSubmit = *packet.terrain;
    }

    const bool drawDecalVolumes = packet.decals != nullptr &&
        packet.decals->enabled &&
        packet.decals->debugDrawVolumes;

    if (!hasTerrain &&
        packet.instancedDrawCount == 0 &&
        packet.animatedDrawCount == 0 &&
        !packet.directionalShadow.enabled &&
        !drawDecalVolumes)
    {
        const BudgetClock::time_point backendStart = BudgetClock::now();
        const RendererResult directSubmitResult = device_->submit(packet);
        recordElapsedStage(
            frameBudgetStats_,
            FrameBudgetStage::BackendSubmit,
            backendStart,
            BudgetClock::now());
        const BudgetClock::time_point postPlanningPoint = BudgetClock::now();
        recordElapsedStage(
            frameBudgetStats_,
            FrameBudgetStage::PostPassPlanning,
            postPlanningPoint,
            postPlanningPoint);
        return directSubmitResult;
    }

    const bool drawTerrainDebugOverlay = hasTerrain && debug::hasTerrainGpuDebugOverlay(terrainSubmit.debug);
    const bool drawShadowLightBounds = packet.directionalShadow.enabled && packet.directionalShadow.debugDrawLightBounds;
    const bool drawShadowCasters = hasTerrain &&
        packet.directionalShadow.enabled &&
        (packet.directionalShadow.debugDrawShadowCasters || packet.directionalShadow.debugDrawCascadeCasters);
    const bool drawMeshShadowCasters = packet.directionalShadow.enabled &&
        packet.directionalShadow.debugDrawCascadeCasters;
    const bool drawCascadeFrusta = packet.directionalShadow.enabled && packet.directionalShadow.debugDrawCascadeFrusta;
    if (drawTerrainDebugOverlay || drawShadowCasters)
    {
        terrainSubmit.debug.captureChunkInfo = true;
    }

    const BudgetClock::time_point terrainStart = BudgetClock::now();
    std::vector<DrawItem> combinedDrawItems;
    combinedDrawItems.reserve(packet.drawItemCount);
    for (std::uint32_t index = 0; index < packet.drawItemCount; ++index)
    {
        combinedDrawItems.push_back(packet.drawItems[index]);
    }

    std::vector<float> terrainModelMatrices;
    std::vector<InstancedDrawBatch> instancedBatches;
    if (hasTerrain)
    {
        const RendererResult terrainResult = terrain_->buildDrawBatches(
            packet.view,
            terrainSubmit,
            terrainModelMatrices,
            instancedBatches);
        if (terrainResult != RendererResult::Success)
        {
            return terrainResult;
        }
    }
    addFrameBudgetBytes(
        frameBudgetStats_,
        frameBudgetStats_.instanceStagedBytes,
        static_cast<std::uint64_t>(terrainModelMatrices.size()) * sizeof(float));
    addFrameBudgetBytes(
        frameBudgetStats_,
        frameBudgetStats_.instanceStagedBytes,
        static_cast<std::uint64_t>(instancedBatches.size()) * sizeof(InstancedDrawBatch));
    recordElapsedStage(
        frameBudgetStats_,
        FrameBudgetStage::TerrainPlanning,
        terrainStart,
        BudgetClock::now());

    const BudgetClock::time_point instancingStart = BudgetClock::now();
    appendPublicInstancedBatches(packet, instancedBatches);
    recordElapsedStage(
        frameBudgetStats_,
        FrameBudgetStage::InstancedPlanning,
        instancingStart,
        BudgetClock::now());

    const BudgetClock::time_point shadowStart = BudgetClock::now();
    std::vector<float> terrainShadowModelMatrices;
    std::vector<float> meshShadowModelMatrices;
    std::vector<InstancedDrawBatch> shadowBatches;
    std::vector<SkinnedShadowDrawBatch> skinnedShadowBatches;
    if (packet.directionalShadow.enabled)
    {
        if (hasTerrain)
        {
            const RendererResult shadowCasterResult = terrain_->buildShadowCasterBatches(
                packet.view,
                terrainSubmit,
                packet.directionalLight,
                packet.directionalShadow,
                terrainShadowModelMatrices,
                shadowBatches);
            if (shadowCasterResult != RendererResult::Success)
            {
                return shadowCasterResult;
            }
        }

        scene::DirectionalShadowCascadeSet cascadeSet;
        if (!buildShadowCascadeSet(packet.directionalLight, packet.directionalShadow, packet.view, cascadeSet))
        {
            return RendererResult::InvalidArgument;
        }

        const RendererResult meshShadowResult = appendMeshShadowCasterBatches(
            packet,
            cascadeSet,
            meshShadowModelMatrices,
            shadowBatches);
        if (meshShadowResult != RendererResult::Success)
        {
            return meshShadowResult;
        }

        const RendererResult skinnedShadowResult = appendSkinnedShadowCasterBatches(
            packet,
            cascadeSet,
            skinnedShadowBatches);
        if (skinnedShadowResult != RendererResult::Success)
        {
            return skinnedShadowResult;
        }
    }
    addFrameBudgetBytes(
        frameBudgetStats_,
        frameBudgetStats_.instanceStagedBytes,
        static_cast<std::uint64_t>(terrainShadowModelMatrices.size() + meshShadowModelMatrices.size()) *
            sizeof(float));
    addFrameBudgetBytes(
        frameBudgetStats_,
        frameBudgetStats_.instanceStagedBytes,
        static_cast<std::uint64_t>(shadowBatches.size()) * sizeof(InstancedDrawBatch));
    addFrameBudgetBytes(
        frameBudgetStats_,
        frameBudgetStats_.skinnedPaletteStagedBytes,
        static_cast<std::uint64_t>(skinnedShadowBatches.size()) * sizeof(SkinnedShadowDrawBatch));
    recordElapsedStage(
        frameBudgetStats_,
        FrameBudgetStage::ShadowPlanning,
        shadowStart,
        BudgetClock::now());

    RenderPacket expandedPacket = packet;
    expandedPacket.drawItems = combinedDrawItems.empty() ? nullptr : combinedDrawItems.data();
    expandedPacket.drawItemCount = static_cast<std::uint32_t>(combinedDrawItems.size());
    expandedPacket.instancedDraws = nullptr;
    expandedPacket.instancedDrawCount = 0;
    expandedPacket.terrain = nullptr;
    const BudgetClock::time_point backendStart = BudgetClock::now();
    const RendererResult submitResult = device_->submitInstanced(
        expandedPacket,
        instancedBatches.empty() ? nullptr : instancedBatches.data(),
        static_cast<std::uint32_t>(instancedBatches.size()),
        shadowBatches.empty() ? nullptr : shadowBatches.data(),
        static_cast<std::uint32_t>(shadowBatches.size()),
        skinnedShadowBatches.empty() ? nullptr : skinnedShadowBatches.data(),
        static_cast<std::uint32_t>(skinnedShadowBatches.size()));
    recordElapsedStage(
        frameBudgetStats_,
        FrameBudgetStage::BackendSubmit,
        backendStart,
        BudgetClock::now());
    if (submitResult != RendererResult::Success)
    {
        return submitResult;
    }

    if (!drawTerrainDebugOverlay &&
        !drawShadowLightBounds &&
        !drawShadowCasters &&
        !drawMeshShadowCasters &&
        !drawCascadeFrusta &&
        !drawDecalVolumes &&
        !packet.animationDebug.drawBounds &&
        !packet.animationDebug.drawSkeletons)
    {
        const BudgetClock::time_point postPlanningPoint = BudgetClock::now();
        recordElapsedStage(
            frameBudgetStats_,
            FrameBudgetStage::PostPassPlanning,
            postPlanningPoint,
            postPlanningPoint);
        return RendererResult::Success;
    }

    const BudgetClock::time_point debugStart = BudgetClock::now();
    std::vector<debug::DebugLineVertex> debugLines;
    lastAnimationDebugLineVertices_ = animation_->appendDebugLines(
        packet.animatedDraws,
        packet.animatedDrawCount,
        packet.animationDebug,
        debugLines);
    if (drawTerrainDebugOverlay)
    {
        const std::uint32_t debugCount = terrain_->copyDebugInfo(nullptr, 0);
        if (debugCount > 0)
        {
            std::vector<TerrainChunkDebugInfo> debugInfo(debugCount);
            (void)terrain_->copyDebugInfo(debugInfo.data(), debugCount);
            (void)debug::buildTerrainDebugLines(
                terrainSubmit.debug,
                debugInfo.data(),
                debugCount,
                debugLines);
        }
    }

    if (drawShadowCasters)
    {
        const std::uint32_t casterDebugCount = terrain_->copyShadowCasterDebugInfo(nullptr, 0);
        if (casterDebugCount > 0)
        {
            std::vector<TerrainChunkDebugInfo> casterDebugInfo(casterDebugCount);
            (void)terrain_->copyShadowCasterDebugInfo(casterDebugInfo.data(), casterDebugCount);
            TerrainDebugOptions casterOptions;
            casterOptions.drawCombinedOverlay = true;
            std::vector<debug::DebugLineVertex> casterLines;
            (void)debug::buildTerrainDebugLines(
                casterOptions,
                casterDebugInfo.data(),
                casterDebugCount,
                casterLines);
            debugLines.insert(debugLines.end(), casterLines.begin(), casterLines.end());
        }
    }

    if (drawMeshShadowCasters)
    {
        constexpr float kStaticMeshCasterColor[4] = {0.95f, 0.35f, 1.0f, 1.0f};
        constexpr float kInstancedMeshCasterColor[4] = {0.95f, 0.70f, 0.25f, 1.0f};
        for (const InstancedDrawBatch& batch : shadowBatches)
        {
            if (batch.shadowCasterKind == ShadowCasterKind::StaticMesh)
            {
                (void)debug::appendAabbDebugLines(batch.bounds, kStaticMeshCasterColor, debugLines);
            }
            else if (batch.shadowCasterKind == ShadowCasterKind::InstancedMesh)
            {
                (void)debug::appendAabbDebugLines(batch.bounds, kInstancedMeshCasterColor, debugLines);
            }
        }
        constexpr float kSkinnedMeshCasterColor[4] = {0.20f, 0.90f, 1.0f, 1.0f};
        for (const SkinnedShadowDrawBatch& batch : skinnedShadowBatches)
        {
            (void)debug::appendAabbDebugLines(batch.bounds, kSkinnedMeshCasterColor, debugLines);
        }
    }

    if (drawShadowLightBounds)
    {
        scene::DirectionalShadowSplit shadowSplit;
        if (scene::buildDirectionalShadowSplit(packet.directionalLight, packet.directionalShadow, shadowSplit))
        {
            (void)debug::appendShadowFrustumDebugLines(shadowSplit.worldCorners, debugLines);
        }
    }

    if (drawCascadeFrusta)
    {
        scene::DirectionalShadowCascadeSet cascadeSet;
        if (scene::buildDirectionalShadowCascadeSet(
                packet.directionalLight,
                packet.directionalShadow,
                packet.view,
                cascadeSet))
        {
            constexpr float kCascadeColors[kMaxDirectionalShadowCascades][4] = {
                {0.20f, 0.55f, 1.0f, 1.0f},
                {0.20f, 0.95f, 0.35f, 1.0f},
                {1.0f, 0.85f, 0.20f, 1.0f},
                {1.0f, 0.45f, 0.12f, 1.0f},
            };
            for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeSet.cascadeCount; ++cascadeIndex)
            {
                (void)debug::appendShadowFrustumDebugLines(
                    cascadeSet.splits[cascadeIndex].worldCorners,
                    kCascadeColors[cascadeIndex],
                    debugLines);
            }
        }
    }

    if (drawDecalVolumes)
    {
        const scene::DecalRenderPlan decalPlan = buildCameraCulledDecalPlan(packet);
        constexpr float kDecalVolumeColor[4] = {0.25f, 0.85f, 1.0f, 1.0f};
        for (std::uint32_t decalIndex = 0; decalIndex < decalPlan.activeCount; ++decalIndex)
        {
            (void)debug::appendAabbDebugLines(decalPlan.items[decalIndex].bounds, kDecalVolumeColor, debugLines);
        }
    }

    const std::uint32_t lineVertexCount = static_cast<std::uint32_t>(debugLines.size());
    addFrameBudgetBytes(
        frameBudgetStats_,
        frameBudgetStats_.debugDrawStagedBytes,
        static_cast<std::uint64_t>(debugLines.size()) * sizeof(debug::DebugLineVertex));
    recordElapsedStage(
        frameBudgetStats_,
        FrameBudgetStage::DebugOverlayPlanning,
        debugStart,
        BudgetClock::now());
    if (lineVertexCount == 0)
    {
        const BudgetClock::time_point postPlanningPoint = BudgetClock::now();
        recordElapsedStage(
            frameBudgetStats_,
            FrameBudgetStage::PostPassPlanning,
            postPlanningPoint,
            postPlanningPoint);
        return RendererResult::Success;
    }

    const RendererResult debugSubmitResult = device_->submitDebugLines(packet.view, debugLines.data(), lineVertexCount);
    const BudgetClock::time_point postPlanningPoint = BudgetClock::now();
    recordElapsedStage(
        frameBudgetStats_,
        FrameBudgetStage::PostPassPlanning,
        postPlanningPoint,
        postPlanningPoint);
    return debugSubmitResult;
}

RendererResult Renderer::endFrame()
{
    const BudgetClock::time_point stageStart = BudgetClock::now();
    if (!initialized_)
    {
        return RendererResult::NotInitialized;
    }

    if (!frameInProgress_)
    {
        return RendererResult::FrameNotInProgress;
    }

    if (device_ == nullptr)
    {
        frameInProgress_ = false;
        return RendererResult::BackendFailure;
    }

    const RendererResult result = device_->endFrame();
    recordElapsedStage(
        frameBudgetStats_,
        FrameBudgetStage::EndFrame,
        stageStart,
        BudgetClock::now());
    frameInProgress_ = false;
    return result;
}

RendererStats Renderer::getStats() const noexcept
{
    if (device_ == nullptr)
    {
        return {};
    }

    RendererStats stats = device_->getStats();
    stats.staticMeshCulling = lastStaticMeshCullingStats_;
    stats.instancedMeshCulling = lastInstancedMeshCullingStats_;
    stats.skinnedMeshCulling = lastSkinnedMeshCullingStats_;
    stats.frameBudget = frameBudgetStats_;
    if (animation_ != nullptr)
    {
        stats.liveSkeletons = animation_->liveSkeletonCount();
        stats.liveSkinnedMeshes = animation_->liveSkinnedMeshCount();
        stats.animationDebugLineVertices = lastAnimationDebugLineVertices_;
    }
    if (terrain_ != nullptr)
    {
        debug::mergeFrameBudgetRuntimeStats(stats.frameBudget, stats, terrain_->getStats());
    }
    return stats;
}

TerrainStats Renderer::getTerrainStats() const noexcept
{
    return terrain_->getStats();
}

std::uint32_t Renderer::copyTerrainDebugInfo(
    TerrainChunkDebugInfo* outItems,
    const std::uint32_t maxItems) const noexcept
{
    return terrain_->copyDebugInfo(outItems, maxItems);
}

std::uint32_t Renderer::copyTerrainBatchDebugInfo(
    TerrainBatchDebugInfo* outItems,
    const std::uint32_t maxItems) const noexcept
{
    return terrain_->copyBatchDebugInfo(outItems, maxItems);
}

std::uint32_t Renderer::copyTerrainShadowCasterDebugInfo(
    TerrainChunkDebugInfo* outItems,
    const std::uint32_t maxItems) const noexcept
{
    return terrain_->copyShadowCasterDebugInfo(outItems, maxItems);
}

bgfx_backend::ShadowPreviewTextureInfo Renderer::getShadowPreviewTexture(const std::uint32_t cascadeIndex) const noexcept
{
    const auto* bgfxDevice = dynamic_cast<const bgfx_backend::BgfxRenderDevice*>(device_.get());
    if (bgfxDevice == nullptr)
    {
        return {};
    }

    return bgfxDevice->getShadowPreviewTexture(cascadeIndex);
}
} // namespace full_renderer::core

namespace full_renderer
{
std::unique_ptr<IRenderer> createRenderer()
{
    return std::make_unique<core::Renderer>(
        std::make_unique<bgfx_backend::BgfxRenderDevice>());
}
} // namespace full_renderer
