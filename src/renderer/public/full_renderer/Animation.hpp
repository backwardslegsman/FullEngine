#pragma once

#include "full_renderer/Fade.hpp"
#include "full_renderer/Handles.hpp"
#include "full_renderer/Terrain.hpp"

#include <cstdint>

namespace full_renderer
{
/** @brief Maximum joints accepted by the first CPU-palette skinning path. */
constexpr std::uint32_t kMaxSkinningJoints = 64;

/** @brief Fixed number of joint influences carried by each skinned vertex. */
constexpr std::uint32_t kMaxSkinningInfluences = 4;

/**
 * @brief One joint in a renderer-owned skeleton metadata resource.
 *
 * The renderer stores hierarchy metadata for validation and debug drawing only;
 * it does not evaluate animation clips or own gameplay animation state.
 * Matrices are column-major floats. `parentIndex` must be `-1` for the single
 * root joint or a non-negative index less than this joint index.
 */
struct SkeletonJointDesc
{
    /** @brief Parent joint index, or `-1` for the skeleton root. */
    std::int32_t parentIndex = -1;

    /**
     * @brief Column-major inverse bind-pose matrix for this joint.
     *
     * Values must be finite. The renderer copies the matrix during
     * `IRenderer::createSkeleton` and does not retain caller storage.
     */
    float inverseBindPose[16] = {};
};

/**
 * @brief Descriptor for renderer-owned skeleton metadata.
 *
 * `joints` points to `jointCount` hierarchy records and must remain valid only
 * for the duration of `IRenderer::createSkeleton`; data is copied. The first
 * skeletal milestone supports up to `kMaxSkinningJoints` joints and expects a
 * single root with parents listed before children. Units are meters in the
 * renderer's Y-up, right-handed local skeleton space.
 */
struct SkeletonDesc
{
    /** @brief Pointer to `jointCount` joint records. */
    const SkeletonJointDesc* joints = nullptr;

    /** @brief Number of joints in the skeleton; must be in `[1, kMaxSkinningJoints]`. */
    std::uint32_t jointCount = 0;
};

/**
 * @brief Fixed vertex format for the first skinned mesh path.
 *
 * Positions and normals are mesh-local. `jointIndices` are stored as floats for
 * portable shader input and must be integer-valued indices into the owning
 * skeleton. `jointWeights` must be finite, non-negative, and sum to one within
 * renderer validation tolerance. Colors are linear RGBA values in `[0, 1]`;
 * normals must be finite and non-zero.
 */
struct SkinnedMeshVertex
{
    /** @brief Mesh-local position in meters. */
    float position[3] = {};

    /** @brief Mesh-local unit normal before skinning. */
    float normal[3] = {0.0f, 1.0f, 0.0f};

    /** @brief Linear RGBA vertex color multiplied by the material color. */
    float colorLinear[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    /** @brief Up to four integer-valued joint indices encoded as floats. */
    float jointIndices[kMaxSkinningInfluences] = {};

    /** @brief Corresponding skinning weights; expected to sum to one. */
    float jointWeights[kMaxSkinningInfluences] = {1.0f, 0.0f, 0.0f, 0.0f};
};

/**
 * @brief One draw-range section inside a renderer-owned skinned mesh.
 *
 * Sections refer to ranges in the 16-bit index buffer supplied to
 * `SkinnedMeshDesc`. They allow callers to submit the same skinned mesh with
 * different materials by issuing one `AnimatedDrawItem` per section. Ranges
 * are triangle-index ranges and must be non-empty, in bounds, and multiples of
 * three. If no sections are supplied during creation, the renderer creates an
 * implicit section covering the whole index buffer.
 */
struct SkinnedMeshSectionDesc
{
    /** @brief First index in the skinned mesh index buffer. */
    std::uint32_t firstIndex = 0;

    /** @brief Number of triangle indices in this section. */
    std::uint32_t indexCount = 0;
};

/**
 * @brief CPU-side descriptor for creating a renderer-owned skinned mesh.
 *
 * The descriptor references an existing live `SkeletonHandle` and fixed-format
 * skinned vertex/index data. The renderer copies vertex and index data before
 * returning. The skeleton metadata must remain live while skinned meshes and
 * animated submissions referencing it are used. Vertex data is validated using
 * the skinned asset contract; invalid joint indices, non-normalized weights,
 * non-finite positions, zero normals, and out-of-range colors are rejected.
 */
struct SkinnedMeshDesc
{
    /** @brief Live skeleton metadata handle that defines valid joint indices. */
    SkeletonHandle skeleton;

    /** @brief Pointer to `vertexCount` skinned vertices; must not be null. */
    const SkinnedMeshVertex* vertices = nullptr;

