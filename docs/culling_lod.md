# Culling and LOD

## Culling order

Implement culling incrementally:

1. Frustum culling against bounding spheres or AABBs.
2. Per-chunk terrain culling.
3. Instance batch culling.
4. LOD selection.
5. Hi-Z occlusion later.

## Requirements

- Keep culling algorithms deterministic and inspectable.
- Avoid complex job systems before the data model is stable.
- Add CPU-side tests for frustum extraction, AABB/frustum tests, and LOD selection where practical.
- Keep debug overlays capable of explaining visibility decisions.

## LOD

LOD should be driven by explicit descriptors and stable rules. Avoid hiding large behavior changes behind magic constants.

LOD decisions should be debuggable by distance, screen size, or whichever metric the implementation chooses.

The current terrain implementation uses distance from the camera position to
chunk bounds center. Each selected LOD maps directly to the mesh/material handles
stored in that chunk's `TerrainLodDesc`. LOD 0 is highest detail, and higher
indices are lower detail. The sample app demonstrates this by sharing generated
chunk-local grid meshes per LOD resolution across all terrain chunks.
Those generated LOD meshes can include simple downward skirts along all four
chunk edges. Skirts reduce visible holes when adjacent chunks select different
LOD resolutions, but they do not change the deterministic LOD rule and do not
perform full edge stitching or geomorphing.

Terrain material selection is independent from geometry LOD. A selected LOD can
reference a terrain-splat material, and the chunk splat map is carried into the
instanced terrain batch key so chunks with different splat maps are submitted in
separate batches.

Terrain chunk residency is separate from culling. Destroyed chunks and stale
chunk handles are rejected before frustum or LOD work. Resident chunks that are
not visible do not select LODs for the camera pass, and their inactive slots may
be reused with a new generation. `TerrainStats` exposes resident, nonresident,
created/destroyed/reused, invalid-handle, stale-handle, splat-fallback, and LOD
bucket counts so streaming-style churn can be inspected without backend handles.

## Shared Culling Diagnostics

Phase 3/P1 hardening adds backend-neutral shared culling diagnostics for the
major renderable categories:

- terrain chunks: active camera frustum culling, LOD selection, splat fallback
  counts, shadow caster counts, and off-camera shadow caster counts
- static mesh draws: submitted, valid-resource, diagnostic camera-frustum,
  invalid-resource, draw-submission, and shadow-caster counts
- GPU-instanced batches: submitted batch counts, diagnostic camera-frustum
  counts, instance-batch draw counts, and shadow-caster batch counts
- skinned draws: submitted, palette/resource-valid, diagnostic
  camera-frustum, draw-submission, and shadow-caster counts
- decals: submitted, active, camera-frustum culled, rejected, rendered, and
  fallback-color counts
- particles: submitted/accepted/rejected/culled batch counts, submitted and
  rendered particle estimates, sorted counts, and fallback texture counts
- shadow casters: backend-neutral per-cascade counts split by terrain, static,
  instanced, and skinned categories

These diagnostics are deterministic for a given frame and validation preset.
They are intended to explain what the CPU planned and what the backend was
asked to draw; they are not GPU timings or occlusion results.
The sample-owned `LargeScene_*` presets raise static draw, instanced instance,
skinned draw, decal, particle, and terrain stress counts within fixed debug
caps. They are opt-in validation states, not default runtime scene content or a
streaming policy.

The Frame Budget diagnostics panel complements the culling table with CPU-side
submission budget data: per-stage renderer planning timings, submitted/visible/
culled totals, coarse draw/pass estimates, staged-byte estimates, terrain chunk
churn, target recreate counts, and adjustable warning thresholds. Timing values
are approximate host CPU measurements. Staged bytes and allocation counts track
known renderer staging categories and are not exact allocator or GPU-memory
accounting.

Static, instanced, and skinned descriptors already carry bounds for shadows and
debugging. The shared diagnostics use finite bounds to classify
camera-frustum-visible versus fully outside objects. When bounds are invalid and
not required by the public descriptor contract, the object is treated as
unbounded/visible for diagnostics so existing submission behavior is preserved.
This milestone does not turn static/instanced/skinned forward rendering into a
new active camera-culling system.

Shadow passes perform separate light-frustum caster queries for each active
cascade. Camera-visible terrain chunks are still the terrain receivers in the
main pass, but off-camera terrain chunks and opt-in mesh/instanced draw bounds
can cast when their bounds intersect a cascade light/shadow frustum. Lit basic
mesh and instanced mesh receivers also sample active CSM cascades in the forward
pass. Terrain shadow caster LOD is
selected from the shadow projection center to the chunk bounds center using the
same sorted terrain LOD thresholds. This keeps off-camera caster LOD valid
for every cascade. Cascade projection stabilization then snaps each light-space
projection center to the active shadow-map texel grid without changing the
camera split ranges.

