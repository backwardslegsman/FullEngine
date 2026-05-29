# src/renderer/AGENTS.md

## Renderer boundary

Renderer code must be usable by a future engine without depending on engine internals.

Renderer code may define handles, descriptors, render scene submission APIs, resource creation APIs, camera/light data structures, frame lifecycle APIs, and debug/profiling hooks.

Renderer code must not require ECS types, game object classes, gameplay state, physics types, AI types, editor-only types, or direct ownership of game simulation data.

The engine pushes renderable state into the renderer through documented interfaces.

## Naming

Use names that distinguish layers clearly:

- `IRenderer`: public high-level renderer.
- `RenderDevice`: lower-level graphics abstraction.
- `BgfxRenderDevice`: bgfx implementation.
- `RenderWorld`: renderer-owned scene representation.
- `RenderView`: camera plus pass settings.
- `DrawItem` or `RenderPacket`: immutable per-frame submission.
- `ResourceManager`: renderer resource ownership.
- `MaterialSystem`: material definitions and pipeline state.
- `ShaderProgram`: compiled shader program wrapper.

Avoid `GameObject`, `Entity`, or `Actor` in renderer core unless sample/demo-only.

## Renderer-wide engineering rules

- Prefer small cohesive classes.
- Prefer value-type descriptors.
- Use RAII internally for backend resources.
- Expose explicit handle destruction publicly.
- Avoid global mutable state.
- Avoid singletons unless wrapping unavoidable backend lifetime.
- Keep headers minimal.
- Separate data preparation from draw submission.
- Keep culling, LOD, and lifetime algorithms deterministic and inspectable.

## Error handling and logging

Use assertions for programmer errors and structured errors for runtime/resource failures.

Do not throw exceptions across public renderer boundaries unless the repository already uses exceptions consistently.

Do not print directly from core systems. Route logging through renderer logging categories such as `Renderer`, `Bgfx`, `Shader`, `Material`, `Mesh`, `Texture`, `Terrain`, `Culling`, `Animation`, and `Debug`.

## Performance principles

Optimize for open-world scalability in this order:

1. establish correct data ownership
2. batch draw submissions
3. reduce state changes
4. cull aggressively
5. use instancing
6. introduce LOD
7. profile
8. only then add advanced GPU-driven or occlusion systems

Do not introduce complex multithreading before the renderer data model is stable.

## Phase 3 hardening priorities

For Phase 3 and later renderer work, check `docs/agents/roadmap.md` and prefer
hardening milestones before new effects:

- pass interaction regressions and independent feature toggles
- resource lifetime, resize, stale-handle, and allocation diagnostics
- open-world culling, LOD, instancing, and stress scenes
- material/shader/animation/transparency policy cleanup
- backend-safe debug previews and performance tooling
- asset pipeline, build, and platform confidence

## Threading and lifetime

Phase 1 may be single-threaded.

Document which thread owns renderer calls. Do not imply public API thread safety unless it is implemented.

Window lifetime must exceed renderer backend lifetime. Renderer-owned resources must be destroyed before shutdown or cleaned up during shutdown.
