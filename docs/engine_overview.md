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
  CPU-only residency registry, but not async IO, persistence, terrain resource
  ownership, renderer handles, or origin rebasing.
- `scene/` owns transforms and renderable extraction from engine state.
- `assets/` owns runtime asset catalogs and cooked asset references.
- `jobs/` owns async loading and work scheduling.
- `renderer_integration/` maps engine state to renderer public descriptors and
  handles. The current slice only contains a smoke adapter proving the engine
  target links against the renderer public API.

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
- chunk residency states: unloaded, loading, resident, and unloading
- a deterministic CPU-only chunk registry with constrained state transitions
- CPU tests for engine lifecycle and world chunk registry behavior

Still future work:

- large-world origin selection and coordinate rebasing
- renderer handle mapping for world chunks
- async loading, streaming jobs, and IO
- asset catalogs and cooked asset manifests
- scene/entity ownership, gameplay simulation, persistence, and editor tooling

## Relationship to `src/engine_bridge`

`src/engine_bridge/` remains a sample/testing seam used by renderer validation
and the sample app. It is useful as a reference for mapping engine-style chunk
IDs to renderer handles, but it is not the engine runtime and should not become
the reusable owner of world policy.
