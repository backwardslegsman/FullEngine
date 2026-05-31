# Engine Integration

This document shows how a future engine integrates with the current foundation
renderer API. Keep examples synchronized with public headers as the API
stabilizes.

## Creation

The engine creates the renderer, prepares backend-neutral platform/window data,
and calls renderer initialization.

```cpp
#include "full_renderer/Renderer.hpp"

#include <memory>

std::unique_ptr<full_renderer::IRenderer> renderer =
    full_renderer::createRenderer();

full_renderer::RendererInitDesc init = {};
init.backbufferWidth = 1280;
init.backbufferHeight = 720;
init.shaderBinaryDirectory = "build/shaders/dx11";
// init.window.nativeDisplay/nativeWindow are filled by the app/platform shell.

if (renderer->initialize(init) != full_renderer::RendererResult::Success)
{
    // Report initialization failure through the host application.
}
```

The renderer does not own platform pointers in `PlatformWindowDesc`. Their
lifetime must exceed renderer shutdown. The shader directory path is copied
during initialization; it must contain compiled shader binaries matching the
active backend profile.

Build modes, dependency roles, presets, and the Windows/D3D11-first platform
matrix are documented in `docs/build.md`. The renderer library target does not
compile or link the SDL3 sample app, Dear ImGui diagnostics UI, or sample-owned
validation harness. The sample app is optional and remains the only layer that
uses SDL3 directly.

## CMake package consumption

Installed builds export the renderer as `FullRenderer::full_renderer`:

```cmake
find_package(FullRenderer CONFIG REQUIRED)
target_link_libraries(my_engine PRIVATE FullRenderer::full_renderer)
```

The installed public headers keep the same include form:

```cpp
#include "full_renderer/Renderer.hpp"
```

The package installs renderer headers and the library target only. It does not
make SDL3, Dear ImGui, the sample app, shader compiler, tests, tools, or the
sample `src/engine_bridge` adapter part of the engine-facing target. When the
package is built with the bgfx backend enabled, its config file calls
`find_dependency(bgfx CONFIG)` so static-library link dependencies resolve
through the consuming project dependency setup. The consumer smoke project in
`tests/consumer_smoke` is the build-level validation that this works from an
installed prefix without source-tree include paths. Shader binaries are not
installed with the package in this milestone; engines remain responsible for
compiling or copying backend/profile-specific shader assets and passing their
runtime directory through `RendererInitDesc`.

## Frame lifecycle

The engine owns simulation and pushes stable render data into the renderer each
frame. Mesh/material resources are created outside the frame loop and draw
packets are submitted between `beginFrame` and `endFrame`.

```cpp
full_renderer::MeshHandle mesh = renderer->createMesh(meshDesc);
full_renderer::MaterialHandle material = renderer->createMaterial(materialDesc);
```

Renderer-facing asset descriptors are not importer objects. Engines or import
tools should convert authored data into the contracts in `docs/assets.md` before
resource creation. Meshes use meters, Y-up, right-handed coordinates, finite
non-zero normals, linear vertex colors, 16-bit triangle indices, and
non-degenerate triangles. `TextureDesc` uploads uncompressed, single-mip RGBA8
bytes and carries explicit `TextureSemantic` plus `TextureColorSpace` metadata
for color, linear data, normal maps, terrain splats, LUTs, and debug textures.
Skeletons require one root, parent-before-child ordering, finite inverse bind
matrices, and skinned vertices with integer joint indices and normalized
weights. Invalid descriptors fail resource creation before backend resources
are allocated.

The current `full_engine` asset path defines a renderer-free cooked manifest
shape for generic asset metadata and terrain chunk asset references. Engine
tools can validate that manifest, summarize declared dependencies, compare
mesh/material/texture asset IDs with an externally supplied
`RendererAssetHandleCatalog`, and convert missing handles into retained
renderer-free load intent. `TerrainManifestLoadState` owns the retained
manifest value, latest readiness/load/staging diagnostics, and a pending load
request queue; it still does not own file IO, async jobs, renderer resources,
or renderer handle catalogs. Once an external loader or test seam creates
renderer handles, `consumeTerrainManifestAssetLoadRequests` can copy those
caller-owned handles into a runtime handle catalog with ordered diagnostics and
all-or-nothing queue clearing. This is the current loading shape, not a
production importer or background streaming system.

Basic mesh materials expose a small alpha policy through `MaterialDesc`.
`MaterialAlphaMode::Opaque` is the default and writes depth in the normal
forward path. `MaterialAlphaMode::AlphaTest` uses
`MaterialDesc::alphaCutoff` and also writes depth, but alpha-clipped shadow
casting is deferred and reported as an unsupported shadow/material combination.
`MaterialAlphaMode::AlphaBlend` is for simple transparent world geometry: it
renders after opaque and alpha-test work within its static, skinned, or
instanced draw family, depth tests, disables depth writes, and is stable-sorted
back-to-front inside that family. Terrain splat materials remain opaque.
Decals, particles, and structure fade are separate frame descriptors, not
gameplay material systems.

