#pragma once

#include "full_renderer/Renderer.hpp"
#include "renderer/debug/LongSessionChurn.hpp"

#include <cstdint>

namespace full_renderer::engine_bridge
{
/** @brief Engine-owned terrain chunk identifier used by the sample integration seam. */
using EngineChunkId = std::uint64_t;

/** @brief Coordinate policy used by the engine bridge before submitting renderer descriptors. */
enum class OriginMode
{
    /** @brief Submit coordinates as already-small renderer-space values. */
    AbsoluteRenderSpace,

    /** @brief Subtract `originWorld` from engine/world coordinates before renderer submission. */
    CameraRelative
};

/**
 * @brief Engine-owned frame origin state used to build renderer-relative descriptors.
 *
 * Values are expressed in meters, Y-up, right-handed engine world space. The
 * seam consumes this descriptor at a frame boundary, converts engine/world
 * positions to renderer-relative floats, and does not retain caller pointers.
 * It is deliberately outside renderer core; engines may replace it with their
 * own floating-origin policy.
 */
struct FrameOriginDesc
{
    /** @brief Origin conversion mode for this frame. */
    OriginMode mode = OriginMode::CameraRelative;

    /** @brief Engine/world-space origin in meters subtracted in camera-relative mode. */
    double originWorld[3] = {};

    /** @brief Engine/world-space camera position in meters for diagnostics. */
    double cameraWorld[3] = {};

    /** @brief Absolute or renderer-relative magnitude above which diagnostics warn. */
    double warningDistanceMeters = 100000.0;
};

/** @brief Backend-neutral diagnostics for the sample engine streaming seam. */
struct EngineStreamingSeamStats
{
    std::uint32_t engineResidentChunkCount = 0;
    std::uint32_t rendererLiveChunkMappingCount = 0;
    std::uint32_t mappingCount = 0;
    std::uint32_t mappingsCreatedThisFrame = 0;
    std::uint32_t mappingsDestroyedThisFrame = 0;
    std::uint32_t mappingsReusedThisFrame = 0;
    std::uint32_t totalMappingsCreated = 0;
    std::uint32_t totalMappingsDestroyed = 0;
    std::uint32_t totalMappingsReused = 0;
    std::uint32_t staleMappingAttempts = 0;
    std::uint32_t materialFallbackMappingCount = 0;
    std::uint32_t largeCoordinateWarningCount = 0;
    std::uint32_t nanOrInfRejectionCount = 0;
    OriginMode originMode = OriginMode::CameraRelative;
    double originWorld[3] = {};
    double cameraWorld[3] = {};
    float cameraRendererRelative[3] = {};
};

/**
 * @brief Thin sample/engine adapter that maps engine chunk IDs to renderer handles.
 *
 * This class demonstrates the ownership boundary for a future engine: the
 * engine owns chunk IDs, residency decisions, and origin policy; the renderer
 * owns opaque handles and backend resources. The seam stores only public
 * renderer handles and never exposes backend objects.
 */
class EngineStreamingSeam
{
public:
    /** @brief Starts a seam frame and records origin diagnostics. */
    bool beginFrame(const FrameOriginDesc& origin) noexcept;

    /** @brief Creates or updates the mapping for an engine-owned terrain chunk. */
    bool createOrUpdateTerrainChunk(
        EngineChunkId id,
        TerrainChunkHandle rendererHandle,
        bool materialFallbackActive) noexcept;

    /** @brief Removes one engine-owned terrain chunk mapping, if present. */
    bool destroyTerrainChunk(EngineChunkId id) noexcept;

    /** @brief Returns the renderer handle mapped to an engine chunk ID, or an invalid handle. */
    TerrainChunkHandle findTerrainChunk(EngineChunkId id) const noexcept;

    /** @brief Returns whether a mapping exists for the engine chunk ID. */
    bool hasTerrainChunk(EngineChunkId id) const noexcept;

    /** @brief Clears frame-local counters while preserving live mappings. */
    void resetFrameCounters() noexcept;

    /** @brief Clears all mappings and counters for deterministic validation reset. */
    void reset() noexcept;

    /** @brief Returns backend-neutral diagnostics for the seam. */
    const EngineStreamingSeamStats& stats() const noexcept { return stats_; }

private:
    struct Mapping
    {
        EngineChunkId id = 0;
        TerrainChunkHandle rendererHandle;
        bool materialFallbackActive = false;
    };

    Mapping* findMutable(EngineChunkId id) noexcept;
    const Mapping* find(EngineChunkId id) const noexcept;
    bool wasDestroyed(EngineChunkId id) const noexcept;
    void rememberDestroyed(EngineChunkId id);
    void refreshCounts() noexcept;

    static constexpr std::uint32_t kMaxMappings = 4096;
    static constexpr std::uint32_t kMaxDestroyedHistory = 4096;

    Mapping mappings_[kMaxMappings] = {};
    std::uint32_t mappingCount_ = 0;
    EngineChunkId destroyedHistory_[kMaxDestroyedHistory] = {};
    std::uint32_t destroyedHistoryCount_ = 0;
    EngineStreamingSeamStats stats_ = {};
};

/** @brief Options for deterministic engine-seam churn validation. */
struct EngineStreamingChurnOptions
{
    debug::LongSessionChurnOptions churn;
    FrameOriginDesc origin;
    bool shiftOriginAtInterval = true;
    std::uint32_t originShiftPeriod = 64;
    double originStepMeters = 2048.0;
};

/** @brief Summary produced by deterministic engine-seam churn validation. */
struct EngineStreamingChurnSummary
{
    debug::LongSessionChurnSummary churn;
    EngineStreamingSeamStats finalStats;
    std::uint32_t simulatedFrames = 0;
    std::uint32_t originShiftCount = 0;
    std::uint32_t resetFinalMappingCount = 0;
    std::uint32_t resetFinalResidentCount = 0;
    std::uint64_t deterministicHash = 0;
};

/** @brief Validates a frame origin descriptor. */
bool isValidFrameOriginDesc(const FrameOriginDesc& origin) noexcept;

/** @brief Converts an engine/world-space position to renderer-relative floats. */
bool rebasePosition(
    const FrameOriginDesc& origin,
    const double worldPosition[3],
    float outRendererPosition[3]) noexcept;

/** @brief Converts engine/world-space AABB corners to a renderer-relative AABB. */
bool rebaseAabb(
    const FrameOriginDesc& origin,
    const double worldMin[3],
    const double worldMax[3],
    Aabb& outRendererBounds) noexcept;

/** @brief Runs deterministic engine-owned streaming seam validation without GPU access. */
EngineStreamingChurnSummary runEngineStreamingSeamChurnSimulation(
    const EngineStreamingChurnOptions& options);
} // namespace full_renderer::engine_bridge