    /** @brief Number of vertices referenced by `vertices`; zero is invalid. */
    std::uint32_t vertexCount = 0;

    /** @brief Pointer to `indexCount` 16-bit triangle indices; must not be null. */
    const std::uint16_t* indices = nullptr;

    /** @brief Number of indices referenced by `indices`; must be a non-zero multiple of three. */
    std::uint32_t indexCount = 0;

    /** @brief Optional material/draw sections over `indices`; null with zero count means one implicit whole-mesh section. */
    const SkinnedMeshSectionDesc* sections = nullptr;

    /** @brief Number of section descriptors available through `sections`. */
    std::uint32_t sectionCount = 0;
};

/**
 * @brief Frame-local final skinning palette supplied by the host engine.
 *
 * The renderer does not evaluate animation clips in this milestone. Callers
 * provide final column-major skinning matrices, typically
 * `currentJointModel * inverseBindPose`, for each joint in the skinned mesh's
 * skeleton. Pointers are read only during `IRenderer::submit`.
 */
struct SkinningPaletteDesc
{
    /** @brief Pointer to `matrixCount` column-major final skinning matrices. */
    const float* skinningMatrices = nullptr;

    /** @brief Number of 4x4 matrices available through `skinningMatrices`. */
    std::uint32_t matrixCount = 0;

    /**
     * @brief Optional current joint model matrices for debug bone rendering.
     *
     * When non-null, the pointer must reference one column-major matrix per
     * skeleton joint. These matrices are not used for skinning; they only allow
     * the renderer debug path to draw bone lines for the submitted pose.
     */
    const float* debugJointModelMatrices = nullptr;

    /** @brief Number of matrices available through `debugJointModelMatrices`. */
    std::uint32_t debugJointModelMatrixCount = 0;
};

/**
 * @brief One skinned mesh/material instance submitted for the current frame.
 *
 * The model matrix transforms skinned mesh-local coordinates into world space.
 * The host owns palette and matrix storage and the renderer reads it during
 * `submit` only. The first Phase 3 path supports forward rendering, CSM
 * receiving, and optional CSM shadow casting from caller-supplied conservative
 * world-space bounds. The renderer does not skin vertices on the CPU to compute
 * bounds.
 */
struct AnimatedDrawItem
{
    /** @brief Renderer-owned skinned mesh resource to draw. */
    SkinnedMeshHandle mesh;

    /** @brief Renderer-owned basic material resource used for this draw. */
    MaterialHandle material;

    /** @brief Skinned mesh section to draw. Defaults to the first or implicit whole-mesh section. */
    std::uint32_t sectionIndex = 0;

    /** @brief Column-major local-to-world transform applied after skinning. */
    float model[16] = {};

    /**
     * @brief Conservative world-space bounds for debug visualization and shadow casting.
     *
     * Bounds are required when `castsShadow` is true. They should enclose the
     * submitted animated pose in meters using the renderer's Y-up right-handed
     * world space. The renderer reads the bounds during `submit` only.
     */
    Aabb bounds;

    /** @brief Final skinning matrices supplied for this draw. */
    SkinningPaletteDesc palette;

    /** @brief Enables CSM receiving in the basic forward skinned mesh shader. */
    bool receivesShadow = true;

    /**
     * @brief Enables this skinned draw as a cascaded directional-shadow caster.
     *
     * Shadow casting is opt-in. When enabled, `bounds` are tested against each
     * active cascade and matching cascades render the skinned mesh through a
     * depth-only GPU skinning path using the same CPU-provided palette as the
     * forward pass. Invalid palettes or bounds reject the submission.
     */
    bool castsShadow = false;

    /**
     * @brief Marks this skinned draw for frame-local selection outline rendering.
     *
     * Selection state is supplied externally by the caller. When enabled in the
     * frame outline descriptor, selected skinned geometry is rendered into the
     * outline mask using the submitted CPU skinning palette.
     */
    bool selected = false;

    /**
     * @brief Externally supplied per-draw structure fade state.
     *
     * Fade ownership belongs to the caller. The renderer applies the scalar in
     * the forward skinned mesh pass only and does not evaluate building or
     * camera obstruction rules. CSM shadow casting remains controlled by
     * `castsShadow` and is not dithered in this first fade milestone.
     */
    FadeDesc fade;
};

/**
 * @brief Optional skeletal debug visualization settings for one submission.
 *
 * Debug rendering is read during `submit` and produces backend-neutral debug
 * lines when enabled. It does not expose Dear ImGui or backend handles through
 * the public renderer API.
 */
struct AnimationDebugOptions
{
    /** @brief Draws bone lines when animated draws provide debug joint model matrices. */
    bool drawSkeletons = false;

    /** @brief Draws submitted animated world-space bounds. */
    bool drawBounds = false;
};
} // namespace full_renderer
