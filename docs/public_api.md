# Public Renderer API

## Goals

The public renderer API should let a future engine initialize the renderer,
create resources, submit frame data, respond to resize events, inspect debug
information, and shut down cleanly.

The current foundation exposes lifecycle, resize, resource creation, terrain
chunk create/update/destroy, frame boundary, render submission, lightweight
stats, and terrain chunk/batch/shadow-caster debug snapshot calls.

## Header

The current public include is:

```cpp
#include "full_renderer/Renderer.hpp"
```

All public symbols live in the `full_renderer` namespace.

Renderer-facing asset contracts for meshes, textures, materials, terrain, and
skeletons are summarized in `docs/assets.md`.

Public headers are expected to compile for an engine target without including
SDL3, Dear ImGui, bgfx, or backend-private headers. The
`full_renderer_public_header_compile_tests` test target exercises this boundary.
Installed packages place these headers under `<prefix>/include/full_renderer`
and export the CMake target `FullRenderer::full_renderer`.

```cmake
find_package(FullRenderer CONFIG REQUIRED)
target_link_libraries(engine PRIVATE FullRenderer::full_renderer)
```

The exported target propagates the public include directory and C++17 compile
feature. It does not expose SDL3, Dear ImGui, bgfx handles, shader programs,
framebuffers, render targets, or view IDs. Static bgfx link dependencies are
resolved by the installed package config through `find_dependency(bgfx CONFIG)`
when the package was built with the bgfx backend enabled.

## Current API shape

```cpp
std::unique_ptr<IRenderer> renderer = createRenderer();

RendererInitDesc init = {};
init.backbufferWidth = 1280;
init.backbufferHeight = 720;
init.shaderBinaryDirectory = "build/shaders/dx11";

RendererResult result = renderer->initialize(init);
```

`RendererInitDesc` and `FrameDesc` use physical backbuffer pixels. Frame time is
seconds and must be finite and non-negative. Public renderer calls are owned by
one renderer thread and do not currently provide synchronization.

`PlatformWindowDesc` stores optional backend-neutral native pointers. The
renderer does not own or destroy those objects. The bgfx backend requires a
valid native window handle for initialization.

`shaderBinaryDirectory` is borrowed for the duration of `initialize` and copied
internally. The current forward and terrain slices expect compiled mesh and
terrain shader binaries plus debug/utility post-pass binaries, including the
particle billboard and color grading shaders, in that directory.

Resize is explicit:

```cpp
RendererResizeDesc resize = {};
resize.backbufferWidth = 1920;
resize.backbufferHeight = 1080;
renderer->resize(resize);
```

Call `resize` between frames after the host receives a window resize or pixel
size change event.

## Resource and submission flow

The Phase 1 renderer uses opaque handles for renderer-owned resources:

```cpp
full_renderer::MeshDesc mesh = {};
mesh.vertices = vertices;
mesh.vertexCount = vertexCount;
mesh.indices = indices;
mesh.indexCount = indexCount;

full_renderer::MeshHandle meshHandle = renderer->createMesh(mesh);

full_renderer::MaterialDesc material = {};
material.alphaMode = full_renderer::MaterialAlphaMode::Opaque;
full_renderer::MaterialHandle materialHandle =
    renderer->createMaterial(material);
```

`MeshDesc` uses the fixed `MeshVertex` format and 16-bit triangle indices.
Source arrays are copied during creation and can be released or reused after the
call returns.

`TextureDesc` creates renderer-owned RGBA8 textures from tightly packed CPU
pixel data. Source pixels are copied during creation. Materials and terrain
chunks reference textures by handle and do not own or destroy them. Texture
descriptors now carry backend-agnostic asset-contract metadata:
`TextureSemantic` describes the intended slot or data meaning, and
`TextureColorSpace` describes expected interpretation. Runtime uploads remain
uncompressed, single-mip RGBA8; multi-mip and compressed runtime payloads are
rejected until a packed asset path exists. Color textures use sRGB metadata,
linear masks/splats/LUTs use linear metadata, and normal maps use encoded-normal
metadata.

