# src/renderer/scene/AGENTS.md

## Scene submission

Scene code owns renderer-facing scene/view data: render packets, draw items, camera/view descriptors, lights, visibility inputs, and per-frame submission data.

The engine owns gameplay state. The renderer receives stable render data for the current frame.

Do not introduce ECS, physics, AI, or gameplay object ownership into this subtree.

## Camera

Define public camera or view descriptors that support:

- position
- orientation or view matrix
- projection parameters
- near/far planes
- viewport size
- jitter offset later for TAA
- previous-frame matrices later for motion vectors

The camera should not own input handling.

## Lighting

Phase 1 supports:

- one directional light
- ambient term
- simple Lambert or Blinn-Phong if needed
- linear-space lighting
- gamma-correct output

Future lighting APIs should allow:

- directional lights
- point lights
- spot lights
- clustered/Forward+ light lists
- shadow-casting flags
- light layers or masks, if needed

## Selection and fade data

Selection is a renderer feature, but selection ownership belongs to the engine.

Public renderer data may accept selected object IDs, per-draw selection flags, outline color, outline thickness, or object ID buffer configuration.

Roof/building fade is a material/rendering feature controlled externally. The renderer may expose per-object fade scalar, material alpha/fade mode, dithered fade, and depth prepass behavior.

Do not implement gameplay selection logic or building visibility decisions in renderer core.

## Post processing

Phase 3/4 post effects may include SSAO, color grading, SMAA, TAA, volumetric fog, and bloom only if explicitly requested.

Post effects should be modular passes with clear inputs and outputs.

For Phase 3 hardening, scene submission and planning helpers should make
feature interactions testable without GPU access. Track explicit pass ordering,
scene color/depth requirements, culling/sorting decisions, and skipped or
invalid work for SSAO, decals, particles, color grading, selection/outline,
weather, fade, and future post passes.
