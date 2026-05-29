#include "renderer/terrain/TerrainSystem.hpp"

#include "renderer/scene/Math.hpp"
#include "renderer/scene/Shadow.hpp"
#include "renderer/terrain/TerrainCulling.hpp"
#include "renderer/terrain/TerrainLod.hpp"

#include <algorithm>
#include <cmath>

namespace full_renderer::terrain
{
namespace
{
bool isValidLod(const TerrainLodDesc& lod) noexcept
{
    return isValid(lod.mesh) &&
        isValid(lod.material) &&
        std::isfinite(lod.maxDistanceMeters) &&
        lod.maxDistanceMeters >= 0.0f;
}

bool validateChunkDesc(const TerrainChunkDesc& desc) noexcept
{
    if (!scene::isValidAabb(desc.bounds) ||
        desc.lods == nullptr ||
        !hasValidLodCount(desc.lodCount))
    {
        return false;
    }

    float previousDistance = -1.0f;
    for (std::uint32_t index = 0; index < desc.lodCount; ++index)
    {
        const TerrainLodDesc& lod = desc.lods[index];
        if (!isValidLod(lod) || lod.maxDistanceMeters < previousDistance)
        {
            return false;
        }
        previousDistance = lod.maxDistanceMeters;
    }

    return true;
}

bool validateSubmitDesc(const TerrainSubmitDesc& desc) noexcept
{
    if (desc.chunkCount > 0 && desc.chunks == nullptr)
    {
        return false;
    }

    return scene::isFinite3(desc.cameraPositionWorld);
}

void setIdentity(float out[16]) noexcept
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

void setChunkModelMatrix(const Aabb& bounds, float out[16]) noexcept
{
    setIdentity(out);
    out[12] = bounds.min[0];
    out[13] = bounds.min[1];
    out[14] = bounds.min[2];
}

void appendMatrix(const float matrix[16], std::vector<float>& outModelMatrices)
{
    outModelMatrices.insert(outModelMatrices.end(), matrix, matrix + 16);
}

Aabb mergedBounds(const Aabb& left, const Aabb& right) noexcept
{
    Aabb bounds;
    for (int axis = 0; axis < 3; ++axis)
    {
        bounds.min[axis] = std::min(left.min[axis], right.min[axis]);
        bounds.max[axis] = std::max(left.max[axis], right.max[axis]);
    }
    return bounds;
}
} // namespace

bool hasValidLodCount(const std::uint32_t lodCount) noexcept
{
    return lodCount > 0 && lodCount <= kMaxTerrainLodLevels;
}

std::uint32_t selectLod(const TerrainLodDesc* lods, const std::uint32_t lodCount, const float distanceMeters) noexcept
{
    if (lods == nullptr || !hasValidLodCount(lodCount) || !std::isfinite(distanceMeters))
    {
        return kInvalidTerrainLodIndex;
    }

    for (std::uint32_t index = 0; index < lodCount; ++index)
    {
        if (distanceMeters <= lods[index].maxDistanceMeters)
        {
            return index;
        }
    }

    return lodCount - 1U;
}

TerrainChunkHandle TerrainSystem::createChunk(const TerrainChunkDesc& desc)
{
    if (!validateChunkDesc(desc))
    {
        return {};
    }

    for (std::uint32_t index = 0; index < chunks_.size(); ++index)
    {
        ChunkRecord& record = chunks_[index];
        if (!record.active)
        {
            record.active = true;
            ++record.generation;
            if (record.generation == 0)
            {
                record.generation = 1;
            }
            record.bounds = desc.bounds;
            record.splatMap = desc.splatMap;
            record.lods.assign(desc.lods, desc.lods + desc.lodCount);
            ++chunksCreatedSinceLastSubmit_;
            ++chunkSlotsReusedSinceLastSubmit_;
            ++totalChunkCreateCount_;
            ++totalChunkSlotReuseCount_;
            updateLiveChunkCount();
            return {index + 1U, record.generation};
        }
    }

    ChunkRecord record;
    record.generation = 1;
    record.active = true;
    record.bounds = desc.bounds;
    record.splatMap = desc.splatMap;
    record.lods.assign(desc.lods, desc.lods + desc.lodCount);
    chunks_.push_back(record);
    ++chunksCreatedSinceLastSubmit_;
    ++totalChunkCreateCount_;
    updateLiveChunkCount();
    return {static_cast<std::uint32_t>(chunks_.size()), record.generation};
}

RendererResult TerrainSystem::updateChunk(const TerrainChunkHandle handle, const TerrainChunkDesc& desc)
{
    if (!validateChunkDesc(desc))
    {
        return RendererResult::InvalidDescriptor;
    }

    ChunkRecord* record = find(handle);
    if (record == nullptr)
    {
        return RendererResult::InvalidArgument;
    }

    record->bounds = desc.bounds;
    record->splatMap = desc.splatMap;
    record->lods.assign(desc.lods, desc.lods + desc.lodCount);
    ++chunksUpdatedSinceLastSubmit_;
    ++totalChunkUpdateCount_;
    writeResidencyStats();
    return RendererResult::Success;
}

void TerrainSystem::destroyChunk(const TerrainChunkHandle handle) noexcept
{
    ChunkRecord* record = find(handle);
    if (record == nullptr)
    {
        return;
    }

    record->active = false;
    record->splatMap = {};
    record->lods.clear();
    ++chunksDestroyedSinceLastSubmit_;
    ++totalChunkDestroyCount_;
    updateLiveChunkCount();
}

void TerrainSystem::shutdown() noexcept
{
    chunks_.clear();
    pendingBatches_.clear();
    pendingBatchCount_ = 0;
    chunksCreatedSinceLastSubmit_ = 0;
    chunksDestroyedSinceLastSubmit_ = 0;
    chunksUpdatedSinceLastSubmit_ = 0;
    chunkSlotsReusedSinceLastSubmit_ = 0;
    totalChunkCreateCount_ = 0;
    totalChunkDestroyCount_ = 0;
    totalChunkUpdateCount_ = 0;
    totalChunkSlotReuseCount_ = 0;
    debugSnapshot_.clear();
    shadowCasterDebugSnapshot_.clear();
    batchDebugSnapshot_.clear();
    stats_ = {};
}

RendererResult TerrainSystem::buildDrawItems(
    const RenderViewDesc& view,
    const TerrainSubmitDesc& submit,
    std::vector<DrawItem>& outDrawItems)
{
    if (!validateSubmitDesc(submit) ||
        !scene::isFinite16(view.view) ||
        !scene::isFinite16(view.projection))
    {
        return RendererResult::InvalidArgument;
    }

    resetFrameStats(submit.chunkCount);
    batchDebugSnapshot_.clear();
    if (submit.debug.captureChunkInfo)
    {
        debugSnapshot_.clear();
        debugSnapshot_.reserve(submit.chunkCount);
    }
    else
    {
        debugSnapshot_.clear();
    }

    float viewProjection[16] = {};
    scene::multiplyColumnMajor4x4(view.projection, view.view, viewProjection);
    const scene::Frustum frustum = scene::extractFrustumFromViewProjection(viewProjection);
    const scene::Vec3 cameraPosition = scene::fromArray(submit.cameraPositionWorld);

    for (std::uint32_t index = 0; index < submit.chunkCount; ++index)
    {
        const TerrainChunkHandle handle = submit.chunks[index];
        const ChunkRecord* record = find(handle);

        TerrainChunkDebugInfo debugInfo;
        debugInfo.handle = handle;
        debugInfo.selectedLod = kInvalidTerrainLodIndex;
        debugInfo.cullResult = TerrainCullResult::InvalidHandle;

        if (record == nullptr)
        {
            ++stats_.culledChunks;
            recordInvalidSubmittedHandle(handle);
            if (submit.debug.captureChunkInfo)
            {
                debugSnapshot_.push_back(debugInfo);
            }
            continue;
        }

        debugInfo.bounds = record->bounds;
        debugInfo.hasSplatMap = isValid(record->splatMap);
        const scene::Vec3 center = scene::aabbCenter(record->bounds);
        debugInfo.distanceMeters = scene::distance(center, cameraPosition);

        if (!isChunkVisible(frustum, record->bounds))
        {
            ++stats_.culledChunks;
            debugInfo.cullResult = TerrainCullResult::OutsideFrustum;
            if (submit.debug.captureChunkInfo)
            {
                debugSnapshot_.push_back(debugInfo);
            }
            continue;
        }

        const std::uint32_t lodIndex = selectLod(
            record->lods.data(),
            static_cast<std::uint32_t>(record->lods.size()),
            debugInfo.distanceMeters);
        if (lodIndex == kInvalidTerrainLodIndex)
        {
            ++stats_.culledChunks;
            ++stats_.lodFallbackChunks;
            if (submit.debug.captureChunkInfo)
            {
                debugSnapshot_.push_back(debugInfo);
            }
            continue;
        }

        const TerrainLodDesc& lod = record->lods[lodIndex];
        DrawItem drawItem;
        drawItem.mesh = lod.mesh;
        drawItem.material = lod.material;
        setIdentity(drawItem.model);
        outDrawItems.push_back(drawItem);

        ++stats_.visibleChunks;
        ++stats_.terrainDraws;
        if (!isValid(record->splatMap))
        {
            ++stats_.splatMapFallbackChunks;
        }
        else
        {
            ++stats_.splatMapResidentChunks;
        }
        if (lodIndex < kMaxTerrainLodLevels)
        {
            ++stats_.visibleChunksByLod[lodIndex];
            ++stats_.terrainBatchesByLod[lodIndex];
        }
        debugInfo.cullResult = TerrainCullResult::Visible;
        debugInfo.selectedLod = lodIndex;
        debugInfo.hasTerrainMaterial = isValid(lod.material);
        debugInfo.cameraVisible = true;
        if (submit.debug.captureChunkInfo)
        {
            debugSnapshot_.push_back(debugInfo);
        }
    }

    return RendererResult::Success;
}

RendererResult TerrainSystem::buildDrawBatches(
    const RenderViewDesc& view,
    const TerrainSubmitDesc& submit,
    std::vector<float>& outModelMatrices,
    std::vector<core::InstancedDrawBatch>& outBatches)
{
    if (!validateSubmitDesc(submit) ||
        !scene::isFinite16(view.view) ||
        !scene::isFinite16(view.projection))
    {
        return RendererResult::InvalidArgument;
    }

    outModelMatrices.clear();
    outBatches.clear();
    resetFrameStats(submit.chunkCount);
    resetPendingBatches();
    if (submit.debug.captureChunkInfo)
    {
        debugSnapshot_.clear();
        debugSnapshot_.reserve(submit.chunkCount);
    }
    else
    {
        debugSnapshot_.clear();
    }
    batchDebugSnapshot_.clear();

    float viewProjection[16] = {};
    scene::multiplyColumnMajor4x4(view.projection, view.view, viewProjection);
    const scene::Frustum frustum = scene::extractFrustumFromViewProjection(viewProjection);
    const scene::Vec3 cameraPosition = scene::fromArray(submit.cameraPositionWorld);

    for (std::uint32_t index = 0; index < submit.chunkCount; ++index)
    {
        const TerrainChunkHandle handle = submit.chunks[index];
        const ChunkRecord* record = find(handle);

        TerrainChunkDebugInfo debugInfo;
        debugInfo.handle = handle;
        debugInfo.selectedLod = kInvalidTerrainLodIndex;
        debugInfo.cullResult = TerrainCullResult::InvalidHandle;

        if (record == nullptr)
        {
            ++stats_.culledChunks;
            recordInvalidSubmittedHandle(handle);
            if (submit.debug.captureChunkInfo)
            {
                debugSnapshot_.push_back(debugInfo);
            }
            continue;
        }

        debugInfo.bounds = record->bounds;
        debugInfo.hasSplatMap = isValid(record->splatMap);
        const scene::Vec3 center = scene::aabbCenter(record->bounds);
        debugInfo.distanceMeters = scene::distance(center, cameraPosition);

        if (!isChunkVisible(frustum, record->bounds))
        {
            ++stats_.culledChunks;
            debugInfo.cullResult = TerrainCullResult::OutsideFrustum;
            if (submit.debug.captureChunkInfo)
            {
                debugSnapshot_.push_back(debugInfo);
            }
            continue;
        }

        const std::uint32_t lodIndex = selectLod(
            record->lods.data(),
            static_cast<std::uint32_t>(record->lods.size()),
            debugInfo.distanceMeters);
        if (lodIndex == kInvalidTerrainLodIndex)
        {
            ++stats_.culledChunks;
            ++stats_.lodFallbackChunks;
            if (submit.debug.captureChunkInfo)
            {
                debugSnapshot_.push_back(debugInfo);
            }
            continue;
        }

        const TerrainLodDesc& lod = record->lods[lodIndex];
        PendingBatch* batch = nullptr;
        for (std::uint32_t batchIndex = 0; batchIndex < pendingBatchCount_; ++batchIndex)
        {
            PendingBatch& candidate = pendingBatches_[batchIndex];
            if (candidate.mesh.id == lod.mesh.id &&
                candidate.material.id == lod.material.id &&
                candidate.splatMap.id == record->splatMap.id &&
                candidate.lodIndex == lodIndex)
            {
                batch = &candidate;
                break;
            }
        }

        float model[16] = {};
        setChunkModelMatrix(record->bounds, model);

        if (batch == nullptr)
        {
            if (pendingBatchCount_ == pendingBatches_.size())
            {
                pendingBatches_.push_back({});
            }

            PendingBatch& newBatch = pendingBatches_[pendingBatchCount_];
            ++pendingBatchCount_;
            newBatch.mesh = lod.mesh;
            newBatch.material = lod.material;
            newBatch.splatMap = record->splatMap;
            newBatch.lodIndex = lodIndex;
            newBatch.cascadeIndex = kInvalidShadowCascadeIndex;
            newBatch.bounds = record->bounds;
            newBatch.matrices.clear();
            appendMatrix(model, newBatch.matrices);
        }
        else
        {
            batch->bounds = mergedBounds(batch->bounds, record->bounds);
            appendMatrix(model, batch->matrices);
        }

        ++stats_.visibleChunks;
        if (!isValid(record->splatMap))
        {
            ++stats_.splatMapFallbackChunks;
        }
        else
        {
            ++stats_.splatMapResidentChunks;
        }
        if (lodIndex < kMaxTerrainLodLevels)
        {
            ++stats_.visibleChunksByLod[lodIndex];
        }
        debugInfo.cullResult = TerrainCullResult::Visible;
        debugInfo.selectedLod = lodIndex;
        debugInfo.hasTerrainMaterial = isValid(lod.material);
        debugInfo.cameraVisible = true;
        if (submit.debug.captureChunkInfo)
        {
            debugSnapshot_.push_back(debugInfo);
        }
    }

    outBatches.reserve(pendingBatchCount_);
    std::uint32_t totalMatrixFloats = 0;
    for (std::uint32_t batchIndex = 0; batchIndex < pendingBatchCount_; ++batchIndex)
    {
        const PendingBatch& pendingBatch = pendingBatches_[batchIndex];
        totalMatrixFloats += static_cast<std::uint32_t>(pendingBatch.matrices.size());
    }
    outModelMatrices.reserve(totalMatrixFloats);
    if (submit.debug.captureBatchInfo)
    {
        batchDebugSnapshot_.reserve(pendingBatchCount_);
    }

    for (std::uint32_t batchIndex = 0; batchIndex < pendingBatchCount_; ++batchIndex)
    {
        const PendingBatch& pendingBatch = pendingBatches_[batchIndex];
        const std::uint32_t matrixOffset = static_cast<std::uint32_t>(outModelMatrices.size());
        outModelMatrices.insert(outModelMatrices.end(), pendingBatch.matrices.begin(), pendingBatch.matrices.end());

        core::InstancedDrawBatch batch;
        batch.mesh = pendingBatch.mesh;
        batch.material = pendingBatch.material;
        batch.splatMap = pendingBatch.splatMap;
        batch.bounds = pendingBatch.bounds;
        batch.modelMatrices = outModelMatrices.data() + matrixOffset;
        batch.instanceCount = static_cast<std::uint32_t>(pendingBatch.matrices.size() / 16U);
        batch.lodIndex = pendingBatch.lodIndex;
        batch.shadowCascadeIndex = kInvalidShadowCascadeIndex;
        batch.shadowCasterKind = core::ShadowCasterKind::Terrain;
        outBatches.push_back(batch);
        if (pendingBatch.lodIndex < kMaxTerrainLodLevels)
        {
            ++stats_.terrainBatchesByLod[pendingBatch.lodIndex];
        }

        if (submit.debug.captureBatchInfo)
        {
            TerrainBatchDebugInfo debugInfo;
            debugInfo.mesh = pendingBatch.mesh;
            debugInfo.material = pendingBatch.material;
            debugInfo.selectedLod = pendingBatch.lodIndex;
            debugInfo.instanceCount = batch.instanceCount;
            debugInfo.bounds = pendingBatch.bounds;
            debugInfo.hasSplatMap = isValid(pendingBatch.splatMap);
            debugInfo.hasTerrainMaterial = isValid(pendingBatch.material);
            batchDebugSnapshot_.push_back(debugInfo);
        }
    }
    stats_.terrainDraws = static_cast<std::uint32_t>(outBatches.size());
    return RendererResult::Success;
}

RendererResult TerrainSystem::buildShadowCasterBatches(
    const RenderViewDesc& cameraView,
    const TerrainSubmitDesc& submit,
    const DirectionalLightDesc& light,
    const DirectionalShadowDesc& shadow,
    std::vector<float>& outModelMatrices,
    std::vector<core::InstancedDrawBatch>& outBatches)
{
    if (!validateSubmitDesc(submit) ||
        !scene::isFinite16(cameraView.view) ||
        !scene::isFinite16(cameraView.projection) ||
        !scene::isValidDirectionalShadowDesc(shadow))
    {
        return RendererResult::InvalidArgument;
    }

    outModelMatrices.clear();
    outBatches.clear();
    shadowCasterDebugSnapshot_.clear();
    resetPendingBatches();
    stats_.shadowCasterChunks = 0;
    stats_.offCameraShadowCasterChunks = 0;
    stats_.shadowRejectedChunks = 0;
    stats_.shadowInvalidResourceChunks = 0;
    stats_.shadowCascadeCount = 0;
    for (std::uint32_t lodIndex = 0; lodIndex < kMaxTerrainLodLevels; ++lodIndex)
    {
        stats_.shadowCasterChunksByLod[lodIndex] = 0;
        stats_.shadowCasterBatchesByLod[lodIndex] = 0;
    }
    for (std::uint32_t cascadeIndex = 0; cascadeIndex < kMaxTerrainShadowCascades; ++cascadeIndex)
    {
        stats_.shadowCascadeCasterChunks[cascadeIndex] = 0;
        stats_.shadowCascadeOffCameraCasterChunks[cascadeIndex] = 0;
        stats_.shadowCascadeRejectedChunks[cascadeIndex] = 0;
        stats_.shadowCascadeInvalidResourceChunks[cascadeIndex] = 0;
        for (std::uint32_t lodIndex = 0; lodIndex < kMaxTerrainLodLevels; ++lodIndex)
        {
            stats_.shadowCascadeCasterChunksByLod[cascadeIndex][lodIndex] = 0;
        }
    }

    if (!shadow.enabled || submit.chunkCount == 0)
    {
        return RendererResult::Success;
    }

    scene::DirectionalShadowCascadeSet cascadeSet;
    if (scene::clampShadowCascadeCount(shadow.cascadeCount) == 1U)
    {
        scene::DirectionalShadowSplit split;
        if (!scene::buildDirectionalShadowSplit(light, shadow, split))
        {
            return RendererResult::InvalidArgument;
        }
        cascadeSet.cascadeCount = 1;
        cascadeSet.splits[0] = split;
        cascadeSet.splits[0].splitIndex = 0;
    }
    else if (!scene::buildDirectionalShadowCascadeSet(light, shadow, cameraView, cascadeSet))
    {
        return RendererResult::InvalidArgument;
    }
    stats_.shadowCascadeCount = cascadeSet.cascadeCount;

    float cameraViewProjection[16] = {};
    scene::multiplyColumnMajor4x4(cameraView.projection, cameraView.view, cameraViewProjection);
    const scene::Frustum cameraFrustum = scene::extractFrustumFromViewProjection(cameraViewProjection);
    const scene::Vec3 shadowCenter = scene::fromArray(shadow.centerWorld);
    const bool captureAllCascadeCasters = shadow.debugDrawCascadeCasters;
    const bool captureRenderedCasters = submit.debug.captureChunkInfo || shadow.debugDrawShadowCasters;

    if (captureRenderedCasters || captureAllCascadeCasters)
    {
        const std::uint32_t multiplier = captureAllCascadeCasters ? cascadeSet.cascadeCount : 1U;
        shadowCasterDebugSnapshot_.reserve(submit.chunkCount * multiplier);
    }

    for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeSet.cascadeCount; ++cascadeIndex)
    {
        const scene::DirectionalShadowSplit& cascade = cascadeSet.splits[cascadeIndex];
        const scene::Frustum lightFrustum = scene::extractFrustumFromViewProjection(cascade.matrices.viewProjection);
        const bool captureCascade = captureAllCascadeCasters || (cascadeIndex == 0 && captureRenderedCasters);

        for (std::uint32_t index = 0; index < submit.chunkCount; ++index)
        {
            const TerrainChunkHandle handle = submit.chunks[index];
            const ChunkRecord* record = find(handle);

            TerrainChunkDebugInfo debugInfo;
            debugInfo.handle = handle;
            debugInfo.selectedLod = kInvalidTerrainLodIndex;
            debugInfo.cullResult = TerrainCullResult::InvalidHandle;
            debugInfo.selectedAsShadowCaster = false;
            debugInfo.shadowCascadeIndex = cascadeIndex;

            if (record == nullptr)
            {
                ++stats_.shadowCascadeInvalidResourceChunks[cascadeIndex];
                ++stats_.shadowInvalidResourceChunks;
                recordInvalidSubmittedHandle(handle);
                if (captureCascade)
                {
                    shadowCasterDebugSnapshot_.push_back(debugInfo);
                }
                continue;
            }

            debugInfo.bounds = record->bounds;
            debugInfo.hasSplatMap = isValid(record->splatMap);
            debugInfo.cameraVisible = isChunkVisible(cameraFrustum, record->bounds);
            const scene::Vec3 center = scene::aabbCenter(record->bounds);
            debugInfo.distanceMeters = scene::distance(center, shadowCenter);

            if (!isChunkVisible(lightFrustum, record->bounds))
            {
                ++stats_.shadowCascadeRejectedChunks[cascadeIndex];
                ++stats_.shadowRejectedChunks;
                debugInfo.cullResult = TerrainCullResult::OutsideFrustum;
                if (captureCascade)
                {
                    shadowCasterDebugSnapshot_.push_back(debugInfo);
                }
                continue;
            }

            const std::uint32_t lodIndex = selectLod(
                record->lods.data(),
                static_cast<std::uint32_t>(record->lods.size()),
                debugInfo.distanceMeters);
            if (lodIndex == kInvalidTerrainLodIndex || lodIndex >= record->lods.size())
            {
                ++stats_.shadowCascadeInvalidResourceChunks[cascadeIndex];
                ++stats_.shadowInvalidResourceChunks;
                ++stats_.lodFallbackChunks;
                if (captureCascade)
                {
                    shadowCasterDebugSnapshot_.push_back(debugInfo);
                }
                continue;
            }

            const TerrainLodDesc& lod = record->lods[lodIndex];
            if (!isValid(lod.mesh) || !isValid(lod.material))
            {
                ++stats_.shadowCascadeInvalidResourceChunks[cascadeIndex];
                ++stats_.shadowInvalidResourceChunks;
                if (captureCascade)
                {
                    shadowCasterDebugSnapshot_.push_back(debugInfo);
                }
                continue;
            }

            PendingBatch* batch = nullptr;
            for (std::uint32_t batchIndex = 0; batchIndex < pendingBatchCount_; ++batchIndex)
            {
                PendingBatch& candidate = pendingBatches_[batchIndex];
                if (candidate.mesh.id == lod.mesh.id &&
                    candidate.material.id == lod.material.id &&
                    candidate.splatMap.id == record->splatMap.id &&
                    candidate.lodIndex == lodIndex &&
                    candidate.cascadeIndex == cascadeIndex)
                {
                    batch = &candidate;
                    break;
                }
            }

            float model[16] = {};
            setChunkModelMatrix(record->bounds, model);

            if (batch == nullptr)
            {
                if (pendingBatchCount_ == pendingBatches_.size())
                {
                    pendingBatches_.push_back({});
                }

                PendingBatch& newBatch = pendingBatches_[pendingBatchCount_];
                ++pendingBatchCount_;
                newBatch.mesh = lod.mesh;
                newBatch.material = lod.material;
                newBatch.splatMap = record->splatMap;
                newBatch.lodIndex = lodIndex;
                newBatch.cascadeIndex = cascadeIndex;
                newBatch.bounds = record->bounds;
                newBatch.matrices.clear();
                appendMatrix(model, newBatch.matrices);
            }
            else
            {
                batch->bounds = mergedBounds(batch->bounds, record->bounds);
                appendMatrix(model, batch->matrices);
            }

            ++stats_.shadowCascadeCasterChunks[cascadeIndex];
            if (!debugInfo.cameraVisible)
            {
                ++stats_.shadowCascadeOffCameraCasterChunks[cascadeIndex];
            }
            if (lodIndex < kMaxTerrainLodLevels)
            {
                ++stats_.shadowCascadeCasterChunksByLod[cascadeIndex][lodIndex];
            }

            ++stats_.shadowCasterChunks;
            if (!debugInfo.cameraVisible)
            {
                ++stats_.offCameraShadowCasterChunks;
            }
            if (lodIndex < kMaxTerrainLodLevels)
            {
                ++stats_.shadowCasterChunksByLod[lodIndex];
            }

            debugInfo.cullResult = TerrainCullResult::Visible;
            debugInfo.selectedLod = lodIndex;
            debugInfo.hasTerrainMaterial = isValid(lod.material);
            debugInfo.selectedAsShadowCaster = true;
            if (captureCascade)
            {
                shadowCasterDebugSnapshot_.push_back(debugInfo);
            }
        }
    }

