# src/engine_bridge/AGENTS.md

## Purpose

This directory is an optional sample adapter layer. It may demonstrate how a future engine could translate engine-owned data into renderer descriptors and frame submissions.

## Rules

- Renderer core must not depend on `src/engine_bridge/`.
- This layer may use sample/demo concepts, but should keep them clearly outside renderer core.
- Do not place reusable renderer logic here.
- Do not introduce a real ECS, game simulation, physics system, AI layer, editor, or networking layer unless explicitly requested.

## Acceptable responsibilities

- Translate sample world data into render packets.
- Show resource creation and destruction flow.
- Demonstrate resize, frame submission, and shutdown.
- Provide smoke-test scenes for integration.
- Provide prototype engine-readiness harnesses: camera bookmarks, feature
  presets, stress scenes, and externally owned selection/weather/particle/fade
  state mapped into renderer descriptors.
