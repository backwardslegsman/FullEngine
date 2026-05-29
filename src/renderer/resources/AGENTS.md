# src/renderer/resources/AGENTS.md

## Resource ownership

Renderer-owned resources use handles and explicit destruction.

Resource categories include:

- mesh buffers
- textures
- materials
- shader programs
- skeletons, later
- animation clips, later
- terrain chunk resources
- instance buffers
- render targets

Rules:

1. Creation descriptors copy necessary metadata.
2. Large binary blobs may be copied immediately or staged explicitly; document behavior.
3. Invalid handle destruction must follow the documented policy.
4. Resource hot reload is allowed later but should not complicate Phase 1 APIs.
5. Asset loading and GPU resource creation must remain separable.

## Materials

Phase 1 material support should be minimal but structured for growth.

Initial material properties:

- base color
- albedo texture
- normal texture, optional
- roughness/metallic fields, even if forward renderer initially uses simple lighting
- alpha mode
- two-sided flag
- shader/pipeline reference

Use renderer terms: material instance, material template, pipeline state, texture binding.

Avoid game-specific material concepts.

## Mesh loading

Keep CPU-side import separate from renderer GPU upload.

Preferred split:

- `MeshAsset`: CPU-side loaded mesh data.
- `MeshHandle`: renderer-owned GPU resource.
- `MeshImporter`: file format loading.
- `MeshUploadDesc`: bridge from CPU mesh data to GPU buffers.

Keep initial mesh format simple. OBJ or glTF can be supported, but do not entangle renderer resource ownership with importer internals.

## Validation

Validate descriptors at creation boundaries. Keep descriptor validation deterministic and unit-testable where practical.

## Engine-readiness hardening

Before production engine use, resource work should add deterministic coverage
for create/destroy/recreate cycles, stale and destroyed handles, repeated
resize, allocation failures, memory estimates, and streaming-style residency
for meshes, textures, terrain chunks, materials, skeletons, render targets, and
post resources.
