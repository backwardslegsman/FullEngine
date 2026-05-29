# docs/AGENTS.md

## Documentation expectations

When adding or changing a public system, update docs and Doxygen-compatible public header comments in the same change.

Documentation should describe behavior, ownership, lifetime, thread expectations, units, coordinate conventions, error behavior, and engine integration concerns. Avoid documenting aspirational features as already implemented.

## Required docs over time

- `renderer_overview.md`
- `renderer_conventions.md`
- `public_api.md`
- `shader_pipeline.md`
- `material_system.md`
- `terrain.md`
- `culling_lod.md`
- `engine_overview.md`
- `engine_integration.md`
- `agents/doxygen_style.md`

## Roadmap and hardening plans

When a change affects renderer readiness for an open-world engine, update the
Phase 3 hardening milestones in `docs/agents/roadmap.md` and the relevant
topic document. At minimum, docs should keep the following plans visible:

- correctness and pass-interaction regressions
- resource lifetime, resize, and handle validity
- open-world terrain, culling, LOD, and streaming pressure
- material, shader, animation, and transparency hardening
- diagnostics, backend-safe texture preview, and performance tooling
- asset pipeline, build, and platform confidence

## Engine integration examples

`engine_integration.md` must show how a future engine:

- creates the renderer
- passes platform/window data
- creates renderer resources
- submits a frame
- handles resize
- shuts down cleanly

## Style

- Prefer short examples over vague prose.
- Mark future work clearly.
- Keep public API docs synchronized with Doxygen comments in headers.
- Prefer Doxygen-compatible examples for public C++ APIs.
- Do not duplicate large code blocks that can drift from source files.
