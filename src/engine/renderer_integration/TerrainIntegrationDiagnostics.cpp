#include "engine/renderer_integration/TerrainIntegrationDiagnostics.hpp"

namespace full_engine
{
TerrainSetupRequestDiagnostics makeTerrainSetupRequestDiagnostics(
    const TerrainChunkRequestApplyResult& result)
{
    TerrainSetupRequestDiagnostics diagnostics;
    diagnostics.requestCount = result.records.size();
    diagnostics.summary = result.summary;

    for (const TerrainChunkRequestRecord& record : result.records)
    {
        if (record.request.type == TerrainChunkRequestType::Add)
        {
            ++diagnostics.addCount;
        }
        else
        {
            ++diagnostics.removeCount;
        }
    }

    return diagnostics;
}

TerrainResidencyRequestDiagnostics makeTerrainResidencyRequestDiagnostics(
    const WorldChunkResidencyApplyResult& result)
{
    TerrainResidencyRequestDiagnostics diagnostics;
    diagnostics.requestCount = result.records.size();
    diagnostics.summary = result.summary;

    for (const WorldChunkResidencyRequestRecord& record : result.records)
    {
        if (record.request.type == WorldChunkResidencyRequestType::MakeResident)
        {
            ++diagnostics.makeResidentCount;
        }
        else
        {
            ++diagnostics.makeUnloadedCount;
        }
    }

    return diagnostics;
}

TerrainPipelineDiagnostics makeTerrainPipelineDiagnostics(
    const WorldRenderSnapshot& snapshot,
    const TerrainRenderPrep& prep,
    const TerrainLifecyclePlan& lifecycle,
    const TerrainRendererCommandList& commands,
    const TerrainDescriptorBuildResult& descriptors,
    const TerrainSubmissionResult& submission,
    const std::size_t handleCount)
{
    TerrainPipelineDiagnostics diagnostics;
    diagnostics.handleCount = handleCount;
    diagnostics.snapshotReadyCount = snapshot.readyCount;
    diagnostics.snapshotNotResidentCount = snapshot.notResidentCount;
    diagnostics.snapshotMissingChunkCount = snapshot.missingChunkCount;
    diagnostics.snapshotInvalidBoundsCount = snapshot.invalidBoundsCount;
    diagnostics.snapshotOutOfRangeCount = snapshot.outOfRangeCount;
    diagnostics.snapshotInvalidInputCount = snapshot.invalidInputCount;
    diagnostics.prep = prep.summary;
    diagnostics.lifecycle = lifecycle.summary;
    diagnostics.commands = commands.summary;
    diagnostics.descriptors = descriptors.summary;
    diagnostics.submission = submission.summary;
    return diagnostics;
}
} // namespace full_engine
