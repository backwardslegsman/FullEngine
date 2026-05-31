# PLANS.md

Use this file as a working template for non-trivial renderer tasks. Keep plans short, factual, and update them as implementation details become clearer.

## Plan template

### Goal

What is being changed and why.

### Scope

Files/directories likely involved.

### Roadmap phase

Which renderer phase or Engine Expansion Track milestone from
`docs/agents/roadmap.md` this belongs to. If the task does not fit the roadmap,
state that explicitly.

For streaming-runtime work, identify whether the slice belongs to the E3.5
streaming loop track: planning-only, retained streaming state, manifest/asset
load coordination, jobs/async integration, or sample/editor diagnostics.

### Engine-readiness milestone

For Phase 3 and later work, identify the relevant hardening milestone from
`docs/agents/roadmap.md`: correctness/pass interactions, resource lifetime and
engine boundary, open-world scale systems, materials/shaders/animation/
transparency, diagnostics/tooling, or asset/build/platform confidence.

### Production-readiness impact

For open-world or engine-facing changes, state whether the work affects
prototype integration, production open-world readiness, build/package
confidence, or validation coverage. If it does not close a production gate,
say which gate remains: streaming/residency, large-world precision, asset
pipeline validation, shader runtime packaging, material/transparency maturity,
performance proof, platform matrix, backend-safe debug capture, or long-session
stability.

For streaming-loop changes, state whether the slice only produces dry-run
intent, queues runtime requests, consumes asset load work, schedules background
jobs, or reaches renderer-visible terrain changes.

### Constraints

Architecture, API, dependency, performance, compatibility, or platform constraints.

### Implementation steps

Small ordered steps. Prefer steps that can be independently compiled or tested.

### Validation

Build commands, tests, sample scenes, screenshots, manual checks, or other evidence that the change works.

### Risks and assumptions

Any uncertainty, migration risk, public API concern, or known limitation.

## Example

### Goal

Add CPU-side frustum/AABB tests used by terrain chunk culling.

### Scope

- `src/renderer/scene/`
- `src/renderer/terrain/`
- `tests/`

### Roadmap phase

Phase 2 — Open-World Basics.

### Engine-readiness milestone

Open-world scale systems.

### Constraints

- Do not require GPU availability for tests.
- Keep math helpers independent of SDL3 and bgfx.
- Keep culling results inspectable for debug overlays.

### Implementation steps

1. Add or identify common `Plane`, `Frustum`, and `Aabb` types.
2. Implement frustum extraction from a view-projection matrix.
3. Implement `intersects(frustum, aabb)`.
4. Add unit tests for inside, outside, and intersecting boxes.
5. Wire terrain chunk culling through the helper without changing public APIs.

### Validation

- Configure and build the project.
- Run CPU-side tests.
- Run the sample scene and verify chunk visibility is unchanged or improved.

### Risks and assumptions

Matrix convention must match `docs/renderer_conventions.md`.