```cpp
full_renderer::FrameDesc frame = {};
frame.backbufferWidth = 1280;
frame.backbufferHeight = 720;
frame.deltaSeconds = 1.0 / 60.0;

if (renderer->beginFrame(frame) == full_renderer::RendererResult::Success)
{
    full_renderer::DrawItem draw = {};
    draw.mesh = mesh;
    draw.material = material;
    // draw.model is a column-major local-to-world matrix.
    draw.bounds = meshWorldBounds;
    draw.castsShadow = true;

    full_renderer::RenderPacket packet = {};
    packet.view = view;
    packet.directionalLight = light;
    packet.directionalShadow.enabled = true;
    packet.directionalShadow.mapResolution = 1024;
    packet.directionalShadow.centerWorld[0] = cameraPosition.x;
    packet.directionalShadow.centerWorld[1] = terrainCenterY;
    packet.directionalShadow.centerWorld[2] = cameraPosition.z;
    packet.directionalShadow.extentMeters = 48.0f;
    packet.directionalShadow.depthBias = 0.004f;
    packet.directionalShadow.strength = 0.45f;
    packet.directionalShadow.cascadeCount = 4;
    packet.directionalShadow.cascadeSplitMode =
        full_renderer::ShadowCascadeSplitMode::Practical;
    packet.directionalShadow.cascadeSplitLambda = 0.5f;
    packet.directionalShadow.cascadeShadowDistanceMeters = 96.0f;
    packet.directionalShadow.cascadeCameraNearMeters = 0.1f;
    packet.directionalShadow.cascadeCameraFarMeters = 100.0f;
    packet.environment.skyEnabled = true;
    packet.environment.fogEnabled = true;
    packet.environment.fogStartMeters = 96.0f;
    packet.environment.fogEndMeters = 180.0f;
    packet.weather.enabled = true;
    packet.weather.wind.enabled = true;
    packet.weather.wind.directionWorld[0] = 1.0f;
    packet.weather.wind.speedMetersPerSecond = 4.0f;
    packet.weather.precipitation.enabled = true;
    packet.weather.precipitation.type = full_renderer::PrecipitationType::Rain;
    packet.weather.precipitation.intensity = 0.35f;
    packet.weather.wetness.enabled = true;
    packet.weather.wetness.amount = 0.25f;
    packet.weather.fogBlend.enabled = true;
    packet.weather.fogBlend.blendAmount = 0.20f;
    packet.weather.fogBlend.fogDistanceScale = 0.8f;
    packet.colorGrading.enabled = true;
    packet.colorGrading.tonemap.enabled = true;
    packet.colorGrading.tonemap.operatorType =
        full_renderer::TonemapOperator::AcesApproximation;
    packet.colorGrading.tonemap.exposureStops = 0.0f;
    packet.colorGrading.contrast = 1.05f;
    packet.colorGrading.saturation = 1.0f;
    packet.selectionOutline.enabled = true;
    packet.selectionOutline.thicknessPixels = 3.0f;
    draw.selected = engineSelectionState.isSelected(drawId);
    packet.drawItems = &draw;
    packet.drawItemCount = 1;

    renderer->submit(packet);
    renderer->endFrame();
}
```

The engine or app owns camera movement and supplies column-major view and
projection matrices. SDL3 and bgfx types do not appear in render packets.

The directional shadow API is frame-local. Enabling
`packet.directionalShadow` requests cascaded shadow maps for terrain and lit
basic mesh receivers. Terrain chunks, opt-in `DrawItem`s, opt-in
`InstancedDrawDesc` batches, and opt-in `AnimatedDrawItem` skinned submissions
can cast when they provide finite world-space bounds. The renderer owns the backend render targets and recreates them
when the requested resolution or active cascade count changes. Shadow caster
selection is separate from camera visibility: submitted resident terrain chunks
whose bounds intersect a cascade light/shadow frustum can cast, including chunks
outside the camera frustum. The renderer does not predict or require streamed-out
terrain to cast. Mesh, instanced mesh, and skinned mesh casters are selected by
their supplied aggregate or conservative animated bounds. Skinned shadow casters
reuse the CPU-provided palette submitted for forward rendering; the renderer
does not evaluate animation clips or CPU-skin vertices to compute bounds.

Shadow resource changes take effect at the next frame submission. Keeping the
same enabled `mapResolution` and `cascadeCount` reuses existing backend targets;
disabling shadows releases active cascade targets. If target allocation fails,
the renderer skips shadow passes for that frame, binds neutral shadow state for
terrain and mesh receivers, and reports the failure in `RendererStats`.

