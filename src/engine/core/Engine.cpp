#include "engine/core/Engine.hpp"

#include <cmath>

namespace full_engine
{
namespace
{
bool isPositiveFinite(const double value) noexcept
{
    return std::isfinite(value) && value > 0.0;
}
} // namespace

EngineResult Engine::initialize(const EngineConfig& config)
{
    if (initialized_)
    {
        return EngineResult::AlreadyInitialized;
    }

    if (!isPositiveFinite(config.fixedDeltaSeconds))
    {
        return EngineResult::InvalidArgument;
    }

    config_ = config;
    initialized_ = true;
    ++diagnostics_.initializeCount;
    return EngineResult::Success;
}

EngineResult Engine::shutdown() noexcept
{
    if (!initialized_)
    {
        return EngineResult::NotInitialized;
    }

    initialized_ = false;
    ++diagnostics_.shutdownCount;
    return EngineResult::Success;
}

EngineResult Engine::tick(const double deltaSeconds) noexcept
{
    if (!initialized_)
    {
        ++diagnostics_.rejectedTickCount;
        return EngineResult::NotInitialized;
    }

    if (!isPositiveFinite(deltaSeconds))
    {
        ++diagnostics_.rejectedTickCount;
        return EngineResult::InvalidArgument;
    }

    ++diagnostics_.tickCount;
    diagnostics_.accumulatedSimulationSeconds += deltaSeconds;
    return EngineResult::Success;
}

bool Engine::isInitialized() const noexcept
{
    return initialized_;
}

const EngineConfig& Engine::config() const noexcept
{
    return config_;
}

const EngineDiagnostics& Engine::diagnostics() const noexcept
{
    return diagnostics_;
}
} // namespace full_engine
