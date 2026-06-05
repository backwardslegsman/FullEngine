# Renderer Roadmap

Use this file for active planning and context. It should stay short enough to
read at the start of an implementation turn. Historical slice-by-slice detail
lives in [implementation_log.md](implementation_log.md); open that file only
when you need chronology or provenance for an older subsystem.

## Agent Context Rules

- Prefer this file for current priorities.
- Prefer `docs/engine_overview.md` for current engine architecture.
- Prefer subsystem READMEs and public headers for exact API behavior.
- Use `implementation_log.md` as an archive, not as default context.
- Keep new roadmap entries concise. Move completed detailed slice notes to the
  implementation log when they stop guiding current work.

## Current Status

The renderer is suitable for serious prototype integration and validation
scenes, but it is not yet production-ready for a large open-world engine.
The most important current proof is the asset/animation path:

- Assimp imports static meshes, skeletons, skinned meshes, material section
  metadata, and animation clips from glTF into renderer-free CPU payloads.
- stb imports direct image files into single-mip RGBA8 CPU texture payloads.
- Loaded payloads can plan and execute renderer uploads for mesh, texture,
  material, skeleton, and skinned mesh resources through caller-owned renderer
  APIs.
- CPU animation sampling and retained playback produce borrowed
  `SkinningPaletteDesc` views for frame-local animated submission.
- The debug-ui sample has a manually verified wolf smoke: real wolf glTF
  skeleton, skinned mesh sections, animation clip, extracted materials,
  decoded textures, uploaded renderer resources, section draws, UV0 texture
  sampling, tangent payload/import/upload coverage, and deterministic
  focus/diagnostics. Smoke run/clear actions are deferred until after
  `endFrame` so skeleton/skinned resource lifecycle calls obey the public
  no-active-frame contract.

The terrain/streaming path is also well advanced:

- `src/engine` owns terrain runtime state, manifests, readiness planning,
  source catalogs, load intent, scheduling seams, per-tick streaming budgets,
  retained tick history, JSONL diagnostics, and policy-driven scheduler ticks.
- The sample can drive camera-based terrain streaming, schedule external load
  work, import/upload dev fixture assets, reconcile completions, and rebuild
  terrain through retained engine state.
- True async IO, worker ownership, production asset packages, and engine-owned
  renderer resource lifetime policy remain future work.

## Active Priorities

1. **Asset pipeline fidelity**
   - Add normal-map sampling for skinned/static basic materials using the
     carried tangent basis and named `Normal` material texture slot.
   - Expand glTF material handling toward useful PBR fields while keeping
     renderer APIs stable and tested.

2. **Animation runtime readiness**
   - Add a minimal engine-owned renderable/animation instance layer only after
     asset and renderer submission boundaries stay stable.
   - Add playback controls, clip selection, and eventually blending/root-motion
     as engine-owned policy. Do not move gameplay animation state into renderer
     core.
   - Evaluate ozz-animation or ACL after payloads, sampling needs, and runtime
     performance constraints are clearer.

3. **Streaming and resource lifetime**
   - Turn retained external load-service and completion-inbox seams into a
     real async-ready worker integration without making the engine own threads
     prematurely.
   - Define renderer resource ownership and destruction/rollback policy for
     assets produced by loaders.
   - Keep all renderer creation/destruction on the renderer owner thread and
     outside active frames where public APIs require it.

4. **Production asset formats and packaging**
   - Move beyond dev ASCII fixtures and direct test paths toward a documented
     asset source/package convention.
   - Define shader runtime asset packaging for installed packages or document
     engine-owned shader asset responsibility.
   - Validate package-consumer builds as asset/shader conventions evolve.

5. **Scale and validation**
   - Build repeatable validation scenes for terrain, shadows, decals, SSAO,
     particles, selection, fade, weather, color grading, and animated actors in
     combination.
   - Add measured CPU/GPU timing and capture-oriented diagnostics before
     choosing culling/scalability upgrades.

## Phase Summary

### Phase 1 - Foundation

Completed at a practical level: SDL3 sample shell, bgfx initialization,
renderer lifecycle, shader compilation, mesh upload, basic material, camera,
forward rendering, directional light, gamma-correct output, resize handling,
and basic stats.

### Phase 2 - Open-World Basics

Completed foundations include terrain chunks, resource lifecycle, instancing,
frustum culling, LOD selection, texture splatting, sky/fog, cascaded shadows,
and chunk/culling debug overlays. Remaining work is hardening at scale rather
than initial feature creation.

### Phase 3 - Kenshi Parity

Foundations are present for skeletal animation, SSAO, decals, particles,
weather hooks, selection outlines, building fade, and color grading. Current
work should prioritize robustness, imported asset fidelity, animation runtime
ownership, and validation scenes over adding unrelated visual effects.

### Phase 4 - Modern Clarity

Deferred until hardening proves the need: Forward+/clustered lighting, TAA or
SMAA, volumetric fog, Hi-Z occlusion, improved vegetation, biome lighting, and
richer debug overlays.

## Hardening Gates

Before treating the renderer as production-ready for a large open-world
engine, finish or explicitly defer:

- optional-pass interaction regression coverage and resize/resource tests
- deterministic resource lifetime and stale-handle coverage across all
  resource types
- mature imported material/transparency policy, including alpha-tested shadows
  and color-space validation
- production asset pipeline validation for mesh units/axes/tangents, texture
  mips/compression/color space, material slots, skeleton hierarchy, weights,
  animation clips, and terrain data
- real engine-owned async streaming and asset IO integration
- renderer resource destruction/rollback policy for loader-created resources
- shader runtime asset/package convention
- measured large-scene performance proof with CPU/GPU timing
- broader platform/backend policy and CI coverage
- backend-safe internal texture previews and capture/debug tooling

## Engine Expansion Track

Use this track when work is explicitly about `src/engine`. The renderer remains
an embedded library consumed through public APIs.

- **E0 Boundary:** `src/engine` is separate from renderer internals.
- **E1 Core:** lifecycle/config/tick diagnostics exist.
- **E2 World/streaming:** world chunks, residency, origin policy, terrain
  snapshots, manifests, source metadata, load requests, scheduler seams, and
  diagnostics exist.
- **E3 Asset/renderer integration:** CPU asset payloads, Assimp/stb import,
  upload planning/execution, animation sampling/playback, and sample terrain/
  wolf smoke paths exist.
- **E4 Simulation/entity layer:** next when scene object identity and runtime
  animation ownership are needed.
- **E5 Editor/tooling:** build on stable runtime APIs later; keep editor state
  out of renderer core and hot engine paths.

## Suggested Next Slices

1. **Tangent contract and import**
   - Implemented: static and skinned mesh payloads carry glTF-style tangents,
     Assimp imports or explicitly generates them, upload planning copies them,
     and renderer vertex layouts validate/store them.

2. **Normal-map shader path**
   - Use named `Normal` material texture slots for static and skinned basic
     materials.
   - Keep PBR deferred; prove useful normal-map lighting first.

3. **Wolf material fidelity pass**
   - Validate base-color/normal/metallic-roughness texture slots on the wolf.
   - Add diagnostics for material slot resolution and shader use.

4. **Animation instance/renderable seam**
   - Add a small engine-owned value layer that binds skeleton/skinned mesh/
     material handles, playback instance, transform, bounds, and section draws.
   - Keep it renderer-integration-facing and CPU-testable.

5. **Async asset worker proof**
   - Replace the fake worker with a minimal externally owned worker/completion
     adapter using the existing retained inbox/reconcile contract.
