# Terrain

## Goals

Terrain should support open-world rendering through chunking, bounds, visibility, LOD, instance integration, and streaming-friendly resource lifetime.

## Design constraints

- Do not require all world data to be resident.
- Use stable chunk handles.
- Keep per-chunk bounds available for culling and debug views.
- Accept externally generated height/material data.
- Keep chunk creation/destruction safe while the app runs.

## Current Phase 2 slice

The current terrain implementation is intentionally small:

- chunks are CPU-side renderer records
- chunks reference existing `MeshHandle` and `MaterialHandle` resources
- chunk descriptors copy world-space bounds and sorted LOD descriptors
- per-frame terrain submission performs frustum culling and distance LOD
- visible chunks sharing mesh/material are grouped into internal GPU instancing batches
- terrain stats and debug snapshots expose culling, chunk LOD, and batch LOD decisions
- terrain residency stats expose live/resident slots, inactive reusable slots, chunk churn, stale submitted handles, and splat fallback/resident counts
- the sample app uploads shared chunk-local grid meshes per LOD resolution
- terrain-splat materials blend four layer textures with an optional RGBA splat map per chunk
- shared sample grid meshes can generate deterministic per-vertex terrain normals from chunk-local height data
- shared sample grid meshes can generate optional downward skirts around chunk edges for first-pass crack mitigation
- terrain-splat materials can optionally blend four tangent-space layer normal maps using the splat weights
- optional GPU debug overlays can draw chunk bounds, LOD colors, material state, and splat fallback state
- the sample can build an optional Dear ImGui diagnostics panel for terrain counters and overlay toggles
- terrain splat materials can receive cascaded directional shadows
- each shadow pass selects resident terrain casters and opt-in mesh/instanced casters with its cascade light/shadow frustum, including off-camera bounds
- CPU-side cascade split data and debug frusta can be generated for up to four cascades
- all active cascades render depth and are sampled by terrain and lit basic mesh materials
- the sample uses a lightweight sky gradient and linear distance fog for open-world depth cues

The renderer does not generate heightmaps, load terrain assets, own simulation
state, or stream terrain data in background jobs yet.

## Public ownership model

Create mesh and material resources first, then create terrain chunks that
reference them. Each `TerrainLodDesc` is the render resource selected for that
LOD: `mesh` supplies the geometry and `material` supplies the visual state.
Terrain LOD meshes are chunk-local: their origin is positioned at the chunk
bounds minimum during submission, which lets many chunks share the same mesh and
material through GPU instancing. The recommended Phase 2 pattern is to upload a
small set of reusable grid meshes, one per LOD resolution, and reference those
same handles from many chunks. Destroy chunks before destroying the referenced
mesh/material resources. Shutdown cleans up remaining chunk records, but it does
not change the ownership rules for explicitly managed renderer resources.

Terrain chunk handles use `id` plus `generation` so stale handles can be
detected after a chunk slot is destroyed and reused.

## Residency lifecycle

Terrain residency is externally controlled. The sample or future engine decides
which chunks exist for the current area, creates or destroys renderer chunk
records, and submits only the resident handles it wants considered for the
frame. The renderer owns only the CPU chunk records and backend draw resources
derived from live handles.

The chunk state model is:

- created: `createTerrainChunk` validates bounds and LOD descriptors, copies
  the descriptor data, and returns a generation-tracked chunk handle
- updated: `updateTerrainChunk` replaces bounds, LOD descriptors, and splat-map
  references on a live chunk while preserving the existing handle/generation
- resident: the chunk slot is active and can be submitted for camera and shadow
  culling
- visible: the submitted resident chunk passed the camera frustum and selected
  a valid LOD
- culled: the submitted resident chunk was outside the camera frustum
- shadow caster: the submitted resident chunk intersected a cascade
  light-frustum and selected a shadow LOD
- material fallback active: the chunk has no splat map and uses deterministic
  layer-0 splat weights, or its material uses layer texture fallback colors
