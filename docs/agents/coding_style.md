# Coding Style and Maintainability

Codex should produce code that looks hand-maintained.

## General style

- Use clear names.
- Prefer small functions.
- Prefer small, cohesive classes.
- Prefer value types for descriptors.
- Keep headers minimal.
- Use forward declarations where practical.
- Make ownership obvious.
- Separate data preparation from draw submission.
- Avoid unexplained magic constants.
- Avoid giant catch-all managers.
- Avoid dead feature stubs unless they clarify near-term architecture.
- Comments explain why, not every line of what.
- Avoid excessive template metaprogramming.
- Avoid macros unless wrapping platform/compiler differences.

## Public API comments

Public headers must use Doxygen-compatible comments for engine-facing APIs. Document behavior, ownership, lifetime, thread expectations, units, parameter semantics, return values, and error behavior.

Prefer comments that explain integration constraints and invariants. Avoid boilerplate comments that merely repeat symbol names, obvious field types, or implementation details.

Use `docs/agents/doxygen_style.md` as the detailed style reference.

## Resource style

- Use RAII internally for backend resources.
- Expose explicit handle destruction publicly for renderer-owned resources.
- Do not expose internal pointers through public handles.
- Define invalid handle behavior and keep it consistent.

## Error handling

Use clear result types for recoverable renderer operations.

Prefer structured errors over exceptions across public renderer boundaries unless the repository already uses exceptions consistently.

Use assertions for programmer errors and structured errors for runtime/resource failures.

Example:

```cpp
enum class RendererError
{
    None,
    InvalidDescriptor,
    BackendInitializationFailed,
    ShaderCompilationFailed,
    ResourceCreationFailed,
    UnsupportedFeature
};

template <typename T>
struct Result
{
    T value;
    RendererError error = RendererError::None;
};
```

## Logging

Route renderer logging through a logging layer. Do not print directly from core systems.

Recommended categories:

- `Renderer`
- `Bgfx`
- `Shader`
- `Material`
- `Mesh`
- `Texture`
- `Terrain`
- `Culling`
- `Animation`
- `Debug`

## Threading

Phase 1 may be single-threaded.

Future threading should separate:

- engine simulation
- render data extraction
- resource loading
- GPU submission

Respect bgfx submission rules. Document which thread owns renderer calls.

Public APIs must not imply thread safety unless explicitly implemented.

## Memory and lifetime

Use explicit lifetime rules.

Important cases:

- window lifetime must exceed renderer backend lifetime
- resources must be destroyed before renderer shutdown or cleaned up during shutdown
- transient per-frame allocations reset at frame boundaries
- terrain/streaming resources may be created and destroyed while the app runs
- public handles must not expose internal pointers

## Engine-readiness style

Phase 3 hardening work should prefer small, testable planning helpers over
large managers. Keep pass ordering, resource reconfiguration decisions,
culling results, stale-handle behavior, and diagnostics deterministic so they
can be tested without GPU availability whenever practical.
