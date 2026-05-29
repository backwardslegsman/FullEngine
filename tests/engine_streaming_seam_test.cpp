#include "engine_bridge/StreamingSeam.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void expect(const bool condition, const char* message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

full_renderer::engine_bridge::FrameOriginDesc cameraRelativeOrigin() noexcept
{
    full_renderer::engine_bridge::FrameOriginDesc origin;
    origin.mode = full_renderer::engine_bridge::OriginMode::CameraRelative;
    origin.originWorld[0] = 1000000.0;
    origin.originWorld[1] = 0.0;
    origin.originWorld[2] = -1000000.0;
    origin.cameraWorld[0] = 1000032.0;
    origin.cameraWorld[1] = 8.0;
    origin.cameraWorld[2] = -999984.0;
    origin.warningDistanceMeters = 100000.0;
    return origin;
}

void mappingLifecycleIsDeterministic(int& failures)
{
    full_renderer::engine_bridge::EngineStreamingSeam seam;
    expect(seam.beginFrame(cameraRelativeOrigin()), "seam accepts valid camera-relative origin", failures);

    const full_renderer::TerrainChunkHandle first{7, 1};
    expect(seam.createOrUpdateTerrainChunk(42, first, false), "seam creates engine chunk mapping", failures);
    expect(seam.hasTerrainChunk(42), "created engine mapping is findable", failures);
    expect(seam.findTerrainChunk(42).id == first.id &&
            seam.findTerrainChunk(42).generation == first.generation,
        "created engine mapping returns renderer handle",
        failures);
    expect(seam.stats().mappingCount == 1 &&
            seam.stats().mappingsCreatedThisFrame == 1,
        "create mapping updates stats",
        failures);

    expect(seam.destroyTerrainChunk(42), "seam destroys engine chunk mapping", failures);
    expect(!seam.hasTerrainChunk(42), "destroyed engine mapping is removed", failures);
    expect(seam.stats().mappingCount == 0 &&
            seam.stats().mappingsDestroyedThisFrame == 1,
        "destroy mapping updates stats",
        failures);
    expect(!seam.destroyTerrainChunk(42), "destroying missing mapping is deterministic", failures);
    expect(seam.stats().staleMappingAttempts == 1,
        "destroying missing mapping increments stale mapping count",
        failures);

    const full_renderer::TerrainChunkHandle second{7, 2};
    expect(seam.createOrUpdateTerrainChunk(42, second, true), "seam recreates prior engine chunk mapping", failures);
    expect(seam.stats().mappingsReusedThisFrame == 1 &&
            seam.stats().materialFallbackMappingCount == 1,
        "recreated mapping counts reuse and material fallback",
        failures);
}

void originRebaseHandlesLargeCoordinates(int& failures)
{
    const full_renderer::engine_bridge::FrameOriginDesc origin = cameraRelativeOrigin();
    float cameraRelative[3] = {};
    expect(full_renderer::engine_bridge::rebasePosition(origin, origin.cameraWorld, cameraRelative),
        "camera position rebases",
        failures);
    expect(std::abs(cameraRelative[0] - 32.0f) < 0.001f &&
            std::abs(cameraRelative[1] - 8.0f) < 0.001f &&
            std::abs(cameraRelative[2] - 16.0f) < 0.001f,
        "camera-relative conversion subtracts origin",
        failures);

    const double minWorld[3] = {999990.0, -1.0, -1000010.0};
    const double maxWorld[3] = {1000010.0, 3.0, -999990.0};
    full_renderer::Aabb bounds;
    expect(full_renderer::engine_bridge::rebaseAabb(origin, minWorld, maxWorld, bounds),
        "large-coordinate AABB rebases",
        failures);
    expect(bounds.min[0] == -10.0f &&
            bounds.max[0] == 10.0f &&
            bounds.min[2] == -10.0f &&
            bounds.max[2] == 10.0f,
        "rebased AABB preserves extents around origin",
        failures);

    double badWorld[3] = {0.0, std::numeric_limits<double>::quiet_NaN(), 0.0};
    expect(!full_renderer::engine_bridge::rebasePosition(origin, badWorld, cameraRelative),
        "NaN world position is rejected",
        failures);
}

void seamChurnIsReproducibleAndBounded(int& failures)
{
    full_renderer::engine_bridge::EngineStreamingChurnOptions options;
    options.churn = full_renderer::debug::makeDefaultLongSessionChurnOptions();
    options.churn.frameCount = 180;
    options.origin = cameraRelativeOrigin();
    options.originShiftPeriod = 32;
    options.originStepMeters = 4096.0;

    const full_renderer::engine_bridge::EngineStreamingChurnSummary first =
        full_renderer::engine_bridge::runEngineStreamingSeamChurnSimulation(options);
    const full_renderer::engine_bridge::EngineStreamingChurnSummary second =
        full_renderer::engine_bridge::runEngineStreamingSeamChurnSimulation(options);

    expect(first.deterministicHash == second.deterministicHash,
        "engine streaming seam churn is deterministic for fixed seed",
        failures);
    expect(first.simulatedFrames == options.churn.frameCount,
        "engine streaming seam churn simulates requested frames",
        failures);
    expect(first.originShiftCount > 0,
        "engine streaming seam churn exercises repeated origin shifts",
        failures);
    expect(first.finalStats.mappingCount <= options.churn.maxResidentTerrainChunks,
        "engine streaming seam mapping count stays bounded",
        failures);
    expect(first.finalStats.totalMappingsCreated > 0 &&
            first.finalStats.totalMappingsDestroyed > 0,
        "engine streaming seam creates and destroys mappings",
        failures);
    expect(first.finalStats.staleMappingAttempts > 0,
        "engine streaming seam counts stale mapping attempts",
        failures);
    expect(first.churn.totalMaterialFallbacks > 0 &&
            first.finalStats.materialFallbackMappingCount <= first.finalStats.mappingCount,
        "engine streaming seam carries material fallback diagnostics",
        failures);
    expect(first.churn.finalPresentInvalidFrames == 0,
        "engine streaming seam churn preserves final present planning",
        failures);
    expect(first.resetFinalMappingCount == 0 &&
            first.resetFinalResidentCount == 0,
        "engine streaming seam reset drains mappings",
        failures);

    options.churn.seed ^= 0x1234U;
    const full_renderer::engine_bridge::EngineStreamingChurnSummary changed =
        full_renderer::engine_bridge::runEngineStreamingSeamChurnSimulation(options);
    expect(first.deterministicHash != changed.deterministicHash,
        "different seed changes engine seam churn hash",
        failures);
}

void absoluteLargeCoordinatesWarn(int& failures)
{
    full_renderer::engine_bridge::FrameOriginDesc origin = cameraRelativeOrigin();
    origin.mode = full_renderer::engine_bridge::OriginMode::AbsoluteRenderSpace;
    full_renderer::engine_bridge::EngineStreamingSeam seam;
    expect(seam.beginFrame(origin), "absolute origin descriptor is structurally valid", failures);
    expect(seam.stats().largeCoordinateWarningCount == 1,
        "absolute large coordinates produce precision-risk diagnostics",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    mappingLifecycleIsDeterministic(failures);
    originRebaseHandlesLargeCoordinates(failures);
    seamChurnIsReproducibleAndBounded(failures);
    absoluteLargeCoordinatesWarn(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
