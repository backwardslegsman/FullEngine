# Asset Pipeline Tooling

Production importer or asset database tooling is not implemented yet. The
engine asset layer now has a tiny dev-only ASCII importer used by tests to
prove `AssetSourceRecord -> LoadedAssetPayload` flow, but that format is not a
shipping content contract. Renderer integration can now use that dev importer
from scheduled manifest asset-load work to produce mesh/texture/material
handles through caller-owned renderer uploads and the existing completion
reconcile path. glTF/PNG/KTX, packed assets, async IO, richer material
authoring, and production packaging are still future work.

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
