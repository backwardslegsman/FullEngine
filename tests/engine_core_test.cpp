#include "engine/core/Engine.hpp"
#include "engine/renderer_integration/RendererHost.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

void expectResult(
    const full_engine::EngineResult actual,
    const full_engine::EngineResult expected,
    const char* message,
    std::vector<std::string>& failures)
{
    expect(actual == expected, message, failures);
}

void testLifecycle(std::vector<std::string>& failures)
{
    full_engine::Engine engine;

    expect(!engine.isInitialized(), "default engine starts uninitialized", failures);
    expect(engine.diagnostics().initializeCount == 0, "default initialize count is zero", failures);

    full_engine::EngineConfig config;
    config.fixedDeltaSeconds = 1.0 / 30.0;
    expectResult(engine.initialize(config), full_engine::EngineResult::Success, "valid initialize succeeds", failures);
    expect(engine.isInitialized(), "engine reports initialized after initialize", failures);
    expect(engine.config().fixedDeltaSeconds == config.fixedDeltaSeconds, "initialize copies config", failures);
    expect(engine.diagnostics().initializeCount == 1, "valid initialize increments diagnostics", failures);

    expectResult(
        engine.initialize(config),
        full_engine::EngineResult::AlreadyInitialized,
        "repeated initialize reports already initialized",
        failures);
    expect(engine.diagnostics().initializeCount == 1, "repeated initialize does not increment diagnostics", failures);

    expectResult(engine.shutdown(), full_engine::EngineResult::Success, "valid shutdown succeeds", failures);
    expect(!engine.isInitialized(), "engine reports uninitialized after shutdown", failures);
    expect(engine.diagnostics().shutdownCount == 1, "valid shutdown increments diagnostics", failures);

    expectResult(
        engine.shutdown(),
        full_engine::EngineResult::NotInitialized,
        "shutdown before initialize reports not initialized",
        failures);
    expect(engine.diagnostics().shutdownCount == 1, "rejected shutdown does not increment diagnostics", failures);
}

void testTick(std::vector<std::string>& failures)
{
    full_engine::Engine engine;

    expectResult(
        engine.tick(1.0 / 60.0),
        full_engine::EngineResult::NotInitialized,
        "tick before initialize reports not initialized",
        failures);
    expect(engine.diagnostics().rejectedTickCount == 1, "tick before initialize is counted as rejected", failures);

    expectResult(engine.initialize(), full_engine::EngineResult::Success, "default initialize succeeds", failures);
    expectResult(engine.tick(0.25), full_engine::EngineResult::Success, "positive tick succeeds", failures);
    expect(engine.diagnostics().tickCount == 1, "valid tick increments tick count", failures);
    expect(
        std::fabs(engine.diagnostics().accumulatedSimulationSeconds - 0.25) < 0.000001,
        "valid tick accumulates simulation seconds",
        failures);

    const double invalidDeltas[] = {
        0.0,
        -0.1,
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(),
    };

    for (const double delta : invalidDeltas)
    {
        expectResult(
            engine.tick(delta),
            full_engine::EngineResult::InvalidArgument,
            "invalid tick delta reports invalid argument",
            failures);
    }

    expect(engine.diagnostics().tickCount == 1, "invalid ticks do not increment tick count", failures);
    expect(engine.diagnostics().rejectedTickCount == 5, "invalid ticks increment rejected count", failures);
    expect(
        std::fabs(engine.diagnostics().accumulatedSimulationSeconds - 0.25) < 0.000001,
        "invalid ticks do not accumulate simulation seconds",
        failures);
}

void testInvalidConfig(std::vector<std::string>& failures)
{
    const double invalidFixedDeltas[] = {
        0.0,
        -1.0 / 60.0,
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(),
    };

    for (const double fixedDelta : invalidFixedDeltas)
    {
        full_engine::Engine engine;
        full_engine::EngineConfig config;
        config.fixedDeltaSeconds = fixedDelta;

        expectResult(
            engine.initialize(config),
            full_engine::EngineResult::InvalidArgument,
            "invalid fixed delta reports invalid argument",
            failures);
        expect(!engine.isInitialized(), "invalid config leaves engine uninitialized", failures);
        expect(engine.diagnostics().initializeCount == 0, "invalid config does not increment initialize count", failures);
    }
}

void testRendererHost(std::vector<std::string>& failures)
{
    const full_engine::RendererHost host;
    expect(host.rendererApiLinked(), "renderer host smoke-check links public renderer API", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testLifecycle(failures);
    testTick(failures);
    testInvalidConfig(failures);
    testRendererHost(failures);

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
