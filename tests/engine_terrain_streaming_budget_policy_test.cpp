#include "engine/streaming/TerrainStreamingBudgetPolicy.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* const message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

void expectBudget(
    const full_engine::TerrainStreamingLoopBudgetOptions& options,
    const std::size_t setupAdds,
    const std::size_t setupRemoves,
    const std::size_t makeResident,
    const std::size_t makeUnloaded,
    const std::size_t pipelineCreates,
    const std::size_t pipelineUpdates,
    const std::size_t pipelineReleases,
    const char* const label,
    std::vector<std::string>& failures)
{
    expect(options.queue.maxSetupAdds == setupAdds, label, failures);
    expect(options.queue.maxSetupRemoves == setupRemoves, label, failures);
    expect(options.queue.maxMakeResident == makeResident, label, failures);
    expect(options.queue.maxMakeUnloaded == makeUnloaded, label, failures);
    expect(options.maxPipelineCreates == pipelineCreates, label, failures);
    expect(options.maxPipelineUpdates == pipelineUpdates, label, failures);
    expect(options.maxPipelineReleases == pipelineReleases, label, failures);
}

void testProfileNames(std::vector<std::string>& failures)
{
    expect(
        std::string(full_engine::terrainStreamingBudgetProfileName(
            full_engine::TerrainStreamingBudgetProfile::Unlimited)) == "Unlimited",
        "unlimited profile name is stable",
        failures);
    expect(
        std::string(full_engine::terrainStreamingBudgetProfileName(
            full_engine::TerrainStreamingBudgetProfile::Conservative)) == "Conservative",
        "conservative profile name is stable",
        failures);
    expect(
        std::string(full_engine::terrainStreamingBudgetProfileName(
            full_engine::TerrainStreamingBudgetProfile::Balanced)) == "Balanced",
        "balanced profile name is stable",
        failures);
    expect(
        std::string(full_engine::terrainStreamingBudgetProfileName(
            full_engine::TerrainStreamingBudgetProfile::CatchUp)) == "CatchUp",
        "catch-up profile name is stable",
        failures);
}

void testUnlimitedProfile(std::vector<std::string>& failures)
{
    const full_engine::TerrainStreamingLoopBudgetOptions options =
        full_engine::selectTerrainStreamingLoopBudgets(
            full_engine::TerrainStreamingBudgetProfile::Unlimited);

    expectBudget(
        options,
        full_engine::kUnlimitedTerrainStreamingQueueBudget,
        full_engine::kUnlimitedTerrainStreamingQueueBudget,
        full_engine::kUnlimitedTerrainStreamingQueueBudget,
        full_engine::kUnlimitedTerrainStreamingQueueBudget,
        full_engine::kUnlimitedTerrainLifecycleBudget,
        full_engine::kUnlimitedTerrainLifecycleBudget,
        full_engine::kUnlimitedTerrainLifecycleBudget,
        "unlimited profile leaves caps unlimited",
        failures);
}

void testNamedProfiles(std::vector<std::string>& failures)
{
    expectBudget(
        full_engine::selectTerrainStreamingLoopBudgets(
            full_engine::TerrainStreamingBudgetProfile::Conservative),
        1,
        1,
        2,
        2,
        1,
        1,
        1,
        "conservative profile selects small caps",
        failures);
    expectBudget(
        full_engine::selectTerrainStreamingLoopBudgets(
            full_engine::TerrainStreamingBudgetProfile::Balanced),
        2,
        2,
        4,
        4,
        2,
        2,
        2,
        "balanced profile selects moderate caps",
        failures);
    expectBudget(
        full_engine::selectTerrainStreamingLoopBudgets(
            full_engine::TerrainStreamingBudgetProfile::CatchUp),
        4,
        4,
        8,
        8,
        4,
        4,
        4,
        "catch-up profile selects larger caps",
        failures);
}

void testUnknownProfileFallsBackToBalanced(std::vector<std::string>& failures)
{
    const full_engine::TerrainStreamingLoopBudgetOptions unknown =
        full_engine::selectTerrainStreamingLoopBudgets(
            static_cast<full_engine::TerrainStreamingBudgetProfile>(99));
    const full_engine::TerrainStreamingLoopBudgetOptions balanced =
        full_engine::selectTerrainStreamingLoopBudgets(
            full_engine::TerrainStreamingBudgetProfile::Balanced);

    expect(unknown.queue.maxSetupAdds == balanced.queue.maxSetupAdds, "unknown profile falls back setup adds", failures);
    expect(unknown.queue.maxMakeResident == balanced.queue.maxMakeResident, "unknown profile falls back residency", failures);
    expect(unknown.maxPipelineCreates == balanced.maxPipelineCreates, "unknown profile falls back lifecycle", failures);
}

