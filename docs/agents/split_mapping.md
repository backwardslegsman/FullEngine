# AGENTS.md Split Mapping

This file records how the previous monolithic guidance is distributed across smaller Codex instruction files.

## Root

`/AGENTS.md` keeps only repo-wide rules:

- repository purpose
- global goals and non-goals
- technology baseline
- required workflow
- minimum definition of done
- references to longer docs

## Directory-specific `AGENTS.md` files

- `src/engine/AGENTS.md` - future engine runtime boundary, ownership, and renderer integration rules.

- `src/app/AGENTS.md` — SDL3 app shell and sample integration rules.
- `src/renderer/AGENTS.md` — renderer boundary, naming, coding, performance, threading, and lifetime rules.
- `src/renderer/public/AGENTS.md` — public API, handles, Doxygen comments, ownership, lifetime, and coordinate conventions.
- `src/renderer/core/AGENTS.md` — renderer orchestration and frame lifecycle.
- `src/renderer/bgfx/AGENTS.md` — bgfx backend containment.
- `src/renderer/scene/AGENTS.md` — scene submission, camera, lighting, post effects, and externally controlled selection/fade data.
- `src/renderer/resources/AGENTS.md` — meshes, materials, textures, shader programs, and resource ownership.
- `src/renderer/terrain/AGENTS.md` — open-world terrain, culling, LOD, instancing, and shadow considerations.
- `src/renderer/animation/AGENTS.md` — future skeletal animation and crowd rendering constraints.
- `src/renderer/debug/AGENTS.md` — debug overlays, captures, and profiling hooks.
- `src/engine_bridge/AGENTS.md` — optional sample adapter layer.
- `assets/shaders/AGENTS.md` — shader source organization.
- `assets/textures/AGENTS.md` — texture asset conventions.
- `assets/meshes/AGENTS.md` — mesh asset conventions.
- `tools/AGENTS.md` — general tooling rules.
- `tools/shader_compile/AGENTS.md` — shader compilation tooling.
- `tools/asset_pipeline/AGENTS.md` — asset import/processing tooling.
- `tests/AGENTS.md` — CPU-side test expectations.
- `docs/AGENTS.md` — documentation expectations, including Doxygen-compatible public header comments.
- `.vscode/AGENTS.md` — workspace configuration rules.

## Long-form reference docs

- `docs/agents/architecture.md` — full architecture and layering guidance.
- `docs/agents/roadmap.md` — compact active phase plan, hardening/readiness milestones, and next recommended slices.
- `docs/agents/implementation_log.md` — historical completed-slice archive kept out of default context.
- `docs/agents/definition_of_done.md` — detailed completion checklists.
- `docs/agents/dependency_policy.md` — dependency acceptance criteria.
- `docs/agents/coding_style.md` — style, public API comments, error handling, logging, threading, and lifetime rules.
- `docs/agents/doxygen_style.md` — detailed Doxygen style for public C++ interfaces.

## Application rule

Keep a rule in root only if it applies to every task. Move it into a nested `AGENTS.md` when it applies to a subtree. Move it into `docs/agents/*.md` when it is long-form reference material.
