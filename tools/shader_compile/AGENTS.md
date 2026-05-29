# tools/shader_compile/AGENTS.md

## Shader tooling

Shader compilation must be documented and repeatable.

Prefer build-integrated or scripted shader compilation.

Do not hard-code user-specific absolute paths.

Generated binaries should not pollute source shader directories unless the repository intentionally adopts that layout.

## Expected behavior

Shader tooling should make clear:

- input shader directory
- output binary directory
- target platforms/profiles
- include paths
- shaderc location or discovery process
- debug vs release shader options

## Validation

When changing shader tooling, verify at least one simple vertex/fragment shader pair can be compiled for the target backend used by the sample app.

For engine-readiness work, shader compilation should become part of the normal
validation path for all active feature shaders, including terrain, shadows,
skinned meshes, SSAO, decals, particles, selection/outline, fade, and color
grading. Keep target profiles and backend assumptions documented.