Cascade fields in `DirectionalShadowDesc` compute up to four split ranges,
debug frusta, per-cascade caster lists, and per-cascade shadow maps. Terrain
splat and basic mesh shaders select the active cascade by camera-view depth. Keep
`cascadeCameraNearMeters` and `cascadeCameraFarMeters` synchronized with the
projection matrix supplied in `RenderViewDesc`.

`packet.environment` controls the lightweight open-world background and fog.
The renderer draws an optional fullscreen sky gradient behind the forward pass
and applies optional linear distance fog to terrain, static meshes, instanced
meshes, and skinned meshes. Fog distances are meters in camera view-space depth. Colors
are linear RGB values and are blended before shader-side gamma output.

`packet.weather` provides small weather render hooks. It is not a weather
simulation system: the engine owns weather state, transitions, gameplay effects,
and precipitation spawning. The renderer consumes current-frame wind,
precipitation, wetness, and fog-blend descriptors only. Wind currently produces
validated backend-private uniform/state and debug stats for shader/sample use.
Precipitation descriptors are hints and diagnostics; visible rain/snow/dust is
submitted by the engine as ordinary particle batches. Wetness is reversible
per-frame render state; the current backend supports simple terrain and basic
mesh color darkening. Fog blend modifies the effective submitted fog color and
distances when base fog is enabled.

`packet.ssao` controls a small renderer-owned screen-space ambient occlusion
path. The descriptor owns no backend resources and is copied during the frame
submission. When enabled, the backend captures submitted scene depth into a
private full-resolution R32F target, evaluates a fixed depth-only AO shader,
optionally runs a simple horizontal/vertical separable blur, and composites that
AO over the current frame as a simple darkening pass. AO evaluation can run at
full or half resolution; half-resolution targets round up odd viewport sizes
and clamp tiny viewports to at least one AO pixel. `debugVisualize` switches the
composite to grayscale AO output for inspection. Disabled SSAO submits no SSAO
depth capture, AO, blur, or composite passes. The implementation does not add a
G-buffer, normal buffer, temporal accumulation, material rewrite, or public
texture preview handle.

`packet.colorGrading` controls a backend-private final color adjustment path.
The descriptor is copied during `submit`, owns no resources, and exposes no
scene color targets, LUT textures, framebuffers, or backend handles. When
enabled, the backend routes the scene through the existing private scene color
target and applies exposure, optional Reinhard or ACES-style tonemapping,
contrast, saturation, gamma, lift, and gain in the final fullscreen
scene-present pass. Neutral enabled values should match the previous output.
Enabled non-finite values reject submission, while out-of-range finite values
are clamped during CPU-side planning. LUT intent can be supplied through
`LutDesc`, but active LUT sampling is deferred in this milestone; missing or
unsupported LUTs fall back to no LUT and report diagnostics.

`packet.selectionOutline` controls a renderer-owned outline pass from
externally supplied selection state. The engine or sample decides what is
selected and sets per-submission `selected` flags; the renderer only consumes
those flags during the frame. Static `DrawItem`s and skinned
`AnimatedDrawItem`s are selected per draw. `InstancedDrawDesc::selected`
currently selects the whole instanced batch, not individual instances. When
enabled and at least one submission is selected, the backend fills a private
mask target depth buffer with submitted visible scene geometry, renders selected
geometry into that mask with depth testing, and composites a global colored
outline over the frame. This keeps outlines tied to the submitted camera view
for visible surfaces. Gameplay picking, mouse rays, editor selection state,
object ID readback, through-wall outlines, and terrain editing selection remain
outside the renderer.

Structure fade is also externally controlled. The engine or sample decides
which roofs, walls, or other structure draws should fade and attaches a
`FadeDesc` to the submitted `DrawItem`, `InstancedDrawDesc`, or
`AnimatedDrawItem`. The renderer does not run obstruction tests, know about
buildings or rooms, or retain fade state after the frame. Static and skinned
fade is per draw; instanced fade is batch-level. Dithered fade is the preferred
opaque-style mode because it keeps depth writes and avoids a transparency
sorting system. Alpha fade is available for simple fades, disables depth writes
for the affected draw, and follows the same stable draw-family sort as
alpha-blend materials. Existing `castsShadow` flags still decide shadow
casting, so faded structures cast full shadows by default.

