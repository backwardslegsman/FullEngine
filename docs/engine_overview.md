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
- renderer-free asset source catalogs that map loadable mesh/material/texture
  asset IDs to opaque caller-supplied source URIs and typed renderer-free
  mesh/material/texture source descriptors, plus request-order lookup
  diagnostics for mapping missing load intent to those source records
- renderer-free loaded asset payloads for mesh, texture, and material data,
  including mesh UV0 and named material texture slots, giving future importers
  a copied CPU data contract before renderer upload, handle creation, async IO,
  or renderer-resource ownership
- a dev-only loaded asset importer that reads tiny tracked ASCII mesh,
  texture, and material fixtures into `LoadedAssetPayload` values, proving the
  first real source-file-to-payload path without adopting a production asset
  format
- an Assimp-backed loaded asset importer that reads static glTF mesh files into
  the same renderer-free `LoadedAssetPayload` mesh contract, aggregating
  multi-mesh scenes in deterministic order, optionally generating missing
  normals, requiring or explicitly defaulting UV0, copying vertex colors, and
  validating source descriptors and payload data while leaving tangents, UV1+,
  skeletal meshes, animations, async IO, and renderer-resource creation to
  later slices
- an stb-backed direct texture image importer that reads image files into the
  renderer-free `LoadedTextureAsset` contract as tightly packed single-mip
  RGBA8 bytes, validating source descriptors and payload data while leaving
  glTF image references, embedded images, mip generation, compression, async
  IO, and renderer-resource creation to later slices
- a glTF material/image reference extractor that maps base-color, normal,
  metallic-roughness, occlusion, and emissive image references into texture
  `AssetSourceRecord`s, emits `LoadedMaterialAsset` payloads with named texture
  asset slots, and leaves actual image decoding/upload to the existing stb and
  renderer-integration paths
- upload-intent planning that translates mapped source descriptors into public
  renderer mesh/texture/material upload expectations without source bytes,
  renderer handles, renderer calls, or resource creation
- loaded-payload upload planning that translates validated CPU mesh/texture
  payloads into owned renderer descriptor work, including UV0, and material
  payloads into named texture-slot upload expectations without invoking
  renderer APIs or creating resources
- loaded-payload upload execution that consumes planned mesh/texture/material
  work through caller-owned public renderer `createMesh`, `createTexture`, and
  `createMaterial` calls, resolving named material texture asset IDs through
  `RendererAssetHandleCatalog` before recording successful handles
- a dev manifest asset-load callback that resolves retained source metadata,
  imports tiny dev mesh/texture/material files, executes caller-owned renderer
  uploads, and publishes handle completions through the existing retained
  service and inbox/reconcile paths
- retained manifest asset source planning that stores caller-supplied source
  catalogs on `TerrainManifestLoadState` and exposes mapped/missing source
  counters plus renderer upload-intent counters through loop diagnostics
  without changing loader or streaming policy
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
- a schedule-only manifest asset-load job boundary that mirrors pending load
  intent into generic jobs without executing callbacks or consuming retained
  requests, giving future async loaders an explicit handoff point
- a manifest asset-load work-packet helper that decodes scheduled
  `ManifestAssetLoad` jobs into caller-owned request packets for external
  workers without executing jobs or touching renderer handles
- a retained manifest asset-load service that owns copied work packets across
  ticks, advances them through caller-owned callbacks, and emits completion
  records for the existing publish/reconcile path without owning workers, IO,
  or renderer resource creation
- retained streaming loop ownership of that asset-load service, so scheduled
  external load jobs can be packetized, progressed, and reconciled from engine
  loop state instead of temporary sample-side vectors
- retained asset-load service input diagnostics that report whether queued
  service work has mapped source metadata and renderer upload-intent coverage
  before caller-owned callbacks or workers attempt to produce handles
- an external manifest asset-load job reconcile pass that consumes retained
  load requests only after caller-owned completed handles satisfy the whole
  batch, removes matching scheduled jobs, and replans readiness
- an external manifest asset-load job completion adapter that publishes
  caller-owned completion records into a temporary completed-handle catalog
  before reconcile, giving async/threaded loaders an explicit handoff shape
  without making the engine own workers, IO, or renderer resource creation
- a retained external completion inbox with explicit remove/replace operations
  so stale worker outputs can be discarded or superseded deterministically
  before the all-or-nothing reconcile pass
- a test-only external worker completion proof that reads scheduled
  `ManifestAssetLoad` jobs, emits completion records from caller-owned handles,
  reconciles them, and replans retained manifest readiness as ready
- reusable manifest asset-load job diagnostics that copy coordinator status,
  pending job counts, mirror/execution/consume/reconcile counters, and final
  readiness counters for debug UI or tooling without retaining job or load
  records
- a CPU-only terrain streaming planner that converts camera/world position,
  chunk size, known chunk IDs, and a terrain runtime snapshot into deterministic
  dry-run setup and residency intent without mutating runtime state
- a retained terrain streaming runtime state that stores the latest dry-run
  streaming plan and queues safe setup/residency intent into `TerrainRuntimeState`
  without applying runtime updates
