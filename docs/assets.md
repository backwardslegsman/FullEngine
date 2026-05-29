# Renderer Asset Contracts

This document defines the renderer-facing asset contracts for meshes, textures,
materials, terrain, and skeletons. It is not an importer specification and does
not turn the runtime renderer into an asset pipeline. Import tools or engine
code should validate and convert authored assets into these descriptors before
creating renderer resources.

## Global Conventions

- Units are meters.
- World and local coordinates are Y-up and right-handed.
- Matrices submitted through public APIs are column-major.
- Public material colors and debug colors are linear RGBA in `[0, 1]`.
- Public descriptor arrays are copied during resource creation unless the
  descriptor explicitly says they are frame-local submission data.
- Frame submissions, skinned palettes, particles, decals, weather, fade, and
  selection data are consumed during the current submit call/frame and are not
  retained as caller-owned pointers.
- Backend texture, framebuffer, render-target, view, shader, and buffer handles
  remain private to renderer/backend code.

## Meshes

`MeshDesc` uses fixed `MeshVertex` data and 16-bit triangle indices.

Required data:

- finite local-space positions in meters
- finite non-zero local-space normals
- linear vertex colors in `[0, 1]`
- non-null vertex and index arrays
- non-zero vertex and index counts
- index count divisible by three
- indices in range of `vertexCount`
- non-degenerate indexed triangles

The runtime path does not generate normals or tangents. Missing or invalid
normals are rejected. Tangent data is not part of the first fixed mesh vertex
format; import tools should preserve enough source data to generate tangents
when future normal-mapped mesh materials are added. Bounds are not stored on
`MeshDesc`; engines submit conservative world-space bounds per draw.

## Textures

`TextureDesc` currently accepts tightly packed, uncompressed, single-mip RGBA8
2D data. The renderer copies bytes during `createTexture`.

Texture metadata records the asset contract:

- `TextureSemantic::Color` expects `TextureColorSpace::Srgb`.
- `TextureSemantic::LinearData` expects `TextureColorSpace::Linear`.
- `TextureSemantic::NormalMap` expects `TextureColorSpace::EncodedNormal`.
- `TextureSemantic::TerrainSplat` expects `TextureColorSpace::Linear`.
- `TextureSemantic::ColorGradingLut` expects `TextureColorSpace::Linear`.
- `TextureSemantic::Debug` accepts `Srgb` or `Linear`.

The current renderer records and validates this policy, but it does not yet
perform an importer-style color conversion or expose a compressed texture
upload path. `mipCount` must be one and `compressed` must be false for runtime
uploads. Import tools should still generate mips for production color, normal,
terrain layer, decal, and particle textures where appropriate; packed mip and
compressed runtime asset upload are future work.

Normal maps use encoded RGB tangent-space normals, unpacked conceptually from
`[0, 1]` to `[-1, 1]`. Terrain normal maps use the renderer's positive green
channel convention by default; `TerrainMaterialDesc::flipNormalMapY` supports
assets authored with the opposite green channel.

## Materials

`MaterialDesc` represents the first small renderer material model, not an
engine material graph.

Supported material paths:

- `MaterialKind::Basic` with opaque, alpha-test, or alpha-blend policy
- `MaterialKind::TerrainSplat` with four albedo layers and optional per-layer
  normal maps
- decals and particles through frame descriptors rather than material resources
- structure fade through per-draw fade descriptors and backend-private state

Basic material colors are linear RGBA. Terrain layer fallback colors are
linear RGBA. Texture handles referenced by materials remain separately owned;
destroyed or stale texture handles use documented fallback paths instead of
dereferencing backend resources.

Alpha policy:

- opaque writes depth and can cast/receive CSM according to draw flags
- alpha-test writes depth in the opaque-style path, but alpha-clipped CSM
  casting is deferred
- alpha-blend renders after opaque-style geometry with depth test enabled and
  depth writes disabled
- terrain splat materials remain opaque
- decals and particles keep their existing specialized pass policies

## Terrain

Terrain chunks are CPU-side renderer records referencing externally created
mesh, material, and optional splat texture handles. The renderer does not own
height data, streaming policy, biome data, save/load state, or chunk residency
decisions.

Terrain contract:

- bounds are finite world-space AABBs in meters
- LOD entries are copied during chunk creation
- LOD count must be `1..kMaxTerrainLodLevels`
- LOD thresholds are finite, non-negative, and sorted from high to low detail
  by increasing maximum distance
- LOD mesh and material handles must be non-zero at chunk creation
- chunk splat maps are optional

Splat channel convention is R/G/B/A for layers 0/1/2/3. Splat maps are linear
weight data. Missing splat maps use deterministic full layer-0 weight. Missing
terrain albedo layer textures use layer fallback colors. Missing normal maps
use flat tangent-space normals. The runtime does not validate authored height
sample grids because height data is not part of `TerrainChunkDesc`; import
tools should validate height dimensions, spacing, seam compatibility, and
normal generation before producing renderer mesh resources.

Large-world policy remains engine-owned: submit camera-relative or
floating-origin-rebased coordinates to the renderer. GPU-facing data is
single-precision.

## Skeletons And Skinned Meshes

The renderer stores skeleton metadata for validation, drawing, shadows, and
debug lines. It does not evaluate clips, blend poses, retarget, or simulate
animation state.

Skeleton contract:

- one root joint
- parents listed before children
- joint count in `[1, kMaxSkinningJoints]`
- finite column-major inverse bind matrices

Skinned mesh contract:

- references a live skeleton
- fixed skinned vertex format with four influences per vertex
- finite positions and non-zero normals
- linear vertex colors in `[0, 1]`
- integer-valued joint indices encoded as floats
- joint indices in range of the owning skeleton
- non-negative weights that sum to one within renderer tolerance
- non-null 16-bit triangle indices with indices in range

`SkinningPaletteDesc` is frame-local. It must provide one finite column-major
skinning matrix per skeleton joint, usually `currentJointModel * inverseBind`.
Optional debug joint model matrices must either be null with zero count or
provide one finite matrix per joint.

## Import-Time Versus Runtime Validation

Runtime validation rejects descriptors that would be unsafe or ambiguous for
renderer resource creation. Import-time validation should additionally catch:

- mesh tangent generation policy and tangent basis compatibility
- authoring-unit conversion into meters
- winding and handedness conversion
- mesh simplification or LOD generation quality
- texture compression and mip-chain generation
- skeleton bind-pose sanity beyond finite matrices
- terrain height-grid dimensions, spacing, seams, and neighbor normal policy

No asset database, editor browser, hot reload daemon, texture compressor, mesh
optimizer, or background streamer exists in this milestone.

## Production Asset Pipeline Gates

The current contracts are enough for runtime descriptor validation and
prototype engine integration. Production open-world content still needs
import-time tooling that validates and reports:

- authored-unit conversion into meters, Y-up, right-handed renderer space
- tangent-basis generation, normal-map green-channel convention, and missing
  tangent/normal warnings
- texture color-space, mip-chain, compression, and alpha policy per slot
- material slot compatibility, fallback behavior, and unsupported shader
  variants before runtime
- terrain height spacing, chunk dimensions, splat normalization, seam/skirt
  policy, normal generation, and material residency manifests
- skeleton hierarchy, inverse bind matrices, joint limits, influence counts,
  weight normalization, and animated bounds policy
- shader/runtime asset manifests that match the selected backend/profile

These tools should remain outside renderer runtime ownership. The renderer
should continue consuming validated descriptors and reporting fallback or
rejection diagnostics when invalid data reaches runtime.