- destroyed: the chunk slot is inactive and may be reused with a new generation
- stale handle: a non-zero handle that no longer resolves to the active slot;
  stale handles do not render, cast shadows, or access backend resources

Duplicate destroys and invalid/default handles are ignored by the chunk owner
path. Invalid update descriptors fail without modifying the live chunk, and
stale update handles are rejected. Submitting invalid or stale chunk handles is
safe: the handles are counted in `TerrainStats` and produce no draw work.

## Bounds, culling, and LOD

Chunk bounds are world-space AABBs in meters, Y-up, right-handed coordinates.
The renderer extracts a frustum from the submitted view/projection matrices and
tests chunk bounds on the CPU.

LOD entries must be sorted highest-detail to lowest-detail by increasing
`maxDistanceMeters`. The current API supports up to four LOD levels. LOD 0 is
the highest-detail mesh/material pair; higher indices are lower-detail levels.
Selection is deterministic and stateless: the renderer chooses the first LOD
whose inclusive max distance contains the camera-to-bounds-center distance.
Distances beyond the final threshold use the last LOD.

Hysteresis, geomorphing, crack stitching, and mesh simplification generation are
not implemented yet. Callers provide all LOD meshes and thresholds externally.
Invalid or missing LOD mesh/material handles are rejected during chunk creation,
so the current renderer does not perform runtime LOD resource fallback.

## Large-world precision policy

Terrain chunk bounds and camera positions submitted to the current renderer are
single-precision meter values. For large worlds, the host should apply a
floating-origin or camera-relative rebase before submitting terrain bounds,
view matrices, camera positions, and related debug data. The renderer treats the
submitted values as already rebased for the current frame; it does not own world
streaming, origin-shift timing, region identity, or persistence.

CPU tests cover large-coordinate planning with caller-rebased view data, but an
explicit renderer origin descriptor and full double-precision world boundary are
deferred.

## Directional terrain shadows

The current shadow path supports up to four cascaded directional shadow maps for
terrain and lit basic mesh receivers. `RenderPacket::directionalShadow` controls whether the
backend renders terrain shadow caster batches plus opt-in mesh, instanced mesh,
and skinned mesh casters into square shadow maps, then samples the selected cascade in the
terrain splat forward shader. Shadow caster selection is a separate CPU pass:
resident chunks submitted in `TerrainSubmitDesc`, `DrawItem`s with
`castsShadow`, `InstancedDrawDesc` batches with `castsShadow`, and
`AnimatedDrawItem` submissions with `castsShadow` are tested
against each cascade's light/shadow frustum. Chunks and draw bounds outside the
camera frustum can still cast if their bounds intersect a cascade light frustum.

The main terrain pass and shadow pass use distinct sets:

- camera-visible chunks: chunks inside the submitted camera frustum, rendered as terrain receivers
- shadow-caster chunks: submitted resident chunks inside a cascade light/shadow frustum
- mesh/instanced/skinned casters: submitted frame-local draw bounds inside a cascade light/shadow frustum
- terrain receivers: camera-visible terrain chunks that sample active shadow cascades in the forward pass

Off-camera chunks must still be resident and submitted by the host to cast. The
renderer does not predict streamed-out terrain.

The shadow projection is a simple orthographic light box centered on
`DirectionalShadowDesc::centerWorld` with half-size
`DirectionalShadowDesc::extentMeters`. The sample tracks the center to the
camera X/Z position. This validates render target creation, depth rendering,
sampling, bias, and terrain material integration, but it is not a complete
large-world solution.

Stable cascade projection is enabled by default through
`DirectionalShadowDesc::stableCascadeProjection`. For each cascade, the renderer
computes tight light-space bounds for the camera frustum slice, computes the
shadow texel size from those bounds and the active shadow-map resolution, snaps
the light-space X/Y center to that texel grid, and expands the X/Y bounds by one
texel so coverage does not shrink. This reduces visible crawl during small
camera movements while preserving the existing split ranges and caster
selection.