full_engine::TerrainStreamingTickEvent tickWithDeferredWork(const std::size_t deferredWork) noexcept
{
    full_engine::TerrainStreamingTickEvent event;
    event.streaming.deferredSetupAddCount = deferredWork;
    return event;
}

void testAdaptiveProfileSelection(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingTickHistory history;

    full_engine::TerrainStreamingAdaptiveBudgetResult result =
        full_engine::selectAdaptiveTerrainStreamingBudgetProfile(history);
    expect(result.profile == full_engine::TerrainStreamingBudgetProfile::Balanced, "empty adaptive history starts balanced", failures);
    expect(result.inspectedTickCount == 0, "empty adaptive history inspects no ticks", failures);

    history.append({});
    history.append({});
    result = full_engine::selectAdaptiveTerrainStreamingBudgetProfile(history);
    expect(result.profile == full_engine::TerrainStreamingBudgetProfile::Conservative, "zero pressure selects conservative", failures);
    expect(result.inspectedTickCount == 2, "adaptive selection counts inspected ticks", failures);
    expect(result.deferredWorkCount == 0, "adaptive selection reports zero pressure", failures);

    history.append(tickWithDeferredWork(3));
    result = full_engine::selectAdaptiveTerrainStreamingBudgetProfile(history);
    expect(result.profile == full_engine::TerrainStreamingBudgetProfile::Balanced, "light pressure selects balanced", failures);
    expect(result.deferredWorkCount == 3, "adaptive selection reports light pressure", failures);

    history.append(tickWithDeferredWork(5));
    result = full_engine::selectAdaptiveTerrainStreamingBudgetProfile(history);
    expect(result.profile == full_engine::TerrainStreamingBudgetProfile::CatchUp, "threshold pressure selects catch up", failures);
    expect(result.deferredWorkCount == 8, "adaptive selection sums recent pressure", failures);
}

void testAdaptiveWindowAndBudgets(std::vector<std::string>& failures)
{
    full_engine::TerrainStreamingTickHistory history;
    history.append(tickWithDeferredWork(20));
    history.append({});
    history.append({});

    full_engine::TerrainStreamingAdaptiveBudgetOptions options;
    options.recentTickCount = 2;
    full_engine::TerrainStreamingAdaptiveBudgetResult result =
        full_engine::selectAdaptiveTerrainStreamingBudgetProfile(history, options);
    expect(result.profile == full_engine::TerrainStreamingBudgetProfile::Conservative, "adaptive window ignores old pressure", failures);
    expect(result.inspectedTickCount == 2, "adaptive window inspects requested recent ticks", failures);
    expect(result.deferredWorkCount == 0, "adaptive window reports recent pressure only", failures);

    options.recentTickCount = 0;
    options.catchUpDeferredWorkThreshold = 0;
    history.append(tickWithDeferredWork(1));
    result = full_engine::selectAdaptiveTerrainStreamingBudgetProfile(history, options);
    expect(result.profile == full_engine::TerrainStreamingBudgetProfile::CatchUp, "zero threshold promotes any pressure", failures);

    const full_engine::TerrainStreamingLoopBudgetOptions budgets =
        full_engine::selectAdaptiveTerrainStreamingLoopBudgets(history, options);
    const full_engine::TerrainStreamingLoopBudgetOptions catchUp =
        full_engine::selectTerrainStreamingLoopBudgets(full_engine::TerrainStreamingBudgetProfile::CatchUp);
    expect(budgets.queue.maxSetupAdds == catchUp.queue.maxSetupAdds, "adaptive budgets use selected profile setup caps", failures);
    expect(budgets.maxPipelineCreates == catchUp.maxPipelineCreates, "adaptive budgets use selected profile lifecycle caps", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testProfileNames(failures);
    testUnlimitedProfile(failures);
    testNamedProfiles(failures);
    testUnknownProfileFallsBackToBalanced(failures);
    testAdaptiveProfileSelection(failures);
    testAdaptiveWindowAndBudgets(failures);

    if (!failures.empty())
    {
        for (const std::string& failure : failures)
        {
            std::cerr << failure << '\n';
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
