# Renderer Conventions

This document records coordinate, unit, matrix, and color conventions used by public renderer APIs.

## Defaults

Until a different convention is explicitly documented:

- world units: meters
- up axis: Y-up
- handedness: right-handed world coordinates
- shader lighting space: linear color
- swapchain output: gamma-correct

## Required documentation

Public APIs and shader interfaces should document:

- matrix storage convention
- clip-space expectations
- projection helper behavior
- normal/tangent basis convention
- texture color-space expectations
- coordinate-space names for vectors and matrices

## Backend differences

If bgfx/backend clip-space details differ by graphics API, hide the difference behind backend setup and view/projection helpers.

Public camera and view data should remain backend-neutral.

## Open-world hardening conventions

Active large-world policy:

- Renderer core consumes single-precision renderer-space coordinates in meters.
- Future engines should keep large absolute world coordinates in engine-owned
  double-precision state and submit camera-relative or floating-origin-rebased
  descriptors to the renderer.
- The optional sample `src/engine_bridge` seam demonstrates this policy with a
  `FrameOriginDesc`: engine/world positions are converted by subtracting an
  engine-owned origin before renderer-facing terrain, bounds, decal, particle,
  and shadow/camera descriptors are built.
- The renderer does not decide origin shifts, streaming regions, or residency.
  Origin changes are frame-boundary input owned by the engine/sample.
- Absolute renderer-space coordinates above the documented warning threshold are
  diagnostics risks, not backend errors; callers should rebase before they lose
  useful float precision.

Future engine-readiness work should continue documenting:

- culling and bounds spaces for terrain, static, instanced, skinned, decal, and
  particle submissions
- texture color-space and compression expectations for asset pipeline tools
- renderer-facing asset contracts for meshes, textures, materials, terrain, and
  skeletons; `docs/assets.md` is the current source of truth
- runtime shader asset responsibility for installed package consumers; shader
  binaries are currently engine/sample-owned runtime assets
- transparency ordering assumptions for particles, alpha fade, decals, and
  transparent materials; the first policy sorts alpha-blend and alpha fade
  back-to-front within each draw family and defers OIT/cross-family sorting