`MaterialDesc` currently supports `MaterialKind::Basic` and
`MaterialKind::TerrainSplat`. Basic materials can use
`MaterialAlphaMode::Opaque`, `MaterialAlphaMode::AlphaTest`, or
`MaterialAlphaMode::AlphaBlend`. Alpha-test materials use
`MaterialDesc::alphaCutoff` in `[0, 1]` and remain in the opaque-style
depth-write path. Alpha-blend materials render after opaque and alpha-test
work within their static, skinned, or instanced draw family, with depth testing
enabled, depth writes disabled, and deterministic back-to-front sorting.
Terrain splat materials must remain opaque; decals and particles use their own
frame descriptors rather than `MaterialDesc` resources. Opaque basic materials
can cast CSM shadows when the draw opts in. Alpha-test and alpha-blend basic
materials do not cast shadows in the first policy; alpha-test shadow clipping is
deferred and diagnosed when requested.

Public resource handles are renderer-owned opaque tokens. Zero/default handles
are invalid; non-zero handles can still be stale after destruction, renderer
shutdown, resource-manager reset, or use with a different renderer instance.
Destroying invalid, stale, or already destroyed handles is ignored. Submitting
invalid or stale draw, material, texture, skinned mesh, terrain, decal,
particle, fade, or selection data never dereferences backend resources; the
renderer rejects, skips, or falls back according to the descriptor contract and
reports invalid/stale handle diagnostics where practical. Mesh, skinned mesh,
and texture creation data is copied during creation. Frame-local render
packets, draw arrays, instance matrices, skinned palettes, decals, particles,
weather, fade, selection, and camera descriptors are consumed during the
current frame submission and must remain valid for the call that submits them.
Destroyed decal and particle texture handles use their documented fallback
color/white texture path when fallback resources are valid; if those backend
fallbacks are unavailable, the optional pass skips safely and reports invalid
resource diagnostics. Terrain material layer textures and chunk splat maps use
their deterministic material fallbacks when referenced textures are stale.
Skinned submissions with missing, stale, or invalid palette data are rejected
before backend resources are touched.

Frame submission is explicit and happens between `beginFrame` and `endFrame`:

```cpp
full_renderer::DrawItem draw = {};
draw.mesh = meshHandle;
draw.material = materialHandle;
// draw.model is a column-major local-to-world matrix.
draw.bounds = meshWorldBounds;
draw.castsShadow = true;

full_renderer::RenderPacket packet = {};
packet.view = view;
packet.directionalLight = light;
packet.directionalShadow.enabled = true;
packet.directionalShadow.centerWorld[0] = cameraX;
packet.directionalShadow.centerWorld[1] = 0.0f;
packet.directionalShadow.centerWorld[2] = cameraZ;
packet.directionalShadow.extentMeters = 48.0f;
packet.selectionOutline.enabled = true;
packet.drawItems = &draw;
packet.drawItemCount = 1;

renderer->submit(packet);
```

`SelectionOutlineDesc` is frame-local renderer input owned by the caller. The
renderer does not perform picking, hit testing, persistent selection management,
or object-ID readback. Submissions opt into the outline with
`DrawItem::selected`, `InstancedDrawDesc::selected`, or
`AnimatedDrawItem::selected`; selection data is consumed only during the current
`submit` call. The first implementation uses one global outline color and a
pixel thickness clamped by the backend. Static mesh draws and skinned mesh draws
are selected per draw. Instanced selection is batch-level: the whole submitted
`InstancedDrawDesc` is outlined together, and per-instance selection IDs are
deferred. The backend owns the selection mask render target and recreates it on
viewport resize; no texture, framebuffer, SDL, bgfx, or ImGui handles are
exposed through public APIs. The current depth policy first fills the mask
target depth buffer with submitted visible scene geometry, then draws selected
geometry with depth testing, so outlines follow the submitted camera view and
are intended for visible selected surfaces. Occluded through-wall outlines and
terrain editing selection are not part of this API.

`FadeDesc` is frame-local externally owned structure-visibility input. The
renderer does not decide which roofs, buildings, rooms, or camera obstructions
fade; callers attach a fade descriptor directly to `DrawItem`,
`InstancedDrawDesc`, or `AnimatedDrawItem` for the current submission. A
visibility of `1.0` preserves ordinary rendering, while `0.0` is fully faded by
the selected mode. Static and skinned submissions fade per draw. Instanced fade
is batch-level in this milestone, so every instance in the batch uses the same
scalar. `FadeMode::Dithered` keeps the opaque-style depth/write path and uses a
stable screen-space dither discard in the shared mesh fragment shader.
`FadeMode::Alpha` uses alpha blending with depth testing enabled and depth
writes disabled; alpha-faded static, skinned, and instanced submissions follow
the same stable draw-family sorting as alpha-blend materials. Faded objects
keep their existing full CSM shadow casting policy unless the caller changes
`castsShadow`. Selection outlines still use submitted selection flags; SSAO,
decals, and soft particles continue to use the renderer's current scene depth
behavior. No building IDs, roof ownership, obstruction queries, or backend
handles are exposed.

