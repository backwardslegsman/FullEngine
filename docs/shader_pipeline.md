# Shader Pipeline

## Source layout

Current source layout:

```text
assets/shaders/
  forward/
    varying.def.sc
    vs_sky.sc
    fs_sky.sc
    vs_mesh.sc
    vs_mesh_instanced.sc
    vs_skinned_mesh.sc
    fs_mesh.sc
  terrain/
    vs_terrain_instanced.sc
    fs_terrain_splat.sc
  shadows/
    vs_terrain_shadow_instanced.sc
    fs_shadow_depth.sc
  debug/
    vs_debug_line.sc
    fs_debug_line.sc
    vs_selection_mesh.sc
    vs_selection_instanced.sc
    vs_selection_skinned.sc
    fs_selection_mask.sc
    vs_outline_fullscreen.sc
    fs_outline_composite.sc
    vs_ssao_depth_mesh.sc
    vs_ssao_depth_instanced.sc
    vs_ssao_depth_skinned.sc
    fs_ssao_depth.sc
    vs_ssao_fullscreen.sc
    fs_ssao.sc
    fs_ssao_blur.sc
    fs_ssao_composite.sc
    fs_decal_projected.sc
    fs_scene_present.sc
    fs_color_grade.sc
    vs_particles.sc
    fs_particles.sc
    vs_imgui.sc
    fs_imgui.sc
    vs_shadow_preview.sc
    fs_shadow_preview.sc
```

## Rules

- Keep shared shader code in `common/` once shared helpers exist.
- Use clear naming by render pass.
- Store generated shader binaries outside source shader directories.
- Add shader compilation commands to the build or documented tooling.
- Include shader reflection metadata later if needed for material binding.
- Keep gamma correction explicit and tested.

## Current tooling

The `full_renderer_shaders` CMake target compiles `vs_sky.sc`, `fs_sky.sc`, `vs_mesh.sc`,
`vs_mesh_instanced.sc`, `vs_skinned_mesh.sc`, `fs_mesh.sc`, `vs_terrain_instanced.sc`, `fs_terrain_splat.sc`,
`vs_terrain_shadow_instanced.sc`, `vs_skinned_shadow.sc`, `fs_shadow_depth.sc`, `vs_debug_line.sc`,
`fs_debug_line.sc`, `vs_selection_mesh.sc`, `vs_selection_instanced.sc`,
`vs_selection_skinned.sc`, `fs_selection_mask.sc`, `vs_outline_fullscreen.sc`,
`fs_outline_composite.sc`, `vs_ssao_depth_mesh.sc`,
`vs_ssao_depth_instanced.sc`, `vs_ssao_depth_skinned.sc`,
`fs_ssao_depth.sc`, `vs_ssao_fullscreen.sc`, `fs_ssao.sc`,
`fs_ssao_blur.sc`, `fs_ssao_composite.sc`, `fs_decal_projected.sc`,
`fs_scene_present.sc`, `fs_color_grade.sc`, `vs_particles.sc`,
`fs_particles.sc`, `vs_imgui.sc`, `fs_imgui.sc`,
`vs_shadow_preview.sc`, and `fs_shadow_preview.sc` with bgfx shaderc from the
vcpkg `bgfx[tools]` host dependency. The target is enabled by
`FULL_RENDERER_ENABLE_SHADER_COMPILE` and is required by the sample build
because the sample points at build-local shader outputs. Generated binaries are
written to the active build directory:

```text
<build-dir>/shaders/dx11/
  vs_mesh.bin
  vs_mesh_instanced.bin
  vs_skinned_mesh.bin
  fs_mesh.bin
  vs_sky.bin
  fs_sky.bin
  vs_terrain_instanced.bin
  fs_terrain_splat.bin
  vs_terrain_shadow_instanced.bin
  vs_skinned_shadow.bin
  fs_shadow_depth.bin
  vs_debug_line.bin
  fs_debug_line.bin
  vs_selection_mesh.bin
  vs_selection_instanced.bin
  vs_selection_skinned.bin
  fs_selection_mask.bin
  vs_outline_fullscreen.bin
  fs_outline_composite.bin
  vs_ssao_depth_mesh.bin
  vs_ssao_depth_instanced.bin
  vs_ssao_depth_skinned.bin
  fs_ssao_depth.bin
  vs_ssao_fullscreen.bin
  fs_ssao.bin
  fs_ssao_blur.bin
  fs_ssao_composite.bin
  fs_decal_projected.bin
  fs_scene_present.bin
  fs_color_grade.bin
  vs_particles.bin
  fs_particles.bin
  vs_imgui.bin
  fs_imgui.bin
  vs_shadow_preview.bin
  fs_shadow_preview.bin
```

