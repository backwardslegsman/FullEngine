# Engine Jobs Skeleton

This directory owns engine-side scheduling primitives. It must stay renderer-free:
no `full_renderer/*`, bgfx, SDL3, Dear ImGui, sample app types, or renderer
implementation headers belong here.

## Current slice

- `JobQueue.hpp/.cpp` defines a deterministic, single-threaded queue for copied
  engine work intent.
- Jobs have stable caller-owned IDs, coarse kind/priority fields, and opaque
  numeric payload slots.
- `runReadyJobs` executes pending jobs synchronously through a caller-provided
  callback, removes completed jobs, and leaves failed or blocked jobs pending
  for retry.

The queue does not create threads, perform file IO, sleep, call renderer APIs,
create resources, or own platform job handles. It is the first seam for future
streaming work to become schedulable without committing to an async runtime yet.

## Future direction

Later slices can add async executors, cancellation, dependencies, budgets, and
file/asset loading policies on top of these value-owned requests once the
single-threaded lifetimes and diagnostics are proven.
