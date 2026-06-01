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
  and residency intent into `TerrainRuntimeState` without applying it. Per-tick
  queue budgets can defer excess setup and residency operations deterministically.
- `TerrainStreamingManifestCoordinator.hpp/.cpp` - manifest-aware bridge that
  plans asset handle readiness, queues missing asset load intent, stages
  manifest terrain setup descriptors, runs streaming policy, and queues safe
  runtime intent without applying it.
- `TerrainStreamingLoopState.hpp/.cpp` - retained synchronous holder for
  manifest load state, streaming runtime state, manifest asset-load jobs, a
  retained manifest asset-load service, and latest coordination diagnostics. It
  can packetize scheduled load jobs, progress caller-owned load callbacks,
  reconcile emitted completions back into retained manifest readiness, and keep
  a compact fixed-capacity tick history ring for budget/deferred-work
  visibility across recent frames.
- `TerrainStreamingLoopUpdate.hpp/.cpp` - synchronous tick-shaped helper that
  runs retained manifest-aware streaming coordination and then applies queued
  terrain setup/residency work through `TerrainRuntimeState::updateWithSnapshot`
  only after streaming succeeds. It can also pass per-tick queue and terrain
  lifecycle budgets into the retained streaming/runtime path.
- `TerrainStreamingBudgetPolicy.hpp/.cpp` - deterministic named budget
  profiles that select setup, residency, and lifecycle caps for the synchronous
  loop. The policy can also choose a profile from retained tick-history
  deferred-work pressure before threaded streaming policy exists.
- `TerrainStreamingSchedulerPolicy.hpp/.cpp` - deterministic single-threaded
  pacing decisions over tick-history summaries and retained loop diagnostics,
  selecting whether the caller should run streaming, asset-load jobs, both, or
  neither.
- `TerrainStreamingSchedulerTick.hpp/.cpp` - policy-driven synchronous tick
  helper that summarizes history, chooses scheduler work, and either runs load
  jobs synchronously before streaming or mirrors them into the retained job
  queue for external execution. It returns one copied result without owning
  renderer handles, resources, threads, or IO.
- `TerrainStreamingSchedulerTickDiagnostics.hpp/.cpp` - compact value
  diagnostics for scheduler tick status, decision pressure, load-job counters,
  and streaming-loop counters suitable for sample/editor display.
- `TerrainStreamingTickHistoryExport.hpp/.cpp` - standard-library JSON Lines
  export for retained streaming tick history so deferred-work and scheduler
  decision behavior can be captured from longer manual sessions.
- `TerrainStreamingTickHistoryImport.hpp/.cpp` - matching JSON Lines import for
  exported streaming tick traces, preserving file order and stored sequence
  values as value-only diagnostics, including scheduler decision fields when
  present.
- `TerrainStreamingTickHistorySummary.hpp/.cpp` - offline value summaries for
  retained or imported tick traces, including status counts, selected budget
  profile counts, request backlog peaks, and deferred-work totals/peaks.

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
execution. The retained loop state groups those pieces into one synchronous
state holder while keeping renderer handles, renderer resources, file paths,
and explicit runtime application caller-owned. The synchronous loop update
helper composes that retained state with terrain runtime queue application for
sample/runtime ticks without owning sample UI mirrors. Per-tick budgets now cap
setup adds/removes, residency transitions, and terrain renderer lifecycle work
without treating deferred work as failure. Recent tick history makes those
deferred counters visible across multiple frames. Named budget profiles now
provide a small engine-owned policy for choosing those caps automatically while
manual debug overrides remain possible, and an adaptive selector can switch
between conservative, balanced, and catch-up profiles from retained
deferred-work pressure. The retained loop diagnostics now expose that selected
profile and pressure directly for sample/editor tooling. Streaming tick history
can be exported and imported as deterministic JSON Lines for offline inspection
and round-trip tests, then summarized into compact deferred-work and budget
profile diagnostics without needing retained loop state. The scheduler policy
can now turn those summaries and current pending load/job counts into a
deterministic single-threaded pacing decision, and the scheduler tick helper
can execute that decision through the existing explicit load-job and streaming
loop seams. It can also stop at a schedule-only load-job boundary so an external
future async system can drain the generic job queue without changing manifest
load state ownership. A reconcile pass now lets caller-owned completed handle
catalogs consume that retained load state only when the whole batch is ready,
remove matching scheduled jobs, and replan readiness without owning async
execution. A completion adapter also accepts caller-owned completed job output
records and publishes their handles into a temporary completed-handle catalog
before reconcile, giving future async workers an explicit handoff-and-return
contract. The retained loop state now owns the load-service progress between
scheduled jobs and completion reconcile, so sample/editor code can trigger
external-style load work without building temporary completion vectors. A
compact diagnostics snapshot lets the sample terrain panel show that scheduler
tick without retaining or reaching through full per-phase result records.
Scheduler-driven streaming ticks annotate retained history with copied
decision/status/pressure fields, so exported traces show policy choices beside
deferred work. The panel can run that tick once or use it as the continuous
camera-streaming path while keeping lower-level manual controls available for
individual phase diagnostics. Later slices can connect the scheduled jobs to
real async IO after the CPU-side policy is proven.
