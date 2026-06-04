# Asset Pipeline Tooling

Production asset database tooling is not implemented yet. The engine asset
layer now has two importer seams behind the same
`AssetSourceRecord -> LoadedAssetPayload` contract:

- a tiny dev-only ASCII importer used by tests and sample debug assets; this
  format is not a shipping content contract
- an Assimp-backed static/skinned/animation glTF importer that validates source
  descriptors, aggregates static multi-mesh scenes into one renderer-free mesh
  payload, optionally generates missing normals, requires or explicitly
  defaults UV0, copies vertex colors, and extracts bind-pose skeletons plus
  skinned mesh weights and raw joint transform animation tracks without
  renderer headers or handles
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
borrowed frame-local `SkinningPaletteDesc` view.
Runtime playback controllers, blending, animation compression, embedded glTF
images, tangent payloads, UV1+ sets, mip generation, compression/KTX,
production material authoring, packed assets, async IO, and production
packaging are still future work.

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