## Skinned Mesh Rendering

The first Phase 3 animation API is intentionally small. Create
`SkeletonHandle` metadata with `createSkeleton`, then create a
`SkinnedMeshHandle` from fixed skinned vertices that reference that skeleton.
Each `AnimatedDrawItem` supplies final CPU-evaluated skinning matrices for the
current frame:

```cpp
full_renderer::SkeletonHandle skeleton = renderer->createSkeleton(skeletonDesc);
full_renderer::SkinnedMeshHandle skinned = renderer->createSkinnedMesh(skinnedDesc);

full_renderer::AnimatedDrawItem animated = {};
animated.mesh = skinned;
animated.material = materialHandle;
animated.palette.skinningMatrices = finalSkinningMatrices;
animated.palette.matrixCount = jointCount;
animated.receivesShadow = true;
animated.castsShadow = true;
animated.bounds = conservativeAnimatedBounds;

packet.animatedDraws = &animated;
packet.animatedDrawCount = 1;
```

Skeleton and skinned mesh data is copied at creation. Palette pointers are
frame-local and read only during `submit`. The renderer does not evaluate clips
or own engine animation state. The current path supports forward rendering and
CSM receiving. Opaque skinned meshes can also cast into active CSM cascades when
`castsShadow` is true, the submitted palette is valid for the mesh skeleton, and
the caller supplies finite conservative world-space bounds. The renderer does
not CPU-skin vertices to derive bounds and does not retain palette pointers
after submission.

The host owns camera behavior and supplies column-major view/projection
matrices. The renderer reads draw packets only for the duration of `submit`.
`DrawItem::castsShadow` is opt-in and requires finite world-space bounds.
`InstancedDrawDesc` provides the same frame-local submission and optional
aggregate shadow bounds for GPU-instanced mesh batches.

`DirectionalShadowDesc` provides cascaded directional shadow configuration. When
enabled, the renderer creates or reuses one square backend-owned shadow render
target per active cascade, selects submitted resident terrain chunks and
opt-in mesh/instanced/skinned draw bounds that intersect each cascade light
frustum, and renders those caster batches into the cascade shadow targets.
Terrain splat
materials and lit basic mesh materials sample the active cascades. Unlit basic
mesh materials remain unshadowed. `mapResolution` is validated in the range
`128..4096`, `extentMeters` is the orthographic half-size in world meters,
`depthBias` is a normalized comparison bias, `slopeBias` adds receiver bias at
grazing light angles, and `strength` is a `[0, 1]` darkening multiplier.
`filterMode` selects nearest, 2x2 PCF, or 3x3 PCF shadow sampling for terrain
and lit basic mesh receivers.
`centerWorld` is still used as the light-box center for the current simple
cascade projections; engines typically track it to the camera.
`stableCascadeProjection` is enabled by default and snaps each cascade's
light-space projection center to shadow texel increments using the active map
resolution. `cascadeBlendEnabled` and `cascadeBlendFraction` control a simple
terrain shader blend band near non-final cascade split boundaries.
`debugDrawLightBounds` and `debugDrawShadowCasters` request GPU debug wire
overlays for the actual cascade light frusta and selected caster chunks. The
light-frustum wires are computed internally from the same view-projection
matrices used by the shadow passes; no backend handles are exposed.

The descriptor also contains CPU-only cascade preparation fields:
`cascadeCount`, `cascadeSplitMode`, `cascadeSplitLambda`,
`cascadeShadowDistanceMeters`, `cascadeCameraNearMeters`, and
`cascadeCameraFarMeters`. Cascade count is clamped to `1..4`; split modes are
uniform, logarithmic, and practical blended splits. Blend fraction is clamped to
`0..0.5` before shader upload. These fields drive debug
cascade frusta, per-cascade terrain caster diagnostics, per-cascade shadow depth
passes, and terrain shader cascade selection. The renderer allocates one backend
shadow target per active cascade and binds the active cascade shadow maps to
terrain splat materials and lit basic mesh materials.

