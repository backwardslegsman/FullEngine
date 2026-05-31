# Dependency Policy

Before adding a dependency, document the items below in the change description or relevant docs.

## Required justification

- Why the dependency is needed.
- Whether it is runtime, build-time, or tooling-only.
- License considerations.
- Whether it affects public APIs.
- Whether it can be isolated behind an internal module.
- Supported platforms and build implications.
- Expected maintenance risk.

## Preferences

Prefer dependencies that are:

- common in C++ rendering projects
- easy to build on Windows, Linux, and macOS
- maintained and versioned
- compatible with CMake integration
- isolated from public renderer APIs unless explicitly part of the public contract

## Avoid

Avoid dependencies that:

- force a bespoke build system
- leak through public renderer APIs unnecessarily
- own global process state without a clear lifetime model
- duplicate already-simple code in the repository
- add large transitive dependency graphs for minor features
- Dear ImGui is an approved optional debug/development dependency, but must not leak into renderer public APIs or core engine-facing abstractions.

## Asset and tooling dependencies

Tooling-only dependencies may be acceptable when they keep runtime code clean. Keep generated files and source files clearly separated.

Do not hard-code user-specific absolute paths in scripts, CMake files, or VSCode configuration.

## Engine-readiness dependencies

For the Phase 3 hardening milestones, prefer tooling dependencies only when
they directly support repeatable validation: shader compilation, asset
validation, screenshot or sample-scene automation, profiling capture, or CI
build/test workflows. Do not add runtime dependencies solely for diagnostics if
the same readiness goal can be met by backend-private renderer code or existing
debug tooling.

Terrain runtime event, snapshot-diff, and cooked asset manifest export/import
currently use standard-library JSON Lines serialization and narrow schema
parsing instead of a third-party JSON dependency. Reconsider a maintained JSON
library only if future tooling needs broader parsing, schema migration, or
richer nested document generation.

## Current optional debug dependencies

Dear ImGui is used only by the SDL3 sample diagnostics UI when
`FULL_RENDERER_ENABLE_DEBUG_UI=ON` and the vcpkg manifest feature `debug-ui` is
enabled. The dependency is development/debug-facing, does not affect renderer
public headers, and is isolated behind sample SDL3 input forwarding plus an
internal bgfx ImGui draw-data wrapper. The current vcpkg feature requests
`imgui[core,sdl3-binding]`.

## Current build/tool dependencies

bgfx is the renderer-library backend dependency. SDL3 is sample-only at the
CMake target boundary, although the development vcpkg manifest still includes
it for local sample builds. `bgfx[tools]` is a host/tooling dependency used by
the `full_renderer_shaders` target when `FULL_RENDERER_ENABLE_SHADER_COMPILE`
is enabled. Library-only CMake presets disable the sample and shader target so
they do not require SDL3, Dear ImGui, or shaderc at configure/build time.
