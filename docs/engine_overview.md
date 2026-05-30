# Engine Overview

This repository now contains the first small `src/engine/` runtime foundation.
The engine is separate from the renderer: it owns world, asset, simulation,
streaming, and scheduling policy, while the renderer remains an embedded
library consumed through public interfaces.

## Boundary

Engine code should treat the renderer as an external package boundary even when
both live in the same repository:

```cmake
target_link_libraries(my_engine PRIVATE FullRenderer::full_renderer)
```

Renderer integration code should include only public renderer headers:

```cpp
#include "full_renderer/Renderer.hpp"
```

The engine must not include renderer implementation headers, bgfx headers, SDL3
sample types, Dear ImGui types, or `src/app` demo state.

## Intended engine areas

- `core/` owns lifecycle, time, configuration, and diagnostics. The current
  slice implements deterministic initialize/shutdown/tick behavior and CPU
  tests.
- `world/` owns chunk IDs, streaming policy, residency, persistence, and
  large-world origin rules. The current slice implements chunk identity and a
  CPU-only residency registry plus double-precision origin rebasing helpers,
  but not async IO, persistence, terrain resource ownership, renderer handles,
  or renderer-submission conversion.
- `scene/` owns transforms and renderable extraction from engine state.
- `assets/` owns runtime asset catalogs and cooked asset references.
- `jobs/` owns async loading and work scheduling.
- `renderer_integration/` maps engine state to renderer public descriptors and
  handles. The current slice contains a smoke adapter and CPU-side render-space
  conversion from engine-owned double-precision world data to bounded
  single-precision relative values, plus a world render snapshot seam that
  prepares chunk status records and terrain prep records. It also contains the
  initial terrain descriptor, lifecycle, command, resource-catalog, submission,
  and handle-map seams used by the sample terrain setup.

Add these directories only as real work needs them. Empty scaffolding is less
useful than a small, tested boundary.

## Current foundation

The engine target is `full_engine`. It is built from the repository source tree
for development and tests, but it is not installed or exported as a package yet.

Implemented pieces:

- lifecycle config, result codes, diagnostics, and tick validation
- a renderer boundary object that keeps renderer public headers out of engine
  core/world headers
- stable signed 3D chunk IDs
- a CPU-only chunk metadata catalog for stable world bounds
- a coordinated world chunk set helper for registry/catalog creation and removal
- a terrain setup helper that coordinates world chunk setup with terrain
  resource metadata while leaving renderer handles and submission separate
- chunk residency states: unloaded, loading, resident, and unloading
- a deterministic CPU-only chunk registry with constrained state transitions
- a CPU-only FIFO request queue for engine-owned residency intent
- double-precision world positions, bounds, and camera-relative origin rebasing
- render-space conversion helpers with conservative float-range validation
- world render snapshots that combine chunk catalog bounds, residency, and
  render-space conversion into diagnostic records
- terrain prep records filtered from ready world render snapshot records
- terrain lifecycle planning, renderer command intent, and renderer-shaped
  descriptor intent for ready terrain chunks
- terrain resource catalogs that associate chunks with externally supplied
  renderer mesh/material/LOD/splat handles without owning those resources
- a submission adapter that calls only public terrain APIs and updates
  `ChunkTerrainHandleMap` after successful create/destroy operations
- sample app terrain wiring that enqueues debug-UI residency changes, runs them
  through the engine integration path, and removes coordinated terrain setup
  state during teardown after renderer terrain handles are destroyed
- CPU and fake-renderer tests for engine lifecycle, world ownership, terrain
  integration seams, and end-to-end terrain pipeline composition

Still future work:

- async loading, streaming jobs, and IO
- asset catalogs and cooked asset manifests
- production terrain streaming policy and editor-owned residency controls
- real engine-owned mesh/material/texture creation and lifetime policy
- renderer descriptor conversion for non-terrain draws and cameras
- scene/entity ownership, gameplay simulation, persistence, and editor tooling

## Relationship to `src/engine_bridge`

`src/engine_bridge/` remains a sample/testing seam used by renderer validation
and the sample app. It is useful as a reference for mapping engine-style chunk
IDs to renderer handles, but it is not the engine runtime and should not become
the reusable owner of world policy.
