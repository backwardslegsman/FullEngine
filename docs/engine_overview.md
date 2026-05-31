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
- `streaming/` owns engine streaming policy as it grows beyond primitive world
  state. The current slice contains a CPU-only terrain planner that decides
  desired load/resident windows and emits dry-run request intent, while staying
  renderer-free and delegating renderer setup/submission to
  `renderer_integration/`.
- `scene/` owns transforms and renderable extraction from engine state.
- `assets/` owns runtime asset catalogs and cooked asset references.
- `jobs/` owns async loading and work scheduling. The current slice contains a
  deterministic single-threaded job queue/executor seam; true background
  threads, IO, and platform job handles are still future work.
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
- a CPU-only FIFO request queue for terrain setup add/remove intent
- double-precision world positions, bounds, and camera-relative origin rebasing
- render-space conversion helpers with conservative float-range validation
- world render snapshots that combine chunk catalog bounds, residency, and
  render-space conversion into diagnostic records
- terrain prep records filtered from ready world render snapshot records
- terrain lifecycle planning, renderer command intent, and renderer-shaped
  descriptor intent for ready terrain chunks
- a reusable terrain pipeline coordinator that runs the world snapshot, terrain
  prep, lifecycle, command, descriptor, submission, and diagnostics sequence
- a stateless terrain runtime controller that applies setup requests, applies
  registered residency requests, runs the terrain pipeline, and returns
  aggregate diagnostics for the caller
- a terrain runtime state object that owns pending setup/residency queues and
  the latest runtime update result, retained event diagnostics, and optional
  snapshot/diff tracking so callers can keep terrain runtime plumbing in one
  engine-owned value
- a generic asset identity catalog for renderer-free asset kind and dependency
  metadata, used as the shared convention beneath specialized asset reference
  catalogs
- generic asset dependency validation that checks declared dependency IDs and
  expected kinds against generic catalog metadata before manifest catalogs are
  accepted
- an in-memory cooked asset manifest value model that groups generic asset
  records and terrain chunk asset descriptors into validated catalogs without
  file IO or renderer-resource ownership
- cooked manifest dependency summaries that report declared asset kinds and
  unique terrain/generic asset references without validating or resolving them
- deterministic JSON Lines import/export for cooked asset manifests, used as
  lightweight schema and tooling support without introducing production asset
  loading policy
- terrain asset dependency validation that checks terrain mesh/material/texture
  references against generic asset metadata before renderer-handle resolution
- initial terrain asset identity catalogs that describe chunk mesh/material/LOD
  and optional splat-map asset references without storing renderer handles
- a terrain asset resolver that maps engine terrain asset IDs through
  externally supplied renderer handle catalogs into the existing terrain
  resource descriptors, without creating or owning renderer resources
- a batch terrain asset resolver that converts caller-selected terrain asset
  descriptors into a value-built terrain resource catalog with ordered
  per-chunk diagnostics
- terrain resource catalogs that associate chunks with externally supplied
  renderer mesh/material/LOD/splat handles without owning those resources
- a submission adapter that calls only public terrain APIs and updates
  `ChunkTerrainHandleMap` after successful create/destroy operations
- sample app terrain wiring that enqueues debug-UI residency changes, runs them
  through the engine integration path, and applies setup add/remove intent
  through `TerrainChunkRequestQueue` from single-chunk and ring-batch runtime
  debug UI controls with engine-owned last-apply setup, residency, and pipeline
  diagnostics
- dry-run terrain setup staging that compares desired manifest-derived terrain
  setup records against current world/resource state and reports add, keep,
  remove, and unsupported-change intent without applying requests
- reusable staged-plan queueing that blocks unsupported descriptor changes and
  queues safe add/remove setup intent through `TerrainRuntimeState`
- manifest-to-runtime terrain staging coordination that validates cooked
  manifest values, resolves terrain resources through externally supplied
  renderer handle catalogs, builds staging plans from caller-provided world
  descriptors, and can queue safe plans without applying them
- manifest load state that owns an imported cooked manifest value plus latest
  staging results/diagnostics, giving callers a small engine-side loading shape
  before file IO, async loading, or renderer-resource creation are introduced
- a synchronous cooked manifest file-load coordinator that imports JSON Lines
  manifest files into `TerrainManifestLoadState` and clears stale retained
  state on failure, while still avoiding async loading and renderer-resource
  creation
- a reload-and-replan coordinator that chains manifest file load, handle
  readiness planning, renderer-free load-request planning, and state-owned
  load-request queueing without consuming requests or creating renderer
  resources
- manifest asset readiness planning that compares retained manifest
  mesh/material/texture references with externally supplied renderer handle
  catalogs and reports ready or missing handle mappings without loading assets
- renderer-free manifest asset load request planning that converts missing
  readiness records into asset ID/kind load intent without IO, async jobs, or
  renderer-resource creation
- retained manifest asset load request queues that deduplicate pending
  mesh/material/texture load intent over repeated readiness scans without
  consuming requests or creating renderer resources
- a production-facing manifest asset load adapter contract that consumes
  pending load intent by copying externally supplied renderer handles into a
  runtime handle catalog with ordered diagnostics, while still avoiding IO,
  async work, renderer calls, and renderer-resource creation
- a callback-driven manifest asset load executor that asks caller-owned loader
  code for missing renderer handles, then uses the same all-or-nothing load
  adapter to consume queued load intent
- manifest load state consume diagnostics that let callers consume the
  state-owned pending load queue through the adapter while keeping source and
  destination renderer handle catalogs externally owned
- a single-threaded engine job queue that retains generic background-work
  intent, executes ready jobs through caller callbacks in deterministic
  priority order, and leaves failed or blocked jobs pending for retry
