#pragma once

#include "full_renderer/Handles.hpp"

#include <cstdint>

namespace full_renderer
{
/** @brief Maximum terrain LOD levels supported by the Phase 2 terrain API. */
constexpr std::uint32_t kMaxTerrainLodLevels = 4;

/** @brief Maximum terrain shadow cascade diagnostics tracked by terrain stats. */
constexpr std::uint32_t kMaxTerrainShadowCascades = 4;

/** @brief Sentinel used when no terrain LOD is selected for a chunk or batch. */
constexpr std::uint32_t kInvalidTerrainLodIndex = UINT32_MAX;

/** @brief Sentinel used when a terrain debug record is not associated with a shadow cascade. */
constexpr std::uint32_t kInvalidShadowCascadeIndex = UINT32_MAX;

/**
 * @brief Opaque handle for a renderer-owned terrain chunk record.
 *
 * Terrain chunks are CPU-side renderer records that reference existing
 * renderer-owned mesh and material handles. Zero values are invalid. The
 * generation value lets the renderer reject stale handles after a chunk slot is
 * destroyed and later reused.
 */
struct TerrainChunkHandle
{
    /** @brief Renderer-assigned chunk slot identifier; zero is invalid. */
    std::uint32_t id = 0;

    /** @brief Renderer-assigned generation used to detect stale handles. */
    std::uint32_t generation = 0;
};

/**
 * @brief Returns whether a terrain chunk handle is not the public invalid sentinel.
 *
 * This does not prove the handle is live or belongs to a particular renderer.
 * Stale-handle checks happen inside renderer methods that consume the handle.
 */
constexpr bool isValid(const TerrainChunkHandle handle) noexcept
{
    return handle.id != 0 && handle.generation != 0;
}

/**
 * @brief World-space axis-aligned bounding box.
 *
 * Bounds use renderer world conventions: meters, Y-up, and right-handed
 * coordinates. A valid box has finite values and `min <= max` on every axis.
 */
struct Aabb
{
    /** @brief Minimum world-space corner in meters. */
    float min[3] = {};

    /** @brief Maximum world-space corner in meters. */
    float max[3] = {};
};

/**
 * @brief One externally generated terrain LOD option for a chunk.
 *
 * Terrain chunks reference existing renderer-owned mesh and material resources.
 * The terrain system does not load assets or generate height data. Terrain LOD
 * meshes are interpreted as chunk-local geometry and are positioned from the
 * chunk bounds during terrain submission. A common integration pattern is to
 * upload one shared grid mesh per LOD resolution and reference those same mesh
 * handles from many chunks. LOD entries must be sorted from highest detail to
 * lowest detail by increasing `maxDistanceMeters`.
 *
 * LOD index 0 is the highest-detail level. Higher indices represent lower
 * detail. Selection is deterministic: the renderer chooses the first LOD whose
 * inclusive threshold contains the camera-to-bounds distance, and chooses the
 * last LOD when the distance exceeds every threshold. Hysteresis is not applied
 * in this milestone.
 *
 * Terrain-splat materials are selected through `material` like any other
 * material. The chunk's optional splat map controls layer weights for all LODs.
 */
struct TerrainLodDesc
{
    /** @brief Mesh rendered when this LOD is selected. Must be a valid live handle. */
    MeshHandle mesh;

    /** @brief Material rendered when this LOD is selected. Must be a valid live handle. */
    MaterialHandle material;

    /**
     * @brief Inclusive maximum camera distance in meters for this LOD.
     *
     * Must be finite and non-negative. If the camera is farther than the last
     * LOD threshold, the last LOD is selected.
     */
    float maxDistanceMeters = 0.0f;
};

/**
 * @brief Descriptor for creating a renderer-owned terrain chunk record.
 *
 * Source LOD descriptors are copied during `createTerrainChunk`. The caller may
 * release or reuse descriptor memory after that call returns. Mesh and material
 * resources referenced by LODs remain owned separately and must outlive chunks
 * that reference them. `lodCount` must be in the inclusive range
 * `[1, kMaxTerrainLodLevels]`.
 *
 * Large-world policy: bounds are currently single-precision world-space
 * meters. Engines targeting very large worlds should submit camera-relative
 * or floating-origin-shifted coordinates to the renderer until an explicit
 * origin descriptor is added. The renderer treats the submitted coordinates as
 * already rebased for the current frame and does not own world streaming or
 * origin-shift policy.
 */
struct TerrainChunkDesc
{
    /** @brief World-space chunk bounds used for culling and distance LOD. */
    Aabb bounds;

