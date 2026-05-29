# Renderer Roadmap

Use this roadmap to determine the appropriate level of complexity for a requested feature. Do not implement all phases at once.

## Phase 1 — Foundation

Implement:

- SDL3 window/input sample shell
- bgfx init and swapchain
- renderer init/shutdown
- shader compilation path
- mesh upload
- basic material
- camera/view setup
- basic forward renderer
- directional light
- gamma-correct output
- resize handling
- simple debug stats

Acceptance criteria:

- A sample app opens an SDL3 window.
- bgfx initializes and renders a mesh.
- Shader compilation is documented and repeatable.
- The renderer can be initialized and shut down cleanly.
- A camera can move independently of renderer internals.
- Public interfaces are documented enough for engine integration.

## Phase 2 — Open-World Basics

Implement:

- terrain chunks
- chunk resource lifecycle
- instancing
- frustum culling
- LOD selection
- texture splatting
- sky/fog
- first cascaded shadow map implementation
- chunk and culling debug overlays

Acceptance criteria:

- A large grid of terrain chunks can be rendered.
- Non-visible chunks are culled.
- Simple LOD changes occur by distance.
- Many repeated objects render through instancing.
- Terrain uses at least a basic splat/material blend.
- Directional shadows work over terrain and meshes.

## Phase 3 — Kenshi Parity

Implement:

- skeletal animation path
- crowd rendering approach
- SSAO
- decals
- particles
- weather hooks
- selection/outline rendering
- roof/building fade
- color grading

Acceptance criteria:

- Animated actors render with shared assets.
- Many actors can be visible with acceptable batching.
- Selection outlines are controlled by external IDs/flags.
- Building fade is externally controllable.
- Weather and color grading can be changed at runtime.

## Phase 3 hardening and engine-readiness milestones

The renderer is suitable for prototype engine integration once Phase 3 feature
foundations are present, but it should not be treated as open-world-engine
ready until the hardening milestones below are complete. Prefer these
milestones before adding more visual effects.

### P0 - Correctness and pass interactions

- Initial CPU-side pass planning hardening is in place for independent
  optional-pass disable behavior, scene target reason ownership, final present
  validity, skipped/blocked reason diagnostics, and basic scene target
  resize/reconfiguration planning.
- The remaining visual-regression slice now has focused fixes and a sample
  validation harness: skinned outline mask projection follows the forward
  skinned path, decal projection/depth reconstruction is stabilized, decal
  frustum culling is conservative near edges, and the sample exposes named
  camera bookmarks plus visual-regression presets.
- Continue regression-testing prior interaction bugs: black screen when
  projected decals are disabled, billboards depending on SSAO/depth state, and
  any new outline/decal regressions found by the validation presets.
- Verify every optional pass can be disabled independently while final
  presentation remains valid.
- Lock down shared scene color/depth ownership so SSAO, decals, particles,
  color grading, selection/outline, and final present never depend accidentally
  on another feature being enabled.
- Add repeatable visual validation scenes for terrain, shadows, decals, SSAO,
  particles, selection, fade, weather, and color grading in combination.
- Add resize regression coverage for all viewport-sized resources.

### P0 - Resource lifetime and engine boundary

- Implemented first hardening slice: backend-neutral live resource counts,
  approximate memory estimates, allocation failure counters, stale/invalid
  handle diagnostics, selection/SSAO/scene/shadow recreate counters, and
  resource-oriented validation presets.
- Implemented mocked allocation-failure and stale-reference test expansion for
  representative mesh, skinned mesh, texture, material, decal, particle,
  skinned-palette, and terrain material texture fallback paths.
- Continue expanding deterministic create/destroy/recreate coverage for backend
  resources that still need mocked failure paths.
- Continue broadening stale/invalid/destroyed handle coverage across all
  submission types and resource managers.
- Preserve the renderer/game boundary: selection, particles, weather, fade,
  buildings, and animation simulation stay engine/sample owned.