Use the `shader-validation` CMake preset for a CI-style shader-only build:

```powershell
cmake --preset shader-validation
cmake --build --preset shader-validation
```

When the sample is configured without Dear ImGui diagnostics
(`FULL_RENDERER_ENABLE_DEBUG_UI=OFF`), the target omits `vs_imgui.sc`,
`fs_imgui.sc`, `vs_shadow_preview.sc`, and `fs_shadow_preview.sc`. Those shaders
remain part of the debug-UI sample path only.

CMake passes that directory to the sample through
`FULL_RENDERER_SAMPLE_SHADER_DIR`, and the sample forwards it through
`RendererInitDesc::shaderBinaryDirectory`.

The installed `FullRenderer` CMake package does not currently install generated
shader binaries. This keeps the renderer library package relocatable and avoids
encoding source-tree or build-tree shader paths into engine consumers. External
engines should compile, copy, or package shaders with their own runtime asset
pipeline, then pass the resolved directory through
`RendererInitDesc::shaderBinaryDirectory` during initialization. The
source-tree `full_renderer_shaders` target remains the supported validation
path for the Windows/D3D11 profile.

Packaging decision for the current P2 milestone: do not install shader binaries
yet, even optionally. The active shader outputs are backend/profile-specific
Windows/D3D11 `s_5_0` binaries and the renderer does not yet have a
cross-backend shader asset convention, install component, or package-config
runtime discovery variable. CI validates shader compilation separately from
the library package/consumer-smoke path. A future optional shader install path
should define an install component, a backend/profile directory convention, and
a documented way for engine code to map that install location into
`RendererInitDesc::shaderBinaryDirectory`.

Current target:

- platform: `windows`
- profile: `s_5_0`
- include paths: bgfx shader includes and `assets/shaders/`
- varying definition: `assets/shaders/forward/varying.def.sc`

The sky pass renders a fullscreen gradient before forward terrain/mesh draws.
It uses linear zenith, horizon, and lower-sky colors from `EnvironmentDesc` and
performs explicit shader-side linear-to-gamma conversion before writing to the
swapchain. It does not sample textures, cubemaps, shadow maps, or post-process
buffers.

The current static mesh shader accepts the fixed Phase 1 mesh vertex format, applies
one basic directional light and material color, applies optional CSM shadowing
and distance fog, and performs explicit shader-side linear-to-gamma conversion
before writing to the swapchain.

Asset descriptors now carry texture semantic and color-space metadata, but the
first runtime upload path still supplies raw RGBA8 bytes to shaders. Import
tools should provide color textures as sRGB-authored data, normal maps as
encoded normal data, and splat/scalar maps as linear data. Shader paths keep
their existing assumptions: material colors are linear, terrain splat weights
are linear, and terrain normal maps are unpacked from encoded RGB.

The basic instanced mesh path uses `vs_mesh_instanced.sc` with four bgfx
instance data vectors that encode a column-major model matrix per terrain
chunk. The basic skinned mesh path uses `vs_skinned_mesh.sc` with four joint
indices and weights per vertex plus a CPU-provided `u_skinningPalette[64]`;
after vertex skinning it shares `fs_mesh.sc` with the static and instanced mesh
paths. Terrain splatting uses `vs_terrain_instanced.sc` and
`fs_terrain_splat.sc`. The terrain fragment shader samples four layer textures
and one RGBA splat map, normalizes weights, optionally samples four
tangent-space layer normal maps, applies simple directional lighting, applies
optional CSM shadowing and distance fog, and performs explicit shader-side
linear-to-gamma conversion before writing to the swapchain. Terrain normal maps
use encoded RGB tangent-space normals with `(128, 128, 255)` as flat; the shader
blends them by splat weights, applies the material normal strength, and
transforms the result with a chunk-local X/Z tangent basis plus the geometric
mesh normal.