    outBatches.reserve(pendingBatchCount_);
    std::uint32_t totalMatrixFloats = 0;
    for (std::uint32_t batchIndex = 0; batchIndex < pendingBatchCount_; ++batchIndex)
    {
        const PendingBatch& pendingBatch = pendingBatches_[batchIndex];
        totalMatrixFloats += static_cast<std::uint32_t>(pendingBatch.matrices.size());
    }
    outModelMatrices.reserve(totalMatrixFloats);

    for (std::uint32_t batchIndex = 0; batchIndex < pendingBatchCount_; ++batchIndex)
    {
        const PendingBatch& pendingBatch = pendingBatches_[batchIndex];
        const std::uint32_t matrixOffset = static_cast<std::uint32_t>(outModelMatrices.size());
        outModelMatrices.insert(outModelMatrices.end(), pendingBatch.matrices.begin(), pendingBatch.matrices.end());

        core::InstancedDrawBatch batch;
        batch.mesh = pendingBatch.mesh;
        batch.material = pendingBatch.material;
        batch.splatMap = pendingBatch.splatMap;
        batch.bounds = pendingBatch.bounds;
        batch.modelMatrices = outModelMatrices.data() + matrixOffset;
        batch.instanceCount = static_cast<std::uint32_t>(pendingBatch.matrices.size() / 16U);
        batch.lodIndex = pendingBatch.lodIndex;
        batch.shadowCascadeIndex = pendingBatch.cascadeIndex;
        batch.shadowCasterKind = core::ShadowCasterKind::Terrain;
        outBatches.push_back(batch);
        if (pendingBatch.lodIndex < kMaxTerrainLodLevels)
        {
            ++stats_.shadowCasterBatchesByLod[pendingBatch.lodIndex];
        }
    }