`packet.decals` is the first projected decal foundation. It is a frame-local
pointer to caller-owned `DecalSubmitDesc` data. Each `DecalDesc` is an oriented
box projector described by a finite local-to-world transform, positive
half-extents in meters, optional texture handle, tint color, opacity, and a
deterministic sort key. The renderer validates the top-level list, builds a
CPU-side plan for up to `kMaxFrameDecals`, rejects invalid individual volumes,
computes conservative world-space bounds and world-to-decal matrices, and
reports submitted, active, camera-frustum culled, rejected, invalid,
max-count, fallback, rendered, and pass-draw counts in `RendererStats`.
Camera-frustum culling is enabled by default for submitted decals; disable
`cullAgainstViewFrustum` only for diagnostics or unusual caller-owned
visibility policies. The projection depth limit and depth-edge fade fields are
in meters; zero keeps the full projector volume with a hard edge.

When active decals are present, the backend switches the opaque scene to a
private color/depth target for that frame, captures readable depth for world
position reconstruction, composites albedo/tint decals after SSAO, and then
presents the private scene color before selection outlines and debug UI. If
there are no active decals, the renderer keeps the direct-to-swapchain path.
Decals do not affect shadow maps, SSAO depth, terrain materials, mesh
materials, or gameplay ownership.

`packet.particles` is a frame-local billboard particle submission path. The
engine or sample owns all emission, lifetime, simulation, collision, and
weather/effects behavior. The renderer only validates and draws particle data
submitted for the current frame:

```cpp
std::vector<full_renderer::Particle> particles = simulateParticlesOutsideRenderer();

full_renderer::ParticleBatchDesc particleBatch = {};
particleBatch.particles = particles.data();
particleBatch.particleCount = static_cast<std::uint32_t>(particles.size());
particleBatch.texture = smokeTexture; // optional; zero uses white fallback.

full_renderer::ParticleSubmitDesc particleSubmit = {};
particleSubmit.enabled = true;
particleSubmit.batches = &particleBatch;
particleSubmit.batchCount = 1;
particleSubmit.cullAgainstViewFrustum = true;
particleSubmit.sortMode = full_renderer::ParticleSortMode::SubmissionOrder;
particleSubmit.softParticlesEnabled = true;
particleSubmit.softParticleFadeDistanceMeters = 0.75f;

packet.particles = &particleSubmit;
```

Weather-driven precipitation uses the same ownership model:

```cpp
full_renderer::WeatherDesc weather = {};
weather.enabled = true;
weather.precipitation.enabled = true;
weather.precipitation.type = full_renderer::PrecipitationType::Rain;
weather.precipitation.intensity = 0.5f;
weather.precipitation.usesParticleBatches = true;

std::vector<full_renderer::Particle> rain =
    simulateRainOutsideRenderer(weather, cameraPosition, frame.deltaSeconds);

full_renderer::ParticleBatchDesc rainBatch = {};
rainBatch.particles = rain.data();
rainBatch.particleCount = static_cast<std::uint32_t>(rain.size());
rainBatch.texture = rainTexture;

full_renderer::ParticleSubmitDesc rainSubmit = {};
rainSubmit.enabled = true;
rainSubmit.batches = &rainBatch;
rainSubmit.batchCount = 1;

packet.weather = weather;
packet.particles = &rainSubmit;
```

Particle arrays and batch arrays are valid only for the `submit` call. The
renderer copies accepted particles into a CPU-side frame plan and expands them
as camera-facing quads in the backend. Each particle uses world-space meters,
linear RGBA tint, a square world-size, optional rotation in radians, and an
optional UV rectangle. The renderer computes conservative batch bounds from
valid particles and can cull whole batches against the active camera frustum.
Submission order remains the default, with optional deterministic batch
back-to-front or per-batch particle back-to-front sorting for simple
transparent effects. Particles render after decals and before selection
outlines, with alpha blending, depth test enabled, and depth writes disabled.
Soft particles use backend-private scene depth for alpha fade near opaque
intersections when the depth input is available; otherwise they render as hard
billboards and report the fallback in stats. They do not cast shadows, receive
CSM, run simulation in the renderer, or expose backend texture handles.

## Resize

The app or engine detects window resize outside the renderer. For the current
foundation, call `resize` with the current physical backbuffer size between
frames and keep subsequent `FrameDesc` dimensions synchronized.

```cpp
full_renderer::RendererResizeDesc resize = {};
resize.backbufferWidth = newPixelWidth;
resize.backbufferHeight = newPixelHeight;
renderer->resize(resize);
```

## Shutdown

Shutdown is idempotent and releases renderer-owned backend state. Explicitly
destroy renderer-owned resources when their lifetime ends; shutdown also cleans
up any remaining Phase 1 resources deterministically.

```cpp
renderer->destroyMaterial(material);
renderer->destroyMesh(mesh);
renderer->shutdown();
```

## Resource Lifetime And Frame Data

