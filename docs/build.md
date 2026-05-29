# Build And Platform Confidence

This document describes the supported build modes for the renderer foundation.
The first supported platform policy is Windows with Visual Studio 2022, vcpkg,
bgfx, and the D3D11 shader profile. Other host platforms may work through
CMake/bgfx, but they are currently unverified.

## Dependencies

Runtime/library dependency:

- `bgfx` for the internal renderer backend. bgfx types and handles remain
  private to backend implementation code.

Sample-only dependency:

- `SDL3` for the sample platform shell, window creation, input, and resize
  handling. SDL3 is not included by renderer public headers.

Optional debug-UI dependency:

- Dear ImGui through the vcpkg manifest feature `debug-ui`, used only by the
  SDL3 sample diagnostics UI when `FULL_RENDERER_ENABLE_DEBUG_UI=ON`.

Tooling dependency:

- `bgfx[tools]` host tools provide `bgfx::shaderc` for shader compilation when
  `FULL_RENDERER_ENABLE_SHADER_COMPILE=ON`.

The checked-in presets assume the repository-local vcpkg checkout at
`external/vcpkg` and a project-local installed dependency tree at
`build/vcpkg_installed`. They set `VCPKG_MANIFEST_MODE=OFF` so CI/preset
configure steps do not perform implicit network installs. Run vcpkg install
explicitly or override `VCPKG_INSTALLED_DIR`/`CMAKE_TOOLCHAIN_FILE` if your
dependency cache lives elsewhere.

The CI helper script can install dependencies into that same local tree when
called with `-InstallDependencies`. If `external/vcpkg/vcpkg.exe` is missing in
CI, the script clones and bootstraps vcpkg before running the manifest install.

## CMake Options

- `FULL_RENDERER_BUILD_SAMPLE` builds the SDL3 sample app. Default: `ON`.
- `FULL_RENDERER_BUILD_TESTS` builds CPU-side tests when `BUILD_TESTING` is
  enabled. Default: `ON`.
- `FULL_RENDERER_ENABLE_BGFX` builds the real bgfx backend. Default: `ON`.
- `FULL_RENDERER_ENABLE_DEBUG_UI` builds the sample Dear ImGui diagnostics UI.
  Default: `OFF`.
- `FULL_RENDERER_ENABLE_SHADER_COMPILE` adds the build-local bgfx shader
  compilation path used by the sample. Default: `ON`.
- `FULL_RENDERER_WARNINGS_AS_ERRORS` treats renderer-library compiler warnings
  as errors. Default: `OFF`.

`full_renderer` is currently a static library target. The install/export path
publishes the CMake package target `FullRenderer::full_renderer` for external
engine projects.

## Presets

All presets are Windows/Visual Studio/vcpkg presets and intentionally avoid
user-specific absolute paths.

- `dev-debug`: sample, tests, bgfx backend, and shader compilation.
- `dev-release`: release variant of the development build.
- `library-only-debug`: renderer library only, without SDL3 sample, tests,
  Dear ImGui, or shader target.
- `package-debug`, `package-release`: renderer library-only package configure
  presets with local install prefixes under `out/install/`.
- `package-no-debug-ui`: renderer library-only package configure preset with
  sample, tests, Dear ImGui, and shader tooling disabled.
- `no-debug-ui-debug`: sample and tests without Dear ImGui.
- `tests-debug`: CPU-side tests without sample or shader target.
- `shader-validation`: configures the sample shader environment and builds only
  `full_renderer_shaders`.
- `ci-debug`, `ci-release`, `ci-library-only`, `ci-no-debug-ui`: CI-style
  wrappers around the same build modes.

## CI-Style Commands

The local equivalent of the Windows package CI workflow is:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Run-WindowsPackageValidation.ps1
```

Use `-InstallDependencies` on a clean CI agent or machine without a prepared
`build/vcpkg_installed` tree:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\Run-WindowsPackageValidation.ps1 -InstallDependencies
```

The script runs CPU tests, library-only build, no-debug-UI build, package
install staging, external consumer-smoke configure/build/run, and shader
validation. It does not launch the SDL3 sample because CI is not assumed to have
a GPU/windowing environment.

Developer debug:

```powershell
cmake --preset dev-debug
cmake --build --preset dev-debug
```

Library only:

```powershell
cmake --preset library-only-debug
cmake --build --preset library-only-debug
```

Install/export package:

```powershell
cmake --preset package-debug
cmake --build --preset package-debug
cmake --install out/build/package-debug --config Debug --prefix out/install/package-debug
```

Consumer smoke project:

```powershell
Push-Location tests/consumer_smoke
cmake --preset consumer-smoke-debug
cmake --build --preset consumer-smoke-debug
Pop-Location
.\out\build\consumer-smoke-debug\Debug\full_renderer_consumer_smoke.exe
```

No-debug-UI install/export package:

```powershell
cmake --preset package-no-debug-ui
cmake --build --preset package-no-debug-ui
cmake --install out/build/package-no-debug-ui --config Debug --prefix out/install/package-no-debug-ui
Push-Location tests/consumer_smoke
cmake --preset consumer-smoke-no-debug-ui
cmake --build --preset consumer-smoke-no-debug-ui
Pop-Location
```

No debug UI:

```powershell
cmake --preset no-debug-ui-debug
cmake --build --preset no-debug-ui-debug
```

Tests:

```powershell
cmake --preset tests-debug
cmake --build --preset tests-debug
ctest --preset tests-debug
```

Shader validation:

```powershell
cmake --preset shader-validation
cmake --build --preset shader-validation
```

Manual smoke launch after `dev-debug`:

```powershell
$process = Start-Process -FilePath .\out\build\dev-debug\Debug\full_renderer_sample.exe -PassThru -WindowStyle Hidden
$exited = $process.WaitForExit(3000)
if ($exited) { exit $process.ExitCode }
Stop-Process -Id $process.Id
exit 0
```

## Shader Outputs

Shader binaries are generated into the active build directory:

```text
<build-dir>/shaders/dx11/
```

The target is deterministic for the Windows/D3D11 profile and fails during
configure/build if `bgfx::shaderc` or required source shader inputs are missing.
The renderer library can be built without shader compilation when the sample is
disabled. The sample currently requires build-local shader compilation because
its default shader directory points at generated build outputs.

The installed CMake package does not install shader binaries by default. Engine
consumers should compile or package shaders through their own runtime asset
pipeline and pass the chosen shader directory through
`RendererInitDesc::shaderBinaryDirectory`. The source-tree
`full_renderer_shaders` target remains the development validation path for the
Windows/D3D11 profile.

Shader binary packaging decision: keep shader install disabled/deferred for
this milestone. The current shader outputs are backend/profile-specific
Windows/D3D11 binaries, the sample uses build-local paths, and external engines
will usually own runtime asset layout. Adding optional shader installation
should wait until the renderer has a backend/profile shader asset convention,
an install component name, and a package-config/runtime-discovery policy that
does not imply D3D11 binaries are portable renderer library assets.

When `FULL_RENDERER_ENABLE_DEBUG_UI=OFF`, the shader target omits the Dear ImGui
and shadow-preview UI shaders. Those UI shaders are compiled only for debug-UI
sample builds.

## Public Header Hygiene

The `full_renderer_public_header_compile_tests` target includes all public
headers from a small external-style test translation unit. It verifies that
public headers do not require SDL3, Dear ImGui, or backend headers.

## Installed Package Layout

The install step writes a relocatable CMake package:

```text
<prefix>/
  include/full_renderer/*.hpp
  lib/full_renderer.lib
  lib/cmake/FullRenderer/
    FullRendererConfig.cmake
    FullRendererConfigVersion.cmake
    FullRendererTargets.cmake
```

External CMake projects consume it with:

```cmake
find_package(FullRenderer CONFIG REQUIRED)
target_link_libraries(my_engine PRIVATE FullRenderer::full_renderer)
```

The exported target publishes only renderer public include directories and
compile features. SDL3, Dear ImGui, the sample app, tests, shader compiler, and
asset validation tools are not required by consumers of the library target.
When the package is built with the bgfx backend enabled, `FullRendererConfig`
uses `find_dependency(bgfx CONFIG)` so static-library consumers resolve the
private backend link dependencies through their normal dependency setup, such as
the same vcpkg toolchain used to build the package.

The consumer smoke project in `tests/consumer_smoke` validates this installed
tree by configuring outside the renderer build, calling `find_package`, linking
to `FullRenderer::full_renderer`, including public headers, and avoiding
source-tree include paths or SDL3/Dear ImGui/bgfx headers.

## CI Automation

The repository includes a first Windows-only GitHub Actions workflow:

```text
.github/workflows/windows-package-validation.yml
```

The workflow intentionally delegates to `tools/ci/Run-WindowsPackageValidation.ps1`
so the same checks can be run locally. The initial CI matrix is deliberately
small:

- OS: `windows-latest`
- configuration: Debug package/test path plus shader validation
- modes: CPU tests, library-only, no-debug-UI, package install, consumer smoke

The workflow avoids GPU/runtime sample launch and secrets. It uses vcpkg only
for dependency installation and keeps the renderer package validation on the
documented Windows/Visual Studio/D3D11-first platform.

## Platform Matrix

- Supported first target: Windows, Visual Studio 2022, x64, bgfx/D3D11 shader
  profile.
- Expected but unverified: other bgfx-supported platforms once shader profiles,
  window handles, and dependency packages are configured.
- Unsupported/undeclared: mobile, consoles, WebAssembly, and non-bgfx backends.

Do not treat unverified platforms as supported until configure/build/test/shader
validation has been run and documented for that platform.

## Known Limitations

- No shared-library packaging yet.
- The vcpkg manifest is still development-oriented and includes SDL3; CMake
  library-only mode does not require SDL3 at configure/build time.
- Shader validation is currently Windows/D3D11-profile only.
- Installed shader/runtime asset packaging is intentionally deferred to engine
  integration or a future asset packaging milestone.
- CI currently proves the Windows package/test/shader validation path. It does
  not yet prove a broad multi-platform/backend matrix, long-session stability,
  or production-scale engine workloads.
- Backend-safe generic texture preview and actual shadow-map preview remain
  deferred.

Build confidence is one part of open-world readiness. Production engine use
also needs the roadmap gates for shader runtime asset packaging, asset
validation tooling, large-world precision, representative performance proof,
and platform/backend coverage matching any support claims.
