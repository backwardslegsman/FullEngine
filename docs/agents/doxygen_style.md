# Doxygen Style for Public APIs

This repository uses Doxygen-compatible comments for public C++ renderer interfaces. The goal is to make public headers useful as both integration guidance and generated API reference documentation.

## Scope

Use Doxygen comments for engine-facing declarations, especially in `src/renderer/public/` or any configured public include directory.

Document:

- public classes and interfaces
- public structs and descriptors
- public enums and enum values when the meaning is not obvious
- public functions and methods
- non-obvious public fields
- handles and lifetime-sensitive types

Internal implementation details may use ordinary comments unless they are part of generated internal documentation.

## Required content

Public API comments must explain the contract, not just the declaration. Cover the relevant items below:

- purpose and intended use
- ownership and lifetime
- thread expectations
- units and coordinate conventions
- parameter semantics
- return value semantics
- error behavior and invalid-input behavior
- whether data is copied, referenced, or consumed
- whether calls are valid before, during, or after a frame
- relationships to matching lifecycle calls such as create/destroy or begin/end

## Preferred Doxygen commands

Prefer standard Doxygen commands so the generated documentation works without custom tooling:

- `@brief` for a concise summary
- `@param` for parameter semantics
- `@return` for return values
- `@retval` for specific boolean or enum outcomes where useful
- `@pre` for required state before a call
- `@post` for state guaranteed after a call
- `@note` for ownership, thread-safety, frame-validity, or coordinate notes
- `@warning` for lifetime hazards, invalidation, backend restrictions, or expensive operations
- `@see` for related descriptors, handles, or lifecycle calls

Use custom commands such as `@thread_safety` only after adding a matching Doxyfile alias. Without an alias, write thread rules as `@note Thread safety: ...`.

## Comment format

Use block comments for public declarations:

```cpp
/**
 * @brief Creates a renderer-owned GPU mesh resource.
 *
 * The renderer copies the vertex/index data needed for GPU upload before the
 * call returns. The caller may release or reuse source buffers after the call.
 *
 * @param desc Mesh creation descriptor. Buffer pointers must be valid for the
 * duration of the call.
 * @return A valid mesh handle on success; an invalid handle if validation or
 * backend resource creation fails.
 *
 * @note Thread safety: Must be called from the renderer owner thread unless a
 * future implementation explicitly documents otherwise.
 * @see destroyMesh
 */
MeshHandle createMesh(const MeshDesc& desc);
```

Single-line Doxygen comments are acceptable for obvious fields or simple handles:

```cpp
/** @brief Opaque handle for a renderer-owned texture resource. */
struct TextureHandle
{
    uint32_t id = 0;
};
```

## Field comments

For descriptor fields, document units, coordinate spaces, default behavior, and lifetime-sensitive pointers. Prefer field comments when the field itself carries the contract:

```cpp
struct FrameDesc
{
    /** @brief Backbuffer width in physical pixels. */
    uint32_t width = 0;

    /** @brief Backbuffer height in physical pixels. */
    uint32_t height = 0;

    /**
     * @brief Optional externally supplied frame time in seconds.
     *
     * A value of zero means the renderer should not derive animation or timing
     * behavior from this field.
     */
    double deltaSeconds = 0.0;
};
```

## What to avoid

Avoid comments like these:

```cpp
/** @brief MeshHandle struct. */
struct MeshHandle;

/** @brief Gets the width. */
uint32_t getWidth() const;
```

They restate the declaration without documenting behavior. Prefer comments that explain ownership, units, invariants, and integration impact.

## Public/private boundary

Do not expose SDL3 or bgfx types in public API documentation unless the API is explicitly a backend or platform extension. If backend-specific behavior must be mentioned, describe it in renderer-owned terms and link to the extension point.

## Readiness documentation

When public descriptors participate in Phase 3 hardening milestones, their
Doxygen comments should identify the ownership boundary and fallback behavior:
external selection/fade/weather/particle/animation ownership, frame-local data
lifetime, invalid handle behavior, optional pass disable behavior, and whether
diagnostics are backend-neutral rather than backend handles.

## Generated documentation

If the repository adds a Doxyfile or documentation build target, keep it repository-local and deterministic. Do not require user-specific absolute paths. Treat documentation warnings for public headers as actionable unless the warning is intentionally suppressed and explained.
