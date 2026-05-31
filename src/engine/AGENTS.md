# src/engine/AGENTS.md

## Purpose

This directory is the home for future engine runtime code. It is separate from
the renderer and should use the renderer as an embedded library through its
public API.

The engine owns game/runtime policy: world state, streaming decisions, asset
catalogs, simulation, jobs, persistence, and the translation from engine data
to renderer-facing frame submissions.

## Intended structure

Use or evolve toward this layout incrementally:

```text
src/engine/
  AGENTS.md
  core/                  # lifecycle, time, configuration, diagnostics
  world/                 # chunks, streaming policy, large-world origin rules
  streaming/             # runtime streaming policy and load/residency planning
  scene/                 # transforms and renderable extraction
  assets/                # runtime asset catalogs and cooked asset ownership
  jobs/                  # async loading and work scheduling
  renderer_integration/  # renderer public API adapter and frame submission
```

Do not create empty subsystems just to match the layout. Add directories when a
requested change needs them.

## Dependency rules

- Engine code may depend on `FullRenderer::full_renderer`.
- Only `renderer_integration/` may include `full_renderer/*` headers.
- Engine code must not include files from `src/renderer/core`,
  `src/renderer/bgfx`, `src/renderer/scene`, `src/renderer/resources`,
  `src/renderer/terrain`, `src/renderer/animation`, or `src/renderer/debug`.
- Engine code must not depend on bgfx, SDL3, Dear ImGui, or sample app types.
- Streaming policy should live in `streaming/` once it needs retained runtime
  state or multi-system coordination; low-level chunk identity and residency
  primitives remain in `world/`.
- Renderer code must not depend on `src/engine/`.
- Shared behavior needed by both engine and renderer should be expressed
  through public renderer APIs, docs, or a deliberately separate utility layer;
  do not reach into renderer internals.

## Engine ownership

The engine owns:

- absolute world coordinates and origin-shift policy
- chunk IDs, residency, streaming priorities, and persistence
- desired streaming windows, load/residency intent, and streaming diagnostics
- entity or scene object identity
- simulation, animation state, gameplay state, selection state, and weather
  policy
- asset catalogs, cooked asset manifests, and runtime shader asset packaging
- background IO and job scheduling

The renderer owns only renderer resources and per-frame rendering work exposed
through public renderer descriptors and handles.

## Renderer integration

The engine should translate engine state into renderer descriptors at an
explicit boundary. Keep that adapter narrow and testable:

- convert engine world coordinates to renderer-relative frame coordinates
- map engine asset IDs and chunk IDs to renderer resource handles
- submit only stable frame data between renderer `beginFrame` and `endFrame`
- treat non-success renderer results as lifecycle, validation, or resource
  errors to be reported through engine diagnostics

Do not move renderer validation harnesses or sample-only UI into the engine.
`src/engine_bridge/` remains a sample/testing adapter until replaced by real
engine code under this directory.

## Testing expectations

Prefer CPU-side tests for engine policy:

- chunk residency and streaming decisions
- origin selection and coordinate rebasing
- asset catalog lookup and missing-asset behavior
- renderer-handle mapping lifetime
- deterministic scheduling or job-queue behavior where practical

Tests should not require a GPU unless the requested change is explicitly an
integration or sample validation task.