Cascade transition blending is enabled by default through
`DirectionalShadowDesc::cascadeBlendEnabled`. The terrain shader keeps the same
primary cascade selection rule, but fragments inside the final
`cascadeBlendFraction` portion of a non-final cascade also sample the next
adjacent cascade and smoothly blend the two shadow factors. The final cascade
does not sample beyond itself, and fragments beyond the last split remain
unshadowed.

Debug light-frustum wireframes are derived from the actual light
view/projection matrices used by the shadow passes. The renderer inverts each
cascade view-projection matrix, transforms the eight clip-space frustum corners
into world space, and submits those edges through the existing GPU debug line
path. This keeps visualization tied to the real shadow render projections
instead of duplicate approximations.

`DirectionalShadowDesc` can describe up to four cascades, a shadow distance,
camera near/far distances, and a split mode: uniform, logarithmic, or practical
blended splits. The CPU helper computes ordered camera-space split ranges,
camera frustum slice corners, and one light view/projection per cascade. Debug
cascade wireframes are derived from those generated cascade matrices.

Terrain shadow caster selection now runs per computed cascade on the CPU. Each
cascade tests submitted resident terrain chunk bounds against that cascade's
actual light view/projection frustum. Each active cascade retains its own
backend shadow caster batches, renders depth into a separate renderer-owned
shadow target, and can be sampled by the terrain forward shader.

Shadow caster LOD is deterministic and stateless for this first slice. It uses
the distance from `DirectionalShadowDesc::centerWorld` to the chunk bounds
center and the same sorted `TerrainLodDesc::maxDistanceMeters` thresholds used
by camera LOD. This avoids relying on camera-visible LOD state for off-camera
casters. Later cascaded shadows may replace this with cascade-specific policies.

Current limitations:

- one shadow render target per active cascade; terrain samples and blends
  adjacent active cascades near non-final split boundaries
- cascade shadow targets are backend-owned, reused while cascade count and
  resolution match, recreated on configuration changes, and released when
  shadows are disabled or the renderer shuts down
- only resident chunks listed in the frame terrain submission can cast
- opt-in static mesh, instanced mesh, and opaque skinned mesh submissions can cast onto terrain
- terrain receives shadows through `MaterialKind::TerrainSplat`
- lit static, instanced, and skinned mesh forward materials receive the same CSM shadows
- unlit mesh materials remain unshadowed
- no atlas or per-cascade resolution scaling yet
- fixed nearest, 2x2 PCF, and 3x3 PCF modes only; no PCSS/contact shadows
- no streaming-aware coverage or crack-specific shadow fixes

## Phase 2 CSM acceptance status

For Phase 2, the CSM path is considered complete at a practical first-pass
quality level:

- terrain receives shadows from all active cascades
- terrain chunks cast into each cascade when resident and inside the cascade light frustum
- opt-in static mesh and instanced mesh submissions cast into cascades
- lit basic static and instanced mesh materials receive CSM shadows
- cascade split modes, distance, stable snapping, transition blending, bias, and PCF filtering are configurable
- cascade frusta, caster bounds, resource validity, per-cascade counters, and shadow-map previews are available through the optional debug UI
- runtime shadow enable/disable, resolution changes, and cascade-count changes release or recreate backend resources at frame submission boundaries
- CPU-side tests cover split ranges, selection boundaries, blend bands, stable snapping, resource reconfiguration planning, debug preview state, and caster/debug aggregation

The sample validation scene uses procedural terrain, static cube casters, and an
instanced cube batch to exercise multiple cascades without introducing engine or
editor concepts. Recommended starting settings are four cascades, a 1024 texel
map resolution, practical split mode with lambda near `0.5..0.6`, stable
projection enabled, cascade blending enabled, 2x2 PCF, and conservative
depth/slope bias tuned from the debug panel.

