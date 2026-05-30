#include "engine/renderer_integration/WorldRenderSnapshot.hpp"

namespace full_engine
{
namespace
{
void incrementCounter(WorldRenderSnapshot& snapshot, const RenderChunkStatus status) noexcept
{
    switch (status)
    {
    case RenderChunkStatus::Ready:
        ++snapshot.readyCount;
        break;
    case RenderChunkStatus::NotResident:
        ++snapshot.notResidentCount;
        break;
    case RenderChunkStatus::MissingChunk:
        ++snapshot.missingChunkCount;
        break;
    case RenderChunkStatus::InvalidBounds:
        ++snapshot.invalidBoundsCount;
        break;
    case RenderChunkStatus::OutOfRange:
        ++snapshot.outOfRangeCount;
        break;
    }
}

RenderChunkStatus statusForRenderSpaceResult(const RenderSpaceResult result) noexcept
{
    switch (result)
    {
    case RenderSpaceResult::Success:
        return RenderChunkStatus::Ready;
    case RenderSpaceResult::InvalidArgument:
        return RenderChunkStatus::InvalidBounds;
    case RenderSpaceResult::OutOfRange:
        return RenderChunkStatus::OutOfRange;
    }

    return RenderChunkStatus::InvalidBounds;
}
} // namespace

WorldRenderSnapshot buildWorldRenderSnapshot(
    const WorldChunkRegistry& registry,
    const WorldChunkRenderDesc* chunks,
    const std::size_t chunkCount,
    const WorldRenderSnapshotOptions& options)
{
    WorldRenderSnapshot snapshot;
    if (chunks == nullptr)
    {
        snapshot.invalidInputCount = chunkCount;
        return snapshot;
    }

    snapshot.chunks.reserve(chunkCount);

    for (std::size_t index = 0; index < chunkCount; ++index)
    {
        const WorldChunkRenderDesc& desc = chunks[index];
        RenderChunkSnapshot record;
        record.id = desc.id;

        const WorldChunkInfo* chunkInfo = registry.findChunk(desc.id);
        if (chunkInfo == nullptr)
        {
            record.status = RenderChunkStatus::MissingChunk;
        }
        else
        {
            record.residency = chunkInfo->residency;
            if (chunkInfo->residency != ChunkResidencyState::Resident)
            {
                record.status = RenderChunkStatus::NotResident;
            }
            else
            {
                const RenderSpaceResult renderSpaceResult =
                    toRenderBounds(options.origin, desc.bounds, record.bounds, options.limits);
                record.status = statusForRenderSpaceResult(renderSpaceResult);
            }
        }

        incrementCounter(snapshot, record.status);
        snapshot.chunks.push_back(record);
    }

    return snapshot;
}
} // namespace full_engine
