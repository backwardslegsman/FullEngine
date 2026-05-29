# Shader Compile Tooling

Shader compilation is currently integrated through the CMake target
`full_renderer_shaders`, available when `FULL_RENDERER_ENABLE_SHADER_COMPILE=ON`
and the sample shader environment is configured.

Recommended validation command:

```powershell
cmake --preset shader-validation
cmake --build --preset shader-validation
```

The Windows package CI helper also runs this target by default:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Run-WindowsPackageValidation.ps1
```

The target uses `bgfx::shaderc` from the vcpkg `bgfx[tools]` host dependency,
compiles the active shader set for the Windows/D3D11 `s_5_0` profile, and writes
generated binaries under the active build directory:

```text
<build-dir>/shaders/dx11/
```

Source shaders remain in `assets/shaders`. Generated binaries should not be
written back into the source shader tree.

Dear ImGui and shadow-preview UI shaders are included only when
`FULL_RENDERER_ENABLE_DEBUG_UI=ON`.

Generated shader binaries are not installed by the `FullRenderer` CMake package
yet. They remain build-local development artifacts. External engines should
provide the runtime shader directory through `RendererInitDesc` until a future
optional shader install component defines backend/profile-specific asset
layout and package discovery rules.

Limitations:

- no non-Windows shader profile presets yet
- no shader reflection metadata output yet
- no installed shader binary/runtime asset component yet
- no backend-safe texture preview or shadow-map preview changes here
