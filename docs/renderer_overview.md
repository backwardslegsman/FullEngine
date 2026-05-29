# Renderer Overview

## Purpose

This renderer is a C++ library intended to be embedded into a future open-world
engine. SDL3 belongs in the sample app/platform shell, and bgfx belongs behind
renderer-facing backend abstractions.

## Current foundation

The current implementation includes the completed Phase 1 foundation, the
Phase 2 open-world basics, and several Phase 3 feature foundations. It is
appropriate for prototype engine integration, installed-package consumer smoke
testing through `FullRenderer::full_renderer`, and renderer validation scenes,
but it still needs the production readiness gates in
`docs/agents/roadmap.md` before it should be considered production-ready for
an open-world engine. It provides:

- a CMake C++17 project
- CMake presets for development, library-only, no-debug-UI, tests, shader
  validation, and CI-style Windows builds
- a documented public renderer lifecycle API
- a minimal core renderer state machine
- an internal bgfx backend that initializes, resets, uses compiled shaders,
  uploads renderer-owned meshes, renders a basic forward pass, and presents
- an SDL3 sample app that owns the window and event loop
- a fixed Phase 1 mesh/material submission API
- externally supplied camera view/projection matrices and directional light data
- simple renderer stats
- renderer-owned terrain chunk records that reference externally generated
  mesh/material LOD resources
- CPU frustum culling, deterministic distance-based terrain LOD selection,
  instanced terrain batch LOD metadata, terrain stats, and terrain debug
  snapshots
- renderer-owned RGBA8 textures and four-layer terrain splat materials
- renderer-facing asset contract validation for meshes, textures, materials,
  terrain chunks, and skeleton/skinned mesh descriptors
- CTest-based CPU-side lifecycle, submission, terrain math, and terrain system
  tests
- CSM receiving/casting for terrain and mesh paths, with skinned caster support
- CPU-provided skinned mesh palettes and animation debug visualization
- selection/outline, SSAO, projected decals, particles, weather hooks,
  structure fade, color grading, and scene/post diagnostics as renderer-facing
  Phase 3 foundations
- an internal debug validation preset and camera-bookmark helper used by the
  sample app for repeatable manual outline, decal, post-pass, and resize smoke
  checks

The renderer does not own game simulation state, load assets, generate height
data, stream chunks in background jobs, simulate animation/particles/weather,
or implement editor/gameplay systems.

## Core design

The renderer accepts renderable state from external systems. It does not own
game simulation, ECS data, physics, AI, networking, or editor state.

The public integration surface includes renderer initialization, shutdown,
resize, initialization-state inspection, mesh and material creation, terrain
chunk creation, render packet submission, explicit frame begin/end calls,
lightweight stats, and terrain debug snapshot queries.

## Main subdirectories

- `src/app/` - sample application and future SDL3 platform glue.
- `src/renderer/public/` - engine-facing renderer API.
- `src/renderer/core/` - frame lifecycle and renderer orchestration.
- `src/renderer/bgfx/` - bgfx backend implementation.
- `src/renderer/scene/` - camera/view descriptors, math, frustum extraction, and scene submission helpers.
- `src/renderer/resources/` - future meshes, materials, textures, shaders, and render targets.
- `src/renderer/terrain/` - terrain chunks, LOD, culling, and streaming-facing terrain records.
- `src/renderer/debug/` - debug snapshots, future overlays, stats, and visualization.

## Engine-readiness milestones

The current renderer is best described as prototype-ready, not production
open-world ready. Before engine production use, prioritize:

1. long-running resource churn tests for repeated create/destroy/reuse of
   terrain chunks, mesh/material/texture resources, skinned palettes, decals,
   particles, viewport targets, and post resources
2. a large-world precision solution beyond the current caller-rebased
   single-precision render-space policy
3. production asset pipeline validation for mesh, texture, material, terrain,
   skeleton, and shader/runtime asset contracts
4. a shader binary packaging or engine-owned runtime shader convention that is
   validated by installed package consumers
5. material/transparency maturity for imported content, especially
   alpha-tested shadow clipping, cross-family transparent ordering decisions,
   and future PBR expansion if the engine requires it
6. representative-scale performance proof with CPU timing, GPU timing where
   available, staged allocation/churn counters, and capture/debug evidence
7. culling and scalability upgrades driven by measured bottlenecks, such as
   spatial acceleration, Hi-Z/occlusion, GPU-driven culling, or caster
   acceleration
8. platform/backend CI coverage that matches the claimed support matrix
9. backend-safe internal texture previews, image/capture regression tooling,
   and debug workflows that keep backend handles private
