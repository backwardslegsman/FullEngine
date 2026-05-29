#include "renderer/debug/LongSessionChurn.hpp"

#include <cstdlib>
#include <iostream>

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

void defaultOptionsAreValid(int& failures)
{
    const full_renderer::debug::LongSessionChurnOptions options =
        full_renderer::debug::makeDefaultLongSessionChurnOptions();
    expect(full_renderer::debug::isValidLongSessionChurnOptions(options),
        "default long-session churn options validate",
        failures);

    full_renderer::debug::LongSessionChurnOptions invalid = options;
    invalid.frameCount = 0;
    expect(!full_renderer::debug::isValidLongSessionChurnOptions(invalid),
        "zero-frame churn options are rejected",
        failures);

    invalid = options;
    invalid.maxResidentTerrainChunks = options.terrainSlotCount + 1U;
    expect(!full_renderer::debug::isValidLongSessionChurnOptions(invalid),
        "resident chunk cap cannot exceed terrain slots",
        failures);

    invalid = options;
    invalid.resizePeriod = 0;
    expect(!full_renderer::debug::isValidLongSessionChurnOptions(invalid),
        "zero resize period is rejected",
        failures);
}

void fixedSeedIsReproducible(int& failures)
{
    const full_renderer::debug::LongSessionChurnOptions options =
        full_renderer::debug::makeDefaultLongSessionChurnOptions();
    const full_renderer::debug::LongSessionChurnSummary first =
        full_renderer::debug::runLongSessionChurnSimulation(options);
    const full_renderer::debug::LongSessionChurnSummary second =
        full_renderer::debug::runLongSessionChurnSimulation(options);

    expect(first.deterministicHash == second.deterministicHash,
        "fixed-seed churn hash is deterministic",
        failures);
    expect(first.totalTerrainChunksCreated == second.totalTerrainChunksCreated,
        "fixed-seed terrain creation count is deterministic",
        failures);
    expect(first.totalStagedBytes == second.totalStagedBytes,
        "fixed-seed staged byte estimate is deterministic",
        failures);

    full_renderer::debug::LongSessionChurnOptions changed = options;
    changed.seed ^= 0x01010101U;
    const full_renderer::debug::LongSessionChurnSummary third =
        full_renderer::debug::runLongSessionChurnSimulation(changed);
    expect(first.deterministicHash != third.deterministicHash,
        "different seed changes churn hash",
        failures);
}

void fastChurnCoversResidencyAndFallbacks(int& failures)
{
    full_renderer::debug::LongSessionChurnOptions options =
        full_renderer::debug::makeDefaultLongSessionChurnOptions();
    options.frameCount = 100;
    const full_renderer::debug::LongSessionChurnSummary summary =
        full_renderer::debug::runLongSessionChurnSimulation(options);

    expect(summary.simulatedFrames == options.frameCount,
        "fast churn simulates requested frames",
        failures);
    expect(summary.peakResidentTerrainChunks <= options.maxResidentTerrainChunks,
        "resident chunk count respects configured cap",
        failures);
    expect(summary.totalTerrainChunksCreated > 0,
        "fast churn creates terrain chunks",
        failures);
    expect(summary.totalTerrainChunksDestroyed > 0,
        "fast churn destroys terrain chunks",
        failures);
    expect(summary.totalTerrainChunkSlotsReused > 0,
        "fast churn reuses terrain slots",
        failures);
    expect(summary.totalStaleChunkHandleAttempts > 0,
        "fast churn exercises stale handle attempts",
        failures);
    expect(summary.totalMaterialFallbacks > 0 &&
            summary.totalTextureFallbacks > 0 &&
            summary.totalLodFallbacks > 0,
        "fast churn exercises material, texture, and LOD fallbacks",
        failures);
    expect(summary.totalDecalsSubmitted > 0 && summary.totalParticleBatchesSubmitted > 0,
        "fast churn exercises decal and particle submissions",
        failures);
    expect(summary.totalSkinnedPaletteRejections > 0,
        "fast churn exercises skinned palette rejection diagnostics",
        failures);
    expect(summary.totalOptionalPassToggles > 0,
        "fast churn exercises optional pass toggles",
        failures);
    expect(summary.totalResizeEvents > 0 &&
            summary.totalSceneTargetRecreates > 0 &&
            summary.totalPostTargetRecreates > 0,
        "fast churn exercises resize and post-target recreation counters",
        failures);
    expect(summary.finalPresentInvalidFrames == 0,
        "fast churn keeps final present valid",
        failures);
    expect(summary.frameDataLifetimeChecks == options.frameCount * 8U &&
            summary.frameDataLifetimeFailures == 0,
        "fast churn performs frame-data lifetime checks without failures",
        failures);
    expect(summary.finalTrackedLiveResourcesAfterReset == 0 &&
            summary.finalResidentTerrainChunksAfterReset == 0,
        "fast churn reset drains tracked live resources",
        failures);
    expect(summary.unboundedGrowthWarnings == 0,
        "fast churn does not report unbounded growth",
        failures);
}

