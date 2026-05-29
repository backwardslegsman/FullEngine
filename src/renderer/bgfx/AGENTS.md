# src/renderer/bgfx/AGENTS.md

## bgfx backend rules

Concentrate bgfx usage in backend implementation files.

Wrap:

- init/shutdown
- platform data setup
- view configuration
- vertex/index buffers
- transient buffers
- textures
- uniforms
- programs
- frame submission
- debug text/stat views
- render targets

Do not expose bgfx state flags casually through high-level material APIs.

Map renderer-owned material and pipeline descriptors to bgfx internally.

When direct bgfx flags are necessary, isolate them in backend-specific code or an explicit low-level extension.

## Lifetime

Make bgfx handle ownership and destruction order explicit.

Renderer-owned resources must be destroyed before backend shutdown or cleaned up deterministically during shutdown.

Window/platform data lifetime must exceed bgfx initialization and shutdown use.

Phase 3 hardening should add diagnostics for resource dimensions, recreate
reasons, allocation failures, skipped passes, and independent feature toggles.
Keep scene color/depth, post targets, shadow maps, and debug preview resources
backend-private.

## Portability

Hide backend-specific clip-space, origin, and projection details behind renderer helpers. Public camera/view APIs should remain backend-neutral.

Do not hard-code a single graphics API unless the task explicitly requires it.

Backend-safe texture preview is a planned diagnostics milestone. Do not expose
bgfx texture/framebuffer/view handles through public APIs or Dear ImGui-facing
public abstractions.
