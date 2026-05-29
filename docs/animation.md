# Skeletal Animation Foundation

Phase 3 starts with renderer-owned skeletal metadata and a skinned mesh path
that accepts final CPU-provided skinning palettes. The renderer does not own
animation state, evaluate clips, blend poses, retarget skeletons, or simulate
crowds. Engines remain responsible for animation sampling and submit final
joint matrices for the current frame.

## Public Model

`SkeletonHandle` identifies renderer-owned skeleton metadata. `SkeletonDesc`
contains a parent-before-child hierarchy and one inverse bind-pose matrix per
joint. The first milestone supports up to 64 joints and requires a single root.
Skeleton data is copied during `createSkeleton`.

`SkinnedMeshHandle` identifies a renderer-owned skinned mesh resource.
`SkinnedMeshDesc` references a live skeleton and fixed-format vertices with
position, normal, color, four joint indices, and four weights. Vertex/index data
is copied during `createSkinnedMesh`.

The skinned asset contract matches the renderer's global conventions: meters,
Y-up, right-handed local coordinates, finite non-zero normals, and linear vertex
colors in `[0, 1]`. Joint indices are encoded as floats for shader input but
must be integer-valued and in range of the owning skeleton. Weights must be
non-negative and sum to one within validation tolerance. The runtime does not
generate skinned tangents or evaluate bind poses beyond finite matrix and
hierarchy checks; import tools remain responsible for authored tangent basis,
unit conversion, and deeper skeleton validation.

`AnimatedDrawItem` submits one skinned mesh instance for the frame. The host
provides `SkinningPaletteDesc::skinningMatrices`, one final matrix per skeleton
joint, usually `currentJointModel * inverseBindPose`. Palette pointers are read
only during `submit`. Optional debug joint model matrices can be supplied for
bone-line visualization.

## Rendering

The bgfx backend owns the skinned vertex/index buffers, a fixed 64-matrix
palette uniform, and `vs_skinned_mesh.sc`. The skinned vertex shader blends
positions and normals by the CPU-provided palette, applies the draw model
matrix, and reuses `fs_mesh.sc` for basic material color, CSM receiving, PCF,
cascade blending, and distance fog.

Animated meshes receive CSM shadows when `AnimatedDrawItem::receivesShadow` is
true and the material is lit. Opaque skinned meshes also cast into every active
CSM cascade when `AnimatedDrawItem::castsShadow` is true, the submitted bounds
intersect that cascade's light frustum, and the CPU-provided palette is valid.
The shadow path uses the same palette data as forward rendering but a
depth-only skinned vertex shader.

## Debug

`AnimationDebugOptions` can draw submitted animated bounds and skeleton bone
lines. Bone lines require `debugJointModelMatrices` in the draw palette. Debug
data is transient and frame-local; the renderer does not retain pose pointers.

The sample application's diagnostics panel shows live skeleton/skinned mesh
counts, submitted/rendered animated draw counts, skinned shadow caster counts,
and skeleton/bounds toggles.

## Limitations

- no animation clip resources or renderer-side pose evaluation
- no animation blending, retargeting, IK, compression, or state machines
- no animation-generated bounds; callers submit conservative animated bounds
- no alpha-tested or transparent skinned shadows yet
- no skeletal instancing, crowds, impostors, or animation texture buffers
- no skeletal physics, ragdolls, or editor tooling

## Engine-readiness hardening

Before using the skinned path for open-world crowds, add:

- stronger externally supplied animated bounds validation and bind-pose fallback
- stress tests for many skinned draws, invalid palettes, maximum joint counts,
  and shadow-casting skinned meshes
- a documented engine-side clip/blend/evaluation ownership model
- a future crowd palette or animation texture buffering policy
- regression scenes for skinned forward rendering, CSM casting, selection
  outlines, structure fade, and debug skeleton lines together
- import-time skeleton and skinned mesh validation for joint limits, inverse
  bind matrices, tangent basis, influence counts, weight normalization, and
  conservative animated bounds
- production crowd performance proof before adding renderer-owned animation
  features; the renderer should not take over clip sampling, state machines, or
  gameplay animation ownership without a separate roadmap milestone