    return RendererResult::Success;
}

TerrainStats TerrainSystem::getStats() const noexcept
{
    return stats_;
}

std::uint32_t TerrainSystem::copyDebugInfo(TerrainChunkDebugInfo* outItems, const std::uint32_t maxItems) const noexcept
{
    const std::uint32_t available = static_cast<std::uint32_t>(debugSnapshot_.size());
    if (outItems == nullptr || maxItems == 0)
    {
        return available;
    }

    const std::uint32_t toCopy = std::min(available, maxItems);
    for (std::uint32_t index = 0; index < toCopy; ++index)
    {
        outItems[index] = debugSnapshot_[index];
    }

    return available;
}

std::uint32_t TerrainSystem::copyBatchDebugInfo(
    TerrainBatchDebugInfo* outItems,
    const std::uint32_t maxItems) const noexcept
{
    const std::uint32_t available = static_cast<std::uint32_t>(batchDebugSnapshot_.size());
    if (outItems == nullptr || maxItems == 0)
    {
        return available;
    }

    const std::uint32_t toCopy = std::min(available, maxItems);
    for (std::uint32_t index = 0; index < toCopy; ++index)
    {
        outItems[index] = batchDebugSnapshot_[index];
    }

    return available;
}

std::uint32_t TerrainSystem::copyShadowCasterDebugInfo(
    TerrainChunkDebugInfo* outItems,
    const std::uint32_t maxItems) const noexcept
{
    const std::uint32_t available = static_cast<std::uint32_t>(shadowCasterDebugSnapshot_.size());
    if (outItems == nullptr || maxItems == 0)
    {
        return available;
    }

    const std::uint32_t toCopy = std::min(available, maxItems);
    for (std::uint32_t index = 0; index < toCopy; ++index)
    {
        outItems[index] = shadowCasterDebugSnapshot_[index];
    }

    return available;
}

