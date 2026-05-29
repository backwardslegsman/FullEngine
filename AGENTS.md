# AGENTS.md

## Repository purpose

This repository contains a C++ renderer built on SDL3 and bgfx. The renderer is intended to become an embeddable renderer library for a future open-world engine.

Prioritize correctness, maintainability, clean interfaces, deterministic behavior where practical, and incremental implementation over flashy rendering features.

## Global goals

- Build a renderer library that can be embedded into a future engine.
- Keep SDL3 and bgfx isolated behind stable renderer-facing abstractions.
- Expose Doxygen-documented C++ interfaces for engine integration.
- Design for open-world scale: chunking, culling, LOD, instancing, and streaming-friendly ownership.
- Keep CPU-side systems testable without requiring a full game runtime.
- For Phase 3 and later, prioritize the engine-readiness hardening milestones in `docs/agents/roadmap.md` before adding more visual effects.

## Global non-goals

For renderer work, do not implement a full game engine, ECS, editor, physics system, AI simulation, networking layer, asset marketplace importer, or bespoke build system unless explicitly requested. When engine work is explicitly requested, keep it under `src/engine/` and preserve the renderer as an embedded library consumed through public APIs.

Do not overfit to a single demo scene. Public renderer interfaces should accept externally owned scene data and remain reusable.

Do not add heavyweight dependencies without explicit justification.

## Technology baseline

- Language: modern C++, prefer C++17 unless the repository standard changes.
- Window/input sample layer: SDL3.
- Graphics backend: bgfx.
- Shader toolchain: bgfx shaderc-compatible shaders.
- Build system: CMake unless the repository already uses another system.
- Target IDE: VSCode.

## Repository shape

Use or evolve toward this layout without breaking an already-working structure:

```text
/
  AGENTS.md
  PLANS.md
  CMakeLists.txt
  cmake/
  external/
  assets/
    shaders/
    textures/
    meshes/
  tools/
    shader_compile/
    asset_pipeline/
  src/
    app/
    engine/
      core/
      world/
      scene/
      assets/
      jobs/
      renderer_integration/
    renderer/
      public/
      core/
      bgfx/
      scene/
      resources/
      terrain/
      animation/
      debug/
    engine_bridge/       # optional sample/testing adapter, not the engine
  tests/
  docs/
```

## Public API documentation

All engine-facing public APIs must use Doxygen-compatible comments in public headers. Public API comments should be suitable both for generated API reference documentation and for humans reading the headers.

At minimum, document:

- brief purpose and usage constraints
- ownership and lifetime
- thread expectations
- units and coordinate conventions
- parameter meaning
- return value semantics
- error behavior
- whether data is copied, referenced, or consumed
- whether calls are valid before, during, or after a frame

Keep comments focused on behavior, invariants, and integration constraints. Do not add boilerplate that merely repeats symbol names or obvious types.

Detailed Doxygen conventions live in `docs/agents/doxygen_style.md`. Public renderer header work should also follow `src/renderer/public/AGENTS.md`.

## Required workflow

When implementing:

- Inspect the existing structure first.
- Identify the applicable phase from `docs/agents/roadmap.md`.
- For Phase 3 work, identify the applicable engine-readiness milestone: correctness/pass interactions, resource lifetime, open-world scale, material/shader/animation/transparency, diagnostics/tooling, or asset/build/platform confidence.
- Keep changes scoped.
- Preserve public API clarity.
- Update Doxygen-compatible comments and docs for public interfaces.
- Add CPU-side tests for algorithmic changes when practical.
- Prefer compiling changes over speculative architecture-only edits.

When refactoring:

- Preserve behavior.
- Keep formatting-only and logic changes separate where practical.
- Explain public API impact.

When debugging:

- Reproduce or reason from the error.
- Fix the smallest responsible layer.
- Prefer root-cause fixes over suppressing symptoms.
- Add regression coverage when practical.

## Minimum definition of done

See `docs/agents/definition_of_done.md` for the full checklist.

At minimum, a renderer change is done when it builds, public APIs have Doxygen-compatible documentation, resource lifetimes are clear, errors are handled or asserted intentionally, and relevant CPU-side tests/docs are updated.

## Reference docs

- `docs/agents/architecture.md` — repository layers, engine boundary, and ownership model.
- `docs/agents/roadmap.md` — phase plan and preferred first milestone.
- `docs/agents/definition_of_done.md` — completion criteria.
- `docs/agents/dependency_policy.md` — dependency approval rules.
- `docs/agents/coding_style.md` — code style and maintainability rules.
- `docs/agents/doxygen_style.md` — public API documentation style and Doxygen conventions.
- `docs/agents/split_mapping.md` — how the previous monolithic guidance maps into this structure.

## Directory-specific rules

Before editing a subdirectory, check for a nearer `AGENTS.md`. More specific `AGENTS.md` files refine or constrain this root guidance for their subtree.