Renderer resource handles are opaque and owned by the renderer instance that
created them. Zero/default handles are invalid. Non-zero handles can become
stale after destruction, renderer shutdown, slot reuse in generation-based
systems, or use with another renderer instance. Destroying invalid, stale, or
already destroyed handles is ignored. Submitting stale mesh, texture, material,
terrain, skinned mesh, decal, particle, fade, or selection data rejects, skips,
or falls back according to the descriptor being used and increments diagnostics
where practical. Backend texture, framebuffer, render target, and view handles
remain private.

Creation descriptors for meshes, skinned meshes, textures, materials, skeletons,
and terrain chunks are copied or converted during the creation call. Per-frame
draw items, instanced matrices, animated skinning palettes, decal arrays,
particle arrays, weather descriptors, fade descriptors, selection flags, camera
views, and debug lines are consumed during the submit call for that frame. The
renderer must not retain pointers to caller-owned frame arrays after the
documented call/frame lifetime.

Optional texture references have deterministic fallback behavior. A destroyed
or stale decal texture falls back to the decal tint/color path when fallback
resources are live; a destroyed or stale particle texture falls back to the
particle white texture path; terrain layer albedo/normal textures and chunk
splat maps fall back to material defaults. If a required fallback resource
cannot be created, the affected optional pass skips and reports invalid-resource
diagnostics while final presentation remains the responsibility of the scene
present path. Skinned mesh submissions are stricter: missing or stale skinned
mesh handles, missing palettes, or palette counts that do not match the
registered skeleton are rejected.

## Boundaries

The renderer must not require ECS objects, gameplay state, physics data, AI
state, networking, or editor-only types. Translate those systems into renderer
descriptors or render packets outside renderer core.

## Debug UI

Dear ImGui is optional and debug-facing. It is not required by the renderer
library public API and no ImGui types appear in `full_renderer` headers. To
build the SDL3 sample with the terrain diagnostics panel, enable the CMake
option and vcpkg manifest feature together:

```powershell
cmake --fresh -S . -B build-debug-ui `
  -G "Visual Studio 17 2022" `
  -DCMAKE_TOOLCHAIN_FILE="D:/FullRenderer/external/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DVCPKG_MANIFEST_FEATURES=debug-ui `
  -DFULL_RENDERER_ENABLE_DEBUG_UI=ON
