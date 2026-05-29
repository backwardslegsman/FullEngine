# assets/meshes/AGENTS.md

## Mesh asset rules

Mesh source assets belong here unless the repository defines a separate source/generated asset split.

Keep importer assumptions documented:

- units
- up axis
- handedness
- tangent basis
- index size
- vertex attribute layout
- scale transforms applied during import

## Renderer boundary

Mesh assets may contain source data for sample scenes. Renderer code should not depend on importer internals or a specific source format.

Keep CPU-side import separate from GPU upload.

## Engine-readiness metadata

Future mesh assets and import tooling should validate bounds, LOD groups,
tangent basis, skeleton/skin metadata, material references, collision-free
renderer upload data, and large-world scale assumptions before streamed
open-world scenes depend on them.