CSM depth passes use `vs_terrain_shadow_instanced.sc`, `vs_skinned_shadow.sc`,
and `fs_shadow_depth.sc` to render selected terrain batches, opt-in static mesh
draw items, opt-in instanced mesh batches, and opt-in skinned mesh draws into
single-channel shadow render targets. Static mesh casters are submitted as
one-instance depth batches so the same instanced depth shader path can be
reused. Skinned casters bind the same CPU-provided `u_skinningPalette[64]`
used by the forward skinned path, then run a depth-only skinned vertex shader
without material texture sampling. The backend allocates one target per active cascade
and renders one depth pass for each cascade. Targets are reused while the active
cascade count and resolution are unchanged, released when shadows are disabled,
and recreated at submission time when those settings change. Allocation failure
falls back to neutral shadow state for the frame instead of exposing backend
handles or stale textures. `fs_terrain_splat.sc` and
`fs_mesh.sc` receive the active cascade split distances, per-cascade light
view-projection matrices, and one shadow-map sampler per cascade. They select
the first cascade whose far split contains the fragment view-space depth, apply
constant plus slope-scaled receiver bias, sample the selected cascade with
nearest, 2x2 PCF, or 3x3 PCF filtering, optionally sample the next cascade
inside the configured blend band, and darken direct lighting by the configured
shadow strength.
Cascade projection stabilization is handled on the CPU before these matrices
are uploaded. Static, instanced, and skinned mesh forward paths share the same
mesh fragment shader and therefore the same CSM receive behavior. PCSS/contact
shadows, atlases, alpha-tested skinned shadows, and transparent skinned shadows
are deferred.

Distance fog is linear in camera view-space depth. The backend uploads
`u_environmentColors` and `u_fogParams` for the sky, terrain, and mesh forward
programs. Fog is applied after material lighting and shadowing, before gamma
conversion. Disabled fog uploads a zero enabled flag and leaves material colors
unchanged.

Weather hooks add a small backend-private uniform path without a material
rewrite. The backend builds an effective environment from `EnvironmentDesc` plus
`WeatherDesc::fogBlend`, then uploads that result through the same fog uniforms.
It also uploads `u_weatherParams`, `u_weatherWindParams`, and
`u_weatherPrecipitationParams`. The current visual weather hook is simple
wetness darkening: `fs_terrain_splat.sc` reads the terrain wetness amount from
`u_weatherParams.x`, and `fs_mesh.sc` reads the basic mesh/skinned/instanced
wetness amount from `u_weatherParams.y`; both use `u_weatherParams.z` as the
maximum darkening factor. Wind and precipitation uniforms are available for
future shader consumers and debug stats, while visible precipitation is still
submitted as ordinary particle batches by the app or engine. Disabled weather
uploads neutral values and adds no passes.

Structure fade uses the shared mesh fragment shader instead of a material
rewrite. Static mesh, basic instanced mesh, and skinned mesh forward draws bind
`u_fadeParams` from the submitted `FadeDesc`. The same uniform also carries
basic material alpha policy for `fs_mesh.sc`: `.z` is the material alpha mode
(`0` opaque, `1` alpha-test, `2` alpha-blend) and `.w` is the alpha-test
cutoff. `FadeMode::Dithered` keeps the ordinary opaque state, depth writes
included, and discards fragments with a stable 4x4 screen-space dither
threshold. `FadeMode::Alpha` multiplies output alpha by the visibility scalar
and submits that draw with alpha blending and depth writes disabled. Alpha
blend and alpha fade draws are stable-sorted back-to-front within static,
skinned, and instanced draw families; the renderer does not add
order-independent transparency. Terrain splat materials do not consume the
fade or alpha-test uniform in this milestone. Shadow depth passes skip
alpha-test and alpha-blend material casters by policy, so alpha-clipped shadow
casting is deferred. Dither-faded objects continue casting their normal full
shadows unless the caller disables `castsShadow`.

SSAO is a small backend-private fullscreen path controlled by `SsaoDesc`.
When enabled, the backend captures submitted static, instanced, terrain, and
skinned geometry depth into a private full-resolution R32F color target using
`vs_ssao_depth_mesh.sc`, `vs_ssao_depth_instanced.sc`,
`vs_ssao_depth_skinned.sc`, and `fs_ssao_depth.sc`. The AO shader is depth-only:
`fs_ssao.sc` samples the private depth target with a fixed 4- or 8-tap pattern
and applies radius, intensity, bias, and power controls. The AO target can be
full viewport size or half resolution; half-resolution dimensions are rounded
up per axis and clamped to at least one pixel. When blur is enabled,
`fs_ssao_blur.sc` runs twice as a simple non-depth-aware separable blur:
horizontal into a temporary target and vertical into the blurred AO target.
`fs_ssao_composite.sc` samples either the raw or blurred AO result and then
alpha-blends it as a simple scene darkening pass, or displays it as grayscale
when debug visualization is enabled. No terrain, mesh, or material shader is
rewritten for SSAO, and no backend texture or framebuffer handle is exposed
through public APIs.