- a manifest-aware terrain streaming coordinator that plans handle readiness,
  queues missing asset-load intent, stages manifest setup descriptors, and then
  feeds safe camera-window setup/residency intent into the streaming runtime
- a retained synchronous terrain streaming loop state that groups manifest load
  state, streaming runtime state, manifest asset-load jobs, and latest
  coordination diagnostics while keeping renderer handles and resources
  externally owned
- a synchronous terrain streaming loop update helper that runs retained
  manifest-aware streaming coordination and applies queued terrain runtime
  setup/residency work only after streaming succeeds
- per-tick terrain streaming budgets that defer excess setup add/remove,
  residency transition, and terrain renderer lifecycle work with deterministic
  counters instead of treating budget exhaustion as failure
- a retained streaming tick history ring that stores recent streaming, queue,
  runtime, lifecycle, and submission counters so deferred work can be inspected
  across several frames
- deterministic JSON Lines export/import for retained streaming tick history,
  matching the existing terrain runtime diagnostics tooling for longer manual
  sessions and offline trace tests
- offline streaming tick-history summaries that report status counts, selected
  budget profile counts, request backlog peaks, and deferred-work totals/peaks
  from retained or imported traces without mutating runtime state
- a deterministic streaming scheduler policy that reads tick-history summaries
  and retained loop diagnostics, then selects whether to run streaming,
  manifest asset-load jobs, both, or neither for a future single-threaded tick
- a policy-driven synchronous streaming scheduler tick helper that executes the
  selected load-job and streaming-loop phases through explicit caller-owned
  callbacks and state, or schedules load jobs for external execution while
  leaving true async/threaded scheduling as future work
- compact scheduler tick diagnostics that copy selected decision, pressure,
  load-job, and streaming-loop counters for sample/editor display without
  retaining full tick records
- retained streaming tick history now carries scheduler decision/status and
  pressure fields beside deferred work, and the JSONL trace round-trips those
  fields for longer-session inspection
- sample debug wiring that can run the scheduler tick once or use it for
  continuous camera-driven streaming, while retaining lower-level manual
  streaming and load-job controls for inspection
- sample debug wiring for the schedule-only load path, including an external
  load scheduling toggle and a reconcile action that drives the retained load
  service with sample-created renderer handles before consuming retained load
  requests
- a simple terrain streaming budget policy that selects deterministic setup,
  residency, and lifecycle caps from named runtime profiles, plus an adaptive
  selector that chooses a profile from retained tick-history pressure before
  threaded streaming policy exists
- retained terrain streaming loop diagnostics that expose the latest adaptive
  budget profile and deferred-work pressure without requiring tools to rescan
  tick history
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
and catalog builder, stores the imported value in `TerrainStreamingLoopState`,
reports renderer-handle readiness for declared mesh/material/texture
references, converts missing handle readiness into renderer-free load intent
counts, can map that intent to typed asset source URI/descriptor records, retains pending load
intent in a deduplicated queue, can consume that intent once externally
supplied handles are available, displays retained/mapped/missing source counts
beside load intent diagnostics, exposes the latest load
consume counters in the debug panel, dry-runs a terrain setup staging plan
through the reusable manifest staging coordinator, can queue safe add/remove
staging operations through the terrain runtime controller, can drive
setup/residency intent from the camera through the retained streaming loop
update helper, can select adaptive automatic streaming budget profiles from
recent deferred-work pressure or use manual debug budget overrides, can run the
manifest asset load job coordinator from
the debug panel using caller-owned callback sources, can optionally drive
camera streaming through the policy-driven scheduler tick, can run that
scheduler load phase through the retained load-service path or an
external-completion handoff, includes a fake external worker panel that imports
tracked dev asset fixture files, uploads them through the caller-owned
renderer, and emits completion records into a retained engine-owned inbox for
that handoff, can run a one-click dev asset smoke that resets terrain setup,
loads fixture-backed mesh/texture/material assets, reconciles handles, and
rebuilds terrain through the streaming loop, displays retained
load-service and completion-inbox progress through compact diagnostics
snapshots, exposes worker-facing helpers for publishing completion batches
without depending on streaming loop state, can export retained streaming tick history, and
submits the mapped renderer terrain handles in its render packet.

Still future work:

- async loading, streaming jobs, and IO
- a scheduler that consumes selected budget profiles or offline summary tooling
  for imported streaming tick traces
- production cooked manifest formats, richer glTF material graph import,
  skeletal/animated mesh import, packed assets, and renderer-resource creation
  policy
- production terrain streaming policy and editor-owned residency controls
- real engine-owned mesh/material/texture creation and lifetime policy
- production material import/rendering policy beyond the current basic and
  terrain-splat descriptor bridge
- renderer descriptor conversion for non-terrain draws and cameras
- scene/entity ownership, gameplay simulation, persistence, and editor tooling

## Relationship to `src/engine_bridge`

`src/engine_bridge/` remains a sample/testing seam used by renderer validation
and the sample app. It is useful as a reference for mapping engine-style chunk
IDs to renderer handles, but it is not the engine runtime and should not become
the reusable owner of world policy.