## Terrain materials and splat maps

Terrain splatting uses ordinary renderer-owned material and texture handles.
Create RGBA8 layer textures, create a `MaterialKind::TerrainSplat` material,
and assign that material to terrain LOD descriptors. Layer albedo textures are
color textures with sRGB asset-contract metadata. Each terrain chunk may also
reference one RGBA splat map, which should use `TextureSemantic::TerrainSplat`
and linear color-space metadata. Channel convention is:

- R: layer 0 weight
- G: layer 1 weight
- B: layer 2 weight
- A: layer 3 weight

Missing layer textures use each layer's fallback color. Missing splat maps are
deterministic: layer 0 receives full weight and layers 1-3 receive zero weight.
Splat UVs are based on chunk-local X/Z mesh coordinates multiplied by the
terrain material UV scale. Terrain LOD selection changes geometry only;
material selection remains whatever material handle the selected LOD references.

Terrain grid helper meshes can generate geometric normals from generated or
externally supplied height samples before upload. The current helper computes
face-normal accumulation from the triangles inside the current chunk and
normalizes each vertex result. Chunk boundary vertices use the triangles
available in that chunk only; missing neighbor chunks are not required. If a
vertex receives no valid triangle normal, the helper falls back to world up.
The renderer also accepts externally supplied normals through `MeshVertex`, so
engine terrain importers can override the helper output.

Terrain grid helper meshes can add skirts during mesh generation. Skirts
duplicate each chunk-edge vertex, move the duplicate downward by a configured
depth in meters, and generate vertical side faces along all four edges. Skirt
vertices keep the edge vertex color, normal, and chunk-local X/Z coordinates,
so they render through the existing terrain splat shader and use the same
material, normal-map, CSM, and fog path as the surface. This hides small cracks
caused by adjacent chunk height differences or different selected LOD mesh
resolutions. The sample expands terrain chunk bounds downward by the skirt
depth so camera culling, shadow caster selection, and debug bounds include
skirt geometry.

Terrain splat materials support one optional tangent-space normal map per
layer. Normal-map textures should use `TextureSemantic::NormalMap` and
`TextureColorSpace::EncodedNormal`. The convention is RGBA8 encoded RGB normals
where `(128, 128, 255)` is flat. The shader derives a simple terrain tangent basis from chunk-local +X,
chunk-local +Z, and the geometric normal, blends layer normal maps with the
same normalized RGBA splat weights as albedo, and mixes the result by
`TerrainMaterialDesc::normalMapStrength`. Missing layer normal textures use a
flat normal fallback. `TerrainMaterialDesc::flipNormalMapY` supports normal
maps authored with the opposite green-channel convention. Normal maps affect
forward terrain lighting only; shadow depth passes continue to render geometry
depth.

Distance fog is applied to terrain after splat material blending, direct
lighting, and CSM shadowing, but before gamma output. Fog does not affect chunk
culling, LOD selection, shadow caster selection, or resource residency.

Weather wetness is an optional scalar render hook layered on top of terrain
shading. When `WeatherDesc::wetness` is enabled and terrain wetness is active,
the terrain fragment shader darkens the already-lit splat result by the
submitted wetness amount and darkening factor before distance fog. This is
reversible per-frame state: it does not paint terrain, simulate accumulation,
create puddle geometry, or alter splat/normal textures.

The Phase 3 SSAO foundation affects terrain through a backend-private
fullscreen pass rather than the terrain material shader. When enabled for a
frame, the backend captures terrain and object depth into a private target,
computes depth-only ambient occlusion, and composites the result over the
forward scene. AO evaluation can run at half resolution and can be smoothed by
a simple non-depth-aware separable blur. The terrain splat shader, CSM
sampling, fog, and normal-map logic are unchanged by SSAO, and disabled SSAO
submits no terrain-specific AO work.

## Terrain seam quality