    /** @brief Pointer to `lodCount` sorted LOD descriptors; must not be null. */
    const TerrainLodDesc* lods = nullptr;

    /** @brief Number of LOD descriptors in `lods`; must be 1..kMaxTerrainLodLevels. */
    std::uint32_t lodCount = 0;

    /**
     * @brief Optional RGBA splat map used by terrain-splat materials.
     *
     * The handle is copied during `createTerrainChunk` and is not owned by the
     * chunk. A zero handle is valid and requests deterministic fallback weights:
     * full weight on terrain layer 0 and zero weight on the other layers.
     */
    TextureHandle splatMap;
};

/**
 * @brief Debug capture flags for per-frame terrain submission.
 *
 * Debug capture stores CPU-visible terrain decisions for later inspection
 * through `copyTerrainDebugInfo` and `copyTerrainBatchDebugInfo`. GPU overlay
 * flags request renderer-owned debug visualization for the submitted terrain
 * packet. The options are copied during `IRenderer::submit` and are valid only
 * for that frame.
 *
 * @note Thread safety: Populate this descriptor on the renderer owner thread.
 * @note GPU overlays are diagnostic-only and disabled by default. They do not
 * change terrain culling, LOD selection, resource ownership, or normal terrain
 * rendering.
 */
struct TerrainDebugOptions
{
    /** @brief Capture one debug record per submitted terrain chunk when true. */
    bool captureChunkInfo = false;

    /**
     * @brief Capture one debug record per generated terrain instance batch.
     *
     * Batch records are produced after culling and LOD selection, so each record
     * describes one backend-facing terrain batch and its selected LOD. This
     * option does not enable GPU debug rendering.
     */
    bool captureBatchInfo = false;

    /**
     * @brief Draw terrain chunk AABB wireframes for the current submission.
     *
     * Bounds are world-space meters and match the boxes used by frustum culling
     * and distance LOD. This option visualizes submitted chunk records captured
     * for the frame; invalid handles do not produce boxes because they have no
     * stable bounds.
     */
    bool drawChunkBounds = false;

    /**
     * @brief Draw visible terrain chunk bounds colored by selected LOD.
     *
     * The overlay uses the selected LOD recorded by the terrain submission path;
     * it does not recompute distance thresholds. Culled chunks are shown only
     * when combined diagnostics are enabled.
     */
    bool drawLodOverlay = false;

    /**
     * @brief Draw terrain chunk bounds colored by material validity.
     *
     * Valid material references are shown distinctly from fallback or missing
     * material state. The current renderer rejects invalid LOD material handles
     * at chunk creation, so this mode is primarily a diagnostic extension point.
     */
    bool drawMaterialOverlay = false;

    /**
     * @brief Draw terrain chunk bounds colored by splat-map fallback state.
     *
     * Chunks without a valid splat map are highlighted because they use the
     * documented layer-0 fallback weights. Chunks with a valid splat map use a
     * neutral diagnostic color.
     */
    bool drawSplatFallbackOverlay = false;

    /**
     * @brief Draw one combined terrain diagnostic overlay.
     *
     * Combined mode prioritizes splat fallback and missing material state, then
     * shows culled chunks and visible LOD colors. It is intended as the default
     * high-signal debug view for the sample app.
     */
    bool drawCombinedOverlay = false;
};

/**
 * @brief Per-frame terrain submission descriptor.
 *
 * The renderer reads this descriptor only during `submit`. Chunk handles must
 * refer to live chunks created by the same renderer. The camera position is
 * world-space meters and is used only for distance-based LOD selection.
 */
struct TerrainSubmitDesc
{
    /** @brief Pointer to `chunkCount` terrain chunk handles, or null when count is zero. */
    const TerrainChunkHandle* chunks = nullptr;

    /** @brief Number of terrain chunk handles available through `chunks`. */
    std::uint32_t chunkCount = 0;

    /** @brief Camera world position in meters used for LOD selection. */
    float cameraPositionWorld[3] = {};

    /** @brief Debug capture settings for this terrain submission. */
    TerrainDebugOptions debug;
};

/** @brief Reason a terrain chunk did or did not produce a draw item. */
enum class TerrainCullResult
{
    /** @brief The chunk passed culling and selected a LOD. */
    Visible,

    /** @brief The chunk bounds were outside the camera frustum. */
    OutsideFrustum,

