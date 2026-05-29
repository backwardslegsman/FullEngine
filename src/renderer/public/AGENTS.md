# src/renderer/public/AGENTS.md

## Public API rules

All engine-facing interfaces belong here or in the configured public include directory.

Public APIs must use Doxygen-compatible comments and document:

- ownership
- lifetime
- thread expectations
- units and coordinate conventions
- whether data is copied, referenced, or consumed
- whether calls are valid before, during, or after a frame
- error behavior and invalid-input behavior

Prefer opaque handles over raw pointers for renderer-owned resources.

Do not expose bgfx handles directly unless implementing an explicitly low-level backend extension.

## Doxygen conventions

Use Doxygen block comments for public classes, structs, enums, functions, and non-obvious public fields. Prefer concise comments that describe behavior and constraints rather than repeating the symbol name.

Preferred commands:

- `@brief` for one-sentence purpose
- `@param` for input/output parameter semantics
- `@return` for return values and invalid-handle/error behavior
- `@retval` for boolean or enum-specific outcomes when useful
- `@pre` and `@post` for lifecycle requirements
- `@note` for thread-safety, ownership, frame-validity, or coordinate notes
- `@warning` for lifetime hazards or backend restrictions
- `@see` for related descriptors, handles, or lifecycle calls

Use custom commands such as `@thread_safety` only if the repository Doxyfile defines them as aliases. Otherwise write thread rules as `@note Thread safety: ...`.

Example:

```cpp
/**
 * @brief Creates a renderer-owned GPU mesh resource.
 *
 * The renderer copies the vertex/index data needed for GPU upload before the
 * call returns. The caller remains responsible for the lifetime of `desc` and
 * its source buffers after this call.
 *
 * @param desc Mesh creation descriptor. Vertex and index buffer pointers must
 * be valid for the duration of the call.
 * @return A valid mesh handle on success; an invalid handle if validation or
 * backend resource creation fails.
 *
 * @note Thread safety: Must be called from the renderer owner thread unless a
 * future implementation explicitly documents otherwise.
 * @see destroyMesh
 */
MeshHandle createMesh(const MeshDesc& desc);
```

See `docs/agents/doxygen_style.md` for the full repository documentation style.

## Public API shape

Prefer descriptors and handles:

```cpp
namespace rhi
{
    /** @brief Opaque handle for a renderer-owned mesh resource. */
    struct MeshHandle
    {
        uint32_t id = 0;
    };

    /**
     * @brief CPU-side mesh creation descriptor.
     *
     * Source buffer pointers must remain valid for the duration of the call
     * that consumes this descriptor. Layout metadata is copied during creation.
     */
    struct MeshDesc
    {
        const void* vertexData = nullptr;
        uint32_t vertexBytes = 0;
        const void* indexData = nullptr;
        uint32_t indexBytes = 0;
        VertexLayoutDesc layout;
    };

    /** @brief Engine-facing renderer lifecycle and frame-submission interface. */
    class IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        /** @brief Initializes renderer backend resources from platform data. */
        virtual bool initialize(const RendererInitDesc& desc) = 0;

        /** @brief Releases renderer-owned resources and backend state. */
        virtual void shutdown() = 0;

        /** @brief Creates a renderer-owned mesh resource from CPU-side data. */
        virtual MeshHandle createMesh(const MeshDesc& desc) = 0;

        /** @brief Destroys a renderer-owned mesh resource. */
        virtual void destroyMesh(MeshHandle handle) = 0;

        /** @brief Begins a frame and resets per-frame renderer state. */
        virtual void beginFrame(const FrameDesc& desc) = 0;

        /** @brief Submits stable render data for the current frame. */
        virtual void submit(const RenderWorldView& view) = 0;

        /** @brief Finalizes frame submission and presents through the backend. */
        virtual void endFrame() = 0;
    };
}
```

Adapt names to the repository style, but keep ownership and lifetime clear.

## Coordinate conventions

Keep APIs consistent with `docs/renderer_conventions.md`.

Until documented otherwise:

- Y-up world
- meters as world units
- right-handed world coordinates
- linear color in shader calculations
- gamma-correct output to the swapchain

If bgfx/backend clip-space details differ by graphics API, hide them behind backend setup and view/projection helpers.

## Stability

Do not add game-specific concepts to public renderer APIs. Selection IDs, fade values, material parameters, draw flags, and instance data are acceptable when described as renderer-facing data.

Phase 3 hardening public APIs should keep ownership boundaries explicit for
externally owned systems such as selection, particles, weather, structure fade,
and animation palettes. Prefer backend-neutral diagnostics for readiness work;
do not expose textures, framebuffers, view IDs, SDL3, or Dear ImGui types.
