# src/renderer/animation/AGENTS.md

## Animation scope

Skeletal animation is Phase 3 work.

Keep animation isolated from renderer core until needed.

Expected future split:

- skeleton asset
- animation clip asset
- pose evaluation, possibly outside renderer
- skinning palette submission
- GPU skinning path
- debug skeleton draw

The renderer should accept final skinning matrices or palette buffers from an engine-facing API.

## Crowds

Crowd rendering should use batching and instancing where possible.

For many animated actors, plan for:

- shared mesh resources
- shared skeleton resources
- animation texture or palette buffering later
- LOD by distance
- impostors or simplified rigs later

Do not build crowd simulation in the renderer.

## Boundaries

Do not add gameplay animation state machines, AI movement, combat state, or simulation logic to this subtree.

## Engine-readiness hardening

For open-world actor readiness, prioritize externally supplied animated bounds,
invalid/max-size palette validation, many-skinned-draw stress tests, skinned
shadow/outline/fade interaction tests, and a documented engine-owned
clip/blend/evaluation pipeline. Crowd simulation remains outside the renderer.