TerrainSystem::ChunkRecord* TerrainSystem::find(const TerrainChunkHandle handle) noexcept
{
    if (!isValid(handle) || handle.id > chunks_.size())
    {
        return nullptr;
    }

    ChunkRecord& record = chunks_[handle.id - 1U];
    if (!record.active || record.generation != handle.generation)
    {
        return nullptr;
    }

    return &record;
}

const TerrainSystem::ChunkRecord* TerrainSystem::find(const TerrainChunkHandle handle) const noexcept
{
    if (!isValid(handle) || handle.id > chunks_.size())
    {
        return nullptr;
    }

    const ChunkRecord& record = chunks_[handle.id - 1U];
    if (!record.active || record.generation != handle.generation)
    {
        return nullptr;
    }

    return &record;
}

void TerrainSystem::updateLiveChunkCount() noexcept
{
    std::uint32_t liveCount = 0;
    for (const ChunkRecord& record : chunks_)
    {
        if (record.active)
        {
            ++liveCount;
        }
    }

    stats_.liveChunks = liveCount;
    writeResidencyStats();
}

void TerrainSystem::resetFrameStats(const std::uint32_t submittedChunks) noexcept
{
    const std::uint32_t liveChunks = stats_.liveChunks;
    const std::uint32_t allocatedChunkSlots = stats_.allocatedChunkSlots;
    const std::uint32_t nonResidentChunks = stats_.nonResidentChunks;
    const std::uint32_t createdSinceLastSubmit = chunksCreatedSinceLastSubmit_;
    const std::uint32_t destroyedSinceLastSubmit = chunksDestroyedSinceLastSubmit_;
    const std::uint32_t updatedSinceLastSubmit = chunksUpdatedSinceLastSubmit_;
    const std::uint32_t reusedSinceLastSubmit = chunkSlotsReusedSinceLastSubmit_;
    stats_ = {};
    stats_.liveChunks = liveChunks;
    stats_.residentChunks = liveChunks;
    stats_.allocatedChunkSlots = allocatedChunkSlots;
    stats_.nonResidentChunks = nonResidentChunks;
    stats_.chunksCreatedSinceLastSubmit = createdSinceLastSubmit;
    stats_.chunksDestroyedSinceLastSubmit = destroyedSinceLastSubmit;
    stats_.chunksUpdatedSinceLastSubmit = updatedSinceLastSubmit;
    stats_.chunkSlotsReusedSinceLastSubmit = reusedSinceLastSubmit;
    stats_.totalChunkCreateCount = totalChunkCreateCount_;
    stats_.totalChunkDestroyCount = totalChunkDestroyCount_;
    stats_.totalChunkUpdateCount = totalChunkUpdateCount_;
    stats_.totalChunkSlotReuseCount = totalChunkSlotReuseCount_;
    stats_.submittedChunks = submittedChunks;
    clearPendingResidencyChurn();
}

