# src/engine/assets/AGENTS.md

## Purpose

This subtree owns engine-side asset identity, catalog lookup, and cooked asset
reference conventions. It must stay separate from renderer resource handles and
renderer submission code.

## Rules

- Do not include `full_renderer/*`, renderer internals, bgfx, SDL3, Dear ImGui,
  or sample app headers.
- Store engine asset identity and metadata only; renderer handle resolution
  belongs in `src/engine/renderer_integration/`.
- Do not add file IO, importers, cooked manifest parsing, async loading, or
  renderer resource creation unless a later slice explicitly asks for it.
- Keep catalogs CPU-only, deterministic, and covered by focused tests.