- Document exactly which frame data is copied and which data must outlive
  submission.

### P1 - Open-world scale systems

- Initial terrain/open-world residency hardening is in place: terrain stats
  now expose resident/inactive slots, create/destroy/reuse churn,
  invalid/stale submitted handles, LOD fallback rejects, splat residency, and
  sample terrain validation presets. CPU tests cover slot reuse, stale handles,
  large-grid culling/LOD planning, streaming-style churn, skirt bounds after
  reuse, and caller-rebased large-coordinate planning.
- Initial shared large-scene culling diagnostics are in place for terrain,
  static mesh draws, GPU-instanced batches, skinned draws, decals, particles,
  and CSM shadow caster categories, with LargeScene_* validation presets and
  CPU-side deterministic aggregation tests.
- Initial large-scene frame budget diagnostics are in place: CPU-side renderer
  planning timings, staged-byte estimates, terrain/resource churn summaries,
  post-pass submitted/skipped counts, adjustable sample warning thresholds, and
  CPU-side aggregation tests.
- Initial long-session open-world churn validation is in place: fixed-seed
  CPU/sample harnesses simulate many frames of terrain chunk slot reuse,
  material/texture/LOD fallbacks, decal/particle/skinned-palette churn,
  optional pass toggles, and resize/post-target recreation without adding real
  async streaming or renderer-owned world policy.
- Initial engine-owned streaming seam and large-world origin hardening are in
  place: `src/engine_bridge` maps engine chunk IDs to renderer terrain handles,
  runs fixed-seed churn through that mapping layer, and demonstrates
  caller-owned camera-relative origin conversion for representative
  large-coordinate diagnostics without moving streaming policy into renderer
  core. The seam is kept out of the installed renderer package target, while
  the public renderer API now exposes in-place terrain chunk descriptor updates
  and terrain shadow-caster debug snapshots for engine integration.
- Harden terrain chunk streaming, LOD transitions, chunk material residency,
  skirts/seams, and large-scene culling stress cases.
- Add shared culling diagnostics for terrain, static meshes, instancing,
  skinned meshes, decals, particles, and shadow casters.
- Establish large-world precision/origin-shift policy.
- Add stress scenes for thousands of chunks, many instances, many decals,
  particles, and skinned actors.
- Add CPU and GPU timing diagnostics for major passes where supported.

### P1 - Materials, shaders, animation, and transparency

- Initial material/shader/transparency policy hardening is in place: public
  basic materials expose opaque, alpha-test, and alpha-blend modes; terrain
  splat remains opaque; alpha-blend and alpha fade are stable-sorted within
  draw families; alpha-test and alpha-blend shadow-caster limitations are
  diagnosed; shader variant keys are tracked internally; and diagnostics/tests
  cover material buckets, variant keys, and transparent sorting.
- Initial renderer-facing asset contract hardening is in place: mesh,
  texture, material, terrain, skeleton, and skinned mesh contracts are
  documented; texture semantic/color-space metadata is validated; static mesh
  descriptors reject non-finite data, zero normals, out-of-range colors,
  invalid indices, and degenerate triangles; and CPU tests cover representative
  asset-contract failures without GPU access.
- Continue refining the material model beyond the first policy, especially
  alpha-tested shadow clipping and imported asset color-space validation.
- Keep the shader variant policy current before growing more one-off shader
  paths.
- Continue hardening color-space conventions across textures, material passes,
  post passes, and swapchain output.
- Strengthen skinned bounds and palette validation, including many skinned
  draws and shadow-casting cases.
- Expand transparent sorting only if needed; full OIT and cross-family sorting
  remain deferred.

### P1 - Diagnostics and tooling

- Add a backend-safe texture preview abstraction for internal targets such as
  shadow maps, SSAO, scene depth, decal depth, and selection mask without
  exposing backend handles.
