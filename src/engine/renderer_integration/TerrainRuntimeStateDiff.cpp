#include "engine/renderer_integration/TerrainRuntimeStateDiff.hpp"

#include <map>

namespace full_engine
{
namespace
{
void countChange(TerrainRuntimeStateDiffSummary& summary, const TerrainRuntimeStateChangeType type) noexcept
{
    switch (type)
    {
    case TerrainRuntimeStateChangeType::Added:
        ++summary.addedCount;
        break;
    case TerrainRuntimeStateChangeType::Removed:
        ++summary.removedCount;
        break;
    case TerrainRuntimeStateChangeType::ReadinessChanged:
        ++summary.readinessChangedCount;
        break;
    case TerrainRuntimeStateChangeType::ResidencyChanged:
        ++summary.residencyChangedCount;
        break;
    case TerrainRuntimeStateChangeType::HandlePresenceChanged:
        ++summary.handlePresenceChangedCount;
        break;
    }
}

TerrainRuntimeStateChange makeChange(
    const ChunkId& id,
    const TerrainRuntimeStateChangeType type,
    const TerrainRuntimeChunkState* previous,
    const TerrainRuntimeChunkState* current)
{
    TerrainRuntimeStateChange change;
    change.id = id;
    change.type = type;
    if (previous != nullptr)
    {
        change.previousReadiness = previous->readiness;
        change.previousResidency = previous->residency;
        change.previousHasTerrainHandle = previous->hasTerrainHandle;
    }
    if (current != nullptr)
    {
        change.currentReadiness = current->readiness;
        change.currentResidency = current->residency;
        change.currentHasTerrainHandle = current->hasTerrainHandle;
    }
    return change;
}

std::map<ChunkId, TerrainRuntimeChunkState> makeMap(const TerrainRuntimeStateSnapshot& snapshot)
{
    std::map<ChunkId, TerrainRuntimeChunkState> chunks;
    for (const TerrainRuntimeChunkState& chunk : snapshot.chunks)
    {
        chunks[chunk.id] = chunk;
    }
    return chunks;
}
} // namespace

TerrainRuntimeStateDiff diffTerrainRuntimeStateSnapshots(
    const TerrainRuntimeStateSnapshot& previous,
    const TerrainRuntimeStateSnapshot& current)
{
    const std::map<ChunkId, TerrainRuntimeChunkState> previousChunks = makeMap(previous);
    const std::map<ChunkId, TerrainRuntimeChunkState> currentChunks = makeMap(current);

    TerrainRuntimeStateDiff diff;
    auto previousIt = previousChunks.begin();
    auto currentIt = currentChunks.begin();
    while (previousIt != previousChunks.end() || currentIt != currentChunks.end())
    {
        TerrainRuntimeStateChange change;
        bool hasChange = false;

        if (currentIt == currentChunks.end() ||
            (previousIt != previousChunks.end() && previousIt->first < currentIt->first))
        {
            change = makeChange(previousIt->first, TerrainRuntimeStateChangeType::Removed, &previousIt->second, nullptr);
            ++previousIt;
            hasChange = true;
        }
        else if (previousIt == previousChunks.end() || currentIt->first < previousIt->first)
        {
            change = makeChange(currentIt->first, TerrainRuntimeStateChangeType::Added, nullptr, &currentIt->second);
            ++currentIt;
            hasChange = true;
        }
        else
        {
            const TerrainRuntimeChunkState& previousState = previousIt->second;
            const TerrainRuntimeChunkState& currentState = currentIt->second;
            if (previousState.readiness != currentState.readiness)
            {
                change = makeChange(
                    previousIt->first,
                    TerrainRuntimeStateChangeType::ReadinessChanged,
                    &previousState,
                    &currentState);
                hasChange = true;
            }
            else if (previousState.residency != currentState.residency)
            {
                change = makeChange(
                    previousIt->first,
                    TerrainRuntimeStateChangeType::ResidencyChanged,
                    &previousState,
                    &currentState);
                hasChange = true;
            }
            else if (previousState.hasTerrainHandle != currentState.hasTerrainHandle)
            {
                change = makeChange(
                    previousIt->first,
                    TerrainRuntimeStateChangeType::HandlePresenceChanged,
                    &previousState,
                    &currentState);
                hasChange = true;
            }

            ++previousIt;
            ++currentIt;
        }

        if (hasChange)
        {
            countChange(diff.summary, change.type);
            diff.changes.push_back(change);
        }
    }

    return diff;
}
} // namespace full_engine