The first seam-quality slice uses skirts rather than full edge stitching.
Skirts are generated once when terrain LOD mesh resources are created; changing
skirt depth requires rebuilding and reuploading those mesh resources. There is
no per-frame terrain mesh regeneration. Skirts reduce visible holes between
chunks and across LOD boundaries, but they do not remove T-junctions, change
LOD selection, or morph vertices between resolutions.

Neighbor-aware normal generation is intentionally deferred. Edge normals are
currently generated from the current chunk's triangles only, and skirt vertices
copy their corresponding surface edge normal. This avoids retaining neighbor
height pointers and keeps generation deterministic for each mesh resource, but
small lighting discontinuities at chunk seams may remain.

## Debugging

`TerrainStats` reports live/resident chunks, allocated chunk slots, inactive
nonresident slots, chunks created/updated/destroyed/reused since the previous
terrain submission, lifetime chunk churn, submitted chunks, visible chunks,
culled chunks, invalid/default handles, stale handles, terrain backend draw count,
visible chunks by LOD, and terrain batches by LOD. It also reports visible
chunks using fallback layer-0 splat weights, visible chunks with resident splat
maps, LOD fallback rejects, shadow caster chunks, off-camera shadow casters,
light-frustum rejects, invalid shadow resource rejects, and shadow caster
counts by LOD.
After terrain instancing, `terrainDraws` is the number of generated terrain
batches, while `visibleChunks` remains the number of chunks that passed culling.
When `TerrainDebugOptions::captureChunkInfo` is enabled for a frame,
`copyTerrainDebugInfo` returns one snapshot record per submitted chunk with
bounds, culling result, selected LOD, material state, splat-map state, and
distance. When `TerrainDebugOptions::captureBatchInfo` is enabled,
`copyTerrainBatchDebugInfo` returns one snapshot record per generated terrain
instance batch with mesh/material handles, selected LOD, instance count,
material/splat-map validity, and enclosing bounds.
When terrain or shadow debug capture requests caster diagnostics,
`copyTerrainShadowCasterDebugInfo` returns terrain shadow-caster records with
cascade indices, light-frustum decisions, and off-camera caster status.

`TerrainDebugOptions` also includes runtime GPU overlay toggles. The renderer
can draw AABB wire boxes for chunk bounds, color visible chunks by selected LOD,
highlight material validity, highlight splat-map fallback use, or use a combined
diagnostics mode. Overlay rendering uses the same per-frame terrain decisions as
the submission path; it does not recull or reselect LODs. Invalid handles do not
draw boxes because they have no stable bounds. LOD-only overlay mode shows
visible chunks, while combined mode also shows retained culled chunk bounds.

The sample app visualizes rendered LOD resources by using different material
colors and different shared grid resolutions per LOD. Its generated terrain LOD
meshes include skirts by default, and chunk debug bounds include the skirt
depth. It reports terrain
counters and visible chunk counts by LOD in the SDL window title. The current
sample scene is also a lightweight CSM validation scene: a larger procedural
terrain grid, static cube casters, and an instanced cube batch are spread across
the view depth range so multiple cascades and shadow caster categories are easy
to inspect. When built with the optional Dear ImGui debug UI, the sample
provides a "Terrain Diagnostics" panel with checkboxes for terrain GPU overlays,
residency/churn counters, invalid/stale handle counters, splat fallback counts,
origin-policy status, CSM reset/preset buttons, grouped cascade controls,
per-cascade stats, resource validity, and projection stability data. A
shadow-caster overlay distinguishes
camera-visible casters from off-camera-only casters, while cascade frusta and
caster overlays can be toggled independently. The shadow-map preview is a
backend-private debug UI path: the sample selects a cascade, the bgfx debug
renderer remaps that cascade's R32F shadow texture into an internal RGBA preview
target, and ImGui displays the preview with black/white depth remap and invert
controls. The public renderer API still does not expose backend texture objects.