Shadow resource reconfiguration is backend-owned and applied at frame
submission time. Changing `enabled`, `mapResolution`, or `cascadeCount` causes
the backend to release or recreate cascade shadow targets at the next shadow
submission. Reusing the same enabled configuration preserves the existing
targets. Skinned shadow casters use the submitted CPU palette with a depth-only
skinned shader path. If allocation fails, the renderer skips shadow rendering for that
frame, binds neutral fallback shadow state for forward passes, and reports the
failure through `RendererStats`; no bgfx handles are exposed publicly.

Shadow stats are exposed through `RendererStats`: whether shadows ran, the
active shadow map resolution, total caster batch count, and shadow pass draw
count. `RendererStats` also reports active cascade count, valid cascade render
targets, per-cascade shadow draw counts, per-cascade terrain/static
mesh/instanced mesh/skinned mesh caster categories, and static/instanced/skinned
mesh receiver counts. It also reports requested versus active shadow resource configuration,
per-frame release/recreate status, lifetime recreate count, and allocation
failure count. Terrain-side caster
diagnostics are exposed through `TerrainStats`,
including selected caster chunks, off-camera casters, light-frustum rejects,
invalid shadow resource rejects, caster counts by LOD, and per-cascade caster
diagnostics for up to four CPU-side cascades.

`EnvironmentDesc` is the frame-local sky and fog descriptor. It contains no
resources and is copied during `submit`. Colors are linear RGB values in
`[0, 1]`; fog distances are meters measured as camera view-space depth. The
current renderer draws an optional fullscreen sky gradient from zenith, horizon,
and lower-sky colors, then applies optional linear distance fog to terrain splat
materials and basic static/instanced mesh materials before gamma output. Fog
uses `fogStartMeters` and `fogEndMeters`, with `fogEndMeters` required to be
greater than the start distance when fog is enabled. Disabling sky skips the sky
draw; disabling fog uploads neutral fog state.

`WeatherDesc` is a frame-local set of renderer-facing weather hooks layered on
top of the base environment and existing particle path. The caller or future
engine owns weather simulation, timing, transitions, gameplay effects, and
precipitation spawning. The renderer copies the descriptor during `submit`,
validates and clamps render-facing values, uploads backend-private uniforms, and
reports coarse stats. `WindDesc` exposes normalized world-space wind direction
and speed in meters per second for current or future shader/sample consumers.
`PrecipitationDesc` exposes type, intensity, direction, tint, and particle scale
hints, but the renderer does not spawn rain, snow, or dust; callers submit
ordinary `ParticleSubmitDesc` batches if precipitation should be visible.
`WetnessDesc` is reversible per-frame render state. The current backend applies
simple color darkening to terrain splat shading and lit/unlit basic static,
instanced, and skinned mesh shading when enabled, without modifying material
resources or adding puddles. `FogBlendDesc` blends the submitted fog color and
scales fog distances only when base fog is enabled; disabling it preserves the
base `EnvironmentDesc`. Volumetric weather, clouds, lightning, accumulation
simulation, vegetation wind, and gameplay weather systems are outside this API.

`SsaoDesc` is a frame-local backend-agnostic SSAO descriptor. It owns no
resources and is copied during `submit`. It contains only scalar tuning values:
enable state, optional half-resolution AO, optional simple separable blur, blur
radius in AO pixels, AO radius in meters, intensity, view-depth bias in meters,
contrast power, maximum encoded view distance in meters, a fixed sample-count
mode, and an optional grayscale debug visualization flag. When enabled, the
bgfx backend uses private render targets and fullscreen shaders to capture
scene depth, evaluate depth-only AO, optionally blur the AO target, and
composite the result. Half-resolution AO rounds each target dimension up and
keeps a minimum of one pixel. When disabled, SSAO submits no work. The
implementation does not expose depth/AO textures, build a G-buffer, sample a
normal buffer, or add material-specific AO controls.