void oneThousandFrameChurnStaysBounded(int& failures)
{
    full_renderer::debug::LongSessionChurnOptions options =
        full_renderer::debug::makeDefaultLongSessionChurnOptions();
    options.frameCount = 1000;
    const full_renderer::debug::LongSessionChurnSummary summary =
        full_renderer::debug::runLongSessionChurnSimulation(options);

    expect(summary.simulatedFrames == 1000,
        "1000-frame churn simulates requested frames",
        failures);
    expect(summary.peakResidentTerrainChunks <= options.maxResidentTerrainChunks,
        "1000-frame churn keeps resident terrain bounded",
        failures);
    expect(summary.peakTrackedLiveResources <=
            options.maxResidentTerrainChunks + options.frameCount,
        "1000-frame tracked resources remain bounded by the simulator policy",
        failures);
    expect(summary.finalPresentInvalidFrames == 0,
        "1000-frame churn keeps final present valid",
        failures);
    expect(summary.frameDataLifetimeFailures == 0,
        "1000-frame churn has no frame-data lifetime failures",
        failures);
}

void disabledChurnModesStayQuiet(int& failures)
{
    full_renderer::debug::LongSessionChurnOptions options =
        full_renderer::debug::makeDefaultLongSessionChurnOptions();
    options.materialFallbackChurnEnabled = false;
    options.decalParticleChurnEnabled = false;
    options.skinnedPaletteChurnEnabled = false;
    options.optionalPassToggleChurnEnabled = false;
    options.resizeChurnEnabled = false;

    const full_renderer::debug::LongSessionChurnSummary summary =
        full_renderer::debug::runLongSessionChurnSimulation(options);

    expect(summary.totalMaterialFallbacks == 0 &&
            summary.totalTextureFallbacks == 0 &&
            summary.totalLodFallbacks == 0,
        "disabled material fallback churn stays quiet",
        failures);
    expect(summary.totalDecalsSubmitted == 0 &&
            summary.totalParticleBatchesSubmitted == 0,
        "disabled decal/particle churn stays quiet",
        failures);
    expect(summary.totalSkinnedPaletteSubmissions == 0,
        "disabled skinned palette churn stays quiet",
        failures);
    expect(summary.totalOptionalPassToggles == 0,
        "disabled optional pass churn stays quiet",
        failures);
    expect(summary.totalResizeEvents == 0 &&
            summary.totalSceneTargetRecreates == 0 &&
            summary.totalPostTargetRecreates == 0,
        "disabled resize churn stays quiet",
        failures);
    expect(summary.finalPresentInvalidFrames == 0,
        "disabled churn modes still keep final present valid",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    defaultOptionsAreValid(failures);
    fixedSeedIsReproducible(failures);
    fastChurnCoversResidencyAndFallbacks(failures);
    oneThousandFrameChurnStaysBounded(failures);
    disabledChurnModesStayQuiet(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