Projected decals use CPU-side oriented-box plans plus backend-private scene
targets. When active decals are present, the forward scene renders into a
private RGBA color target with a private depth attachment. A depth-capture pass
writes normalized view depth to an R32F texture, `fs_decal_projected.sc`
reconstructs world position and projects it into each decal volume, and
`fs_scene_present.sc` resolves the composited scene color back to the swapchain
before selection outlines when color grading is disabled. No forward terrain,
mesh, or skinned material shader samples decal data, and no public texture or
framebuffer handle is exposed.
Depth reconstruction uses the submitted camera's current inverse projection and
inverse view matrices, with a signed homogeneous divide before scaling the
view ray by captured view depth. CPU frustum culling uses conservative
projector bounds near frustum edges so small camera movements do not drop
nearly visible decals abruptly.
The decal shader supports simple receiver-depth tuning: a normalized projector
depth limit discards receiver points beyond the configured projection range,
and an optional edge fade attenuates decal alpha near that depth limit. This is
an albedo/tint color composite only; it does not modify normals, roughness,
lighting, shadow maps, or SSAO depth.

Color grading is folded into the backend-private scene present path. When
`ColorGradingDesc::enabled` is true, the backend routes scene color through the
private scene target and submits `fs_color_grade.sc` with the existing
fullscreen vertex shader. The fragment shader samples scene color, follows the
renderer's existing gamma-to-linear convention for post-scene adjustment,
applies exposure, optional Reinhard or small ACES-inspired tonemapping,
contrast, saturation, gamma, lift, and gain, then writes gamma output to the
swapchain. LUT intent is validated and reported by the CPU plan, but active LUT
sampling is deferred in this milestone; unsupported or missing LUTs fall back
to no LUT. Selection outlines, debug overlays, and Dear ImGui are submitted
after this pass so they remain ungraded.

Scene/post/present planning is now centralized in a small CPU-side helper. The
helper does not schedule a render graph; it records the explicit order used by
the backend and reports why the shared scene color/depth target is needed:
SSAO, projected decals, accepted soft particles, color grading, or an internal
forced scene-target path. Selection outline masks are separate backend-owned
resources and do not require the shared scene target on their own. The intended
order after opaque scene rendering is SSAO, projected decals, particles,
scene/color-graded present, selection mask/outline, debug overlays, then Dear
ImGui. SSAO runs before decals so AO uses the opaque scene depth before decal
color compositing; particles run after decals and before grading so transparent
effects are part of the final image adjustment; selection outlines remain after
grading so they stay crisp and ungraded. The shared diagnostics also track
scene target dimensions, readable-depth requirements, resize/recreate events,
skipped or blocked pass reasons, invalid resources, and planned fullscreen pass
counts without exposing backend handles. Resource lifetime diagnostics share the
same backend-private ownership boundary: shadow, scene, SSAO, selection, and
fallback targets report recreate/allocation counters and approximate CPU-side
memory estimates, but no bgfx texture, framebuffer, render target, or view ID
handles leave backend code.

Particles use a small backend-private billboard pass driven by
`ParticleSubmitDesc`. The CPU particle planner validates frame-local batches
and particles, computes conservative batch bounds, optionally culls whole
batches against the active camera frustum, and can deterministically sort
accepted batches or particles back-to-front. The bgfx backend expands accepted
particles into transient quad vertex/index buffers. `vs_particles.sc`
transforms those already-expanded world-space quad vertices with the submitted
camera view/projection and passes view depth to the fragment shader.
`fs_particles.sc` samples the batch texture, or the backend white fallback,
multiplies by per-particle linear RGBA color, optionally fades alpha against a
backend-private captured scene-depth texture for soft particles, then performs
explicit gamma output. Particles render after SSAO and active projected decals,
before selection outlines and debug UI. The pass alpha blends, depth tests
against the current scene, disables depth writes, and does not touch material
shaders, shadow maps, or CSM receiving.

## Shader variant discipline