`ColorGradingDesc` is a frame-local final image adjustment descriptor. It owns
no resources and is copied during `submit`. Disabled color grading preserves
the existing scene output and does not force a grading pass. Enabled neutral
values are intended to remain visually equivalent to the previous output:
tonemap disabled, exposure `0`, contrast/saturation/gamma `1`, lift `{0,0,0}`,
gain `{1,1,1}`, and LUT strength `0`. When enabled, the bgfx backend routes the
scene through the existing backend-private scene color target and folds
tonemapping and grading into the final scene-present fullscreen pass before
selection outlines, debug overlays, and Dear ImGui.

`TonemapDesc` supports a disabled/no-op mode plus simple Reinhard and
ACES-inspired approximation operators. `exposureStops` is an EV scalar where
`1` doubles scene color before the tonemap stage and `-1` halves it. Basic
grading controls include contrast, saturation, output gamma, RGB lift, and RGB
gain. Values must be finite when color grading is enabled and are clamped by
the CPU-side plan before shader upload. `LutDesc` records optional LUT intent
using a renderer texture handle and strength, but active LUT sampling is
deferred in this milestone; missing, invalid, or unsupported LUTs fall back to
no LUT and report diagnostics. No scene color texture, LUT texture, framebuffer,
bgfx, SDL, or ImGui handles are exposed publicly.

`DecalSubmitDesc` is the first Phase 3 decal foundation. It is a per-frame,
caller-owned list of up to `kMaxFrameDecals` projected decal descriptors.
Each `DecalDesc` describes an oriented box projector with a column-major
local-to-world transform, positive half-extents in meters, optional renderer
texture handle, tint color, opacity, and deterministic sort key. The renderer
validates the top-level list during `submit`, then rejects invalid individual
decal volumes in a CPU-side render plan instead of crashing the frame. Opacity
is clamped to `[0, 1]`; zero texture handles use fallback tint color. The
submission can request CPU-side camera-frustum culling, a projection depth
limit in meters, and a small depth-edge fade distance. Culling keeps partially
intersecting projector bounds active and removes only volumes fully outside the
submitted camera frustum.

When active decals are submitted, the bgfx backend renders the main scene into
a backend-private scene color/depth target, captures a readable depth texture,
and composites projected albedo/tint decals over the scene in deterministic
sort-key order. Missing decal textures use the tint/fallback path. Projection
depth tuning is applied in the decal shader after world-position
reconstruction; it does not affect shadow maps, SSAO depth, or material
resources. The scene targets, decal textures, framebuffers, bgfx, SDL, and
ImGui details remain private; public APIs expose only descriptors, handles, and
stats.

`ParticleSubmitDesc` is the first Phase 3 particle foundation. It is a
per-frame, caller-owned list of up to `kMaxFrameParticleBatches` particle
batches and `kMaxFrameParticles` individual particles. The renderer owns no
emitters, lifetime simulation, collision, weather behavior, or effects
scripting. A future engine or the sample app fills `Particle` arrays for the
current frame and passes a pointer through `packet.particles`; the renderer
validates and copies accepted particle data during `submit` only.

Each `ParticleBatchDesc` has one optional renderer-owned RGBA8 texture handle,
one blend mode, and one per-batch ordering hint. A zero or invalid texture
handle uses a backend-owned white fallback texture multiplied by each
particle's linear RGBA color. `Particle` positions are world-space meters,
`sizeMeters` is the square billboard side length in world units,
`rotationRadians` spins the quad around the view-facing axis, and `uvRect` is a
normalized texture-frame rectangle. `ParticleSubmitDesc` can cull computed
batch bounds against the active camera frustum, preserve submission order, sort
accepted batches back-to-front, or sort particles inside each batch
back-to-front. Sorting and culling operate on copied planning data and never
mutate caller arrays.

Particles alpha blend over the scene after opaque rendering, SSAO, and decals;
depth testing is enabled and depth writes are disabled. Optional soft-particle
fade captures backend-private scene depth and fades alpha near opaque
intersections. If that private depth input is unavailable, particles fall back
to normal hard alpha billboards and report the inactive state through
`RendererStats`. Particles do not cast or receive shadows. No bgfx texture,
framebuffer, SDL, or ImGui handles are exposed.

