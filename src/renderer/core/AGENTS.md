# src/renderer/core/AGENTS.md

## Core renderer responsibility

Core renderer code orchestrates renderer lifecycle, frame lifecycle, validation, pass scheduling, high-level backend calls, and shared implementation that is not specific to bgfx, terrain, resources, animation, or debug overlays.

Keep this layer free of SDL3 and game-engine concepts.

## Frame lifecycle

Renderer frame flow remains explicit:

```text
poll/input/update outside renderer
renderer.beginFrame(frameDesc)
renderer.submit(worldView or render packets)
renderer.render()
renderer.endFrame()
```

The renderer must not call arbitrary game update code.

`beginFrame` should handle per-frame reset, view setup, transient allocator reset, and debug scope start.

`submit` should accept stable render data for the current frame.

`endFrame` should submit the backend frame, collect stats, and finalize debug/profiling data.

## Pass structure

Keep rendering passes modular. Do not make the main forward pass depend on all future post effects being present.

Accept simple Phase 1 implementations, but leave clear seams for later terrain, shadows, post processing, and debug passes.

Phase 3 hardening should keep pass ordering explicit and testable. SSAO,
decals, particles, color grading, selection/outline, debug overlays, and final
present should not require unrelated optional features to be enabled.

## Validation

Validate public descriptors near the API boundary. Report recoverable resource/runtime failures using structured errors or documented failure values.

Use assertions for programmer errors only.

Open-world readiness validation should cover resize/reconfiguration, stale
handles, skipped passes, missing resources, and deterministic diagnostics.