The renderer tracks a small internal shader variant key for diagnostics and
policy validation. The key is backend-private and never exposes shader program
handles. Variant names should follow the pass-first convention already used in
the shader tree: `forward/static`, `forward/instanced`, `forward/skinned`,
`terrain/splat`, `shadow/depth`, `selection/mask`, `decal/projected`,
`particle/billboard`, `post/color-grade`, and `debug/*`.

Allowed variant axes for the current renderer are:

- transform path: static, instanced, skinned, or terrain splat
- material alpha mode: opaque, alpha-test, or alpha-blend
- lit/unlit and CSM receive state
- fog enabled
- terrain layer normal maps
- alpha fade or dither fade
- utility pass family: shadow, selection, decal, particle, color grading, or
  debug

Adding a new shader should be justified by a new pass family or a feature axis
that cannot be represented clearly by the existing shared shader plus uniforms.
Otherwise, prefer common shader helpers and the existing variant key so
diagnostics can count unsupported or missing combinations.

## Material render order

The first material ordering policy is explicit but not a full transparency
framework:

1. CSM shadow depth passes for opaque supported casters.
2. sky/background.
3. static mesh opaque and alpha-test draws, then static mesh alpha-blend/alpha
   fade sorted within the static draw family.
4. skinned mesh opaque and alpha-test draws, then skinned alpha-blend/alpha
   fade sorted within the skinned draw family.
5. instanced terrain/basic mesh opaque and alpha-test batches, then instanced
   alpha-blend/alpha fade sorted within the instanced batch family.
6. SSAO/depth-dependent passes.
7. projected decals.
8. particles with their configured particle sort mode.
9. scene present and optional color grading.
10. selection/outline.
11. debug overlays.
12. Dear ImGui.

This keeps deterministic submission behavior while documenting the limitation:
transparent objects are not globally sorted across static, skinned, instanced,
and particle families.

Selection outlines, debug overlays, and Dear ImGui remain after color grading
so they stay crisp and ungraded.

The debug line shaders draw GPU wire boxes for terrain diagnostics. The ImGui
shaders are used only by the optional sample debug UI path and are isolated from
normal renderer materials. They render Dear ImGui draw data through bgfx after
the forward/debug terrain views.

Selection outline rendering is also debug/utility style shader work, but it is
driven by public frame submission flags rather than ImGui. The backend renders
selected static, instanced, and skinned submissions into a private RGBA8 mask
target using `vs_selection_mesh.sc`, `vs_selection_instanced.sc`,
`vs_selection_skinned.sc`, and `fs_selection_mask.sc`. It then samples that mask
with `vs_outline_fullscreen.sc` and `fs_outline_composite.sc`, detects outside
mask edges, and alpha-blends a global outline color over the backbuffer. The
mask target is resized with the viewport and is not exposed through public APIs.
The skinned selection vertex path uses the same CPU palette, `u_model[0]`, and
`u_viewProj` projection convention as the forward skinned mesh shader so mask
positions do not drift or invert relative to visible skinned geometry.
Before selected pixels are written, the backend fills the mask target depth
buffer from the submitted visible scene geometry, including terrain/instanced
batches and skinned draws. The selected mask pass then uses `LEQUAL` depth
testing, so the outline follows the submitted camera view instead of drawing
selected objects through unrelated geometry. Through-wall outlines, per-object
colors, object ID readback, and per-instance selection are deferred.

`vs_shadow_preview.sc` and
`fs_shadow_preview.sc` are also debug-only; they sample a selected cascade's
R32F shadow texture, apply configurable black/white remap and optional
inversion, and write an RGBA preview target for ImGui display. The preview uses
backend-owned resources and does not change normal terrain or mesh shader
behavior. The CSM diagnostics panel controls only
existing renderer/debug flags and sample-owned shadow descriptors; it does not
introduce shader variants or editor-owned render state.

## Shader hardening plan

Before adding more visual effects, remaining shader work should focus on
alpha-tested shadow clipping, tighter color-space validation for imported
assets, shader compilation validation in normal configure/build/test workflows,
backend profile selection beyond the current Windows sample target, and a
backend-safe debug texture preview abstraction for internal targets without
exposing backend handles.

For production open-world integration, shader binaries also need a runtime
asset convention. The current package deliberately leaves shader binaries under
the engine or sample's asset responsibility. A future milestone should either
keep that engine-owned policy and validate it in the consumer smoke project, or
add an optional installed shader component with backend/profile directories,
configuration rules, and a package-config discovery story that maps cleanly
into `RendererInitDesc::shaderBinaryDirectory`.
