# src/renderer/terrain/AGENTS.md

## Open-world terrain rules

Design for:

- terrain chunks
- stable chunk handles
- chunk visibility
- per-chunk bounds
- instance batches
- LOD groups
- streaming creation/destruction
- debug visualization of culling and LOD decisions

Do not require all world data to be resident.

Terrain APIs should accept externally generated height/material data so a later engine can stream or procedurally generate chunks.

## Implementation order

1. CPU-side chunk descriptors.
2. GPU mesh or grid generation.
3. Chunk bounds and frustum culling.
4. LOD selection.
5. Texture splatting.
6. Normal generation or normal maps.
7. Terrain shadow participation.
8. Biome/material variation.

## Culling

Implement culling incrementally:

1. Frustum culling against bounding spheres or AABBs.
2. Per-chunk terrain culling.
3. Instance batch culling.
4. LOD selection.
5. Hi-Z occlusion later.

Keep culling outputs inspectable in debug overlays.

Do not prematurely optimize with complex job systems before the data model is stable.

## Instancing

Instancing should support:

- per-instance transform
- per-instance color or material index later
- per-instance bounds strategy
- dynamic instance buffer updates
- batching by mesh/material/pass

Avoid one draw call per object for large crowds or vegetation.

## Shadows

Cascaded shadow maps are Phase 2.

Required design considerations:

- cascade split configuration
- stable cascades if possible
- shadow distance
- per-cascade view/projection
- debug cascade visualization
- terrain and instanced object support

Start with one directional shadow map if needed before full CSM.

## Engine-readiness hardening

Before terrain is used as open-world engine infrastructure, prioritize chunk
streaming create/destroy stress tests, large-scene culling/LOD diagnostics,
resource residency rules for terrain meshes/materials/textures, seam/skirt
regression scenes, off-camera shadow caster validation, and a documented
large-world precision or origin-shift policy.
