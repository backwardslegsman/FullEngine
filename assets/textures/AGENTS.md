# assets/textures/AGENTS.md

## Texture asset rules

Texture source assets belong here unless the repository defines a separate source/generated asset split.

Document color space expectations:

- albedo/base-color textures are generally sRGB source data
- normal, roughness, metallic, height, mask, and lookup textures are generally linear data
- shader calculations should happen in linear space
- swapchain output should be gamma-correct

## Generated files

Do not mix generated platform-specific texture binaries with source textures unless the repository intentionally adopts that layout.

Prefer clear generated output directories and repeatable tooling.

## Naming

Use names that identify material role, not game-specific behavior, for reusable assets.

## Engine-readiness metadata

Future texture assets and tooling should make color space, compression/mip
policy, fallback behavior, and streaming residency expectations explicit for
terrain layers, decals, particles, LUTs, masks, and material textures.
