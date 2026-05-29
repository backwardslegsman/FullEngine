#include "renderer/debug/FrameBudget.hpp"

#include <cstddef>

namespace full_renderer::debug
{
namespace
{
std::uint32_t stageIndex(const FrameBudgetStage stage) noexcept
{
    return static_cast<std::uint32_t>(stage);
}

bool isStageIndexValid(const std::uint32_t index) noexcept
{
    return index < kFrameBudgetStageCount;
}

std::uint64_t addSaturating(const std::uint64_t lhs, const std::uint64_t rhs) noexcept
{
    constexpr std::uint64_t kMax = UINT64_MAX;
    return lhs > (kMax - rhs) ? kMax : lhs + rhs;
}

std::uint64_t multiplySaturating(const std::uint64_t lhs, const std::uint64_t rhs) noexcept
{
    if (lhs == 0 || rhs == 0)
    {
        return 0;
    }
    constexpr std::uint64_t kMax = UINT64_MAX;
    return lhs > (kMax / rhs) ? kMax : lhs * rhs;
}

void addStagedBytes(std::uint64_t& total, const std::uint64_t value) noexcept
{
    total = addSaturating(total, value);
}

void incrementIfUsed(std::uint32_t& allocationEstimateCount, const std::uint64_t bytes) noexcept
{
    if (bytes > 0)
    {
        ++allocationEstimateCount;
    }
}

void addSubmittedRenderables(FrameBudgetStats& stats, const std::uint32_t count) noexcept
{
    const std::uint64_t total =
        static_cast<std::uint64_t>(stats.totalSubmittedRenderables) + count;
    stats.totalSubmittedRenderables = total > UINT32_MAX ? UINT32_MAX : static_cast<std::uint32_t>(total);
}

void addWarning(
    const bool condition,
    const std::uint32_t bit,
    FrameBudgetEvaluation& evaluation) noexcept
{
    if (!condition)
    {
        return;
    }

    evaluation.warningMask |= bit;
    ++evaluation.warningCount;
}

std::uint32_t submittedDecalCount(const RendererStats& stats) noexcept
{
    return stats.decalSubmittedCount;
}

std::uint32_t submittedParticleCount(const RendererStats& stats) noexcept
{
    return stats.particleSubmittedCount;
}

std::uint32_t submittedSkinnedDrawCount(const RendererStats& stats) noexcept
{
    return stats.submittedAnimatedDraws;
}
} // namespace

const char* frameBudgetStageName(const FrameBudgetStage stage) noexcept
{
    switch (stage)
    {
    case FrameBudgetStage::BeginFrame:
        return "Begin frame";
    case FrameBudgetStage::SubmitValidation:
        return "Submit validation";
    case FrameBudgetStage::CullingDiagnostics:
        return "Culling diagnostics";
    case FrameBudgetStage::TerrainPlanning:
        return "Terrain planning";
    case FrameBudgetStage::StaticMeshPlanning:
        return "Static mesh planning";
    case FrameBudgetStage::InstancedPlanning:
        return "Instanced planning";
    case FrameBudgetStage::SkinnedPlanning:
        return "Skinned planning";
    case FrameBudgetStage::ShadowPlanning:
        return "Shadow planning";
    case FrameBudgetStage::DecalPlanning:
        return "Decal planning";
    case FrameBudgetStage::ParticlePlanning:
        return "Particle planning";
    case FrameBudgetStage::SelectionOutlinePlanning:
        return "Selection planning";
    case FrameBudgetStage::PostPassPlanning:
        return "Post/pass planning";
    case FrameBudgetStage::DebugOverlayPlanning:
        return "Debug overlay planning";
    case FrameBudgetStage::BackendSubmit:
        return "Backend submit";
    case FrameBudgetStage::EndFrame:
        return "End frame";
    case FrameBudgetStage::Count:
        break;
    }

    return "Unknown";
}

FrameBudgetThresholds makeDefaultFrameBudgetThresholds() noexcept
{
    return {};
}

void recordFrameBudgetStage(
    FrameBudgetStats& stats,
    const FrameBudgetStage stage,
    const std::uint64_t microseconds) noexcept
{
    const std::uint32_t index = stageIndex(stage);
    if (!isStageIndexValid(index))
    {
        return;
    }

    stats.cpuTimingEnabled = 1;
    stats.recordedStageMask |= (1U << index);
    stats.totalCpuMicroseconds =
        stats.totalCpuMicroseconds >= stats.stageCpuMicroseconds[index] ?
            stats.totalCpuMicroseconds - stats.stageCpuMicroseconds[index] :
            0U;
    stats.totalCpuMicroseconds = addSaturating(stats.totalCpuMicroseconds, microseconds);
    stats.stageCpuMicroseconds[index] = microseconds;
}

FrameBudgetStats estimateFrameBudgetSubmission(const RenderPacket& packet) noexcept
{
    FrameBudgetStats stats;

    stats.totalSubmittedRenderables = packet.drawItemCount +
        packet.instancedDrawCount +
        packet.animatedDrawCount;

    stats.staticDrawStagedBytes = multiplySaturating(packet.drawItemCount, sizeof(DrawItem));
    stats.instanceStagedBytes = multiplySaturating(packet.instancedDrawCount, sizeof(InstancedDrawDesc));
    if (packet.instancedDraws != nullptr)
    {
        for (std::uint32_t index = 0; index < packet.instancedDrawCount; ++index)
        {
            const InstancedDrawDesc& batch = packet.instancedDraws[index];
            stats.instanceStagedBytes = addSaturating(
                stats.instanceStagedBytes,
                multiplySaturating(batch.instanceCount, sizeof(float) * 16U));
        }
    }

    stats.skinnedPaletteStagedBytes = multiplySaturating(packet.animatedDrawCount, sizeof(AnimatedDrawItem));
    if (packet.animatedDraws != nullptr)
    {
        for (std::uint32_t index = 0; index < packet.animatedDrawCount; ++index)
        {
            const AnimatedDrawItem& draw = packet.animatedDraws[index];
            stats.skinnedPaletteStagedBytes = addSaturating(
                stats.skinnedPaletteStagedBytes,
                multiplySaturating(draw.palette.matrixCount, sizeof(float) * 16U));
        }
    }

    if (packet.decals != nullptr && packet.decals->enabled)
    {
        addSubmittedRenderables(stats, packet.decals->decalCount);
        stats.decalStagedBytes = addSaturating(
            multiplySaturating(packet.decals->decalCount, sizeof(DecalDesc)),
            sizeof(DecalSubmitDesc));
    }

    if (packet.particles != nullptr && packet.particles->enabled)
    {
        addSubmittedRenderables(stats, packet.particles->batchCount);
        stats.particleStagedBytes = addSaturating(
            multiplySaturating(packet.particles->batchCount, sizeof(ParticleBatchDesc)),
            sizeof(ParticleSubmitDesc));
        if (packet.particles->batches != nullptr)
        {
            for (std::uint32_t index = 0; index < packet.particles->batchCount; ++index)
            {
                const ParticleBatchDesc& batch = packet.particles->batches[index];
                stats.particleStagedBytes = addSaturating(
                    stats.particleStagedBytes,
                    multiplySaturating(batch.particleCount, sizeof(Particle)));
            }
        }
    }

    if (packet.terrain != nullptr)
    {
        addSubmittedRenderables(stats, packet.terrain->chunkCount);
    }

    addStagedBytes(stats.totalStagedBytes, stats.staticDrawStagedBytes);
    addStagedBytes(stats.totalStagedBytes, stats.instanceStagedBytes);
    addStagedBytes(stats.totalStagedBytes, stats.skinnedPaletteStagedBytes);
    addStagedBytes(stats.totalStagedBytes, stats.decalStagedBytes);
    addStagedBytes(stats.totalStagedBytes, stats.particleStagedBytes);

    incrementIfUsed(stats.frameAllocationEstimateCount, stats.staticDrawStagedBytes);
    incrementIfUsed(stats.frameAllocationEstimateCount, stats.instanceStagedBytes);
    incrementIfUsed(stats.frameAllocationEstimateCount, stats.skinnedPaletteStagedBytes);
    incrementIfUsed(stats.frameAllocationEstimateCount, stats.decalStagedBytes);
    incrementIfUsed(stats.frameAllocationEstimateCount, stats.particleStagedBytes);

    return stats;
}

void mergeFrameBudgetRuntimeStats(
    FrameBudgetStats& budget,
    const RendererStats& rendererStats,
    const TerrainStats& terrainStats) noexcept
{
    budget.totalVisibleRenderables =
        terrainStats.visibleChunks +
        rendererStats.staticMeshCulling.visibleCount +
        rendererStats.instancedMeshCulling.visibleCount +
        rendererStats.skinnedMeshCulling.visibleCount +
        rendererStats.decalActiveCount +
        rendererStats.particleAcceptedBatchCount;

    budget.totalCulledRenderables =
        terrainStats.culledChunks +
        rendererStats.staticMeshCulling.frustumCulledCount +
        rendererStats.instancedMeshCulling.frustumCulledCount +
        rendererStats.skinnedMeshCulling.frustumCulledCount +
        rendererStats.decalCulledCount +
        rendererStats.particleCulledBatchCount;

    const std::uint32_t drawSubmissionEstimate =
        rendererStats.renderedDraws +
        rendererStats.shadowPassDraws +
        rendererStats.ssaoDepthPassDraws +
        rendererStats.ssaoPassDraws +
        rendererStats.ssaoBlurPassDraws +
        rendererStats.ssaoCompositeDraws +
        rendererStats.decalPassDraws +
        rendererStats.particleDrawCalls +
        rendererStats.selectionMaskDraws +
        rendererStats.selectionOutlineDraws +
        (rendererStats.colorGradingPassSubmitted ? 1U : 0U) +
        rendererStats.postPassCount;
    if (drawSubmissionEstimate > budget.totalDrawSubmissionEstimate)
    {
        budget.totalDrawSubmissionEstimate = drawSubmissionEstimate;
    }

    budget.terrainChunksCreatedThisFrame = terrainStats.chunksCreatedSinceLastSubmit;
    budget.terrainChunksDestroyedThisFrame = terrainStats.chunksDestroyedSinceLastSubmit;
    budget.terrainChunkSlotsReusedThisFrame = terrainStats.chunkSlotsReusedSinceLastSubmit;
    budget.renderTargetRecreateCount =
        rendererStats.shadowResourceReconfigured +
        rendererStats.postSceneTargetReconfigured;
    budget.postPassSubmittedCount = rendererStats.postPassCount;
    budget.postPassSkippedCount = rendererStats.postSkippedPassCount;
}

FrameBudgetEvaluation evaluateFrameBudget(
    const RendererStats& rendererStats,
    const TerrainStats& terrainStats,
    const FrameBudgetThresholds& thresholds) noexcept
{
    FrameBudgetStats budget = rendererStats.frameBudget;
    mergeFrameBudgetRuntimeStats(budget, rendererStats, terrainStats);

    FrameBudgetEvaluation evaluation;
    addWarning(
        thresholds.totalCpuMicroseconds > 0 &&
            budget.totalCpuMicroseconds > thresholds.totalCpuMicroseconds,
        kFrameBudgetWarningCpuTime,
        evaluation);
    addWarning(
        thresholds.totalStagedBytes > 0 &&
            budget.totalStagedBytes > thresholds.totalStagedBytes,
        kFrameBudgetWarningStagedBytes,
        evaluation);
    addWarning(
        thresholds.terrainChunkChurn > 0 &&
            (budget.terrainChunksCreatedThisFrame +
                budget.terrainChunksDestroyedThisFrame +
                budget.terrainChunkSlotsReusedThisFrame) > thresholds.terrainChunkChurn,
        kFrameBudgetWarningTerrainChurn,
        evaluation);
    addWarning(
        thresholds.drawSubmissionCount > 0 &&
            budget.totalDrawSubmissionEstimate > thresholds.drawSubmissionCount,
        kFrameBudgetWarningDrawSubmissions,
        evaluation);
    addWarning(
        thresholds.particleCount > 0 &&
            submittedParticleCount(rendererStats) > thresholds.particleCount,
        kFrameBudgetWarningParticles,
        evaluation);
    addWarning(
        thresholds.decalCount > 0 &&
            submittedDecalCount(rendererStats) > thresholds.decalCount,
        kFrameBudgetWarningDecals,
        evaluation);
    addWarning(
        thresholds.skinnedDrawCount > 0 &&
            submittedSkinnedDrawCount(rendererStats) > thresholds.skinnedDrawCount,
        kFrameBudgetWarningSkinnedDraws,
        evaluation);
    return evaluation;
}
} // namespace full_renderer::debug
