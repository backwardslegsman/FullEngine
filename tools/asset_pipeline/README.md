# Asset Pipeline Tooling

Production asset database tooling is not implemented yet. The engine asset
layer now has two importer seams behind the same
`AssetSourceRecord -> LoadedAssetPayload` contract:

- a tiny dev-only ASCII importer used by tests and sample debug assets; this
  format is not a shipping content contract
- an Assimp-backed static glTF mesh importer that validates source descriptors,
  aggregates multi-mesh scenes into one renderer-free mesh payload, optionally
  generates missing normals, requires or explicitly defaults UV0, and copies
  vertex colors without renderer headers or handles
- an stb-backed direct image importer that decodes texture files into tightly
  packed, single-mip RGBA8 `LoadedTextureAsset` payloads with caller-authored
  semantic/color-space metadata
- a glTF material/image reference extractor that maps referenced base-color,
  normal, metallic-roughness, occlusion, and emissive images into texture source
  records and emits material payloads that refer to those textures by named
  engine asset slots

Renderer integration can use loaded payloads to produce mesh/texture/material
handles through caller-owned renderer uploads and the existing completion
reconcile path. The asset layer also has renderer-free skeleton and skinned mesh
payload contracts for hierarchy/bind-pose data and four-influence skinned
vertices, but skeletal import and renderer upload planning are still future
work. Embedded glTF images, tangent payloads, UV1+ sets, mip generation,
compression/KTX, production material authoring, animation clips, packed assets,
async IO, and production packaging are still future work.

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
