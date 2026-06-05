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

- Assimp imports static meshes, mesh-bone skeletons, skinned meshes, material
  section metadata, and animation clips from glTF into renderer-free CPU
  payloads. It also has an opt-in FBX characterization path that can import
  skeleton and animation clip payloads from animated scene nodes when no mesh
  skin bones/weights are available, plus a rigid node-attached prototype that
  imports static mesh pieces driven by those sampled node transforms.
- stb imports direct image files into single-mip RGBA8 CPU texture payloads.
- Loaded payloads can plan and execute renderer uploads for mesh, texture,
  material, skeleton, and skinned mesh resources through caller-owned renderer
  APIs.
- CPU animation sampling and retained playback produce borrowed
  `SkinningPaletteDesc` views for frame-local animated submission.
- The debug-ui sample has a manually verified wolf smoke: real wolf glTF
  skeleton, skinned mesh sections, animation clip, extracted materials,
  decoded textures, uploaded renderer resources, section draws, UV0 texture
  sampling, tangent payload/import/upload coverage, basic normal-map sampling,
  material slot resolution diagnostics for raw/extracted/imported/resolved
  base-color/normal/metallic-roughness/occlusion/emissive refs, and
  deterministic focus/diagnostics. Current wolf material limits are authored
  fixture limits: the glTF declares base-color texture keys only for some
  materials and no normal or metallic-roughness texture keys. Smoke run/clear
  actions are deferred until after `endFrame` so
  skeleton/skinned resource lifecycle calls obey the public no-active-frame
  contract.

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
   - Expand glTF material handling toward useful PBR fields now that
     base-color and normal-map slots can affect basic shading.
   - Keep renderer APIs stable and tested while adding material fidelity.
   - Long-term validation target: render the Intel GPU Research Sponza scene
     with a full PBR animated knight. Those large local assets are ignored and
     should remain optional until packaging/asset-download policy exists.
   - FBX support starts with Assimp characterization probes for those optional
     assets before changing coordinate, skeleton, animation, or material import
     policy.

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
- optional full-scene PBR validation using Sponza plus an animated knight
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
   - Implemented: static, instanced, and skinned basic materials sample the
     named `Normal` texture slot through imported tangent bases.

3. **Wolf material fidelity pass**
   - Implemented: the wolf smoke reports authored and resolved material slots,
     distinguishes shader-active base-color/normal sections from future PBR
     slots, audits raw glTF slot keys versus extractor output, and tests that
     wolf material refs resolve through upload.

4. **Animation instance/renderable seam**
   - Add a small engine-owned value layer that binds skeleton/skinned mesh/
     material handles, playback instance, transform, bounds, and section draws.
   - Keep it renderer-integration-facing and CPU-testable.

5. **Async asset worker proof**
   - Replace the fake worker with a minimal externally owned worker/completion
     adapter using the existing retained inbox/reconcile contract.

6. **FBX characterization and node animation import**
   - Implemented as a CPU-only Assimp scene probe with optional local Sponza
     and animated knight smoke diagnostics plus deterministic JSON report
     formatting for ignored local outputs under `out/diagnostics`.
   - Current optional probe results: Sponza parses as 405 static meshes with
     UV0/tangents/material textures but exceeds the current 16-bit mesh/index
     contract by a wide margin; the Intel knight FBX files expose animation
     channels that match scene nodes, but no Assimp mesh bones/weights through
     the current probe flags.
   - Opt-in `AnimatedSceneNodes` import can now produce CPU skeleton and
     animation clip payloads from animated scene nodes with identity inverse
     bind poses. This is useful for animation characterization and future
     controller work, but it is not skinning-ready without real skin weights
     and bind-pose data.
   - Implemented skin-data availability audit across practical Assimp flag
     configurations. Local Knight audit result: all tested configs parse, but
     none expose mesh bones, weights, or bind-pose offset matrices; five static
     mesh nodes sit under/match animated node candidates. The current
     recommendation is rigid node-attached mesh prototype or source conversion,
     not true Assimp skin recovery.
  - Implemented rigid node-attached import and draw building: Assimp can import
     node-derived skeleton/clip payloads plus static mesh attachments, upload
     those mesh payloads through existing upload planning/execution, sample the
     clip, and build ordinary static `DrawItem`s from sampled node model
     matrices. This is useful for Knight-style rigid animation experiments but
     remains explicitly separate from true skinned FBX recovery.
   - Implemented debug-ui Knight rigid smoke: when the optional ignored Knight
     FBX is present, the sample can import the node skeleton/clip plus rigid
     mesh attachments, upload attachment meshes, sample animation on the CPU,
     and submit static draws driven by sampled node transforms. Run/clear
     actions are deferred until after `endFrame`; cleanup destroys only
     smoke-owned meshes. Filtered imported attachments are now reported with
     source mesh/node names, upload-plan status, and mesh-contract diagnostics
     such as degenerate triangle counts so partial visibility is explainable.
   - Use the next visual results to choose the next FBX branch:
     transform/unit/axis normalization if the rigid Knight is visible but
     mis-scaled or mis-oriented, source conversion/dedicated FBX parser
     evaluation for true skinning, 32-bit/sectioned static mesh import for
     Sponza, or material extraction.
