# tools/asset_pipeline/AGENTS.md

## Asset pipeline rules

Asset pipeline tools may import, validate, transform, compress, or package assets. Keep these tools separate from renderer runtime code.

Do not entangle importer internals with renderer GPU upload.

## Expected behavior

Asset tooling should make clear:

- source input paths
- generated output paths
- supported formats
- unit and axis conversions
- texture color-space handling
- error reporting behavior

## Reproducibility

Generated assets should be reproducible from source assets and documented tool commands.

Do not depend on machine-local absolute paths.

Open-world readiness asset work should define validation for mesh units/axis,
tangent basis, bounds, skeleton hierarchy, terrain chunks, texture color space,
mip/compression settings, and material references before large streamed content
depends on the renderer.
