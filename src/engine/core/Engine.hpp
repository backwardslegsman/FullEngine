#pragma once

#include <cstdint>

namespace full_engine
{
/** @brief Result code for engine lifecycle and update operations. */
enum class EngineResult
{
    Success,
    AlreadyInitialized,
    NotInitialized,
    InvalidArgument,
};

/**
 * @brief Engine-level configuration owned by the future runtime.
 *
 * Values are copied during `Engine::initialize`. Time values are expressed in
 * seconds. The first engine slice uses only a fixed-step target; scheduling and
 * accumulated simulation policy remain engine-owned future work.
 */
struct EngineConfig
{
    double fixedDeltaSeconds = 1.0 / 60.0;
};

/**
 * @brief CPU-side diagnostics for deterministic engine lifecycle tests.
 *
 * Counters are process-local engine state only. They do not include renderer
 * statistics, GPU work, or sample application state.
 */
struct EngineDiagnostics
{
    std::uint64_t initializeCount = 0;
    std::uint64_t shutdownCount = 0;
    std::uint64_t tickCount = 0;
    std::uint64_t rejectedTickCount = 0;
    double accumulatedSimulationSeconds = 0.0;
};

/**
 * @brief Minimal engine runtime spine for lifecycle and tick validation.
 *
 * This type deliberately owns no renderer, world, ECS, physics, editor, or
 * asset pipeline state. It establishes deterministic engine lifecycle behavior
 * before larger runtime systems are added.
 */
class Engine
{
public:
    /** @brief Initializes the engine by copying a validated config. */
    EngineResult initialize(const EngineConfig& config = EngineConfig{});

    /** @brief Shuts down the engine if it is currently initialized. */
    EngineResult shutdown() noexcept;

    /** @brief Advances engine-owned simulation time by `deltaSeconds`. */
    EngineResult tick(double deltaSeconds) noexcept;

    /** @brief Returns whether the engine is currently initialized. */
    bool isInitialized() const noexcept;

    /** @brief Returns the copied engine config. Valid after successful initialize. */
    const EngineConfig& config() const noexcept;

    /** @brief Returns current lifecycle and tick diagnostics. */
    const EngineDiagnostics& diagnostics() const noexcept;

private:
    bool initialized_ = false;
    EngineConfig config_ = {};
    EngineDiagnostics diagnostics_ = {};
};
} // namespace full_engine
