# tests/AGENTS.md

## Testing expectations

Add CPU-side tests where practical for:

- handle allocators
- resource registries
- math helpers
- frustum extraction
- AABB/frustum tests
- LOD selection
- terrain chunk indexing
- material descriptor validation
- shader path resolution

Do not require GPU availability for all tests.

For rendering behavior, prefer small sample scenes and screenshot/manual validation until automated image tests exist.

Phase 3 hardening tests should prioritize:

- regression coverage for prior feature interaction bugs
- optional pass disable behavior and final presentation planning
- repeated resize/reconfiguration planning
- stale/invalid/destroyed handle behavior
- deterministic culling, sorting, LOD, and post-pass planning
- resource diagnostics aggregation and allocation-failure fallback paths
- stress-scene helpers that do not require GPU availability where practical

## Test style

- Keep tests deterministic.
- Avoid depending on local absolute paths.
- Prefer small focused tests over large integration-only tests.
- Add regression tests for fixed bugs when practical.
- Keep CPU-side tests independent of SDL3 and bgfx unless the test is explicitly for integration.