    /** @brief The submitted chunk handle was invalid or stale. */
    InvalidHandle
};

/**
 * @brief CPU-visible debug record for a submitted terrain chunk.
 *
 * Records are produced only when terrain debug capture is enabled for a frame.
 * They are snapshots and remain valid until the next terrain submission or
 * renderer shutdown.
 */
struct TerrainChunkDebugInfo
{
    /** @brief Submitted terrain chunk handle. */
    TerrainChunkHandle handle;

    /** @brief World-space bounds used for culling; empty for invalid handles. */
    Aabb bounds;

    /** @brief Visibility/culling decision for this chunk. */
    TerrainCullResult cullResult = TerrainCullResult::InvalidHandle;

    /** @brief Selected LOD index when visible; `kInvalidTerrainLodIndex` when culled. */
    std::uint32_t selectedLod = kInvalidTerrainLodIndex;

    /** @brief Camera-to-bounds distance in meters used for LOD selection. */
    float distanceMeters = 0.0f;

    /** @brief True when the chunk descriptor referenced a non-zero splat map handle. */
    bool hasSplatMap = false;

    /** @brief True when a visible chunk selected a valid material handle for rendering. */
    bool hasTerrainMaterial = false;

    /**
     * @brief True when this chunk was inside the camera frustum for this debug record.
     *
     * Camera-visible terrain debug records set this for visible chunks. Shadow
     * caster debug records use it to distinguish chunks that are also visible in
     * the main terrain pass from off-camera-only shadow casters.
     */
    bool cameraVisible = false;

    /**
     * @brief True when this record came from directional shadow caster selection.
     *
     * Shadow caster selection uses the light/shadow frustum and may include
     * resident chunks that were outside the camera frustum. Normal terrain
     * camera-culling records leave this false.
     */
    bool selectedAsShadowCaster = false;

    /**
     * @brief Shadow cascade index for caster debug records.
     *
     * Records produced by per-cascade shadow caster diagnostics set this to the
     * cascade that selected or rejected the chunk. Normal camera-culling records
     * use `kInvalidShadowCascadeIndex`.
     */
    std::uint32_t shadowCascadeIndex = kInvalidShadowCascadeIndex;
};

/**
 * @brief CPU-visible debug record for one generated terrain instance batch.
 *
 * Batch records are snapshots from the latest terrain submission with
 * `TerrainDebugOptions::captureBatchInfo` enabled. They expose renderer-owned
 * mesh/material handles and the selected terrain LOD without exposing backend
 * API objects.
 */
struct TerrainBatchDebugInfo
{
    /** @brief Mesh shared by all terrain instances in this batch. */
    MeshHandle mesh;

    /** @brief Material shared by all terrain instances in this batch. */
    MaterialHandle material;

    /** @brief Selected terrain LOD shared by this batch. */
    std::uint32_t selectedLod = kInvalidTerrainLodIndex;

    /** @brief Number of terrain chunk instances contained in this batch. */
    std::uint32_t instanceCount = 0;

    /** @brief World-space bounds enclosing all instances in this batch. */
    Aabb bounds;

    /** @brief True when this batch uses a submitted splat map instead of fallback weights. */
    bool hasSplatMap = false;

    /** @brief True when this batch references a valid selected material handle. */
    bool hasTerrainMaterial = false;
};

/**
 * @brief Lightweight terrain counters for the most recent terrain submission.
 *
 * These counters are CPU-side diagnostics and coarse integration checks. They
 * are not GPU timing data.
 */
struct TerrainStats
{
    /** @brief Number of live renderer-owned terrain chunk records. */
    std::uint32_t liveChunks = 0;

    /** @brief Alias for live terrain chunk records currently resident in the renderer. */
    std::uint32_t residentChunks = 0;

    /** @brief Allocated terrain chunk slots, including inactive slots retained for reuse. */
    std::uint32_t allocatedChunkSlots = 0;

    /** @brief Allocated chunk slots that are inactive and available for reuse. */
    std::uint32_t nonResidentChunks = 0;

    /** @brief Terrain chunks created since the previous terrain submission consumed churn stats. */
    std::uint32_t chunksCreatedSinceLastSubmit = 0;

    /** @brief Terrain chunks destroyed since the previous terrain submission consumed churn stats. */
    std::uint32_t chunksDestroyedSinceLastSubmit = 0;