Cascade transition blending is a shader-side operation after selection and does
not change culling or caster lists. Near a non-final cascade's far split, the
terrain shader samples the adjacent next cascade and blends shadow factors over
the configured fraction of the current cascade depth span.

Shadow-frustum debug wireframes are built from the actual light
view-projection matrix used for the shadow pass. The CPU helper inverts that
matrix and transforms the eight clip-space corners into world space, so the
debug wire is suitable as the first single-split visualization seam before
cascaded shadow maps.

The CPU CSM preparation path computes up to four ordered split ranges from the
configured camera near/far distances, shadow distance, split mode, and practical
split lambda. For each split, the renderer derives camera frustum slice corners
from the submitted camera view/projection, builds a directional-light
orthographic projection around that slice, and exposes debug frustum wire data
from the generated light view-projection matrix. All active cascades render
depth and are sampled by terrain splat materials.

Per-cascade caster selection is CPU-side. Every configured cascade tests
submitted resident chunk AABBs, shadow-enabled draw item bounds, and
shadow-enabled instanced batch bounds against that cascade's generated
light-space frustum. Terrain diagnostics record per-cascade chunk counts,
off-camera chunk counts, rejects, invalid-resource rejects, and caster LOD
buckets. Renderer diagnostics also split backend shadow batches into terrain,
static mesh, and instanced mesh categories. Each cascade receives its own
backend depth pass and shadow target, and the terrain forward shader selects
among active cascades by camera-view depth.

The sample CSM validation scene intentionally spreads terrain chunks, static
mesh casters, and an instanced caster batch across the camera depth range. This
makes per-cascade culling, caster membership, transition bands, and stable
projection overlays visible without adding engine/editor concepts.

## Debug visibility

The terrain submission path can retain CPU snapshots of chunk culling and LOD
selection. GPU debug overlays are controlled per terrain submission through
`TerrainDebugOptions` and use the retained decisions. Bounds mode draws the
world-space AABBs used for culling. In the sample those AABBs are expanded
downward by the generated skirt depth, so bounds and shadow-caster overlays
include skirt geometry. LOD mode colors visible chunks by the selected LOD
index. Combined mode also colors retained culled chunk bounds and prioritizes
splat/material fallback states when present.

Large-world precision is currently caller-rebased: engines should submit
floating-origin or camera-relative terrain bounds, camera positions, and view
matrices. CPU tests cover large-coordinate planning when the caller has already
rebased the view, but the renderer does not yet own an explicit origin-shift
descriptor.

Deferred work includes full crack stitching, geomorphing, shadow atlases,
renderer-owned origin-shift descriptors, occlusion-aware caster selection,
streaming-aware material/texture residency beyond deterministic fallbacks,
neighbor-aware normal recomputation, animated actor shadows, and advanced
shadow filtering such as PCSS/contact shadows.

## Open-world hardening plan

Before relying on this path in an open-world engine, add:

- terrain chunk streaming create/destroy stress tests (initial CPU churn and
  slot-reuse tests are in place)
- shared culling diagnostics for terrain, static meshes, instancing, skinned
  meshes, decals, particles, and shadow casters
- large-scene stress scenes with thousands of resident and nonresident bounds
  (initial terrain large-grid planning tests are in place)
- shared large-scene culling diagnostics across terrain, static meshes,
  instancing, skinned meshes, decals, particles, and shadow casters (initial
  CPU aggregation and sample UI are in place)
- stable large-world origin or precision policy (currently caller-rebased)
- engine-owned origin conversion is demonstrated by `src/engine_bridge`:
  absolute double-precision positions are rebased to renderer-relative float
  bounds before culling, LOD, and shadow-caster planning
- visibility/caster regression scenes for off-camera shadow casters
- clear counters for submitted, culled, rejected, rendered, and invalid work in
  each culling path
- representative CPU and GPU timing on real large scenes before adding more
  complex scalability systems
- spatial acceleration, Hi-Z/occlusion, GPU-driven culling, or shadow-caster
  acceleration only after diagnostics show the current CPU bounds paths are the
  bottleneck
- long-session validation that proves culling stats stay deterministic while
  streamed resources are created, destroyed, and reused. The initial
  fixed-seed `OpenWorld_*` churn presets and CPU tests exercise this without
  async IO, background jobs, or renderer-owned streaming policy.
