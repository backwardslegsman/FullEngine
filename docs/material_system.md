# Material System

## Current scope

Material support is intentionally small but now has an explicit first policy so
new paths do not grow as one-off shader variants.

Initial material properties:

- base color
- basic lit/unlit forward material
- alpha mode for basic materials: opaque, alpha test, or alpha blend
- terrain splat material with four albedo layers and optional layer normal maps
- renderer-owned RGBA8 textures

Texture slots follow the asset contract in `docs/assets.md`. Albedo-like color
textures use `TextureSemantic::Color` with sRGB metadata, scalar masks and
terrain splat maps use linear metadata, and normal maps use encoded-normal
metadata. The current runtime does not perform a full importer color conversion;
it validates the contract and keeps shader/backend handles private.

## First material model

The first renderer material model is:

- `Basic` lit or unlit mesh material, used by static mesh, GPU-instanced mesh,
  and skinned mesh submissions.
- `TerrainSplat`, used by terrain chunks and terrain instanced batches.
- Utility pass materials for decals, particles, selection masks, CSM depth,
  SSAO, color grading, and debug overlays. These are backend-private shader
  paths rather than public material resources.

`MaterialDesc::alphaMode` applies only to `MaterialKind::Basic`.
`MaterialAlphaMode::Opaque` is the default and renders in the opaque-style
forward bucket with depth writes enabled. `MaterialAlphaMode::AlphaTest`
renders in the opaque-style bucket with depth writes enabled and discards
fragments whose final material alpha is below `MaterialDesc::alphaCutoff`.
`MaterialAlphaMode::AlphaBlend` renders after opaque and alpha-test geometry
with depth testing enabled, depth writes disabled, and stable back-to-front
sorting within each draw family.

Terrain splat materials must remain opaque in this milestone. Decals and
particles keep their own descriptors and passes; they are not authored as
`MaterialDesc` resources.

## Transparency policy

The first transparent ordering policy is conservative and deterministic:

- each static, skinned, and instanced draw family renders its opaque and
  alpha-test work before that family's transparent work
- projected decals composite after opaque scene content and before particles
  and color grading
- alpha-blend static mesh draws, skinned mesh draws, instanced batches, and
  alpha-mode structure fades are sorted back-to-front by camera distance within
  their own draw family, with submission order as the stable tie-breaker
- particles keep the existing particle sort mode: submission order by default,
  optional batch or per-batch particle distance sorting
- dither-faded structures stay in the opaque-style depth-write path and avoid
  transparent sorting

This is not order-independent transparency. Intersecting transparent meshes,
transparent objects across different draw families, and complex particle/object
intersections can still show ordinary alpha-sorting artifacts.

## Shadow and depth policy

Opaque basic materials cast and receive CSM shadows according to the submitted
draw flags. Alpha-blend basic materials receive CSM shadows when lit, but do
not cast shadows by default. Alpha-test materials render and depth-write in the
forward pass, but alpha-clipped CSM casting is not implemented yet, so
shadow-caster submissions using alpha-test materials are skipped and counted as
unsupported shadow/material combinations. Particles and decals do not cast
shadows. Dither-faded structures follow the existing structure-fade shadow
policy and continue to cast full shadows unless the caller disables
`castsShadow`.

Alpha-blend materials and alpha fade use depth testing with depth writes
disabled. Alpha-test and dither fade keep depth writes enabled. SSAO and decal
depth inputs are still based on the opaque scene-depth policy; transparent
objects do not add a new depth prepass in this milestone.

## Shader variant policy

Shader variant selection is represented internally by a small deterministic
key, not by public backend handles. Variant axes are intentionally limited to:

- pass family: forward static, forward instanced, forward skinned, terrain
  splat, shadow depth, selection mask, decal, particle, color grading, or debug
- lit/unlit
- skinned, instanced, or terrain-splat transform path
- opaque, alpha-test, or alpha-blend material mode
- CSM receive state
- fog state
- normal-map use for terrain layers
- dither fade or alpha fade