The optional debug UI also includes a "Large Scene / Culling Diagnostics"
section. It aggregates terrain, static mesh, instanced mesh, skinned mesh,
decal, particle, and shadow-caster counters into one table so open-world stress
presets can be inspected without backend handles. Terrain rows use active
terrain culling and LOD stats; static/instanced/skinned rows use finite submitted
bounds for diagnostic frustum classification while preserving the current
forward-submission behavior.

The companion "Frame Budget" section reports terrain chunk create/destroy/reuse
churn alongside CPU-side submission timing and staged-byte estimates. Terrain
churn values come from `TerrainStats`; timing and staged bytes are approximate
debug diagnostics and should be used to compare validation presets, not as
machine-independent performance gates.

The sample and CPU tests also include an "Open World Churn / Long Session"
validation harness. It uses a fixed seed to simulate engine-owned residency
changes over many frames: terrain chunk slots are created, destroyed, and
reused; material, texture, and LOD fallbacks appear and disappear; decals,
particles, skinned palettes, optional passes, and resize/post-target recreate
events churn deterministically. This harness never performs async streaming,
file IO, or world simulation inside the renderer. It exists to prove that the
renderer-facing create/destroy/update/submit contract stays stable over long
sessions and that diagnostic counters stay bounded and reproducible.

For large worlds, terrain submission follows the renderer-wide caller-rebased
policy. Engine/sample code should convert absolute chunk bounds and camera
positions to renderer-relative coordinates before creating or submitting
terrain chunks. The `src/engine_bridge` seam demonstrates the mapping from
engine chunk IDs to renderer `TerrainChunkHandle` values plus the frame-origin
conversion used by the `EngineStreamingSeam_*` and `LargeWorld_*` presets.
Destroyed mappings are removed before renderer submission; stale mapping
attempts are counted by the seam rather than dereferencing renderer resources.

## Implementation order

1. CPU-side chunk descriptors.
2. GPU mesh or grid generation.
3. Chunk bounds and frustum culling.
4. LOD selection.
5. Texture splatting.
6. Terrain editing normal recomputation.
7. Terrain shadow participation.
8. Biome/material variation.

## Open-world readiness plan

Before using terrain as the backbone of a streamed open world, harden:

- the engine-owned residency interface around real async IO and streaming
  decisions; the renderer should continue receiving explicit create, destroy,
  update, and submit calls rather than owning world streaming policy
- repeated chunk create/destroy and LOD resource residency (initial CPU stress
  coverage is in place)
- long-session churn coverage that repeatedly reuses chunk slots, material
  references, splat maps, normal maps, shadows, and post targets over many
  frames (initial fixed-seed CPU/sample validation is in place; real engine
  streaming integration remains external)
- chunk bounds, culling, and debug snapshots under thousands of chunks
  (initial large-grid planning coverage is in place)
- skirt/seam behavior across mixed LODs and material boundaries
- large-world precision or origin-shift policy (current policy is
  caller-rebased single-precision submission with a sample engine-bridge
  conversion seam)
- shadow caster selection for off-camera terrain chunks
- splat/normal texture streaming and fallback diagnostics
- stress validation scenes that combine terrain with decals, particles,
  selection outlines, SSAO, color grading, and weather
- shared culling diagnostics that compare terrain, static, instanced, skinned,
  decal, particle, and shadow-caster behavior in one sample panel
- representative CPU/GPU timing and memory/churn measurements before adding
  terrain-specific scalability features such as occlusion, clipmaps, or
  GPU-driven terrain culling

## Debugging

Terrain systems should expose debug visualization for chunk bounds, visibility, selected LOD, culling result, and shadow participation where practical.

On-screen labels, persistent debug capture, GPU picking, full edge stitching,
geomorphing, height-aware normal blending, roughness/metallic splatting,
parallax/displacement/tessellation, neighbor-aware streaming normal updates,
runtime normal-map baking, and editor-style terrain inspection are deferred.