- Add pass/resource diagnostics for dimensions, formats, recreate reasons,
  skipped passes, invalid resources, and allocation failures.
- Turn the sample app into a repeatable renderer validation harness with
  camera bookmarks and feature presets.
- Keep debug UI as a control/diagnostic surface, not an editor.

### P2 - Asset pipeline, builds, and platform confidence

- Extend the initial mesh, texture, skeleton, material, and terrain asset
  contracts into importer/tool validation, including richer tangent-basis,
  bounds, mip-chain, and compression checks.
- Initial build/platform confidence hardening is in place: CMake presets cover
  development, library-only, no-debug-UI, tests, shader validation, and
  CI-style Windows builds; shader compilation is gated by an explicit option;
  library-only builds no longer require shaderc; and public-header compile
  coverage verifies engine-facing headers stay free of SDL3/ImGui/backend
  dependencies.
- Initial install/export packaging is in place: library-only builds install
  public headers, the static renderer library, and a relocatable
  `FullRenderer::full_renderer` CMake package. An external-style consumer smoke
  project validates `find_package(FullRenderer CONFIG REQUIRED)` from a staged
  install prefix without source-tree include paths.
- Initial CI automation is in place for Windows package confidence: a local
  PowerShell runner and GitHub Actions workflow cover CPU tests, library-only,
  no-debug-UI, package install staging, external consumer smoke, and shader
  validation. Shader binaries remain build-local validation artifacts for now;
  optional shader install/runtime asset packaging is deferred until a
  backend/profile asset convention is defined.
- Add import/validation tooling without entangling importer internals with
  renderer runtime ownership.
- Stabilize CMake/vcpkg presets, library-only builds, no-debug-UI builds, and
  shader compilation validation.
- Define the supported backend/platform matrix, with Windows/D3D11 first unless
  another target is explicitly added.
- Broaden CI-style configure/build/test/shader validation as dependency setup
  and the supported platform/backend matrix expand.

## Open-world production readiness gates

Current status: the renderer is ready for serious prototype integration,
validation scenes, and package-consumer smoke testing. It is not yet a
production-ready open-world engine renderer. Treat the package target
`FullRenderer::full_renderer` as suitable for external engine experiments while
the remaining P2/P3 gates below are worked down.

Before relying on it as the primary renderer for a shipped or large-scale
open-world engine, finish or explicitly defer:

- real engine-owned async streaming and residency integration. Initial
  fixed-seed renderer-facing churn and engine-bridge seam tests now cover
  terrain chunks, engine-to-renderer mapping, materials, textures, LODs,
  decals, particles, skinned palettes, optional passes, post targets, and
  origin shifts, but they intentionally stop short of background IO, streaming
  policy, or world persistence.
- a production large-world precision proof beyond the current caller-rebased
  single-precision render-space policy. The engine bridge now documents and
  tests camera-relative origin conversion at representative coordinates, but
  real projects still need engine-owned origin choice and streaming-region
  policy validated on content.
- production asset pipeline validation for mesh units/axes/tangents, texture
  color space/mips/compression, material slots, terrain height/splat/normal
  data, skeleton hierarchy/weights/palettes, and shader/runtime asset manifests
- a deliberate runtime shader binary packaging convention for installed
  packages or a documented engine-owned shader asset responsibility, validated
  by the consumer smoke flow
- a mature material/transparency plan for imported content, including
  alpha-tested shadow clipping, cross-family transparent ordering decisions,
  roughness/metallic or PBR expansion if needed, and color-space validation
  that matches the asset pipeline
- measured scale proof on representative large scenes, including CPU and GPU
  timing, staged allocation/churn counters, long-session stability, and manual
  or automated regression scenes
- culling and scalability upgrades only where diagnostics prove they are
  needed, such as spatial acceleration, Hi-Z/occlusion, GPU-driven culling, or
  shadow-caster acceleration
