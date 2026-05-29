#include "renderer/debug/FrameBudget.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
void expect(const bool condition, const char* message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

void recordStageReplacesPreviousDuration(int& failures)
{
    full_renderer::FrameBudgetStats stats;
    full_renderer::debug::recordFrameBudgetStage(
        stats,
        full_renderer::FrameBudgetStage::TerrainPlanning,
        120);
    full_renderer::debug::recordFrameBudgetStage(
        stats,
        full_renderer::FrameBudgetStage::TerrainPlanning,
        40);

    const std::uint32_t stageIndex =
        static_cast<std::uint32_t>(full_renderer::FrameBudgetStage::TerrainPlanning);
    expect(stats.cpuTimingEnabled == 1, "recording a stage enables CPU timing diagnostics", failures);
    expect((stats.recordedStageMask & (1U << stageIndex)) != 0U, "recording a stage sets its mask bit", failures);
    expect(stats.stageCpuMicroseconds[stageIndex] == 40, "stage duration is replaced deterministically", failures);
    expect(stats.totalCpuMicroseconds == 40, "total duration tracks replaced stage durations", failures);
}

void submissionEstimatesStagedBytes(int& failures)
{
    full_renderer::DrawItem drawItems[2] = {};
    full_renderer::InstancedDrawDesc instanced;
    float matrices[3][16] = {};
    instanced.modelMatrices = &matrices[0][0];
    instanced.instanceCount = 3;

    full_renderer::AnimatedDrawItem skinned;
    float palette[2][16] = {};
    skinned.palette.skinningMatrices = &palette[0][0];
    skinned.palette.matrixCount = 2;

    full_renderer::DecalDesc decals[4] = {};
    full_renderer::DecalSubmitDesc decalSubmit;
    decalSubmit.enabled = true;
    decalSubmit.decals = decals;
    decalSubmit.decalCount = 4;

    full_renderer::Particle particles[5] = {};
    full_renderer::ParticleBatchDesc particleBatch;
    particleBatch.particles = particles;
    particleBatch.particleCount = 5;
    full_renderer::ParticleSubmitDesc particleSubmit;
    particleSubmit.enabled = true;
    particleSubmit.batches = &particleBatch;
    particleSubmit.batchCount = 1;

    full_renderer::TerrainChunkHandle terrainHandles[6] = {};
    full_renderer::TerrainSubmitDesc terrainSubmit;
    terrainSubmit.chunks = terrainHandles;
    terrainSubmit.chunkCount = 6;

    full_renderer::RenderPacket packet;
    packet.drawItems = drawItems;
    packet.drawItemCount = 2;
    packet.instancedDraws = &instanced;
    packet.instancedDrawCount = 1;
    packet.animatedDraws = &skinned;
    packet.animatedDrawCount = 1;
    packet.decals = &decalSubmit;
    packet.particles = &particleSubmit;
    packet.terrain = &terrainSubmit;

    const full_renderer::FrameBudgetStats stats =
        full_renderer::debug::estimateFrameBudgetSubmission(packet);

    expect(stats.staticDrawStagedBytes == sizeof(drawItems), "static draw staged bytes are estimated", failures);
    expect(stats.instanceStagedBytes == sizeof(full_renderer::InstancedDrawDesc) + sizeof(matrices),
        "instanced descriptor and matrix bytes are estimated",
        failures);
    expect(stats.skinnedPaletteStagedBytes == sizeof(full_renderer::AnimatedDrawItem) + sizeof(palette),
        "skinned descriptor and palette bytes are estimated",
        failures);
    expect(stats.decalStagedBytes == sizeof(full_renderer::DecalSubmitDesc) + sizeof(decals),
        "decal descriptor bytes are estimated",
        failures);
    expect(stats.particleStagedBytes ==
            sizeof(full_renderer::ParticleSubmitDesc) +
                sizeof(full_renderer::ParticleBatchDesc) +
                sizeof(particles),
        "particle batch and descriptor bytes are estimated",
        failures);
    expect(stats.totalSubmittedRenderables == 15,
        "submitted renderable estimate includes terrain chunks, draw categories, decals, and particle batches",
        failures);
    expect(stats.frameAllocationEstimateCount == 5,
        "allocation estimate counts active staging categories",
        failures);
}

void runtimeStatsMergeBudgetCounters(int& failures)
{
    full_renderer::RendererStats rendererStats;
    rendererStats.renderedDraws = 7;
    rendererStats.shadowPassDraws = 3;
    rendererStats.postPassCount = 2;
    rendererStats.postSkippedPassCount = 1;
    rendererStats.staticMeshCulling.visibleCount = 5;
    rendererStats.staticMeshCulling.frustumCulledCount = 2;
    rendererStats.instancedMeshCulling.visibleCount = 1;
    rendererStats.decalActiveCount = 4;
    rendererStats.decalCulledCount = 1;
    rendererStats.particleAcceptedBatchCount = 2;
    rendererStats.particleCulledBatchCount = 1;
    rendererStats.shadowResourceReconfigured = 1;
    rendererStats.postSceneTargetReconfigured = 1;

    full_renderer::TerrainStats terrainStats;
    terrainStats.visibleChunks = 8;
    terrainStats.culledChunks = 3;
    terrainStats.chunksCreatedSinceLastSubmit = 6;
    terrainStats.chunksDestroyedSinceLastSubmit = 2;
    terrainStats.chunkSlotsReusedSinceLastSubmit = 1;

    full_renderer::FrameBudgetStats budget;
    full_renderer::debug::mergeFrameBudgetRuntimeStats(budget, rendererStats, terrainStats);

    expect(budget.totalVisibleRenderables == 20, "visible renderable aggregation is deterministic", failures);
    expect(budget.totalCulledRenderables == 7, "culled renderable aggregation is deterministic", failures);
    expect(budget.totalDrawSubmissionEstimate == 12, "draw/pass submission estimate aggregates renderer stats", failures);
    expect(budget.terrainChunksCreatedThisFrame == 6 &&
            budget.terrainChunksDestroyedThisFrame == 2 &&
            budget.terrainChunkSlotsReusedThisFrame == 1,
        "terrain churn counters are merged",
        failures);
    expect(budget.renderTargetRecreateCount == 2, "target recreate estimates are merged", failures);
    expect(budget.postPassSubmittedCount == 2 && budget.postPassSkippedCount == 1,
        "post pass submitted/skipped counters are merged",
        failures);
}

void thresholdEvaluationReportsWarnings(int& failures)
{
    full_renderer::RendererStats rendererStats;
    rendererStats.frameBudget.totalCpuMicroseconds = 200;
    rendererStats.frameBudget.totalStagedBytes = 4096;
    rendererStats.frameBudget.totalDrawSubmissionEstimate = 20;
    rendererStats.decalSubmittedCount = 9;
    rendererStats.particleSubmittedCount = 12;
    rendererStats.submittedAnimatedDraws = 3;

    full_renderer::TerrainStats terrainStats;
    terrainStats.chunksCreatedSinceLastSubmit = 4;

    full_renderer::debug::FrameBudgetThresholds thresholds;
    thresholds.totalCpuMicroseconds = 100;
    thresholds.totalStagedBytes = 1024;
    thresholds.terrainChunkChurn = 1;
    thresholds.drawSubmissionCount = 10;
    thresholds.particleCount = 10;
    thresholds.decalCount = 8;
    thresholds.skinnedDrawCount = 2;

    const full_renderer::debug::FrameBudgetEvaluation evaluation =
        full_renderer::debug::evaluateFrameBudget(rendererStats, terrainStats, thresholds);

    expect((evaluation.warningMask & full_renderer::debug::kFrameBudgetWarningCpuTime) != 0U,
        "CPU threshold warning is reported",
        failures);
    expect((evaluation.warningMask & full_renderer::debug::kFrameBudgetWarningStagedBytes) != 0U,
        "staged-byte threshold warning is reported",
        failures);
    expect((evaluation.warningMask & full_renderer::debug::kFrameBudgetWarningTerrainChurn) != 0U,
        "terrain churn threshold warning is reported",
        failures);
    expect((evaluation.warningMask & full_renderer::debug::kFrameBudgetWarningDrawSubmissions) != 0U,
        "draw submission threshold warning is reported",
        failures);
    expect((evaluation.warningMask & full_renderer::debug::kFrameBudgetWarningParticles) != 0U,
        "particle threshold warning is reported",
        failures);
    expect((evaluation.warningMask & full_renderer::debug::kFrameBudgetWarningDecals) != 0U,
        "decal threshold warning is reported",
        failures);
    expect((evaluation.warningMask & full_renderer::debug::kFrameBudgetWarningSkinnedDraws) != 0U,
        "skinned threshold warning is reported",
        failures);
    expect(evaluation.warningCount == 7, "warning count matches warning mask", failures);
}
} // namespace

int main()
{
    int failures = 0;
    recordStageReplacesPreviousDuration(failures);
    submissionEstimatesStagedBytes(failures);
    runtimeStatsMergeBudgetCounters(failures);
    thresholdEvaluationReportsWarnings(failures);

    if (failures != 0)
    {
        return EXIT_FAILURE;
    }

    std::cout << "frame budget tests passed\n";
    return EXIT_SUCCESS;
}