The current backend still reuses a small number of compiled shader programs and
passes feature state through uniforms where practical. New shader code should
prefer shared helpers and a documented variant key over adding another
untracked program path.

## Color-space policy

Material and particle colors submitted through descriptors are linear values.
The current RGBA8 texture path is treated as color data for albedo, terrain
layers, decals, particles, and generated fallback textures, while terrain
normal maps are encoded tangent-space RGB data and are not color corrected.
Forward terrain and mesh shaders light in the renderer's linear convention,
then write explicit gamma output. Decal, particle, and color-grading passes
document their own conversion points in `docs/shader_pipeline.md`. Debug colors
are linear debug values unless a pass explicitly states otherwise.

## Terminology

Use renderer terms:

- material instance
- material template
- pipeline state
- texture binding
- shader program

Avoid game-specific material concepts in renderer APIs.

## Resource ownership

Materials are renderer-owned resources. Public APIs should use handles and explicit destruction or documented lifetime management.

Material descriptors should document which data is copied and which resources are referenced by handle.

Textures are renderer-owned resources created from tightly packed RGBA8 CPU data.
Texture source memory is copied during creation. Materials reference textures by
handle and do not own them; callers should keep referenced textures alive while
materials using them can be submitted. If a referenced texture is destroyed or
stale at submission time, the backend must not dereference it; terrain splat
materials use the documented layer-color, flat-normal, or splat-map fallback
and increment stale-handle diagnostics where practical.

## Terrain Splat Materials

The first terrain material path supports four albedo layers and one RGBA splat
map per terrain chunk:

- R controls layer 0
- G controls layer 1
- B controls layer 2
- A controls layer 3

Layer albedo texture handles are optional. Missing layer textures use a
deterministic white fallback texture multiplied by the layer fallback color.
Missing chunk splat maps use full layer-0 weight. UVs are derived from
chunk-local X/Z mesh coordinates multiplied by `TerrainMaterialDesc::uvScale`.

Each layer can also reference an optional tangent-space normal map. The initial
normal-map convention is RGBA8 encoded RGB, with `(128, 128, 255)` representing
a flat normal. Missing normal maps use a backend flat-normal fallback, so normal
maps are never required for terrain to render. `TerrainMaterialDesc` exposes
`normalMapStrength` in `[0, 1]` and `flipNormalMapY` for assets authored with
the opposite green-channel convention. The terrain shader blends layer normals
with the same normalized splat weights as albedo and transforms them using a
simple basis derived from the generated or supplied geometric normal.

The current terrain diagnostics report per-chunk splat-map fallback use and can
highlight those chunks with the GPU debug overlay. Missing layer albedo,
normal, and splat texture use also contributes to the material fallback texture
counter in `RendererStats`. Resource-lifetime tests cover stale terrain albedo,
normal, and splat-map references through a mocked backend seam without
requiring bgfx initialization.

Terrain splat materials and basic mesh materials can receive cascaded
directional shadows. The shaders select the active cascade by camera-view
depth, sample the backend-owned shadow map for that cascade, and apply the
configured shadow strength to direct lighting. Basic static, instanced, and
skinned mesh forward paths share the same fragment shader after their vertex
transforms, so lit skinned submissions receive the same CSM shadows and fog.
Basic static and instanced mesh submissions also cast into cascades when their
frame descriptors opt into shadow casting with world-space bounds. Opaque
skinned submissions opt into cascade shadow casting with
`AnimatedDrawItem::castsShadow`, conservative submitted bounds, and a valid
CPU-provided palette. Unlit basic materials remain unshadowed.

Terrain splat materials and basic mesh materials also receive linear distance
fog from `EnvironmentDesc`. Fog is evaluated from camera view-space depth and is
applied after material lighting/shadowing but before gamma conversion. It does
not add material-specific parameters yet.

