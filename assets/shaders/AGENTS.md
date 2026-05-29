# assets/shaders/AGENTS.md

## Shader organization

Organize shaders by pass and feature level:

```text
assets/shaders/
  common/
    uniforms.sh
    lighting.sh
    color_space.sh
  forward/
    mesh.vert.sc
    mesh.frag.sc
  terrain/
    terrain.vert.sc
    terrain.frag.sc
  shadows/
    depth.vert.sc
    depth.frag.sc
  debug/
    line.vert.sc
    line.frag.sc
```

Rules:

- Keep shared shader code in `common/`.
- Use clear naming by render pass.
- Store generated shader binaries outside source shader directories where possible.
- Add shader compilation commands to the build or documented tooling.
- Include shader reflection metadata later if needed for material binding.
- Keep gamma correction explicit and tested.
- For Phase 3 hardening, prefer a deliberate shader variant policy before
  adding more one-off feature shaders. Validate independent disable paths for
  SSAO, decals, particles, color grading, selection/outline, and fade.

## Boundaries

Shader source belongs here. Shader compilation scripts and generated-output layout belong under `tools/shader_compile/` or the build system.
