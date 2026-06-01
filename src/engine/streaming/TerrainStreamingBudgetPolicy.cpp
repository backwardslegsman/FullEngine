#include "engine/streaming/TerrainStreamingBudgetPolicy.hpp"

#include <vector>

namespace full_engine
{
namespace
{
std::size_t deferredWorkCount(const TerrainStreamingTickEvent& event) noexcept
{
    return event.streaming.deferredSetupAddCount +
        event.streaming.deferredSetupRemoveCount +
        event.streaming.deferredMakeResidentCount +
        event.streaming.deferredMakeUnloadedCount +
        event.runtimeLifecycle.deferredCreateCount +
        event.runtimeLifecycle.deferredUpdateCount +
        event.runtimeLifecycle.deferredReleaseCount;
}

TerrainStreamingLoopBudgetOptions makeProfile(
    const std::size_t setupAdds,
    const std::size_t setupRemoves,
    const std::size_t makeResident,
    const std::size_t makeUnloaded,
    const std::size_t pipelineCreates,
    const std::size_t pipelineUpdates,
    const std::size_t pipelineReleases) noexcept
{
    TerrainStreamingLoopBudgetOptions options;
    options.queue.maxSetupAdds = setupAdds;
    options.queue.maxSetupRemoves = setupRemoves;
    options.queue.maxMakeResident = makeResident;
    options.queue.maxMakeUnloaded = makeUnloaded;
    options.maxPipelineCreates = pipelineCreates;
    options.maxPipelineUpdates = pipelineUpdates;
    options.maxPipelineReleases = pipelineReleases;
    return options;
}
} // namespace

const char* terrainStreamingBudgetProfileName(
    const TerrainStreamingBudgetProfile profile) noexcept
{
    switch (profile)
    {
    case TerrainStreamingBudgetProfile::Unlimited:
        return "Unlimited";
    case TerrainStreamingBudgetProfile::Conservative:
        return "Conservative";
    case TerrainStreamingBudgetProfile::Balanced:
        return "Balanced";
    case TerrainStreamingBudgetProfile::CatchUp:
        return "CatchUp";
    }

    return "Unknown";
}

TerrainStreamingLoopBudgetOptions selectTerrainStreamingLoopBudgets(
    const TerrainStreamingBudgetProfile profile) noexcept
{
    switch (profile)
    {
    case TerrainStreamingBudgetProfile::Unlimited:
        return {};
    case TerrainStreamingBudgetProfile::Conservative:
        return makeProfile(1, 1, 2, 2, 1, 1, 1);
    case TerrainStreamingBudgetProfile::Balanced:
        return makeProfile(2, 2, 4, 4, 2, 2, 2);
    case TerrainStreamingBudgetProfile::CatchUp:
        return makeProfile(4, 4, 8, 8, 4, 4, 4);
    }

    return selectTerrainStreamingLoopBudgets(TerrainStreamingBudgetProfile::Balanced);
}

TerrainStreamingAdaptiveBudgetResult selectAdaptiveTerrainStreamingBudgetProfile(
    const TerrainStreamingTickHistory& history,
    const TerrainStreamingAdaptiveBudgetOptions& options) noexcept
{
    TerrainStreamingAdaptiveBudgetResult result;

    const std::vector<TerrainStreamingTickEvent> events = history.events();
    if (events.empty())
    {
        result.profile = TerrainStreamingBudgetProfile::Balanced;
        return result;
    }

    const std::size_t requestedWindow =
        options.recentTickCount == 0 ? 5 : options.recentTickCount;
    const std::size_t firstEvent =
        events.size() > requestedWindow ? events.size() - requestedWindow : 0;
    for (std::size_t index = firstEvent; index < events.size(); ++index)
    {
        result.deferredWorkCount += deferredWorkCount(events[index]);
        ++result.inspectedTickCount;
    }

    if (result.deferredWorkCount == 0)
    {
        result.profile = TerrainStreamingBudgetProfile::Conservative;
        return result;
    }

    const std::size_t catchUpThreshold =
        options.catchUpDeferredWorkThreshold == 0 ?
            1 :
            options.catchUpDeferredWorkThreshold;
    result.profile = result.deferredWorkCount >= catchUpThreshold ?
        TerrainStreamingBudgetProfile::CatchUp :
        TerrainStreamingBudgetProfile::Balanced;
    return result;
}

TerrainStreamingLoopBudgetOptions selectAdaptiveTerrainStreamingLoopBudgets(
    const TerrainStreamingTickHistory& history,
    const TerrainStreamingAdaptiveBudgetOptions& options) noexcept
{
    const TerrainStreamingAdaptiveBudgetResult result =
        selectAdaptiveTerrainStreamingBudgetProfile(history, options);
    return selectTerrainStreamingLoopBudgets(result.profile);
}
} // namespace full_engine