`WeatherDesc` can add reversible per-frame wetness darkening without changing
material resources. The first hook applies a scalar darkening factor to terrain
splat shading and to the shared basic mesh fragment shader used by static,
instanced, and skinned submissions. It is not a PBR wetness model: there are no
wetness maps, puddles, roughness/metallic changes, accumulation simulation, or
per-material authoring controls yet. Disabling wetness uploads neutral shader
state and preserves existing material output.

Structure fade is render-state input, not a material resource. Callers attach
`FadeDesc` to static draws, instanced batches, or skinned draws when they want
externally decided roof/building visibility changes. The shared basic mesh
fragment shader can dither-discard fragments with a stable screen-space pattern
or multiply alpha for a simple blended fade. Dither keeps opaque-style depth
writes; alpha fade uses the same stable transparent ordering policy as
alpha-blend basic mesh draws within each draw family. Fade does not add
per-material building categories, obstruction logic, roof ownership, or
persistent material mutation. Faded structures continue to use the existing
shadow-casting policy by default.

Selection outlines are not material-authored. A selected static, instanced, or
skinned submission writes its geometry into a backend-private mask with a
depth-only/color-only utility shader, then a fullscreen composite shader applies
the global outline style from `SelectionOutlineDesc`. The current milestone does
not add per-material outline colors, transparent outline policy, or material
customization for selection.

SSAO is also outside material authoring for the first Phase 3 foundation. When
`SsaoDesc::enabled` is set, the backend captures submitted geometry depth into
a private target, evaluates a depth-only AO buffer at full or half resolution,
optionally applies a simple non-depth-aware separable blur, and composites it
over the scene after the forward material passes. Materials do not expose AO
texture slots, per-material AO masks, or per-material AO tuning yet, and no
material shader code is rewritten for AO.

Projected decals are likewise outside material authoring. `DecalSubmitDesc`
describes frame-local oriented projector volumes with optional texture handles,
tint, opacity, deterministic sort keys, and optional projection-depth tuning.
The CPU plan can cull projector bounds against the active camera frustum before
the backend composites albedo/tint decals over the scene color using
reconstructed world position from a private depth capture. Decals do not modify
material resources, terrain splat layers, normals, roughness, metallic, or
lighting; they are a post-forward color projection pass.

Particles are outside material authoring as well. `ParticleSubmitDesc` provides
frame-local billboard batches with one optional RGBA8 texture handle per batch,
per-particle linear color/alpha, size, rotation, and UV rectangle. The backend
uses a white fallback texture when a particle batch has no valid texture and
alpha-blends camera-facing quads after opaque rendering and decals. Particle
polish adds renderer-side batch culling, optional deterministic sorting, and
soft alpha fade against backend-private scene depth when available; none of
those features add material slots or receiver material changes. Particles do
not receive CSM shadows, cast shadows, or participate in terrain/mesh lighting
in this foundation.

Color grading is a final image adjustment path, not a material property.
`ColorGradingDesc` runs after the forward material passes, SSAO, decals, and
particles through a backend-private fullscreen scene-present shader. It can
apply exposure, simple tonemapping, contrast, saturation, gamma, lift, and gain
without changing terrain, mesh, skinned, decal, or particle material
descriptors. LUT intent is validated and reported, but active LUT sampling is a
deferred extension point; missing or unsupported LUTs fall back to no LUT.

## Remaining material work

Future work may add alpha-tested shadow clipping, cross-family transparent
sorting, roughness/metallic splatting, material LOD, height-aware normal
blending, triplanar projection, virtual texturing, streaming integration,
weighted blended transparency, or OIT. Those are deliberately outside the first
policy hardening pass.

For production open-world content, the most important material gates are:

- import-time validation that authored texture slots match the renderer's
  color-space, normal-map, mip, compression, and alpha policies
- alpha-tested shadow clipping or a documented production limitation for
  foliage, fences, and other cutout assets
- a deliberate cross-family transparency decision before relying on large
  numbers of transparent meshes and particles together
- a PBR or richer material roadmap only if the target engine content requires
  roughness, metallic, specular, environment lighting, or material LOD
- shader variant growth discipline so imported material features do not create
  an uncontrolled program matrix