`RendererStats` reports selection/outline diagnostics when enabled: selected
static mesh draw count, selected instanced batch and instance counts, selected
skinned draw count, mask pass submission count, outline composite pass count,
and whether the backend-owned mask target was valid for the frame. Disabled
outline rendering submits no selection mask or outline composite passes.
It also reports SSAO diagnostics: whether SSAO was requested, private depth and
AO target validity, blur target validity, active AO dimensions, whether
half-resolution and blur were active, depth-capture draw count, fullscreen
AO/blur/composite pass counts, depth-input status, and whether the latest
composite displayed grayscale AO for debugging.
Decal diagnostics include submitted, active, camera-frustum culled, rejected,
invalid-descriptor rejected, max-count rejected, fallback-color, rendered, and
pass-draw counts plus scene target/input validity and the planned projection
depth/fade settings.
Particle diagnostics include submitted, accepted, rejected, and frustum-culled
batch counts; submitted, accepted, rejected, and culled particle counts; sorted
batch and particle counts; fallback texture batch counts; soft-particle
requested/active/depth-input state; draw-call count; and backend resource
status.
Color grading diagnostics include requested/enabled state, scene color target
validity, grading resource validity, final pass submission, tonemap operator,
effective exposure/contrast/saturation/gamma values, LUT requested/active/
fallback state, LUT sampling support, debug output mode, and clamped value
count.
Scene/post diagnostics report backend-neutral planning state for the shared
scene color/depth path: viewport and scene target dimensions, whether an
intermediate scene target was required or active, a reason bitmask for SSAO,
decals, soft particles, color grading, or internal forced submission,
readable-depth requirements, present mode, planned pass counts, fullscreen pass
counts, skipped/blocked reason bits, invalid resource counts, and scene target
recreate/failure counters. Selection outline masks are separate backend-owned
targets and do not require the shared scene color target by themselves. These
are diagnostics copied into `RendererStats`; they do not expose bgfx textures,
framebuffers, view IDs, or render target handles.
Weather diagnostics include enabled/neutral state, effective wind direction and
speed, precipitation type/intensity and particle-batch ownership hint, wetness
amount and terrain/mesh wetness draw counts, fog-blend amount, effective fog
color/distances, and clamped value count.
Structure fade diagnostics include submitted and active fade descriptor counts,
fully visible, partially faded, and fully hidden counts, active alpha and
dither draw counts, and unsupported fade target counts. Invalid fade
descriptors are rejected during submission validation rather than reaching the
backend.
Material/shader diagnostics include live material counts by kind; per-frame
opaque, alpha-test, alpha-blend, transparent sorted/unsorted, dither-fade,
fallback material/texture, unsupported variant, observed shader variant, and
unsupported alpha-material shadow-caster counters. These are backend-neutral
diagnostic counters; they do not expose shader programs, textures,
framebuffers, render targets, view IDs, or other backend handles.

## Terrain chunks

Terrain chunks are renderer-owned CPU records that reference existing mesh and
material resources. The renderer does not generate height data or load terrain
assets; the app or engine creates mesh/material resources first, then registers
chunks with world-space bounds and sorted LOD descriptors. LOD meshes are
chunk-local so many chunks can share the same mesh/material and be submitted as
internal GPU instancing batches. Each LOD descriptor directly names the
renderer-owned mesh and material used when that LOD is selected; invalid or
missing handles reject chunk creation rather than falling back at render time.

```cpp
full_renderer::TerrainLodDesc lods[2] = {};
lods[0].mesh = nearMesh;
lods[0].material = nearMaterial;
lods[0].maxDistanceMeters = 32.0f;
lods[1].mesh = farMesh;
lods[1].material = farMaterial;
lods[1].maxDistanceMeters = 256.0f;

full_renderer::TerrainChunkDesc chunk = {};
chunk.bounds = bounds;
chunk.lods = lods;
chunk.lodCount = 2;

full_renderer::TerrainChunkHandle handle =
    renderer->createTerrainChunk(chunk);
```

Terrain submission is optional per frame. When present, core performs CPU
frustum culling and distance LOD selection, then expands visible chunks into the
internal terrain instancing path.

```cpp
full_renderer::TerrainSubmitDesc terrain = {};
terrain.chunks = chunkHandles;
terrain.chunkCount = chunkCount;
terrain.cameraPositionWorld[0] = cameraX;
terrain.cameraPositionWorld[1] = cameraY;
terrain.cameraPositionWorld[2] = cameraZ;
terrain.debug.captureChunkInfo = true;

packet.terrain = &terrain;
renderer->submit(packet);
```

