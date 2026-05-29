# tools/AGENTS.md

## Tooling rules

Tools should be repeatable, documented, and platform-conscious.

Do not hard-code user-specific absolute paths.

Prefer scripts and CMake targets that can be run from a clean checkout.

Separate source assets from generated outputs.

## Dependency handling

Tooling-only dependencies are acceptable when they keep runtime code clean, but document why they are needed and how they are invoked.

Do not let tooling dependencies leak into renderer runtime or public renderer APIs.

Phase 3 hardening tooling should focus on repeatable configure/build/test,
shader compilation validation, asset validation, sample smoke presets,
profiling capture, and CI-friendly commands. Avoid tools that only work on one
machine or hide generated outputs.