- a broader supported platform/backend matrix or a clearly documented
  Windows/D3D11-only support statement with CI coverage matching that policy
- backend-safe internal texture previews, capture/debug tooling, and eventual
  image or GPU-capture regression workflows that do not expose backend handles
  through public APIs

Recommended next sequence:

1. P2 asset/import validation tooling and shader runtime asset convention.
2. P2/P3 package-consumer stability tests and real engine-owned streaming
   integration seams built on the fixed-seed churn harness.
3. P3 large-world origin-shift or camera-relative descriptor hardening.
4. P3 representative-scale performance proof with CPU/GPU timing and capture
   diagnostics.
5. P3 culling/scalability upgrades driven by the measured bottlenecks.

## Engine Expansion Track

This track is separate from renderer phases. Use it when work is explicitly
about building engine runtime systems under `src/engine/`. The renderer remains
an embedded library consumed through public APIs; engine expansion should not
move gameplay, streaming policy, or editor concepts into renderer internals.

### E0 - Repository boundary and guidance

- Create `src/engine/` with local `AGENTS.md` guidance.
- Keep `src/engine_bridge/` documented as a sample/testing adapter, not the
  engine runtime.
- Document dependency direction: renderer never depends on engine, engine uses
  `FullRenderer::full_renderer`, and the sample app may compose both.
- Do not add engine runtime code, CMake targets, or public C++ engine APIs until
  a later milestone asks for them.

### E1 - Minimal engine core skeleton

- Add the first compileable engine target only when requested.
- Establish engine lifecycle, time step, configuration, and diagnostics
  ownership without taking over renderer lifecycle internals.
- Add CPU-side tests for deterministic lifecycle and configuration behavior.

### E2 - World and streaming ownership

- Define engine-owned chunk IDs, streaming regions, residency state, and
  large-world origin policy.
- Translate world coordinates to renderer-relative frame data in
  `src/engine/renderer_integration/`.
- Keep async IO and streaming decisions in the engine, not the renderer.

### E3 - Asset catalog and cooked runtime asset conventions

- Define engine-owned asset IDs, manifests, dependency lookup, and cooked asset
  responsibilities.
- Map engine assets to renderer mesh, texture, material, skeleton, terrain, and
  shader-runtime inputs through renderer public APIs.
- Keep importer/tool internals out of renderer runtime ownership.

### E4 - Simulation and entity layer

- Add entity or scene object identity only after world, assets, and renderer
  integration boundaries are stable.
- Keep gameplay simulation, animation state, selection state, weather policy,
  and persistence engine-owned.
- Submit renderable snapshots to the renderer rather than exposing engine
  objects to renderer internals.

### E5 - Editor and tooling integration

- Build editor/tooling on top of engine runtime APIs after load, modify,
  serialize, and validate paths exist.
- Keep editor-only state out of renderer core and engine runtime hot paths.
- Use renderer debug surfaces as diagnostics, not as editor data ownership.

## Phase 4 — Modern Clarity

Implement:

- Forward+ or clustered lighting
- TAA or SMAA
- volumetric fog
- Hi-Z occlusion
- improved vegetation
- biome-specific lighting
- richer debug overlays

Acceptance criteria:

- Lighting scales beyond a few local lights.
- Anti-aliasing path is configurable.
- Occlusion and cluster debug views exist.
- Biome lighting can be driven externally.
- Vegetation supports wind/material variation hooks.

## Preferred first milestone

If starting from an empty repository, implement the first milestone in this order:

1. CMake project skeleton.
2. SDL3 sample app window.
3. bgfx initialization through an internal backend wrapper.
4. Public `IRenderer` initialization/shutdown interface.
5. Shader compile script or CMake target.
6. Hardcoded triangle or cube.
7. Mesh upload API.
8. Camera view/projection.
9. Directional light uniform.
10. Gamma-correct output.
11. Minimal docs for public API and conventions.

Keep this milestone small and working before adding terrain.