`TerrainLodDesc::maxDistanceMeters` thresholds are world-space meters and are
inclusive. LOD 0 is highest detail; higher indices are lower detail. The Phase 2
terrain API supports up to four LOD levels per chunk and does not apply
hysteresis yet. A typical integration uploads a small shared set of chunk-local
grid meshes, such as 16x16, 8x8, 4x4, and 2x2 subdivisions, then references
those meshes from every streamed chunk descriptor. The sample's internal grid
helper can generate those meshes with downward edge skirts to hide small
LOD/chunk cracks, but this is still ordinary mesh data submitted through
`createMesh`. The public terrain chunk API does not own or regenerate skirt
geometry; callers that change seam settings rebuild the mesh resources and
provide bounds that include any skirt depth.

Terrain chunk handles include an `id` and generation. Destroying a chunk marks
its slot inactive; a later chunk may reuse the slot with a new generation.
Submitting an invalid/default or stale terrain chunk handle is safe and
deterministic: it is counted in `TerrainStats`, produces no terrain draw, and
does not cast shadows.

Terrain chunk descriptors can also be replaced in place between frames:

```cpp
full_renderer::TerrainChunkDesc replacement = {};
replacement.bounds = rebasedBounds;
replacement.lods = replacementLods;
replacement.lodCount = replacementLodCount;
replacement.splatMap = currentSplatMap;

full_renderer::RendererResult updateResult =
    renderer->updateTerrainChunk(existingChunk, replacement);
```

`updateTerrainChunk` keeps the existing handle and generation stable while
copying replacement bounds, LOD descriptors, and splat map references. Engines
can use this for externally owned residency maps when streamed chunk metadata or
material residency changes but the renderer chunk slot should stay associated
with the same engine chunk ID. Invalid replacement descriptors fail without
modifying the live chunk; invalid or stale handles return
`RendererResult::InvalidArgument`.

Terrain coordinates are currently single-precision meters. Engines targeting
very large worlds should submit terrain bounds, camera positions, and view
matrices after applying their own floating-origin or camera-relative rebase.
The renderer treats submitted values as current-frame render coordinates and
does not own world streaming or origin-shift policy. The sample
`src/engine_bridge` seam demonstrates one engine-owned pattern: keep absolute
world positions outside the renderer, map engine chunk IDs to renderer chunk
handles, subtract a frame origin before building renderer descriptors, and
submit only renderer-relative floats to the public API.

Terrain splat materials are regular material handles created by setting
`MaterialDesc::kind` to `MaterialKind::TerrainSplat`. They support four albedo
layers plus optional per-layer normal maps. Chunk splat maps are optional RGBA8
textures assigned through `TerrainChunkDesc::splatMap`; R/G/B/A map to layers
0/1/2/3. Missing layer textures use layer fallback colors, and missing chunk
splat maps use full layer-0 weight. Missing terrain normal maps use a flat
tangent-space fallback. `TerrainMaterialDesc::normalMapStrength` controls how
strongly blended layer normal maps affect lighting, and
`TerrainMaterialDesc::flipNormalMapY` supports the opposite green-channel
normal-map convention. Normal maps are ordinary renderer-owned texture handles
referenced by the material; callers keep them live while the material can be
submitted.

`getTerrainStats()` reports live/resident chunk counts, allocated and inactive
chunk slots, create/update/destroy/reuse churn, invalid and stale submitted
handles, submitted/visible/culled chunks, terrain batch draw counts, visible
chunks by LOD, terrain batches by LOD, chunks using fallback splat weights,
chunks with resident splat maps, LOD fallback rejects, and directional shadow
caster-selection counts.
`copyTerrainDebugInfo()` exposes the latest captured per-chunk bounds, culling
result, selected LOD, and distance. `copyTerrainBatchDebugInfo()` exposes the
latest captured terrain instance batch records, including selected LOD and
instance count. `copyTerrainShadowCasterDebugInfo()` exposes backend-neutral
terrain caster decisions and cascade indices from the latest shadow-caster
selection diagnostics without exposing shadow-map or render-target handles.

`TerrainDebugOptions` can also request GPU terrain overlays during the same
frame submission. These options are renderer-facing debug controls, not ImGui
or editor APIs. They support chunk bounds, selected-LOD coloring, material
validity coloring, splat fallback highlighting, and a combined diagnostics mode.
The overlay uses the CPU debug records produced by the submitted terrain packet,
so enabling any GPU overlay forces chunk debug capture for that frame.

