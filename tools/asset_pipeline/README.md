# Asset Pipeline Tooling

Production asset database tooling is not implemented yet. The engine asset
layer now has two importer seams behind the same
`AssetSourceRecord -> LoadedAssetPayload` contract:

- a tiny dev-only ASCII importer used by tests and sample debug assets; this
  format is not a shipping content contract
- an Assimp-backed static/skinned/animation glTF importer that validates source
  descriptors, aggregates static multi-mesh scenes into one renderer-free mesh
  payload, optionally generates missing normals or tangents, requires or
  explicitly defaults UV0, copies vertex colors and glTF-style tangent
  handedness, and extracts bind-pose skeletons plus skinned mesh weights and
  raw joint transform animation tracks without renderer headers or handles
- an opt-in Assimp `AnimatedSceneNodes` skeleton source mode for FBX-style
  assets that expose animation channels on scene nodes but no mesh bones or
  weights. It can import renderer-free skeleton and animation clip payloads
  with identity inverse bind poses for characterization/runtime animation data,
  but it is not skinning-ready mesh import.
- a CPU-only Assimp scene probe used to characterize optional local FBX assets
  before committing to FBX import policy; it reports scene counts, attribute
  availability, node/channel skeleton candidates, skeleton/animation metadata,
  bounds, root transform, and likely blockers without producing payloads or
  renderer resources, and can emit deterministic JSON reports for ignored local
  diagnostics. Its skin-data availability audit runs practical Assimp flag
  configurations and reports whether real mesh bones, weights, and finite
  bind-pose offset matrices are exposed.
- an Assimp rigid node-attached import prototype for FBX-style assets where
  animation channels drive scene nodes but no skin weights are recoverable. It
  imports a node-derived skeleton/clip plus static mesh attachments, then
  renderer integration can build ordinary static `DrawItem`s from sampled node
  matrices. This is rigid animation only, not recovered skinned mesh import.
  The debug-ui sample can run this path against the optional ignored Intel
  Knight FBX, upload the imported attachment meshes, sample the clip each
  frame, and submit ordinary static draws with focused diagnostics. It also
  reports imported attachments filtered before upload with source names,
  upload-plan status, and renderer mesh-contract reasons such as degenerate
  triangles.
- an stb-backed direct image importer that decodes texture files into tightly
  packed, single-mip RGBA8 `LoadedTextureAsset` payloads with caller-authored
  semantic/color-space metadata
- a glTF material/image reference extractor that maps referenced base-color,
  normal, metallic-roughness, occlusion, and emissive images into texture source
  records and emits material payloads that refer to those textures by named
  engine asset slots

Renderer integration can use loaded payloads to produce mesh, texture,
material, skeleton, and skinned mesh handles through caller-owned renderer
uploads and the existing completion reconcile path. Bind-pose skeleton/skinned
mesh import and raw animation clip import are available through Assimp. The
engine asset layer can sample one loaded animation clip against a matching
skeleton into CPU local/model/skinning palette matrices for later renderer
submission, and renderer integration can expose those sampled matrices as a
borrowed frame-local `SkinningPaletteDesc` view. A minimal retained playback
state can advance one clip per caller-defined instance and retain the latest
pose/palette without owning draw submission. Static, instanced, and skinned
mesh UV0 plus tangent data are now uploaded into the public renderer vertex
contract and forwarded to the basic material shader for base-color and
normal-map sampling. The sample debug UI can now run a tiny glTF skeletal smoke
path that imports fixture payloads, uploads skeleton and skinned mesh resources
through caller-owned renderer APIs, ticks playback, and submits one imported
`AnimatedDrawItem`. It can also run a wolf fixture smoke path that aggregates
the skinned meshes from a mixed glTF scene while skipping unskinned meshes for
the skinned import mode, preserves skinned mesh material sections, decodes
referenced texture images, uploads material payloads, and submits one animated
draw per section using resolved material handles. The sample wolf smoke exposes
deterministic camera focus and compact draw/material/palette diagnostics,
including authored/resolved material-slot counts and shader-active base-color
and normal section counts. A CPU-only glTF slot audit compares raw material
texture keys with extractor refs, imported texture payloads, and resolved
texture IDs so asset-authored gaps are separated from importer or upload
failures. The sample queues smoke run/clear button actions during the debug-UI
frame and executes them after `endFrame`, keeping skeleton/skinned and
Knight-smoke mesh resource creation/destruction outside an active renderer
frame. Blending, animation compression, FBX normalization/import policy, FBX
32-bit/sectioned static mesh import for Sponza, source conversion/dedicated
parser evaluation for true Intel knight skinning, embedded glTF images, UV1+
sets, normal-map strength/green-channel controls, full PBR
material-section shading, mip generation, compression/KTX, production material
authoring, packed assets, async IO, and production packaging are still future
work.

Future validation tools should consume authored assets, convert them to the
renderer-facing contracts in `docs/assets.md`, and report actionable errors
without depending on bgfx initialization or renderer-owned backend resources.

Expected first validation commands should cover:

- mesh units, handedness, winding, finite data, normals, tangents, bounds, and
  degenerate triangles
- texture dimensions, RGBA8 or future packed formats, semantic/color-space
  compatibility, mips, compression, and normal-map convention
- material texture slots, alpha policy, shadow compatibility, and fallback
  behavior
- terrain chunk dimensions, splat/weight maps, normal generation, LODs, and
  bounds
- skeleton hierarchy, inverse bind matrices, joint indices, influence counts,
  weights, and palette constraints

This directory intentionally does not contain editor UI, background streaming,
or runtime renderer ownership code.