```

The sample owns SDL3 input forwarding through the Dear ImGui SDL3 backend. A
small internal bgfx wrapper submits ImGui draw data after renderer terrain
debug overlays. Runtime controls:

- F1 toggles the ImGui diagnostics panel.
- F2 cycles through sample-owned camera bookmarks for repeatable visual checks.
- F3 cycles through sample-owned validation presets and applies their bookmark.
- F5 toggles terrain chunk bounds.
- F6 toggles selected-LOD overlay.
- F7 toggles splat fallback overlay.
- F8 toggles combined terrain diagnostics.
- F9 toggles cascade frustum wires.
- F10 toggles cascade caster bounds.
- F11 resets CSM validation, environment, weather, SSAO, color grading, decal, particle, outline, structure fade, selection, validation camera, and debug overlay defaults.

The terrain diagnostics panel groups CSM controls into enable/reset, cascade
configuration, bias/filtering, resource/overlay controls, per-cascade stats, and
projection stability tables. It exposes the terrain shadow toggle, map
resolution, depth bias, slope bias, filter mode, strength, split mode/lambda,
shadow distance, stable projection, blend bands, and independent overlay toggles
for light bounds, shadow casters, cascade frusta, and cascade casters. The
light-bounds and cascade-frustum wireframes are computed from the actual
light view/projection matrices used by the shadow passes. The panel includes a
backend-private shadow-map preview toggle. When enabled, the sample asks the
internal bgfx debug path to remap the selected cascade's current R32F shadow
texture into an RGBA preview target, then displays that target through ImGui.
The controls include cascade selection, preview size, black/white depth remap,
and inversion; no bgfx texture handles are exposed through the public renderer
API. It reports requested versus active shadow
resources, allocation/recreate status, per-cascade caster counts, off-camera
caster counts, rejects, invalid-resource rejects, LOD buckets, texel sizes,
snap offsets, and snapped projection centers. Active cascades render depth into
their own backend shadow targets and are sampled by terrain and lit mesh
materials according to camera-view depth, with optional adjacent-cascade blend
bands near non-final split boundaries.
The panel is diagnostic only. It does not add editor state, terrain editing, GPU
picking, persistent capture, or gameplay selection behavior.

The diagnostics panel also includes a Selection / Outline section. It exposes
the global outline toggle, linear outline color, pixel thickness, sample-owned
selection toggles for one static mesh, the instanced batch, and the skinned
mesh, plus backend stats for selected submissions, mask draws, outline composite
draws, and mask target validity. These controls mutate sample-owned render
packet data only; they do not introduce renderer-owned selection state.

The Structure Fade section exposes sample-owned fade state: a visibility
scalar, alpha/dither mode, and toggles for one static mesh, the instanced batch,
and the skinned mesh. It reports fade descriptor counts, visible/partial/hidden
breakdowns, alpha/dither draw counts, and unsupported target counts. The sample
does not implement camera obstruction detection, roof ownership, or building
gameplay rules.

The Materials / Shaders / Transparency section reports material counts by kind,
opaque/alpha-test/alpha-blend draw counts, transparent sorted versus unsorted
draw counts, particle transparent batch sorting, dither-fade draw counts,
fallback material and texture counters, shader variants observed this frame,
unsupported variant requests, and alpha-material shadow-caster warnings. It is
diagnostic only; it does not expose shader programs, backend resource handles,
or material editor state.

The sample scene is a lightweight CSM validation scene. It generates a larger
procedural terrain grid, places multiple static cube casters through the view
depth range, and submits an instanced cube batch that casts onto terrain. The
default camera and shadow settings are chosen to make multiple cascade regions,
caster categories, blend bands, and stable projection behavior easy to inspect.
The optional debug UI also exposes an Environment section for sky colors, fog
color, fog start/end distances, and reset-to-default behavior.

The diagnostics panel includes a Weather section. It mutates sample-owned
weather state, maps that state into `WeatherDesc`, and, for precipitation,
generates ordinary sample-owned particle batches. Clear/rain/snow/dust buttons
are simple sample presets. The UI exposes wind direction/speed, precipitation
type/intensity/tint/scale, wetness amount/darkening, terrain/mesh wetness
toggles, weather fog blend, the sample precipitation particle budget, and stats
for effective wind, precipitation, wetness draws, fog blend, and clamped values.
The renderer does not own a weather timeline, emission system, clouds, or
gameplay weather effects.

The diagnostics panel includes an SSAO section with enable, half-resolution,
blur, debug visualize, radius, blur radius, intensity, bias, power, max
distance, sample count, reset-to-default, active AO resolution, and backend
status counters. The AO buffer can be visualized only through the backend-owned
fullscreen debug path; generic ImGui/backend texture previews and actual
shadow-map texture previews remain out of scope for this milestone.

The diagnostics panel includes a Color Grading section with neutral, high
contrast, and desaturated presets. It exposes enable state, tonemap operator,
exposure, contrast, saturation, gamma, lift/gain, LUT intent, and debug
isolation modes. Renderer stats report scene-target validity, final pass
submission, effective values after clamping, LUT fallback state, and whether
active LUT sampling is supported. Selection outlines, debug overlays, and Dear
ImGui remain after grading so they stay crisp and ungraded.

The diagnostics panel also includes a Scene / Post Passes section. It reports
the viewport, shared scene color/depth target status, readable-depth
requirements, final present mode, planned pass counts, fullscreen pass counts,
invalid resource counts, scene target recreate/failure counters, and a compact
reason mask for why the backend-private intermediate target was required. The
order remains explicit rather than graph-scheduled: SSAO, decals, particles,
scene/color-graded present, selection outline, debug overlays, and Dear ImGui.
These diagnostics are backend-neutral and expose no bgfx texture, framebuffer,
or view ID handles. Selection masks are owned separately and no longer force
the shared scene color/depth target by themselves; hard particles also keep the
direct presentation path and require readable scene depth only when soft
particles are both enabled and accepted for rendering. A separate skipped
reason mask reports empty decal lists, empty particle plans, missing selected
objects, missing scene targets, missing readable depth, invalid viewport, or
pass-list limits.

The diagnostics panel also includes a Resource Lifetime section. It reports
live renderer-owned mesh, skinned mesh, texture, material, and skeleton counts;
backend-neutral approximate byte estimates for mesh buffers, textures,
materials, fallback textures, shadow targets, scene targets, SSAO targets, and
selection targets; lifetime allocation failure counters; resource recreate
counters; fallback resource validity; and invalid/stale handle-use counters.
The byte values are CPU-side estimates derived from submitted descriptors and
viewport target dimensions, not GPU memory queries. Destroyed or stale handles
never expose backend resources; destroy calls ignore them, while submissions
reject or skip them according to the API contract and increment diagnostics
where practical.

The diagnostics panel includes a Visual Regression section. It is a
sample-owned validation harness rather than editor tooling: named presets set
the sample camera bookmark, feature toggles, selected submissions, decal debug
volumes, and simple debug overlays for repeatable manual checks. The current
presets cover BaselineDirect, Outline_Static, Outline_Skinned, Decal_Terrain,
Decal_Mesh, Combined_PostPasses, Resize_Check, All_Features_Smoke,
Resource_Resize_PostTargets, Resource_AllOptionalPasses_Toggle, LargeScene_*,
and OpenWorld_* churn presets. Camera
bookmarks store position, target, field of view, and near/far distances, then
apply through the sample camera variables; they do not mutate renderer
internals. Use Outline_Static and Outline_Skinned to check that selection masks
follow the submitted camera and that skinned outlines use the same palette and
transform convention as forward skinned draws. Use Decal_Terrain and
Decal_Mesh while moving toward and around the target decals to check for
world-locked projection, stable depth reconstruction, and conservative culling
near frustum edges. Combined_PostPasses is intended to smoke-test SSAO,
decals, particles, color grading, selection/outline, debug overlays, and Dear
ImGui together.

The diagnostics panel also includes a Large Scene / Culling Diagnostics
section. It aggregates backend-neutral counts for terrain chunks, static mesh
draws, GPU-instanced batches, skinned draws, decals, particles, and CSM shadow
casters. Terrain, decals, and particles report their active CPU culling paths.
Static, instanced, and skinned rows use finite submitted bounds for diagnostic
frustum classification while preserving the current forward submission
behavior. The LargeScene_* presets exercise terrain grids, static meshes,
instancing, skinned actors, decals, particles, shadow casters, all renderable
types together, optional passes disabled, large-coordinate/origin policy, and
resize/reconfiguration smoke checks. These presets adjust sample-owned stress
counts through bounded debug controls; the renderer still consumes ordinary
per-frame descriptors and does not own gameplay population, streaming, or
visibility policy.

The diagnostics panel also includes a Frame Budget section for large-scene
validation. It shows the active preset/bookmark, CPU-side renderer planning
time by stage, submitted/visible/culled totals, staged-byte estimates,
terrain/resource churn, post-pass submitted/skipped counts, and adjustable
warning thresholds. These diagnostics are intentionally approximate: they use a
host CPU timer and known renderer staging counters, not GPU timing queries,
global allocator hooks, or machine-specific benchmark gates. The panel can hold
a snapshot while the sample keeps rendering.

The diagnostics panel also includes an Open World Churn / Long Session
section. It is a deterministic harness owned by the sample/tests, not a
streaming subsystem: fixed-seed options simulate many frames of ordinary
renderer-facing create/destroy/update/submit churn for terrain chunks,
materials, textures, LOD resources, decals, particles, skinned palettes,
optional pass toggles, and resize/post-target recreation. Controls allow a
fast run, an opt-in heavy run, material fallback churn, decal/particle churn,
optional pass churn, resize churn, and an engine streaming seam mode. The
summary reports created, destroyed, reused, stale/invalid attempts, fallback
counts, frame-data lifetime checks, peak live resources, staged-byte estimates,
reset/final-present status, engine-to-renderer mapping counts, current origin
mode, large-coordinate warnings, and camera-relative diagnostics.

`src/engine_bridge/StreamingSeam.*` is the current integration seam. It is not
renderer core and it is not installed as the public renderer API. It
demonstrates the pattern a future engine should own: engine chunk IDs map to
renderer-owned `TerrainChunkHandle` values, engine-side residency decisions
call create/update/destroy on the seam, and missing/destroyed mappings are
deterministic diagnostics rather than backend access. The seam stores only
public renderer handles; it never exposes bgfx objects, framebuffers, view IDs,
or shader programs.

The diagnostics panel also includes a Decals section. It exposes sample-owned
decal planning, camera-frustum culling, projection-depth, depth-edge fade, and
debug-volume toggles plus backend stats for submitted, active, culled,
rejected, fallback-color, debug-volume, pass-draw, depth-input, and color-input
counts. The sample supplies a few lightweight decal volumes, including an
off-camera decal for culling stats and generated RGBA decal textures when
texture creation succeeds, without adding gameplay decal spawning or editor
state.

The diagnostics panel also includes a Particles section. It exposes the
renderer particle toggle, sample-owned emission toggle/count, batch culling,
sort mode, soft-particle fade, and stats for submitted/accepted/rejected/culled
batches and particles, sorted batches/particles, fallback texture batches, draw
calls, depth-input status, and backend particle resource validity. The sample
emits a small app-owned smoke plume, an off-camera batch for culling stats, and
optional weather-driven precipitation particles by generating per-frame
`Particle` arrays and submitting them through `ParticleSubmitDesc`; the
renderer does not retain or simulate that state.

The sample also creates a tiny two-joint skinned mesh. The sample app computes
final skinning palettes on the CPU and submits them through `AnimatedDrawItem`;
the renderer only uploads and applies those matrices. The skinned draw opts into
CSM shadow casting with conservative bounds so its depth-only skinned pass can
cast onto terrain and other lit receivers. The Animation section in the optional
debug UI can draw skeleton bone lines and skinned mesh bounds.

The sample marks one static cube, the instanced cube batch, and the two-joint
skinned mesh as selected by default so the outline pass can be validated without
adding picking or interaction logic.

The sample terrain meshes are generated from deterministic chunk-local height
samples and uploaded with generated geometric normals. The sample terrain
material also creates small procedural tangent-space normal maps for each splat
layer. Engines can use the same helper path for test terrain, or upload their
own `MeshVertex::normal` values and layer normal textures through
`TerrainMaterialLayerDesc::normalTexture`. Runtime terrain material editing is
not part of the sample; changing normal-map strength or texture handles requires
creating or replacing renderer-owned material resources outside the frame.

The sample also enables generated terrain skirts for its shared terrain LOD
meshes. Skirts are ordinary mesh geometry created before `createMesh`; they
duplicate edge vertices and extend them downward by a fixed meter depth. The
sample expands chunk bounds by that depth so culling, shadows, and debug bounds
cover the skirted geometry. Engines that change skirt depth should rebuild and
reupload the affected terrain mesh resources rather than attempting per-frame
reconfiguration.

Terrain residency remains engine/sample owned. The renderer owns only
generation-tracked terrain chunk records; the host decides which chunks are
created, updated, destroyed, and submitted for the frame. Use
`updateTerrainChunk` between frames when streamed chunk bounds, LOD descriptors,
or splat-map residency changes but the engine-owned chunk ID should keep the
same renderer chunk handle. Destroyed chunks and stale handles are safe to
submit but produce no draw or shadow work and are counted in terrain
diagnostics. `TerrainStats` exposes resident/nonresident slots,
create/update/destroy/reuse churn, invalid/stale handles, splat
fallback/resident counts, LOD buckets, and shadow caster terrain counts for
streaming-style validation. `copyTerrainShadowCasterDebugInfo` exposes
backend-neutral terrain shadow-caster records and cascade indices when terrain
or shadow debug capture requests them.

Large-world coordinates are caller-rebased by policy. Engine integrations that
use floating origins should keep absolute world state in engine-owned
double-precision data, choose an origin at frame boundaries, and subtract that
origin before filling terrain bounds, camera positions, view matrices, decals,
particles, shadow centers, and debug data for the frame. The engine bridge
`FrameOriginDesc` demonstrates this conversion and reports precision-risk
warnings. Renderer core still treats submitted coordinates as single-precision
render space and does not own world streaming policy, origin-shift timing, IO,
or persistence.

For a first engine integration, start from the sample CSM validation defaults:
four cascades, 1024 texel shadow maps, practical split mode, lambda near `0.5`,
stable projection enabled, cascade blending enabled, 2x2 PCF filtering, and
debug overlays disabled until needed. Tune depth and slope bias from the debug
panel while inspecting the per-cascade previews. Remaining Phase 2 limitations
are intentional: no shadow atlas, no PCSS/contact shadows, no animated actor
shadows, no occlusion-aware caster selection, no shadow caching, no per-object
shadow masks, and no editor-facing shadow tools.

## Engine-readiness plan

For a real open-world engine, treat the current renderer as prototype-ready and
package-consumer ready, not production-ready. An engine can start integrating
against the public descriptors, external ownership model, validation presets,
and installed `FullRenderer::full_renderer` package target now. Production use
still needs the gates tracked in `docs/agents/roadmap.md`.

The most important remaining integration work is:

- replacing the current deterministic engine-bridge seam with a real
  engine-owned streaming implementation. The sample/tests now exercise
  repeated terrain, material, texture, LOD, decal, particle, skinned palette,
  optional pass, shadow, scene-target, post-target, mapping, and origin churn
  through fixed-seed renderer-facing calls, but intentionally do not implement
  async IO, background jobs, or world persistence.
- validating the caller-rebased large-world precision contract on real content
  at representative scale. The bridge proves the conversion and diagnostics;
  production integration still needs engine-side policy for origin choice,
  streaming regions, and asset residency.
- building import-time validation for mesh units/axes/tangents, texture
  color-space/mip/compression policy, material slots, terrain data, skeletons,
  and skinned weights before relying on large streamed content
- defining runtime shader asset responsibility for installed packages: engine
  owned shader packaging, or a future optional installed shader component with
  backend/profile-specific discovery
- maturing material and transparency behavior for production assets,
  especially alpha-tested shadows, cross-family transparent ordering limits,
  and any future PBR expansion
- collecting representative CPU/GPU timing and memory/churn data on real
  large scenes before adding scalability systems such as occlusion, Hi-Z, or
  GPU-driven culling
- extending CI only for platforms/backends the project is ready to support, and
  documenting unverified targets as experimental
- adding backend-safe internal texture previews, capture tooling, and later
  image-regression workflows without exposing backend handles through public
  APIs
