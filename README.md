# FullRenderer

FullRenderer is a C++17 renderer built on bgfx with an SDL3 sample shell. The
renderer is intended to remain an embeddable library for a future open-world
engine, with engine-facing APIs documented under `src/renderer/public`.

The repository also contains an initial `src/engine/` foundation. That engine
area is separate from renderer internals and consumes the renderer through the
public CMake target `FullRenderer::full_renderer`.

## Current status

- Renderer foundation, open-world terrain basics, and several Phase 3 feature
  foundations are present.
- A minimal `full_engine` target now exists with lifecycle/tick validation,
  renderer-boundary smoke coverage, engine-owned world chunk state and bounds,
  coordinated world/terrain chunk setup, large-world origin helpers, residency
  request queuing, and a terrain runtime path through reusable diagnostics,
  pipeline coordination, descriptor intent, renderer submission, retained
  snapshot diffs, and JSON Lines diagnostics export/import.
- The SDL3 sample keeps renderer mesh/material/texture ownership local, but its
  terrain chunks now flow through engine world snapshots, terrain prep,
  lifecycle planning, renderer command intent, descriptor building, submission,
  chunk-handle association, runtime setup/residency request application,
  debug-UI residency toggles, terrain setup add/remove controls, and
  snapshot-diff inspection.
- The renderer is suitable for prototype engine integration and package
  consumer smoke testing.
- Production open-world readiness work is tracked in
  `docs/agents/roadmap.md`.

## Repository layout

```text
src/app/                 SDL3 sample app and validation shell
src/renderer/            renderer implementation and public API
src/engine/              engine core, world ownership, and renderer integration
src/engine_bridge/       sample/testing adapter, not the real engine
assets/                  shader, texture, and mesh source assets
tools/                   shader, asset, and CI tooling
tests/                   CPU-side tests and consumer smoke checks
docs/                    architecture, build, and integration docs
```

## Build

The first supported development environment is Windows with Visual Studio 2022,
CMake, vcpkg, bgfx, and the D3D11 shader profile.

Developer debug build:

```powershell
cmake --preset dev-debug
cmake --build --preset dev-debug
```

Library-only build:

```powershell
cmake --preset library-only-debug
cmake --build --preset library-only-debug
```

Package validation:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Run-WindowsPackageValidation.ps1
```

Use `-InstallDependencies` with that script on a clean machine or CI agent.
More details are in `docs/build.md`.

CPU tests, including the engine foundation tests, can be configured with:

```powershell
cmake --preset tests-debug
cmake --build --preset tests-debug
ctest --test-dir out\build\tests-debug --output-on-failure -C Debug
```

## Documentation

- `docs/renderer_overview.md` explains the renderer foundation and readiness.
- `docs/engine_overview.md` explains the current engine boundary.
- `docs/engine_integration.md` shows how an engine consumes the renderer.
- `docs/agents/roadmap.md` tracks renderer phases and the engine expansion
  track.

## GitHub workflow

The repository includes `.github/workflows/windows-package-validation.yml`,
which runs the Windows package validation script on pushes, pull requests, and
manual dispatch.

## License

No license has been selected yet. Add a `LICENSE` file before publishing if the
repository should be open source or shared outside a private organization.
