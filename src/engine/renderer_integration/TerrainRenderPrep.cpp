#include "engine/renderer_integration/TerrainRenderPrep.hpp"

namespace full_engine
{
namespace
{
void countSkippedStatus(TerrainRenderPrepSummary& summary, const RenderChunkStatus status) noexcept
{
    switch (status)
    {
    case RenderChunkStatus::Ready:
        break;
    case RenderChunkStatus::NotResident:
        ++summary.skippedNotResidentCount;
        break;
    case RenderChunkStatus::MissingChunk:
        ++summary.skippedMissingChunkCount;
        break;
    case RenderChunkStatus::InvalidBounds:
        ++summary.skippedInvalidBoundsCount;
        break;
    case RenderChunkStatus::OutOfRange:
        ++summary.skippedOutOfRangeCount;
        break;
    }
}
} // namespace

TerrainRenderPrep prepareTerrainRenderChunks(const WorldRenderSnapshot& snapshot)
{
    TerrainRenderPrep prep;
    prep.summary.invalidInputCount = snapshot.invalidInputCount;
    prep.chunks.reserve(snapshot.readyCount);

    for (const RenderChunkSnapshot& source : snapshot.chunks)
    {
        if (source.status != RenderChunkStatus::Ready)
        {
            countSkippedStatus(prep.summary, source.status);
            continue;
        }

        TerrainChunkRenderPrep chunk;
        chunk.id = source.id;
        chunk.bounds = source.bounds;
        chunk.sourceStatus = RenderChunkStatus::Ready;
        prep.chunks.push_back(chunk);
        ++prep.summary.readyCount;
    }

    return prep;
}
} // namespace full_engine
