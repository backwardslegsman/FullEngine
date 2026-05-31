# Renderer And Engine Architecture Guidance

## Layering rule

The renderer must be usable by a future engine without depending on engine internals.

Renderer code may define:

- handles
- descriptors
- render scene submission APIs
- resource creation APIs
- camera and light data structures
- frame lifecycle APIs
- debug and profiling hooks

Renderer code must not require:

- a specific ECS
- game object classes
- gameplay state
- physics types
- AI types
- editor-only types
- direct ownership of game simulation data

The engine pushes renderable state into the renderer through documented interfaces.

The real engine runtime lives under `src/engine/`. The renderer must not depend
on it. Engine code may depend on the installed/public renderer package target
`FullRenderer::full_renderer`, and renderer integration code should include only
public `full_renderer/*` headers.

## Recommended repository layout

```text
/
  AGENTS.md
  PLANS.md
  CMakeLists.txt
  cmake/
  external/
  assets/
    shaders/
    textures/
    meshes/
  tools/
    shader_compile/
    asset_pipeline/
  src/
    app/                 # SDL3 shell, sample app, platform glue
    engine/              # future engine runtime, separate from renderer
      core/              # lifecycle, time, configuration, diagnostics
      world/             # chunks, streaming policy, origin rules
      streaming/         # runtime streaming policy and load/residency planning
      scene/             # transforms and renderable extraction
      assets/            # runtime catalogs and cooked asset ownership
      jobs/              # async loading and work scheduling
      renderer_integration/
                          # adapter using renderer public APIs only
    renderer/
      public/            # engine-facing renderer headers
      core/              # renderer implementation
      bgfx/              # bgfx-specific backend code
      scene/             # render scene, cameras, lights, draw data
      resources/         # meshes, materials, textures, shader programs
      terrain/           # terrain chunks, splatting, LOD
      animation/         # skeletal animation later
      debug/             # debug draw, overlays, captures
    engine_bridge/       # optional sample/testing adapter layer only
  tests/
  docs/
```

If the current repository differs, preserve working structure and migrate incrementally.

## Layer responsibilities

### `src/app/`

Owns sample application behavior, SDL3 window creation, event polling, input, resize handling, and demo camera controls.

The sample app demonstrates integration. It must not become the engine. It may
depend on both the renderer and engine for demos, but SDL3 and Dear ImGui stay
in the app/debug tooling layer unless explicitly approved for a platform/tool
boundary.

### `src/engine/`

Contains future engine runtime systems. Engine code owns world state, streaming
policy, asset catalogs, jobs, simulation, persistence, and the translation from
engine-owned data into renderer-facing descriptors.

Engine code should use the renderer as a package/library boundary:

- link `FullRenderer::full_renderer`
- include only public `full_renderer/*` headers from `renderer_integration/`
- keep bgfx, SDL3, Dear ImGui, sample app types, and renderer internals out of
  engine subsystems
- keep `src/engine/` independent from `src/engine_bridge/`

Streaming runtime policy should be split once it grows beyond primitive world
state. `world/` owns chunk identity, bounds, and residency state transitions;
`streaming/` should own desired load/resident windows, prioritization,
request-plan generation, and coordination with manifest/asset load state. It
must remain renderer-free and feed renderer-facing work only through
`renderer_integration/`.

### `src/renderer/public/`

Contains engine-facing renderer APIs. These APIs should be stable, documented, and independent of bgfx and SDL3 types whenever practical.

### `src/renderer/core/`

Contains renderer orchestration, frame lifecycle, pass scheduling, validation, and implementation shared across backends.

### `src/renderer/bgfx/`

Contains bgfx backend implementation details. Public renderer layers should not leak bgfx handles or flags except through explicitly low-level extension APIs.

### `src/renderer/scene/`

Contains renderer-facing scene/view data: cameras, lights, draw packets, visibility inputs, and frame submission models.

### `src/renderer/resources/`

Owns renderer resources such as meshes, textures, materials, shader programs, render targets, and instance buffers.

### `src/renderer/terrain/`

Owns terrain chunk render resources, chunk bounds, LOD selection, culling integration, and streaming-friendly chunk lifetimes.

### `src/renderer/animation/`

Owns renderer-facing animation resource types and GPU skinning path when Phase 3 work begins. Pose evaluation may remain outside the renderer.

### `src/renderer/debug/`

Owns debug draw, overlays, capture hooks, and debug visualization. Debug tools must not pollute public game-facing APIs.

### `src/engine_bridge/`

Optional sample/testing adapter layer. It may translate hypothetical engine data
into renderer data for validation harnesses, but it is not the real engine.
Renderer core and `src/engine/` must not depend on it for reusable runtime
behavior.

## Dependency direction

Keep dependencies one-way:

```text
src/renderer/          -> no engine, app, SDL3 sample, or editor dependencies
src/engine/            -> renderer public API/package only
src/engine_bridge/     -> sample/testing adapter; no renderer internals
src/app/               -> platform/sample shell; may compose renderer and engine
```

## Naming conventions

Use names that distinguish layers clearly:

- `Renderer` or `IRenderer`: public high-level renderer.
- `RenderDevice`: lower-level graphics device abstraction.
- `BgfxRenderDevice`: bgfx implementation.
- `RenderWorld`: renderer-owned scene representation.
- `RenderView`: camera plus view/projection and pass settings.
- `RenderPacket` or `DrawItem`: immutable per-frame draw submission.
- `ResourceManager`: owns renderer resources.
- `MaterialSystem`: material definitions and pipeline state.
- `ShaderProgram`: compiled shader program wrapper.

Avoid names such as `GameObject`, `Entity`, or `Actor` inside renderer core unless they are sample/demo-only.

## Frame lifecycle

Renderer frame flow should remain explicit:

```text
poll/input/update outside renderer
renderer.beginFrame(frameDesc)
renderer.submit(worldView or render packets)
renderer.render()
renderer.endFrame()
```

The renderer should not call arbitrary game update code.

`beginFrame` handles per-frame reset, view setup, transient allocator reset, and debug scope start.

`submit` accepts stable render data for the current frame.

`endFrame` submits the backend frame, collects stats, and finalizes debug/profiling data.

## Performance order

Optimize for open-world scalability in this order:

1. establish correct data ownership
2. batch draw submissions
3. reduce state changes
4. cull aggressively
5. use instancing
6. introduce LOD
7. profile
8. add advanced GPU-driven or occlusion systems only when justified

Do not introduce complex multithreading before the renderer data model is stable.

## Engine-readiness gates

Before treating the renderer as ready for production open-world engine use,
harden these areas in the order tracked by `docs/agents/roadmap.md`:

1. correctness of feature interactions and independent pass disable paths
2. backend resource lifetime, resize, stale handle, allocation diagnostics, and
   long-session create/destroy/reuse stability
3. terrain/instance/skinned/decal/particle culling, large-scene stress cases,
   and engine-owned streaming/residency boundaries
4. large-world precision policy, either caller-rebased camera-relative
   submission or an explicit origin-shift descriptor
5. material, shader variant, animation bounds, transparency, and asset
   color-space policies for imported production content
6. runtime shader asset packaging responsibility for installed engine
   consumers
7. backend-safe internal texture previews, GPU timing, capture diagnostics, and
   validation scenes
8. asset pipeline validation tooling, build presets, package export, and
   platform/backend CI coverage matching the claimed support matrix
