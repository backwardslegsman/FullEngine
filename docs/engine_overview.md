# Engine Overview

This repository now reserves `src/engine/` for future engine runtime work. The
engine is separate from the renderer: it owns world, asset, simulation,
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

- `core/` owns lifecycle, time, configuration, and diagnostics.
- `world/` owns chunk IDs, streaming policy, residency, persistence, and
  large-world origin rules.
- `scene/` owns transforms and renderable extraction from engine state.
- `assets/` owns runtime asset catalogs and cooked asset references.
- `jobs/` owns async loading and work scheduling.
- `renderer_integration/` maps engine state to renderer public descriptors and
  handles.

Add these directories only as real work needs them. Empty scaffolding is less
useful than a small, tested boundary.

## Relationship to `src/engine_bridge`

`src/engine_bridge/` remains a sample/testing seam used by renderer validation
and the sample app. It is useful as a reference for mapping engine-style chunk
IDs to renderer handles, but it is not the engine runtime and should not become
the reusable owner of world policy.
