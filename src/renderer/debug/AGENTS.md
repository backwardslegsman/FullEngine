# src/renderer/debug/AGENTS.md

## Debug and tooling visibility

Every major renderer system should eventually provide debug visibility.

Useful overlays:

- frame stats
- draw call counts
- visible object counts
- terrain chunk bounds
- culling results
- LOD level coloring
- shadow cascades
- light clusters
- material/texture inspection
- GPU timings where available
- backend-safe internal texture previews for shadow, depth, SSAO, decal, and
  selection resources

Debug rendering must not pollute public game-facing APIs.

## Implementation rules

- Keep debug code behind clear runtime toggles or build flags.
- Do not make normal render paths depend on debug overlays.
- Prefer reusable debug primitives such as lines, boxes, text, and screen overlays.
- Keep capture/profiling hooks backend-aware but public APIs backend-neutral.
- Dear ImGui is an approved optional debug/development dependency, but must not leak into renderer public APIs or core engine-facing abstractions.

Phase 3 hardening diagnostics should explain feature interaction failures:
pass order, resource validity, resize/recreate reasons, skipped passes,
submitted/culled/rejected/rendered counts, and allocation failures.