The current public API intentionally does not expose the backend shadow-map
texture for UI previews. The optional sample debug UI implements shadow-map
preview through internal renderer/bgfx headers and backend-owned preview
textures, so engine integrations still receive diagnostics through ordinary
stats and debug options instead of backend texture handles.

## Shared culling diagnostics

`RendererStats` includes `CullingCategoryStats` rows for static mesh,
GPU-instanced, and skinned draw submissions. These counters are backend-neutral
debug data: submitted descriptors, public-handle validity, diagnostic
camera-frustum visibility, culled counts, shadow-caster counts, fallback counts
where applicable, draw submissions, and usable-bounds counts. Terrain,
decals, particles, and shadow cascades continue to expose their richer
category-specific counters through `TerrainStats` and existing renderer stats.

For static, instanced, and skinned categories, frustum-visible and
frustum-culled counts are diagnostic when finite bounds are available. Bounds
that are optional in the public descriptor contract remain optional; invalid
optional bounds are treated as unbounded/visible for diagnostics so existing
forward rendering behavior is not changed. Shadow-casting bounds remain
required by the shadow-caster descriptor rules.

## Frame budget diagnostics

`RendererStats::frameBudget` exposes backend-neutral CPU/frame submission
budget diagnostics for the latest frame. It reports CPU-side timing slots in
microseconds, estimated staged bytes for static draws, instancing, skinned
palettes, decals, particles, and debug lines, coarse draw/pass submission
counts, terrain chunk churn, render-target recreate counts, and post-pass
submitted/skipped counts.

These values are debug diagnostics, not a profiler contract. CPU timings use a
monotonic host timer and do not include GPU execution time. Staged byte and
allocation counts are exact only for known renderer staging containers; they do
not replace global allocator tracking, GPU memory queries, or external profiling
tools. No bgfx buffers, textures, framebuffers, render targets, or view IDs are
exposed.

## Result codes

Recoverable public operations return `RendererResult` instead of throwing across
the engine boundary. Callers should treat non-`Success` values as lifecycle,
validation, or backend errors and report them through the host application's
logging layer.

Resource creation returns invalid handles on validation or backend failure.
Destroying invalid handles is ignored. Non-zero destroyed or foreign handles
are treated as stale and must not produce backend draws. Terrain chunk handles
include a generation value so stale handles can be rejected after slot reuse.
`RendererStats` exposes live resource counts, approximate memory estimates,
allocation failure counters, resize/recreate counters, and invalid/stale handle
use counters for diagnostics. Memory values are estimates based on descriptor
sizes and render-target dimensions, not exact GPU accounting.
Allocation-failure coverage now includes mocked CPU-side tests for mesh,
skinned mesh, texture, material, decal, particle, and skinned-palette resource
paths; real backends report the categories they can observe without GPU
readback or public backend handles.

## Documentation requirements

Every public interface should use Doxygen-compatible comments and document:

- ownership
- lifetime
- thread expectations
- units and coordinate conventions
- whether data is copied, referenced, or consumed
- whether calls are valid before, during, or after a frame
- parameter semantics, return values, and error behavior

Keep SDL3 and bgfx types out of public APIs unless an explicitly low-level
extension is being implemented.

## Public API hardening plan

Before the renderer is considered open-world-engine ready, public APIs should
have stable contracts for:

- stale, destroyed, invalid, and reused handle behavior across all resource and
  submission paths
- frame-local pointer lifetimes for draw items, animated palettes, decals,
  particles, weather, fade, and debug data
- optional pass disable behavior for SSAO, decals, particles, selection,
  color grading, and scene present
- renderer-facing but externally owned systems such as selection, weather,
  particles, building fade, and animation sampling
- backend-neutral diagnostics for memory estimates, resource allocation
  failures, skipped passes, and resize/reconfiguration
- production large-world precision policy beyond the current caller-rebased
  single-precision render-space convention and sample engine-bridge seam
- installed-package integration details, especially runtime shader asset
  responsibility and any future optional shader install component
- asset pipeline validation boundaries so importer/tooling contracts can grow
  without pulling importer-specific types into the runtime public API