void TerrainSystem::resetPendingBatches() noexcept
{
    for (std::uint32_t index = 0; index < pendingBatchCount_; ++index)
    {
        pendingBatches_[index].matrices.clear();
    }
    pendingBatchCount_ = 0;
}

void TerrainSystem::recordInvalidSubmittedHandle(const TerrainChunkHandle handle) noexcept
{
    if (isValid(handle))
    {
        ++stats_.staleHandleChunks;
    }
    else
    {
        ++stats_.invalidHandleChunks;
    }
}

void TerrainSystem::writeResidencyStats() noexcept
{
    stats_.residentChunks = stats_.liveChunks;
    stats_.allocatedChunkSlots = static_cast<std::uint32_t>(chunks_.size());
    stats_.nonResidentChunks = stats_.allocatedChunkSlots >= stats_.liveChunks ?
        stats_.allocatedChunkSlots - stats_.liveChunks :
        0U;
    stats_.chunksCreatedSinceLastSubmit = chunksCreatedSinceLastSubmit_;
    stats_.chunksDestroyedSinceLastSubmit = chunksDestroyedSinceLastSubmit_;
    stats_.chunksUpdatedSinceLastSubmit = chunksUpdatedSinceLastSubmit_;
    stats_.chunkSlotsReusedSinceLastSubmit = chunkSlotsReusedSinceLastSubmit_;
    stats_.totalChunkCreateCount = totalChunkCreateCount_;
    stats_.totalChunkDestroyCount = totalChunkDestroyCount_;
    stats_.totalChunkUpdateCount = totalChunkUpdateCount_;
    stats_.totalChunkSlotReuseCount = totalChunkSlotReuseCount_;
}

void TerrainSystem::clearPendingResidencyChurn() noexcept
{
    chunksCreatedSinceLastSubmit_ = 0;
    chunksDestroyedSinceLastSubmit_ = 0;
    chunksUpdatedSinceLastSubmit_ = 0;
    chunkSlotsReusedSinceLastSubmit_ = 0;
}
} // namespace full_renderer::terrain