- a manifest asset-load job mirroring helper that copies pending
  mesh/material/texture load intent into generic `ManifestAssetLoad` jobs
  without consuming asset requests or mutating renderer handle catalogs
- a job-driven manifest asset load proof in CPU tests, showing pending
  load intent can be mirrored into jobs, completed through caller callbacks,
  consumed through `TerrainManifestLoadState`, and replanned as ready
  without production async or renderer-resource creation
- a production-facing single-threaded manifest asset load job coordinator that
  owns the repeated mirror, execute, consume, and readiness-replan sequence
  while keeping loader callbacks and renderer resource creation external
- a CPU-only terrain streaming planner that converts camera/world position,
  chunk size, known chunk IDs, and a terrain runtime snapshot into deterministic
  dry-run setup and residency intent without mutating runtime state
- a retained terrain streaming runtime state that stores the latest dry-run
  streaming plan and queues safe setup/residency intent into `TerrainRuntimeState`
  without applying runtime updates
- a manifest-aware terrain streaming coordinator that plans handle readiness,
  queues missing asset-load intent, stages manifest setup descriptors, and then
  feeds safe camera-window setup/residency intent into the streaming runtime
- manifest runtime staging diagnostics that expose coordinator status, asset
  resolution counters, staging counters, and queued-request counters as
  reusable value snapshots
- reusable terrain integration diagnostics that snapshot setup requests,
  asset batch resolves, residency requests, pipeline counters, and
  renderer-submission outcomes as value-only engine data for debug UI or
  tooling surfaces
- deterministic JSON Lines import/export for terrain runtime events and
  snapshot diffs without adding a third-party JSON dependency
- CPU and fake-renderer tests for engine lifecycle, world ownership, terrain
  integration seams, and end-to-end terrain pipeline composition

## Terrain runtime milestone

The terrain runtime path is now an engine-owned composition layer rather than
sample-only glue. `TerrainIntegrationDiagnostics` captures setup, residency,
pipeline, and renderer-submission counters as value snapshots.
`runTerrainPipeline` owns the ordered translation from world chunk state to
renderer terrain submission. `updateTerrainRuntime` applies setup requests,
filters and applies registered residency requests, runs the pipeline, clears
handled queues, and returns one aggregate update result. `TerrainRuntimeState`
wraps those queues and the latest result for callers that want a compact
runtime holder with `hasPendingRequests()` dirty-state checks and a compact
fixed-capacity event log of recent update diagnostics, without transferring
ownership of registries, catalogs, renderer handles, or renderer resources.
The retained event log can be inspected by the sample debug UI and exported as
deterministic JSON Lines for lightweight tooling or bug reports. The sample can
also export the latest retained snapshot diff. Matching standard-library
importers read those exported schemas back into value snapshots for tests and
future diagnostics tooling.
`TerrainRuntimeStateSnapshot` provides the same kind of value-only status
surface for chunk setup, residency, resources, and terrain handle readiness, so
sample and editor UI can query one engine-owned summary instead of probing each
owner independently. The sample diagnostics panel displays those readiness
counters directly alongside asset batch resolve, request, and
renderer-submission counters. `TerrainRuntimeStateDiff` compares two snapshots
and reports added, removed, and changed chunk state in deterministic order.
`TerrainRuntimeState` can opt into that tracking during an update, storing the
latest snapshot and latest diff against the previous tracked snapshot without
changing the plain update path. Snapshot diffs can also be exported and
imported as deterministic JSON Lines, matching the event diagnostics tooling
while keeping the data CPU-only, value-owned, and independent of renderer
resources or sample UI state.

The sample still owns demo UI state and renderer mesh/material/texture
creation. It declares sample terrain asset IDs, generic asset metadata, and
terrain chunk asset references through an in-memory cooked manifest, validates
generic asset dependencies, builds generic and terrain asset catalogs from that
manifest, validates terrain asset dependencies, resolves them through
externally supplied renderer handle catalogs into a terrain resource catalog,
queues setup and residency intent
through `TerrainRuntimeState`, updates that state before renderer frame
submission, mirrors display state from engine registries, displays the retained
runtime snapshot/diff in its debug panel, exports event/diff diagnostics on
demand, displays and exports its generated cooked manifest for schema/tooling
inspection, validates that exported manifest back through the engine importer
and catalog builder, stores the imported value in `TerrainManifestLoadState`,
reports renderer-handle readiness for declared mesh/material/texture
references, converts missing handle readiness into renderer-free load intent
counts, retains pending load intent in a deduplicated queue, can consume that
intent once externally supplied handles are available, exposes the latest load
consume counters in the debug panel, dry-runs a terrain setup staging plan
through the reusable manifest staging coordinator, can queue safe add/remove
staging operations through the terrain runtime controller, can drive
setup/residency intent from the camera through the manifest-aware streaming
coordinator debug toggle, can run the manifest asset load job coordinator from
the debug panel using its sample-created renderer handles as the external load
callback source, and submits the mapped renderer terrain handles in its render
packet.

Still future work:

- async loading, streaming jobs, and IO
- a streaming runtime loop that sequences manifest load state, asset load
  requests, terrain setup staging, residency requests, and terrain runtime
  updates without sample-owned orchestration
- production cooked manifest formats, importers, and renderer-resource
  creation policy
- production terrain streaming policy and editor-owned residency controls
- real engine-owned mesh/material/texture creation and lifetime policy
- renderer descriptor conversion for non-terrain draws and cameras
- scene/entity ownership, gameplay simulation, persistence, and editor tooling

## Relationship to `src/engine_bridge`

`src/engine_bridge/` remains a sample/testing seam used by renderer validation
and the sample app. It is useful as a reference for mapping engine-style chunk
IDs to renderer handles, but it is not the engine runtime and should not become
the reusable owner of world policy.