    /** @brief Terrain chunk records updated since the previous terrain submission consumed churn stats. */
    std::uint32_t chunksUpdatedSinceLastSubmit = 0;

    /** @brief Inactive terrain chunk slots reused since the previous terrain submission consumed churn stats. */
    std::uint32_t chunkSlotsReusedSinceLastSubmit = 0;

    /** @brief Lifetime count of successful terrain chunk creations. */
    std::uint32_t totalChunkCreateCount = 0;

    /** @brief Lifetime count of successful terrain chunk destructions. */
    std::uint32_t totalChunkDestroyCount = 0;

    /** @brief Lifetime count of successful terrain chunk descriptor updates. */
    std::uint32_t totalChunkUpdateCount = 0;

    /** @brief Lifetime count of inactive terrain chunk slots reused for new chunks. */
    std::uint32_t totalChunkSlotReuseCount = 0;

    /** @brief Number of chunk handles submitted in the most recent terrain packet. */
    std::uint32_t submittedChunks = 0;

    /** @brief Number of submitted chunks that passed frustum culling. */
    std::uint32_t visibleChunks = 0;

    /** @brief Number of submitted chunks culled or rejected before drawing. */
    std::uint32_t culledChunks = 0;

    /** @brief Submitted chunks rejected because their handle was the public invalid sentinel. */
    std::uint32_t invalidHandleChunks = 0;

    /** @brief Submitted chunks rejected because their non-zero handle did not resolve to a live record. */
    std::uint32_t staleHandleChunks = 0;

    /** @brief Submitted chunks rejected because no valid LOD could be selected. */
    std::uint32_t lodFallbackChunks = 0;

    /** @brief Number of terrain backend draws or instanced batches generated. */
    std::uint32_t terrainDraws = 0;

    /** @brief Visible terrain chunk counts by selected LOD index 0..3. */
    std::uint32_t visibleChunksByLod[kMaxTerrainLodLevels] = {};

    /** @brief Generated terrain instance batch counts by selected LOD index 0..3. */
    std::uint32_t terrainBatchesByLod[kMaxTerrainLodLevels] = {};

    /** @brief Visible terrain chunks using fallback layer-0 splat weights. */
    std::uint32_t splatMapFallbackChunks = 0;

    /** @brief Visible terrain chunks using a non-zero submitted splat map handle. */
    std::uint32_t splatMapResidentChunks = 0;

    /** @brief Resident terrain chunks selected by the latest directional shadow light frustum. */
    std::uint32_t shadowCasterChunks = 0;

    /** @brief Shadow caster chunks that were outside the camera frustum for the same frame. */
    std::uint32_t offCameraShadowCasterChunks = 0;

    /** @brief Submitted resident chunks rejected by the latest light-frustum caster selection. */
    std::uint32_t shadowRejectedChunks = 0;

    /** @brief Submitted chunks rejected by shadow caster selection because handles or resources were invalid. */
    std::uint32_t shadowInvalidResourceChunks = 0;

    /** @brief Shadow caster chunk counts by selected shadow LOD index 0..3. */
    std::uint32_t shadowCasterChunksByLod[kMaxTerrainLodLevels] = {};

    /** @brief Generated shadow caster instance batch counts by selected shadow LOD index 0..3. */
    std::uint32_t shadowCasterBatchesByLod[kMaxTerrainLodLevels] = {};

    /** @brief Number of CPU-side shadow cascades evaluated by the latest terrain submission. */
    std::uint32_t shadowCascadeCount = 0;

    /** @brief Per-cascade terrain caster counts for active rendered cascades. */
    std::uint32_t shadowCascadeCasterChunks[kMaxTerrainShadowCascades] = {};

    /** @brief Per-cascade off-camera terrain caster counts. */
    std::uint32_t shadowCascadeOffCameraCasterChunks[kMaxTerrainShadowCascades] = {};

    /** @brief Per-cascade terrain chunks rejected by that cascade's light frustum. */
    std::uint32_t shadowCascadeRejectedChunks[kMaxTerrainShadowCascades] = {};

    /** @brief Per-cascade terrain chunks rejected due to invalid handles or resources. */
    std::uint32_t shadowCascadeInvalidResourceChunks[kMaxTerrainShadowCascades] = {};

    /** @brief Per-cascade caster chunk counts by selected LOD index. */
    std::uint32_t shadowCascadeCasterChunksByLod[kMaxTerrainShadowCascades][kMaxTerrainLodLevels] = {};
};
} // namespace full_renderer
