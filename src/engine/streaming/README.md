# Engine Streaming Runtime Skeleton

This directory is reserved for engine-owned streaming policy and runtime-loop
coordination. It should stay renderer-free: no `full_renderer/*`, bgfx, SDL3,
Dear ImGui, sample app types, or renderer implementation headers belong here.

## Intended responsibility

The streaming layer will decide *what should be loaded or resident*, not how the
renderer draws it. It should coordinate existing engine-owned pieces:

- world chunk IDs, bounds, residency, and origin policy from `world/`
- cooked manifest and asset load state from `assets/` and
  `renderer_integration/`
- future async IO/job scheduling from `jobs/`
- renderer-facing setup and residency requests through
  `renderer_integration/`

`jobs/` currently provides a deterministic single-threaded queue/executor seam.
Streaming systems can mirror pending asset-load intent into jobs before a real
threaded loader exists.

## Implemented files

- `TerrainStreamingPlanner.hpp/.cpp` - CPU-only desired chunk window planning
  from camera/world position, chunk size, load radius, resident radius, known
  chunk IDs, and a terrain runtime state snapshot. It emits dry-run setup and
  residency intent without mutating runtime owners.
- `TerrainStreamingRuntime.hpp/.cpp` - retained streaming state that stores
  the latest dry-run plan, compact queue diagnostics, and can queue safe setup
  and residency intent into `TerrainRuntimeState` without applying it.
- `TerrainStreamingManifestCoordinator.hpp/.cpp` - manifest-aware bridge that
  plans asset handle readiness, queues missing asset load intent, stages
  manifest terrain setup descriptors, runs streaming policy, and queues safe
  runtime intent without applying it.

## Planned files

Add these only when an implementation slice needs them:

- `TerrainStreamingDiagnostics.hpp/.cpp` - compact value snapshots for planner
  and runtime-loop counters, suitable for sample/editor UI.
- `StreamingRequests.hpp/.cpp` - generic future request queues if terrain-only
  setup/residency requests need to become shared streaming primitives.

## First milestone shape

The first real slice is planning-only:

1. Read current camera/world position and known terrain chunk IDs.
2. Compute desired load and resident sets.
3. Compare those sets to `TerrainRuntimeStateSnapshot` or equivalent current
   state.
4. Emit add/remove setup intent and make-resident/make-unloaded residency
   intent as a dry-run plan in deterministic order.
5. Do not mutate registries, catalogs, resource handles, renderer state, or
   async jobs.

The retained runtime state can queue safe plan intent through
`TerrainRuntimeState`. The manifest coordinator connects retained manifest
values and asset-load intent to that streaming plan. Pending manifest asset-load
requests can now be mirrored into generic jobs for deterministic callback
execution; later slices can connect those callbacks to real async IO after the
single-threaded CPU-side policy is proven.
