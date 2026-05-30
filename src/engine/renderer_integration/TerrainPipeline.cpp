#include "engine/renderer_integration/TerrainPipeline.hpp"

#include <vector>

namespace full_engine
{
TerrainPipelineRunResult runTerrainPipeline(
    full_renderer::IRenderer& renderer,
    const WorldChunkRegistry& registry,
    const WorldChunkCatalog& worldCatalog,
    const TerrainResourceCatalog& resources,
    ChunkTerrainHandleMap& handles,
    const TerrainPipelineRunOptions& options)
{
    TerrainPipelineRunResult result;
    const std::vector<WorldChunkDesc> chunkDescs = worldCatalog.descs();

    result.snapshot = buildWorldRenderSnapshot(
        registry,
        chunkDescs.data(),
        chunkDescs.size(),
        options.snapshotOptions);
    result.prep = prepareTerrainRenderChunks(result.snapshot);
    result.lifecycle = planTerrainLifecycle(result.prep, handles, options.lifecycleOptions);
    result.commands = buildTerrainRendererCommands(result.lifecycle);
    result.descriptors = buildTerrainDescriptors(result.commands, resources);
    result.submission = submitTerrainCommands(renderer, result.descriptors, result.commands, handles);
    result.succeeded = result.submission.summary.rendererFailedCount == 0 &&
        result.submission.summary.handleMapFailedCount == 0;
    result.diagnostics = makeTerrainPipelineDiagnostics(
        result.snapshot,
        result.prep,
        result.lifecycle,
        result.commands,
        result.descriptors,
        result.submission,
        handles.mappedCount());
    return result;
}
} // namespace full_engine
