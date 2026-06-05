#pragma once

#include "full_renderer/Animation.hpp"
#include "full_renderer/ColorGrading.hpp"
#include "full_renderer/Fade.hpp"
#include "full_renderer/Handles.hpp"
#include "full_renderer/Terrain.hpp"

#include <cstdint>
#include <memory>

namespace full_renderer
{
/**
 * @brief Result code returned by recoverable renderer operations.
 *
 * Public renderer calls use result codes instead of throwing exceptions across
 * the engine boundary. `Success` means the requested state transition completed.
 * Other values describe validation, lifecycle, or backend failures that callers
 * can report or handle deterministically.
 */
enum class RendererResult
{
    /** @brief The operation completed successfully. */
    Success,

    /** @brief A descriptor contained invalid dimensions, timing, or platform data. */
    InvalidDescriptor,

    /** @brief A call argument was structurally valid but referenced invalid frame or resource data. */
    InvalidArgument,

    /** @brief Initialization was requested after the renderer was already initialized. */
    AlreadyInitialized,

    /** @brief The operation requires a successful initialize call first. */
    NotInitialized,

    /** @brief A frame was begun while another frame was still active. */
    FrameAlreadyInProgress,

    /** @brief A frame-ending operation was requested without an active frame. */
    FrameNotInProgress,

    /** @brief The internal rendering backend failed or was unavailable. */
    BackendFailure,

    /** @brief The supplied platform window data is not supported by the backend. */
    UnsupportedPlatform
};

/**
 * @brief Fixed Phase 1 vertex format for uploaded mesh data.
 *
 * Positions and normals are in world-model local space using meters, Y-up, and
 * right-handed coordinates. `tangent` uses the glTF tangent convention: xyz is
 * the local tangent direction and w is the handedness sign used to reconstruct
 * a bitangent from normal and tangent. `uv0` carries the primary texture
 * coordinate set. Colors are linear RGBA values in the range `[0, 1]`. The
 * renderer copies vertex data during `createMesh`; source storage may be
 * released or reused after that call returns.
 */
struct MeshVertex
{
    /** @brief Local-space position in meters. */
    float position[3] = {};

    /** @brief Local-space unit normal used by the basic forward light. */
    float normal[3] = {0.0f, 1.0f, 0.0f};

    /** @brief Primary mesh UV coordinate set. */
    float uv0[2] = {};

    /** @brief Linear RGBA vertex color multiplied by the material color. */
    float colorLinear[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    /**
     * @brief Local-space tangent basis direction and handedness sign.
     *
     * `xyz` must be finite and non-zero. `w` must be approximately `+1` or
     * `-1`; shader normal-map use is deferred, but the data is validated and
     * copied so material import can preserve the authored tangent basis.
     */
    float tangent[4] = {1.0f, 0.0f, 0.0f, 1.0f};
};

/**
 * @brief CPU-side descriptor for creating a renderer-owned mesh.
 *
 * The Phase 1 mesh path accepts a fixed vertex format and 16-bit triangle
 * indices. Vertex and index pointers must remain valid for the duration of the
 * `createMesh` call only; the renderer copies data needed by the backend before
 * returning. `indexCount` must be a non-zero multiple of three. Positions must
 * be finite, normals and tangent xyz must be finite and non-zero, tangent w
 * must be approximately `+1` or `-1`, colors must be linear values in `[0, 1]`,
 * indices must be in range, and degenerate triangles are rejected.
 */
struct MeshDesc
{
    /** @brief Pointer to `vertexCount` fixed-format vertices; must not be null. */
    const MeshVertex* vertices = nullptr;

    /** @brief Number of vertices referenced by `vertices`; zero is invalid. */
    std::uint32_t vertexCount = 0;

    /** @brief Pointer to `indexCount` 16-bit triangle indices; must not be null. */
    const std::uint16_t* indices = nullptr;

    /** @brief Number of indices referenced by `indices`; must be a non-zero multiple of three. */
    std::uint32_t indexCount = 0;
};

/** @brief Pixel format for renderer-owned CPU-uploaded texture data. */
enum class TextureFormat
{
    /** @brief Four 8-bit channels in red, green, blue, alpha byte order. */
    Rgba8
};

/**
 * @brief Renderer-facing meaning of uploaded texture data.
 *
 * The semantic is an asset-contract hint used for validation, documentation,
 * and diagnostics. It does not expose backend texture objects and it does not
 * make the runtime renderer an importer. Callers should set the semantic that
 * matches the asset slot the texture will occupy.
 */
enum class TextureSemantic
{
    /** @brief Color data such as albedo, decal color, particle color, or UI/debug color. */
    Color,

    /** @brief Linear scalar/vector data such as roughness, weights, masks, or generic lookup data. */
    LinearData,

    /** @brief Encoded tangent-space normal data where RGB stores normal direction. */
    NormalMap,

    /** @brief Linear RGBA terrain layer weights where R/G/B/A map to terrain layers 0/1/2/3. */
    TerrainSplat,

    /** @brief Color-grading lookup data following the color-grading LUT convention. */
    ColorGradingLut,

    /** @brief Debug or tooling texture data that is not part of material shading policy. */
    Debug
};

/**
 * @brief Color-space contract for uploaded texture bytes.
 *
 * The current runtime upload path copies bytes as supplied; this flag records
 * how the renderer-facing asset contract expects shader code or future import
 * tooling to interpret the data. It is validated against `TextureSemantic`.
 */
enum class TextureColorSpace
{
    /** @brief Color values authored in sRGB space, normally used for albedo-like color data. */
    Srgb,

    /** @brief Linear scalar, mask, weight, or lookup data. */
    Linear,

    /** @brief Encoded tangent-space normals, normally unpacked from `[0, 1]` to `[-1, 1]`. */
    EncodedNormal
};

/**
 * @brief CPU-side descriptor for creating a renderer-owned 2D texture.
 *
 * The renderer copies pixel data needed by the backend before returning.
 * Dimensions are texels and must be non-zero. The initial texture path accepts
 * tightly packed, uncompressed, single-mip RGBA8 data only; `dataSizeBytes`
 * must be at least `width * height * 4`.
 */
struct TextureDesc
{
    /** @brief Texture width in texels; zero is invalid. */
    std::uint32_t width = 0;

    /** @brief Texture height in texels; zero is invalid. */
    std::uint32_t height = 0;

    /** @brief Pixel format of `data`; only `Rgba8` is currently supported. */
    TextureFormat format = TextureFormat::Rgba8;

    /**
     * @brief Asset-contract semantic for the uploaded bytes.
     *
     * The semantic is copied during `createTexture` and validated with
     * `colorSpace`. It is not a backend texture handle and does not retain
     * importer-owned state.
     */
    TextureSemantic semantic = TextureSemantic::Color;

    /**
     * @brief Expected color-space interpretation for this texture.
     *
     * Albedo-like color assets should use `Srgb`; scalar masks and terrain
     * splat weights should use `Linear`; tangent-space normal maps should use
     * `EncodedNormal`.
     */
    TextureColorSpace colorSpace = TextureColorSpace::Srgb;

    /**
     * @brief Number of mip levels represented by `data`.
     *
     * The current runtime upload path accepts only a single base level. Import
     * tools may generate mips for future packed asset formats, but descriptors
     * submitted directly to the renderer must set this to one.
     */
    std::uint32_t mipCount = 1;

    /**
     * @brief True when `data` contains compressed texture payload.
     *
     * Compressed runtime upload is not supported by this first asset contract;
     * callers must submit uncompressed RGBA8 bytes and leave this false.
     */
    bool compressed = false;

    /** @brief Pointer to tightly packed pixel data; must remain valid for the call. */
    const void* data = nullptr;

    /** @brief Number of bytes available through `data`. */
    std::uint32_t dataSizeBytes = 0;
};

/** @brief Material pipeline used when interpreting a material descriptor. */
enum class MaterialKind
{
    /** @brief Basic mesh material using vertex color and a linear base color. */
    Basic,

    /** @brief Terrain material that blends four albedo layers and optional normal maps with an RGBA splat map. */
    TerrainSplat
};

/**
 * @brief Alpha/depth policy for the first forward material model.
 *
 * This is renderer-facing render state, not a full transparent material system.
 * Opaque and alpha-test materials are planned in the opaque-style forward
 * bucket. Alpha-blend materials are planned after opaque-style geometry with
 * depth testing enabled and depth writes disabled. Transparent draws are
 * stable-sorted back-to-front within their draw family by the implementation;
 * cross-family and intersecting transparency remain known limitations.
 */
enum class MaterialAlphaMode
{
    /** @brief Ordinary opaque rendering with depth writes enabled. */
    Opaque,

    /**
     * @brief Discard fragments whose final vertex/material alpha is below `MaterialDesc::alphaCutoff`.
     *
     * Alpha-test materials keep opaque-style depth writes in the forward pass.
     * Shadow-map alpha-test clipping is not implemented yet, so alpha-test
     * materials are diagnosed as unsupported shadow casters.
     */
    AlphaTest,

    /**
     * @brief Alpha blend final material output after opaque-style geometry.
     *
     * Alpha-blend materials use depth testing and no depth writes. They do not
     * cast shadows in this first policy milestone.
     */
    AlphaBlend
};

/** @brief Maximum terrain splat layers supported by the first terrain material path. */
constexpr std::uint32_t kMaxTerrainMaterialLayers = 4;

/**
 * @brief One terrain material layer for RGBA texture and normal splatting.
 *
 * `albedoTexture` is a renderer-owned texture handle. A zero handle is valid
 * and uses `fallbackColorLinear` as a deterministic 1x1 fallback. Layer colors
 * are linear RGBA values and multiply sampled albedo in the terrain shader.
 * `normalTexture` is optional; a zero handle uses a flat tangent-space normal.
 * Texture handles are referenced by the material and must remain live while the
 * material can be submitted.
 */
struct TerrainMaterialLayerDesc
{
    /** @brief Optional renderer-owned RGBA8 albedo texture for this layer. */
    TextureHandle albedoTexture;

    /**
     * @brief Optional renderer-owned tangent-space normal map for this layer.
     *
     * The first terrain normal-map path expects RGBA8 data using encoded RGB
     * tangent-space normals, where `(0.5, 0.5, 1.0)` is flat. By default the
     * green channel is interpreted as positive bitangent, matching the
     * renderer's terrain tangent basis. A zero handle uses a deterministic flat
     * normal and does not prevent material creation.
     */
    TextureHandle normalTexture;

    /** @brief Linear RGBA fallback/multiplier color for this layer. */
    float fallbackColorLinear[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

/**
 * @brief Terrain material data for a four-layer RGBA splat shader.
 *
 * Splat channel convention is R/G/B/A for layers 0/1/2/3. The descriptor is
 * copied during `createMaterial`; texture handles are retained as references
 * and must remain live while materials using them are submitted. UVs are derived
 * from chunk-local X/Z mesh coordinates and multiplied by `uvScale`.
 *
 * Terrain normal maps are blended with the same normalized splat weights as
 * albedo layers. The shader builds a simple terrain tangent basis from local
 * +X, local +Z, and the generated or supplied geometric normal, then mixes the
 * blended normal-map result with the geometric normal by `normalMapStrength`.
 * This affects forward lighting only; shadow depth passes continue to use
 * geometry depth.
 */
struct TerrainMaterialDesc
{
    /** @brief Fixed terrain layer array. Zero texture handles use documented fallbacks. */
    TerrainMaterialLayerDesc layers[kMaxTerrainMaterialLayers];

    /** @brief Multiplier applied to chunk-local X/Z coordinates before sampling textures. */
    float uvScale = 0.25f;

    /**
     * @brief Blend strength for terrain layer normal maps in `[0, 1]`.
     *
     * Zero uses only the mesh geometric normal. One uses the fully blended
     * tangent-space layer normal maps where present, with missing normal maps
     * contributing a flat normal. Values outside `[0, 1]` or non-finite values
     * are rejected during material creation.
     */
    float normalMapStrength = 0.0f;

    /**
     * @brief Flips the terrain normal-map green channel when true.
     *
     * Leave false for the renderer's default +Y/positive-bitangent convention.
     * Set true for DirectX-style normal maps that encode the opposite green
     * channel direction. The setting is copied during material creation.
     */
    bool flipNormalMapY = false;
};

/**
 * @brief Texture slots for the basic material model.
 *
 * Zero texture handles are valid deterministic fallbacks. Non-zero handles
 * must refer to live renderer-owned textures when `createMaterial` is called
 * and while the material can be submitted. The first basic mesh shader path
 * samples `baseColor` with mesh UV0; the other slots are copied for future
 * shader work.
 */
struct BasicMaterialTextureDesc
{
    /** @brief Optional base-color/albedo texture. */
    TextureHandle baseColor;

    /** @brief Optional tangent-space normal map. */
    TextureHandle normal;

    /** @brief Optional packed metallic/roughness texture. */
    TextureHandle metallicRoughness;

    /** @brief Optional occlusion texture. */
    TextureHandle occlusion;

    /** @brief Optional emissive color texture. */
    TextureHandle emissive;
};

/**
 * @brief Descriptor for a minimal renderer-owned forward material.
 *
 * Colors are linear RGBA values in `[0, 1]`. The current material model is a
 * deliberately small Phase 1 surface color plus an optional directional-light
 * response. The basic mesh shader multiplies vertex color, base color, and the
 * optional base-color texture sampled with UV0. Descriptor data is copied
 * during `createMaterial`.
 */
struct MaterialDesc
{
    /** @brief Material pipeline. Basic materials ignore `terrain`. */
    MaterialKind kind = MaterialKind::Basic;

    /** @brief Linear RGBA multiplier applied to mesh vertex color. */
    float baseColorLinear[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    /** @brief Enables the basic directional-light term when true. */
    bool lit = true;

    /**
     * @brief Alpha/depth bucket for basic forward materials.
     *
     * `TerrainSplat` materials currently require `Opaque`; terrain alpha
     * materials are deferred. The renderer copies this value during
     * `createMaterial` and uses it for CPU-side render bucket planning,
     * transparent sorting diagnostics, and backend-private draw state.
     */
    MaterialAlphaMode alphaMode = MaterialAlphaMode::Opaque;

    /**
     * @brief Alpha-test cutoff in `[0, 1]`.
     *
     * Used only when `alphaMode` is `MaterialAlphaMode::AlphaTest`. The cutoff
     * compares against final vertex/material alpha in the forward material
     * shader. Non-finite or out-of-range values fail material creation.
     */
    float alphaCutoff = 0.5f;

    /** @brief Terrain splat material settings used when `kind` is `TerrainSplat`. */
    TerrainMaterialDesc terrain;

    /** @brief Basic material texture slots used when `kind` is `Basic`. */
    BasicMaterialTextureDesc basicTextures;
};

/**
 * @brief Camera/view matrices supplied by the host for the current frame.
 *
 * Matrices are column-major 4x4 floats, matching the memory layout accepted by
 * bgfx and common engine math libraries. The host owns camera behavior and
 * supplies both matrices every frame; the renderer copies them during
 * `submit`. Projection matrices should be prepared for the active backend clip
 * convention when the renderer exposes that helper in a later milestone.
 */
struct RenderViewDesc
{
    /** @brief Column-major world-to-view matrix. */
    float view[16] = {};

    /** @brief Column-major view-to-clip projection matrix. */
    float projection[16] = {};
};

/**
 * @brief Basic directional light parameters for the Phase 1 forward pass.
 *
 * Direction is a world-space vector pointing from the surface toward the light.
 * Color and intensity are linear. Descriptor data is copied during `submit`.
 */
struct DirectionalLightDesc
{
    /** @brief World-space light direction; zero length is invalid. */
    float directionWorld[3] = {0.0f, 1.0f, 0.0f};

    /** @brief Linear RGB light color. */
    float colorLinear[3] = {1.0f, 1.0f, 1.0f};

    /** @brief Non-negative light strength multiplier. */
    float intensity = 1.0f;
};

/** @brief Maximum CPU-side directional shadow cascades described by the public configuration. */
constexpr std::uint32_t kMaxDirectionalShadowCascades = 4;

/**
 * @brief Split distribution used for directional terrain shadow cascades.
 *
 * These modes configure deterministic CPU split ranges, per-cascade shadow
 * render passes, terrain shader cascade selection, and debug frustum
 * visualization. Split distances are measured in camera-view meters.
 */
enum class ShadowCascadeSplitMode
{
    /** @brief Evenly divides the configured shadow distance in view-space meters. */
    Uniform,

    /** @brief Uses logarithmic spacing, placing more resolution near the camera. */
    Logarithmic,

    /** @brief Blends uniform and logarithmic splits using `cascadeSplitLambda`. */
    Practical
};

/**
 * @brief Terrain shadow filtering mode for CSM sampling.
 *
 * Filtering is applied in the terrain splat shader after cascade selection and
 * before shadow-strength darkening. Higher quality modes perform more shadow
 * map taps and are intended for debug-tunable quality/performance tradeoffs.
 */
enum class ShadowFilterMode
{
    /** @brief Single nearest-depth comparison; fastest and sharpest. */
    Nearest,

    /** @brief Four-tap box PCF around the selected shadow texel. */
    Pcf2x2,

    /** @brief Nine-tap box PCF around the selected shadow texel. */
    Pcf3x3
};

/**
 * @brief Directional cascaded shadow settings for terrain and lit mesh rendering.
 *
 * When enabled, terrain batches plus opt-in static and instanced mesh casters
 * selected by each cascade's light/shadow frustum are rendered into
 * renderer-owned shadow maps. Terrain splat materials and lit basic mesh
 * materials select and sample the appropriate cascade in the forward pass.
 * Shadow caster selection considers renderer-submitted bounds for the frame,
 * so off-camera resident chunks and opt-in draw bounds may cast when they
 * intersect a cascade light frustum. The descriptor is copied during `submit`;
 * it owns no resources and exposes no backend handles.
 *
 * `centerWorld` and `extentMeters` define an axis-aligned orthographic light
 * projection in world units. A typical first integration sets `centerWorld` to
 * the camera position and uses `extentMeters` as the half-size coverage radius.
 * Stable cascade snapping reduces shimmer, but large worlds still need future
 * streaming-aware coverage and precision work.
 *
 * @note Thread safety: Populate and submit this descriptor on the renderer
 * owner thread.
 * @note Frame validity: Settings are read only during `IRenderer::submit`.
 * @warning Skinned/animated casters, shadow atlases, and advanced filtering are
 * not implemented in this phase.
 */
struct DirectionalShadowDesc
{
    /** @brief Enables directional shadow maps for active cascades. */
    bool enabled = false;

    /**
     * @brief Requested square shadow map resolution in texels.
     *
     * Valid values are `128..4096`. The backend recreates cascade shadow
     * targets when this value changes. If shadows are disabled, this value is
     * ignored.
     */
    std::uint32_t mapResolution = 1024;

    /**
     * @brief World-space center of the orthographic shadow projection.
     *
     * Values are meters, Y-up, and right-handed. The sample tracks this to the
     * camera position. Stable cascade projection snaps the derived light-space
     * projection center; this world-space center remains caller supplied.
     */
    float centerWorld[3] = {};

    /**
     * @brief Half-size of the orthographic projection in meters.
     *
     * The shadow box covers approximately `extentMeters * 2` in light-view X/Y
     * and depth. Must be finite and greater than zero when shadows are enabled.
     */
    float extentMeters = 48.0f;

    /**
     * @brief Constant depth bias applied during shadow comparison.
     *
     * Bias is expressed in normalized shadow depth units. Must be finite and
     * non-negative. Larger values reduce self-shadowing at the cost of detached
     * shadows.
     */
    float depthBias = 0.0025f;

    /**
     * @brief Slope-scaled depth bias applied during receiver shadow comparison.
     *
     * The shader adds `slopeBias * (1 - dot(normal, lightDirection))` to
     * `depthBias`. Must be finite and non-negative. Increasing this can reduce
     * grazing-angle acne, but too much bias may detach shadows.
     */
    float slopeBias = 0.0015f;

    /**
     * @brief Reserved world-space normal-offset bias in meters.
     *
     * The current terrain shader validates and stores this value but does not
     * offset the receiver position yet. It remains an engine-facing extension
     * point for a later normal-offset bias implementation.
     */
    float normalBias = 0.0f;

    /**
     * @brief Linear shadow darkening strength in `[0, 1]`.
     *
     * Zero means shadows have no visual effect; one applies the full first-pass
     * darkening factor.
     */
    float strength = 0.45f;

    /**
     * @brief Requests a debug wireframe for the current single shadow projection.
     *
     * The wireframe is built from the actual light view/projection matrix used
     * by the shadow pass and rendered through the GPU debug line path. This is
     * debug-only and has no effect when shadows are disabled.
     */
    bool debugDrawLightBounds = false;

    /**
     * @brief Requests debug wire boxes for chunks selected as shadow casters.
     *
     * Camera-visible caster chunks and off-camera-only caster chunks are colored
     * differently by the renderer debug overlay. This flag has no effect when
     * shadows are disabled and does not change terrain culling or rendering.
     */
    bool debugDrawShadowCasters = false;

    /**
     * @brief Requested active terrain shadow cascade count.
     *
     * Values are clamped to `[1, kMaxDirectionalShadowCascades]` by the split
     * computation helpers. The backend renders one shadow depth target per
     * active cascade, and terrain splat shaders select among those cascades by
     * camera-view depth.
     */
    std::uint32_t cascadeCount = 1;

    /**
     * @brief Distribution mode for CPU-side cascade split ranges.
     *
     * This affects split distances, per-cascade light projections, shadow
     * caster selection, and terrain shader cascade selection.
     */
    ShadowCascadeSplitMode cascadeSplitMode = ShadowCascadeSplitMode::Practical;

    /**
     * @brief Blend factor for practical cascade splits in `[0, 1]`.
     *
     * Zero gives uniform splits and one gives logarithmic splits. Ignored by
     * pure uniform and logarithmic modes. Must be finite when shadows are
     * enabled.
     */
    float cascadeSplitLambda = 0.5f;

    /**
     * @brief Snaps each cascade orthographic projection to shadow texel increments.
     *
     * Enabled by default to reduce visible shadow shimmer during small camera
     * movements. Snapping is CPU-side, deterministic, and uses the clamped
     * `mapResolution` for each active cascade. Disabling this keeps the tighter
     * unsnapped light-space bounds, which is mainly useful for debugging.
     */
    bool stableCascadeProjection = true;

    /**
     * @brief Enables smooth terrain shadow blending across cascade boundaries.
     *
     * When enabled, terrain fragments inside the end portion of a non-final
     * cascade sample both the current cascade and the next cascade, then blend
     * their shadow factors. This reduces hard split lines at the cost of one
     * extra shadow lookup inside the blend band. The setting is copied during
     * `submit` and has no effect when fewer than two cascades are active.
     */
    bool cascadeBlendEnabled = true;

    /**
     * @brief Fraction of the current cascade depth span used as the blend band.
     *
     * Values are clamped to `[0, 0.5]` before shader upload. A value of zero
     * disables blending even when `cascadeBlendEnabled` is true. The band is
     * measured in camera-view meters near each cascade's far split and only
     * blends with the next adjacent cascade; the final cascade never samples
     * beyond itself.
     */
    float cascadeBlendFraction = 0.08f;

    /**
     * @brief Filtering mode used when terrain samples cascade shadow maps.
     *
     * Defaults to a small 2x2 PCF kernel for softer terrain shadows. The setting
     * is copied during `submit`, has no effect when shadows are disabled, and
     * never exposes backend sampler or texture handles.
     */
    ShadowFilterMode filterMode = ShadowFilterMode::Pcf2x2;

    /**
     * @brief Maximum camera-view distance covered by CPU-side cascade splits.
     *
     * Units are world meters. Split computation clamps the effective final split
     * to `min(cameraFarMeters, cascadeShadowDistanceMeters)`. Must be finite and
     * greater than `cameraNearMeters` when shadows are enabled.
     */
    float cascadeShadowDistanceMeters = 96.0f;

    /**
     * @brief Camera near plane distance used for CPU-side cascade split ranges.
     *
     * Units are world meters. This is copied during `submit`; callers should
     * keep it synchronized with the projection matrix supplied in
     * `RenderViewDesc`.
     */
    float cascadeCameraNearMeters = 0.1f;

    /**
     * @brief Camera far plane distance used for CPU-side cascade split ranges.
     *
     * Units are world meters. Split computation uses the lesser of this value
     * and `cascadeShadowDistanceMeters` as the effective shadow far distance.
     */
    float cascadeCameraFarMeters = 100.0f;

    /**
     * @brief Requests debug wireframes for all computed CPU-side cascade frusta.
     *
     * The wireframes are derived from each cascade's generated light
     * view/projection matrix. This is debug-only and does not alter the shadow
     * render passes or terrain shader cascade selection.
     */
    bool debugDrawCascadeFrusta = false;

    /**
     * @brief Requests debug wire boxes for terrain chunks selected by each cascade.
     *
     * The renderer evaluates resident submitted terrain chunks against every
     * generated cascade frustum. This flag visualizes those selected casters; it
     * does not allocate extra shadow maps or change terrain culling policy.
     */
    bool debugDrawCascadeCasters = false;
};

/**
 * @brief Simple frame-local sky and distance fog settings.
 *
 * The descriptor is copied during `IRenderer::submit` and owns no resources.
 * Colors are linear RGB values in `[0, 1]`. Distances are world meters measured
 * from the submitted camera/view position through view-space depth. The current
 * implementation provides a fullscreen sky gradient plus linear distance fog
 * for terrain splat materials and lit/unlit basic mesh materials; it does not
 * implement volumetric fog, weather, clouds, or post-processing.
 *
 * @note Thread safety: Populate and submit this descriptor on the renderer
 * owner thread.
 * @note Frame validity: Settings are read only during `IRenderer::submit`.
 */
struct EnvironmentDesc
{
    /** @brief Enables the fullscreen sky gradient background. */
    bool skyEnabled = true;

    /** @brief Linear RGB color at the top of the sky gradient. */
    float skyZenithColorLinear[3] = {0.34f, 0.55f, 0.82f};

    /** @brief Linear RGB color around the horizon. */
    float skyHorizonColorLinear[3] = {0.72f, 0.82f, 0.90f};

    /** @brief Linear RGB color below the horizon/lower screen. */
    float skyGroundColorLinear[3] = {0.47f, 0.54f, 0.50f};

    /** @brief Enables linear distance fog in forward terrain and mesh shaders. */
    bool fogEnabled = true;

    /** @brief Linear RGB fog color blended into material color before gamma output. */
    float fogColorLinear[3] = {0.58f, 0.64f, 0.66f};

    /** @brief View-space depth in meters where linear fog begins. */
    float fogStartMeters = 96.0f;

    /**
     * @brief View-space depth in meters where linear fog reaches full strength.
     *
     * Must be greater than `fogStartMeters` when fog is enabled. If fog is
     * disabled, the renderer ignores both distances and uploads neutral fog
     * parameters.
     */
    float fogEndMeters = 180.0f;
};

/**
 * @brief Renderer-facing precipitation category for current-frame weather hooks.
 *
 * The renderer does not spawn or simulate precipitation. A caller or sample
 * maps this value to its own particle batches or other render submissions.
 */
enum class PrecipitationType
{
    /** @brief No precipitation visuals are requested. */
    None,

    /** @brief Rain-style precipitation; sample code may map this to small fast particles. */
    Rain,

    /** @brief Snow-style precipitation; sample code may map this to larger slower particles. */
    Snow,

    /** @brief Dust/ash-style precipitation; sample code may map this to drifting particles. */
    Dust
};

/**
 * @brief Current-frame wind render hook.
 *
 * Wind is backend-neutral renderer input. The renderer validates and uploads a
 * clamped direction/speed for shader paths that can use it, but does not
 * simulate gusts, vegetation, particles, or gameplay effects. Values are copied
 * during `IRenderer::submit`.
 */
struct WindDesc
{
    /** @brief Enables wind hook planning for the current frame. */
    bool enabled = false;

    /** @brief World-space wind direction; zero length falls back to +X during planning. */
    float directionWorld[3] = {1.0f, 0.0f, 0.0f};

    /** @brief Wind speed in meters per second; finite values are clamped non-negative. */
    float speedMetersPerSecond = 0.0f;

    /** @brief Render-facing gust strength in `[0, 1]`; no gust simulation is performed. */
    float gustStrength = 0.0f;
};

/**
 * @brief Current-frame precipitation render hook.
 *
 * Precipitation emission, lifetime, simulation, and weather transitions belong
 * to the caller. The renderer consumes this descriptor for validation, stats,
 * and backend-neutral uniforms only. Samples can use it to fill
 * `ParticleSubmitDesc` batches for the same frame.
 */
struct PrecipitationDesc
{
    /** @brief Enables precipitation render-state planning. */
    bool enabled = false;

    /** @brief Precipitation category used by the caller/sample to choose visuals. */
    PrecipitationType type = PrecipitationType::None;

    /** @brief Current-frame precipitation strength in `[0, 1]`. */
    float intensity = 0.0f;

    /** @brief World-space fall direction; zero length falls back to downward. */
    float directionWorld[3] = {0.0f, -1.0f, 0.0f};

    /** @brief Linear RGBA tint intended for caller-owned precipitation particles. */
    float particleTintLinear[4] = {0.70f, 0.78f, 0.90f, 0.42f};

    /** @brief Multiplier for caller-owned precipitation particle size; clamped non-negative. */
    float particleSizeScale = 1.0f;

    /** @brief Multiplier for caller-owned precipitation particle alpha; clamped to `[0, 1]`. */
    float particleAlphaScale = 1.0f;

    /**
     * @brief Indicates precipitation is represented by caller-owned particle batches.
     *
     * This is a render/debug contract only; the renderer does not create those
     * batches and cannot infer which submitted particle batches came from
     * weather.
     */
    bool usesParticleBatches = true;
};

/**
 * @brief Current-frame wetness render hook.
 *
 * Wetness is reversible per-frame render state. It does not modify material
 * resources, simulate accumulation, create puddles, or paint terrain. The first
 * implementation supports simple color darkening for terrain and basic mesh
 * forward shaders when enabled.
 */
struct WetnessDesc
{
    /** @brief Enables wetness render planning. */
    bool enabled = false;

    /** @brief Wetness amount in `[0, 1]`. */
    float amount = 0.0f;

    /** @brief Maximum color darkening applied at full wetness in `[0, 1]`. */
    float darkeningAmount = 0.20f;

    /** @brief Applies wetness darkening to terrain splat shading when true. */
    bool terrainWetnessEnabled = true;

    /** @brief Applies wetness darkening to basic static, instanced, and skinned mesh shading when true. */
    bool meshWetnessEnabled = true;
};

/**
 * @brief Weather-driven blend over the existing sky/fog descriptor.
 *
 * This descriptor adjusts the submitted `EnvironmentDesc` for the current
 * frame. It does not own weather transitions or replace the base environment;
 * disabling it preserves the original fog settings.
 */
struct FogBlendDesc
{
    /** @brief Enables weather fog blending for the current frame. */
    bool enabled = false;

    /** @brief Linear RGB fog color blended toward when enabled. */
    float weatherFogColorLinear[3] = {0.50f, 0.56f, 0.60f};

    /** @brief Blend amount in `[0, 1]` between base fog color and weather fog color. */
    float blendAmount = 0.0f;

    /**
     * @brief Multiplier for base fog start/end distances.
     *
     * Values below one pull fog closer for precipitation or dust. The renderer
     * clamps to a positive implementation range and preserves valid fog order.
     */
    float fogDistanceScale = 1.0f;
};

/**
 * @brief Current-frame renderer-facing weather hooks.
 *
 * Weather simulation, transitions, spawning, gameplay meaning, audio, and
 * regions are caller-owned. The renderer consumes this descriptor during
 * `IRenderer::submit`, validates and clamps render-facing values, uploads a
 * small backend-private weather uniform, and reports diagnostics. The
 * descriptor owns no resources and exposes no backend handles.
 *
 * @note Thread safety: Populate and submit on the renderer owner thread.
 * @note Frame validity: Values are copied during the current `submit` call.
 */
struct WeatherDesc
{
    /** @brief Enables weather hook planning; disabled weather produces neutral render state. */
    bool enabled = false;

    /** @brief Wind render hook for this frame. */
    WindDesc wind;

    /** @brief Precipitation render hook for this frame; emission remains caller-owned. */
    PrecipitationDesc precipitation;

    /** @brief Wetness render hook for terrain/basic mesh shading. */
    WetnessDesc wetness;

    /** @brief Weather-driven fog blend over `RenderPacket::environment`. */
    FogBlendDesc fogBlend;
};

/**
 * @brief Frame-local selected-object outline rendering options.
 *
 * Selection ownership belongs to the caller. The renderer only consumes
 * per-submission `selected` flags during `IRenderer::submit`, renders selected
 * object-like submissions into a backend-owned mask, and composites one global
 * outline style over the current frame. Colors are linear RGBA values.
 * Thickness is expressed in physical pixels and is clamped by the backend.
 *
 * @note Thread safety: Populate and submit this descriptor on the renderer
 * owner thread.
 * @note Frame validity: Settings are read only during `IRenderer::submit`.
 */
struct SelectionOutlineDesc
{
    /** @brief Enables selected-object mask rendering and outline compositing. */
    bool enabled = false;

    /** @brief Linear RGBA outline color composited over the scene. */
    float colorLinear[4] = {1.0f, 0.85f, 0.18f, 1.0f};

    /** @brief Desired outline thickness in physical pixels. */
    float thicknessPixels = 2.0f;
};

/**
 * @brief Frame-local screen-space ambient occlusion settings.
 *
 * SSAO is renderer-owned post-processing input. The descriptor owns no
 * resources and is copied during `IRenderer::submit`. Distances are meters in
 * camera view space. The current implementation captures submitted scene depth
 * into backend-private textures, computes a depth-only AO buffer, and composites
 * it over the frame; it does not expose backend texture or framebuffer handles.
 *
 * @note Thread safety: Populate and submit this descriptor on the renderer
 * owner thread.
 * @note Frame validity: Settings are read only during `IRenderer::submit`.
 */
struct SsaoDesc
{
    /** @brief Enables the backend-private SSAO depth capture, AO pass, and composite. */
    bool enabled = false;

    /**
     * @brief Renders the AO buffer at half viewport resolution.
     *
     * The backend keeps scene-depth capture at full viewport resolution, then
     * evaluates and optionally blurs AO at half size rounded up to at least one
     * pixel per axis. If backend resources cannot be created, SSAO is skipped
     * for the frame and reported through `RendererStats`.
     */
    bool halfResolution = true;

    /**
     * @brief Enables a backend-private separable blur of the AO buffer.
     *
     * The blur is a simple non-depth-aware fullscreen pass pair. It smooths the
     * first depth-only AO result without changing material shaders.
     */
    bool blurEnabled = true;

    /** @brief Blur radius in AO-buffer pixels; valid range is `[0, 4]`. */
    float blurRadiusPixels = 1.0f;

    /**
     * @brief Radius in meters used to derive the screen-space sampling radius.
     *
     * Values must be finite and greater than zero when SSAO is enabled. The
     * backend clamps the effective shader radius to a small implementation
     * range to keep the first pass inexpensive.
     */
    float radiusMeters = 1.5f;

    /** @brief AO darkening multiplier in `[0, 4]`. */
    float intensity = 0.55f;

    /** @brief View-depth bias in meters used to reduce self-occlusion artifacts. */
    float biasMeters = 0.08f;

    /** @brief Contrast exponent applied to the AO term; valid range is `[0.1, 8]`. */
    float power = 1.4f;

    /** @brief Maximum view depth in meters encoded into the private SSAO depth target. */
    float maxDistanceMeters = 90.0f;

    /**
     * @brief Number of fixed shader samples to use.
     *
     * The first shader path supports `4` or `8` samples; other valid values are
     * clamped to the nearest supported mode by the backend.
     */
    std::uint32_t sampleCount = 8;

    /**
     * @brief Displays the AO buffer as grayscale instead of compositing darkening.
     *
     * This is a renderer-owned fullscreen visualization mode. It does not
     * expose texture handles or require ImGui texture previews.
     */
    bool debugVisualize = false;
};

/** @brief Maximum per-frame projected decals consumed by the first decal planner. */
constexpr std::uint32_t kMaxFrameDecals = 64;

/**
 * @brief One frame-local projected decal volume.
 *
 * Decals are caller-owned renderer submissions, not gameplay objects. The
 * renderer copies the descriptor during `IRenderer::submit` and does not retain
 * pointers after the call. `transform` is a column-major local decal frame to
 * world matrix; its translation is the projector center and its basis columns
 * orient the decal volume. `halfExtentsMeters` defines the positive local
 * volume extents in meters before the transform basis is applied. A zero
 * `albedoTexture` uses `tintColorLinear` as a fallback color.
 */
struct DecalDesc
{
    /** @brief Column-major local decal frame to world transform; must be finite and invertible. */
    float transform[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};

    /** @brief Positive local projector half-extents in meters. */
    float halfExtentsMeters[3] = {1.0f, 1.0f, 1.0f};

    /** @brief Optional renderer-owned RGBA8 decal texture; zero uses fallback tint color. */
    TextureHandle albedoTexture;

    /** @brief Linear RGBA tint/fallback color in `[0, 1]`. */
    float tintColorLinear[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    /** @brief Decal opacity. Values are clamped to `[0, 1]` during planning. */
    float opacity = 1.0f;

    /** @brief Caller-provided deterministic ordering key; lower values are planned first. */
    std::uint32_t sortKey = 0;
};

/**
 * @brief Frame-local projected decal submission list.
 *
 * The descriptor owns no resources and is read only during `submit`.
 * `decals` may be null only when `decalCount` is zero or decals are disabled.
 * The renderer validates and plans up to `kMaxFrameDecals` oriented-box decal
 * volumes, optionally culls them against the active camera frustum, and can
 * draw debug volumes. Active projected compositing is backend-private and uses
 * renderer-owned scene color/depth targets when available.
 *
 * @note Thread safety: Populate and submit on the renderer owner thread.
 * @note Frame validity: Decal pointer storage must remain valid only for the
 * current `IRenderer::submit` call.
 */
struct DecalSubmitDesc
{
    /** @brief Enables decal planning and diagnostics for the current frame. */
    bool enabled = false;

    /** @brief Draws planned decal oriented bounds through the debug line path. */
    bool debugDrawVolumes = false;

    /**
     * @brief Culls decal bounds against the active camera frustum when true.
     *
     * Frustum culling is CPU-side and deterministic for the submitted frame.
     * Partially intersecting decal volumes remain active. Disable this only for
     * diagnostics or when the caller wants all valid decals to reach backend
     * planning regardless of camera visibility.
     */
    bool cullAgainstViewFrustum = true;

    /**
     * @brief Optional projection depth limit in meters.
     *
     * Zero uses the full local projector depth (`halfExtentsMeters[1]`).
     * Positive values clamp visible decal projection to the nearer of this
     * value and the decal's local half-depth. Non-finite or negative values are
     * treated as zero during planning.
     */
    float maxProjectionDepthMeters = 0.0f;

    /**
     * @brief Optional edge fade distance in meters near the projection depth limit.
     *
     * Zero keeps a hard projector-volume clip. Positive values fade decal
     * opacity as receiver depth approaches the configured projection depth
     * limit. Values are clamped to the active depth limit during planning.
     */
    float projectionEdgeFadeMeters = 0.0f;

    /** @brief Pointer to `decalCount` frame-local decal descriptors. */
    const DecalDesc* decals = nullptr;

    /** @brief Number of descriptors available through `decals`; planning clamps to `kMaxFrameDecals`. */
    std::uint32_t decalCount = 0;
};

/** @brief Maximum per-frame particle batches consumed by the first particle planner. */
constexpr std::uint32_t kMaxFrameParticleBatches = 16;

/** @brief Maximum individual particles copied into the first frame-local particle plan. */
constexpr std::uint32_t kMaxFrameParticles = 1024;

/**
 * @brief Blend policy for a submitted particle batch.
 *
 * The initial particle path supports conventional source-alpha blending over
 * the rendered scene. Values are backend-neutral and may be expanded in later
 * milestones without changing ownership of particle simulation.
 */
enum class ParticleBlendMode
{
    /** @brief Alpha blend using particle texture alpha multiplied by particle color alpha. */
    Alpha
};

/**
 * @brief Deterministic particle ordering policy for frame-local particle planning.
 *
 * Sorting is optional and applies only to copied frame-local particle planning
 * data. The renderer never mutates caller-owned particle arrays. Distance
 * sorting uses the submitted camera view for the current frame and preserves
 * stable source order for equal distances.
 */
enum class ParticleSortMode
{
    /** @brief Draw particles in the order supplied by the caller. */
    SubmissionOrder,

    /** @brief Sort accepted batches back-to-front by their computed batch bounds center. */
    BatchDistanceBackToFront,

    /** @brief Sort particles back-to-front within each accepted batch. */
    ParticleDistanceBackToFront
};

/**
 * @brief One frame-local camera-facing particle billboard.
 *
 * Particle emission, lifetime, simulation, and destruction are caller-owned.
 * The renderer copies accepted particles during `IRenderer::submit` and never
 * retains pointers to caller arrays after the call. Positions are world-space
 * meters in the renderer's Y-up right-handed convention. `sizeMeters` is the
 * square billboard side length in world units. Colors are linear RGBA in
 * `[0, 1]`, `rotationRadians` rotates the billboard around the camera-facing
 * axis, and `uvRect` is `{u0, v0, u1, v1}` for one texture frame.
 */
struct Particle
{
    /** @brief World-space billboard center in meters. */
    float positionWorld[3] = {};

    /** @brief Square billboard side length in meters; must be finite and positive. */
    float sizeMeters = 1.0f;

    /** @brief Linear RGBA particle tint multiplied with the batch texture. */
    float colorLinear[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    /** @brief Rotation around the view-facing axis in radians. */
    float rotationRadians = 0.0f;

    /** @brief Texture coordinate rectangle `{u0, v0, u1, v1}` for this particle. */
    float uvRect[4] = {0.0f, 0.0f, 1.0f, 1.0f};
};

/**
 * @brief One frame-local particle billboard batch.
 *
 * A batch shares one optional renderer-owned texture, blend mode, and sorting
 * policy. `particles` may be null only when `particleCount` is zero. The
 * pointed-to array is read during `IRenderer::submit` only; the renderer copies
 * accepted particle data into frame-local planning storage. A zero or invalid
 * texture handle uses a backend-owned white fallback texture with particle
 * color/alpha still applied.
 */
struct ParticleBatchDesc
{
    /** @brief Pointer to `particleCount` frame-local particle descriptors. */
    const Particle* particles = nullptr;

    /** @brief Number of particle descriptors available through `particles`. */
    std::uint32_t particleCount = 0;

    /** @brief Optional renderer-owned RGBA8 texture shared by the batch. */
    TextureHandle texture;

    /** @brief Blend policy for this batch. */
    ParticleBlendMode blendMode = ParticleBlendMode::Alpha;

    /**
     * @brief Optional ordering policy for particles inside this batch.
     *
     * `SubmissionOrder` preserves the caller's particle order. 
     * `ParticleDistanceBackToFront` sorts copied particles inside this batch
     * using the current frame camera position. `BatchDistanceBackToFront` is
     * ignored at batch scope; use `ParticleSubmitDesc::sortMode` for
     * submission-level batch sorting.
     */
    ParticleSortMode sortMode = ParticleSortMode::SubmissionOrder;
};

/**
 * @brief Frame-local particle billboard submission list.
 *
 * The descriptor owns no resources. Particle emission and simulation are
 * external to the renderer; callers submit the particles they want rendered for
 * the current frame. Disabled or empty submissions are valid and produce no
 * particle pass work. The renderer validates and copies up to
 * `kMaxFrameParticleBatches` batches and `kMaxFrameParticles` particles during
 * `IRenderer::submit`. Batch frustum culling and sorting operate on copied
 * render-plan data only. Soft particles use backend-private scene depth when
 * available and fall back to ordinary hard alpha billboards if the depth input
 * cannot be produced for the frame.
 *
 * @note Thread safety: Populate and submit on the renderer owner thread.
 * @note Frame validity: Batch and particle pointer storage must remain valid
 * only for the current `IRenderer::submit` call.
 */
struct ParticleSubmitDesc
{
    /** @brief Enables particle planning and billboard rendering for the current frame. */
    bool enabled = false;

    /** @brief Pointer to `batchCount` frame-local particle batches. */
    const ParticleBatchDesc* batches = nullptr;

    /** @brief Number of particle batches available through `batches`. */
    std::uint32_t batchCount = 0;

    /**
     * @brief Culls accepted batch bounds against the submitted camera frustum.
     *
     * Bounds are conservatively computed from valid particle positions and
     * billboard sizes in world meters. Disabling this preserves pre-polish
     * behavior by rendering every accepted batch regardless of camera
     * visibility.
     */
    bool cullAgainstViewFrustum = true;

    /**
     * @brief Optional submission-level sort policy.
     *
     * `BatchDistanceBackToFront` sorts accepted batches by distance to the
     * current frame camera. `ParticleDistanceBackToFront` asks each accepted
     * batch to sort copied particles internally. Sorting is deterministic for
     * equal distances and never mutates caller-owned arrays.
     */
    ParticleSortMode sortMode = ParticleSortMode::SubmissionOrder;

    /**
     * @brief Enables soft-particle depth fading when backend-private depth is available.
     *
     * When enabled, the backend captures opaque scene depth before the
     * particle pass and fades particle alpha near opaque intersections. If the
     * depth path is unavailable or resource creation fails, particles render
     * normally and the inactive soft-particle state is reported in stats.
     */
    bool softParticlesEnabled = false;

    /**
     * @brief Fade distance in meters for soft-particle intersections.
     *
     * Values are clamped during planning. Zero disables the fade even when
     * `softParticlesEnabled` is true. The default is conservative for smoke or
     * dust near terrain.
     */
    float softParticleFadeDistanceMeters = 0.75f;
};

/**
 * @brief One mesh/material instance submitted for the current frame.
 *
 * The model matrix is column-major and transforms local mesh coordinates into
 * world space. Handles must be live resources created by the same renderer.
 * Draw data is referenced only for the duration of `submit`.
 */
struct DrawItem
{
    /** @brief Renderer-owned mesh resource to draw. */
    MeshHandle mesh;

    /** @brief Renderer-owned material resource used for this draw. */
    MaterialHandle material;

    /** @brief Column-major local-to-world transform. */
    float model[16] = {};

    /**
     * @brief World-space bounds used when this draw casts cascaded shadows.
     *
     * Bounds are ignored unless `castsShadow` is true. When shadow casting is
     * enabled, bounds must be finite, non-inverted, expressed in meters, and in
     * the same Y-up right-handed world space as terrain chunks.
     */
    Aabb bounds;

    /**
     * @brief Enables this draw as a directional-shadow caster.
     *
     * Shadow casting is opt-in so existing submissions without bounds remain
     * valid. When true, `submit` tests `bounds` against each active cascade
     * light frustum and renders the mesh into matching cascade depth passes.
     * Lit basic mesh materials also receive CSM shadow data in the forward
     * mesh pass; unlit materials remain unshadowed.
     */
    bool castsShadow = false;

    /**
     * @brief Marks this draw for frame-local selection outline rendering.
     *
     * The caller owns selection state. The renderer consumes this flag only
     * during `IRenderer::submit`; when outline rendering is disabled it has no
     * rendering cost.
     */
    bool selected = false;

    /**
     * @brief Externally supplied per-draw structure fade state.
     *
     * The caller decides whether this draw should fade. The renderer consumes
     * the scalar only during this frame's forward pass and does not perform
     * camera obstruction, roof, building, room, or gameplay visibility logic.
     * Shadow casting remains controlled by `castsShadow` and uses the existing
     * full-opacity shadow policy in this milestone.
     */
    FadeDesc fade;
};

/**
 * @brief One GPU-instanced mesh/material batch submitted for the current frame.
 *
 * The caller owns the model-matrix storage and the renderer reads it only
 * during `submit`. Matrices are column-major local-to-world transforms laid out
 * as `instanceCount * 16` contiguous floats. Handles must be live resources
 * created by the same renderer. This descriptor is backend-neutral and does not
 * expose bgfx instance buffers.
 */
struct InstancedDrawDesc
{
    /** @brief Renderer-owned mesh shared by all instances in the batch. */
    MeshHandle mesh;

    /** @brief Renderer-owned material shared by all instances in the batch. */
    MaterialHandle material;

    /** @brief Pointer to `instanceCount` column-major model matrices. */
    const float* modelMatrices = nullptr;

    /** @brief Number of model matrices available through `modelMatrices`. */
    std::uint32_t instanceCount = 0;

    /**
     * @brief Aggregate world-space bounds for the instanced batch.
     *
     * Bounds are ignored unless `castsShadow` is true. When shadow casting is
     * enabled, bounds must enclose the submitted instances in world-space meters
     * and use the renderer's Y-up right-handed coordinate convention.
     */
    Aabb bounds;

    /**
     * @brief Enables the instanced batch as a directional-shadow caster.
     *
     * Shadow casting is opt-in. When enabled, the aggregate `bounds` are tested
     * against each active cascade and the whole batch is submitted to matching
     * cascade depth passes. Per-instance caster culling is deferred.
     */
    bool castsShadow = false;

    /**
     * @brief Marks the whole instanced batch for selection outline rendering.
     *
     * This first implementation is batch-level for instancing: every instance
     * in the batch contributes to the outline mask when selected. Per-instance
     * selection IDs are deferred.
     */
    bool selected = false;

    /**
     * @brief Externally supplied batch-level structure fade state.
     *
     * Fade applies to every instance in the batch. Per-instance fade scalars
     * are deferred so the existing instancing data layout and batching behavior
     * remain unchanged.
     */
    FadeDesc fade;
};

/**
 * @brief Immutable render data submitted for one frame.
 *
 * The host owns all pointer storage. `submit` reads the view, light, draw
 * arrays, and optional terrain descriptor during the call only. `drawItems`
 * `instancedDraws`, and `animatedDraws` may be null only when their matching
 * counts are zero.
 */
struct RenderPacket
{
    /** @brief Camera matrices for this submission. */
    RenderViewDesc view;

    /** @brief Directional light used by the basic forward pass. */
    DirectionalLightDesc directionalLight;

    /** @brief Optional cascaded terrain shadow settings for the directional light. */
    DirectionalShadowDesc directionalShadow;

    /** @brief Optional sky and distance fog settings for this frame. */
    EnvironmentDesc environment;

    /** @brief Optional current-frame weather render hooks. */
    WeatherDesc weather;

    /** @brief Optional selected-object outline settings for this frame. */
    SelectionOutlineDesc selectionOutline;

    /** @brief Optional depth-only screen-space ambient occlusion settings. */
    SsaoDesc ssao;

    /** @brief Optional backend-private final color grading and tonemap settings. */
    ColorGradingDesc colorGrading;

    /** @brief Optional frame-local projected decal submissions. */
    const DecalSubmitDesc* decals = nullptr;

    /** @brief Optional frame-local particle billboard submissions. */
    const ParticleSubmitDesc* particles = nullptr;

    /** @brief Pointer to `drawItemCount` draw items, or null for an empty packet. */
    const DrawItem* drawItems = nullptr;

    /** @brief Number of draw items available through `drawItems`. */
    std::uint32_t drawItemCount = 0;

    /**
     * @brief Pointer to `instancedDrawCount` GPU-instanced draw batches.
     *
     * Instanced descriptors are referenced only for the duration of `submit`.
     * They are rendered through the renderer's internal instancing path and may
     * optionally cast shadows when their aggregate bounds are supplied.
     */
    const InstancedDrawDesc* instancedDraws = nullptr;

    /** @brief Number of instanced draw descriptors available through `instancedDraws`. */
    std::uint32_t instancedDrawCount = 0;

    /**
     * @brief Pointer to `animatedDrawCount` skinned mesh draw items.
     *
     * Animated descriptors are referenced only for the duration of `submit`.
     * The renderer expects final CPU-provided skinning palettes and does not
     * evaluate animation clips or retain pose pointers after the call returns.
     */
    const AnimatedDrawItem* animatedDraws = nullptr;

    /** @brief Number of animated draw descriptors available through `animatedDraws`. */
    std::uint32_t animatedDrawCount = 0;

    /** @brief Optional skeletal debug visualization settings for this submission. */
    AnimationDebugOptions animationDebug;

    /**
     * @brief Optional terrain chunk submission for this frame.
     *
     * When non-null, the renderer reads terrain submission data during
     * `submit`, performs CPU frustum culling and LOD selection, and expands
     * visible chunks into internal terrain instance batches. The pointed-to
     * descriptor and handle array are referenced only for the duration of the
     * call.
     */
    const TerrainSubmitDesc* terrain = nullptr;
};

/**
 * @brief Backend-neutral culling diagnostics for one renderable category.
 *
 * These counters are CPU-side debug and validation data for the most recent
 * frame. They do not expose backend objects and are not GPU timing data.
 * Categories that do not implement a field leave it at zero. Bounds-based
 * visibility is diagnostic for categories whose public descriptors treat
 * bounds as optional; the renderer preserves existing submission behavior
 * unless a category explicitly documents active CPU culling.
 */
struct CullingCategoryStats
{
    /** @brief Renderable descriptors submitted for this category. */
    std::uint32_t submittedCount = 0;

    /** @brief Submitted descriptors considered resident or otherwise eligible for planning. */
    std::uint32_t residentCount = 0;

    /** @brief Submitted descriptors with renderer-facing handles that passed public validation. */
    std::uint32_t validResourceCount = 0;

    /** @brief Submitted descriptors rejected or diagnosed because handles or resources were invalid. */
    std::uint32_t invalidResourceCount = 0;

    /** @brief Submitted descriptors that intersected the active camera frustum or had no usable bounds. */
    std::uint32_t visibleCount = 0;

    /** @brief Submitted descriptors with usable bounds fully outside the active camera frustum. */
    std::uint32_t frustumCulledCount = 0;

    /** @brief Submitted descriptors rejected by distance-only rules, when a category uses them. */
    std::uint32_t distanceCulledCount = 0;

    /** @brief Visible/rendered count by LOD level for categories with explicit LOD buckets. */
    std::uint32_t lodCounts[kMaxTerrainLodLevels] = {};

    /** @brief Descriptors selected as directional shadow casters across all cascades. */
    std::uint32_t shadowCasterCount = 0;

    /** @brief Shadow casters selected while outside the camera frustum. */
    std::uint32_t offCameraShadowCasterCount = 0;

    /** @brief Descriptors rejected by validation, culling, count limits, or missing required data. */
    std::uint32_t rejectedCount = 0;

    /** @brief Descriptors using documented material, texture, or color fallback behavior. */
    std::uint32_t fallbackResourceCount = 0;

    /** @brief Backend-facing draw or batch submissions represented by this category. */
    std::uint32_t drawSubmissionCount = 0;

    /** @brief Descriptors with finite bounds usable for CPU-side diagnostic culling. */
    std::uint32_t approximateBoundsCount = 0;
};

/**
 * @brief CPU-side frame budget stage identifiers for debug diagnostics.
 *
 * Stages describe renderer submission/planning work only. Durations are
 * measured with the host CPU monotonic clock by the renderer implementation and
 * do not include GPU execution time, driver queue depth, or operating-system
 * scheduler effects. The enum values are stable array indices for
 * `FrameBudgetStats::stageCpuMicroseconds`.
 */
enum class FrameBudgetStage : std::uint32_t
{
    /** @brief `IRenderer::beginFrame` validation and backend frame setup. */
    BeginFrame = 0,

    /** @brief `RenderPacket` and animated submission validation. */
    SubmitValidation,

    /** @brief Shared CPU culling diagnostic aggregation. */
    CullingDiagnostics,

    /** @brief Terrain chunk culling, LOD, and camera draw batch planning. */
    TerrainPlanning,

    /** @brief Static mesh draw staging and planning estimates. */
    StaticMeshPlanning,

    /** @brief Public and terrain instanced-batch staging/planning. */
    InstancedPlanning,

    /** @brief Skinned draw and palette staging/planning estimates. */
    SkinnedPlanning,

    /** @brief Directional shadow cascade and caster batch planning. */
    ShadowPlanning,

    /** @brief Projected decal planning and staged descriptor estimates. */
    DecalPlanning,

    /** @brief Particle batch planning, sorting/culling estimates, and staged descriptor estimates. */
    ParticlePlanning,

    /** @brief Selection/outline planning estimates. */
    SelectionOutlinePlanning,

    /** @brief Scene/post/final-present planning estimates. */
    PostPassPlanning,

    /** @brief Debug overlay line generation and staging. */
    DebugOverlayPlanning,

    /** @brief Backend submission entry point, including backend-private pass planning. */
    BackendSubmit,

    /** @brief `IRenderer::endFrame` backend finalization. */
    EndFrame,

    /** @brief Number of frame budget stages. Not a recorded stage. */
    Count
};

/** @brief Number of CPU timing slots in `FrameBudgetStats::stageCpuMicroseconds`. */
constexpr std::uint32_t kFrameBudgetStageCount = static_cast<std::uint32_t>(FrameBudgetStage::Count);

/**
 * @brief Backend-neutral CPU/frame submission budget diagnostics.
 *
 * These counters describe the most recent frame submission in debug-friendly
 * renderer terms. Timing values are CPU-side measurements in microseconds and
 * are approximate. Staged byte and allocation counters are exact only for
 * known renderer staging containers; they do not replace a global allocator,
 * GPU memory accounting, or an external profiler.
 */
struct FrameBudgetStats
{
    /** @brief Non-zero when CPU timing diagnostics were gathered for this frame. */
    std::uint32_t cpuTimingEnabled = 0;

    /** @brief Bitmask of `FrameBudgetStage` slots recorded in `stageCpuMicroseconds`. */
    std::uint32_t recordedStageMask = 0;

    /** @brief Last-frame CPU duration by `FrameBudgetStage`, in microseconds. */
    std::uint64_t stageCpuMicroseconds[kFrameBudgetStageCount] = {};

    /** @brief Sum of recorded last-frame CPU stage durations in microseconds. */
    std::uint64_t totalCpuMicroseconds = 0;

    /** @brief Total renderable descriptors or batches submitted by the caller/sample. */
    std::uint32_t totalSubmittedRenderables = 0;

    /** @brief Total renderable descriptors or batches visible/accepted after CPU planning. */
    std::uint32_t totalVisibleRenderables = 0;

    /** @brief Total renderable descriptors or batches culled by CPU planning. */
    std::uint32_t totalCulledRenderables = 0;

    /** @brief Coarse total draw/pass submission estimate from renderer stats. */
    std::uint32_t totalDrawSubmissionEstimate = 0;

    /** @brief Estimated bytes staged for static draw descriptors. */
    std::uint64_t staticDrawStagedBytes = 0;

    /** @brief Estimated bytes staged for instanced descriptors and model matrices. */
    std::uint64_t instanceStagedBytes = 0;

    /** @brief Estimated bytes staged for skinned draw descriptors and palettes. */
    std::uint64_t skinnedPaletteStagedBytes = 0;

    /** @brief Estimated bytes staged for decal submission descriptors. */
    std::uint64_t decalStagedBytes = 0;

    /** @brief Estimated bytes staged for particle batches and particle descriptors. */
    std::uint64_t particleStagedBytes = 0;

    /** @brief Estimated bytes staged for debug overlay line vertices. */
    std::uint64_t debugDrawStagedBytes = 0;

    /** @brief Sum of estimated staged bytes tracked by this diagnostic block. */
    std::uint64_t totalStagedBytes = 0;

    /** @brief Number of known frame-local staging containers used as an allocation estimate. */
    std::uint32_t frameAllocationEstimateCount = 0;

    /** @brief Terrain chunks created since the previous terrain submission consumed churn stats. */
    std::uint32_t terrainChunksCreatedThisFrame = 0;

    /** @brief Terrain chunks destroyed since the previous terrain submission consumed churn stats. */
    std::uint32_t terrainChunksDestroyedThisFrame = 0;

    /** @brief Terrain chunk slots reused since the previous terrain submission consumed churn stats. */
    std::uint32_t terrainChunkSlotsReusedThisFrame = 0;

    /** @brief Viewport/post/shadow target recreations or releases reported during this frame. */
    std::uint32_t renderTargetRecreateCount = 0;

    /** @brief Post-style passes submitted according to backend-neutral post diagnostics. */
    std::uint32_t postPassSubmittedCount = 0;

    /** @brief Post-style passes skipped according to backend-neutral post diagnostics. */
    std::uint32_t postPassSkippedCount = 0;
};

/**
 * @brief Lightweight renderer counters collected for diagnostics.
 *
 * Values describe the most recently processed frame and current live resources.
 * They are intended for sample/debug displays and coarse integration checks,
 * not detailed GPU profiling.
 */
struct RendererStats
{
    /** @brief Number of frames successfully ended since initialization. */
    std::uint64_t frameIndex = 0;

    /** @brief Draw items accepted by `submit` during the active or last frame. */
    std::uint32_t submittedDraws = 0;

    /** @brief Draw items forwarded to the backend during the active or last frame. */
    std::uint32_t renderedDraws = 0;

    /** @brief Shared culling diagnostics for static mesh draw submissions. */
    CullingCategoryStats staticMeshCulling;

    /** @brief Shared culling diagnostics for GPU-instanced mesh batch submissions. */
    CullingCategoryStats instancedMeshCulling;

    /** @brief Shared culling diagnostics for skinned mesh draw submissions. */
    CullingCategoryStats skinnedMeshCulling;

    /** @brief CPU/frame submission budget diagnostics for the latest frame. */
    FrameBudgetStats frameBudget;

    /** @brief Live basic material resources using the first mesh material path. */
    std::uint32_t materialBasicCount = 0;

    /** @brief Live terrain splat material resources. */
    std::uint32_t materialTerrainSplatCount = 0;

    /** @brief Forward draws or batches submitted through opaque-style material state. */
    std::uint32_t materialOpaqueDraws = 0;

    /** @brief Forward draws or batches submitted through alpha-test material state. */
    std::uint32_t materialAlphaTestDraws = 0;

    /** @brief Forward draws or batches submitted through alpha-blend material or alpha-fade state. */
    std::uint32_t materialAlphaBlendDraws = 0;

    /** @brief Transparent material/fade draws that were stable-sorted by the renderer. */
    std::uint32_t transparentSortedDraws = 0;

    /** @brief Transparent material/fade draws left in submission order by documented policy. */
    std::uint32_t transparentUnsortedDraws = 0;

    /** @brief Particle batches whose CPU-side plan performed distance sorting. */
    std::uint32_t transparentParticleSortedBatches = 0;

    /** @brief Accepted particle batches rendered in submission order. */
    std::uint32_t transparentParticleUnsortedBatches = 0;

    /** @brief Forward draws or batches using opaque-style dither fade state. */
    std::uint32_t materialDitherFadeDraws = 0;

    /** @brief Material descriptors rejected by validation or unsupported combinations. */
    std::uint32_t invalidMaterialCount = 0;

    /** @brief Material descriptors or draw submissions using documented material fallback behavior. */
    std::uint32_t fallbackMaterialCount = 0;

    /** @brief Material texture references using documented fallback textures. */
    std::uint32_t fallbackMaterialTextureCount = 0;

    /** @brief Unsupported shader variant requests diagnosed by CPU-side policy. */
    std::uint32_t unsupportedShaderVariantRequestCount = 0;

    /** @brief Unique backend-private shader variant policy keys observed in the latest frame. */
    std::uint32_t shaderVariantCountInUse = 0;

    /** @brief Alpha material submissions that requested shadow casting where the policy does not support it. */
    std::uint32_t alphaMaterialShadowUnsupportedCount = 0;

    /** @brief Color-space policy warnings diagnosed by material validation or submission planning. */
    std::uint32_t materialColorSpaceWarningCount = 0;

    /** @brief Number of live renderer-owned mesh resources. */
    std::uint32_t liveMeshes = 0;

    /** @brief Number of live renderer-owned skeleton metadata resources. */
    std::uint32_t liveSkeletons = 0;

    /** @brief Number of live renderer-owned skinned mesh resources. */
    std::uint32_t liveSkinnedMeshes = 0;

    /** @brief Number of live renderer-owned texture resources. */
    std::uint32_t liveTextures = 0;

    /** @brief Number of live renderer-owned material resources. */
    std::uint32_t liveMaterials = 0;

    /**
     * @brief Approximate bytes used by live static mesh vertex and index buffers.
     *
     * The value is CPU-side accounting based on descriptor sizes and backend
     * target dimensions. It is not a GPU memory query and may omit driver
     * padding or backend-internal allocations.
     */
    std::uint64_t meshBufferBytes = 0;

    /** @brief Approximate bytes used by live skinned mesh vertex and index buffers. */
    std::uint64_t skinnedMeshBufferBytes = 0;

    /** @brief Approximate bytes used by live renderer-owned texture resources. */
    std::uint64_t textureBytes = 0;

    /** @brief Approximate bytes used by live renderer-owned material descriptors. */
    std::uint64_t materialBytes = 0;

    /** @brief Approximate bytes used by live shadow render targets. */
    std::uint64_t shadowTargetBytes = 0;

    /** @brief Approximate bytes used by the backend-private scene color/depth targets. */
    std::uint64_t sceneTargetBytes = 0;

    /** @brief Approximate bytes used by backend-private SSAO targets. */
    std::uint64_t ssaoTargetBytes = 0;

    /** @brief Approximate bytes used by backend-private selection/outline targets. */
    std::uint64_t selectionTargetBytes = 0;

    /** @brief Approximate bytes used by backend-owned fallback textures. */
    std::uint64_t fallbackTextureBytes = 0;

    /** @brief Approximate total bytes for tracked renderer-owned resources. */
    std::uint64_t totalEstimatedResourceBytes = 0;

    /** @brief Lifetime count of failed static mesh backend allocation attempts. */
    std::uint32_t meshAllocationFailureCount = 0;

    /** @brief Lifetime count of failed skinned mesh backend allocation attempts. */
    std::uint32_t skinnedMeshAllocationFailureCount = 0;

    /** @brief Lifetime count of failed texture backend allocation attempts. */
    std::uint32_t textureAllocationFailureCount = 0;

    /**
     * @brief Lifetime count of failed material resource allocation attempts.
     *
     * Most current materials are CPU-side descriptors, so real backends may
     * leave this at zero. Mocked resource-lifetime tests use it to verify that
     * material creation failures do not register live resources.
     */
    std::uint32_t materialAllocationFailureCount = 0;

    /** @brief Lifetime count of successful selection mask resource recreations. */
    std::uint32_t selectionMaskResourceRecreateCount = 0;

    /** @brief Lifetime count of failed selection mask allocation attempts. */
    std::uint32_t selectionMaskAllocationFailureCount = 0;

    /** @brief Lifetime count of successful SSAO target recreations. */
    std::uint32_t ssaoResourceRecreateCount = 0;

    /** @brief Lifetime count of failed SSAO target allocation attempts. */
    std::uint32_t ssaoAllocationFailureCount = 0;

    /** @brief Non-zero when the backend-owned fallback texture set is valid. */
    std::uint32_t fallbackResourceValid = 0;

    /** @brief Lifetime count of default/zero public handles rejected by backend validation. */
    std::uint32_t invalidHandleUseCount = 0;

    /**
     * @brief Lifetime count of non-zero handles rejected because their resource is not live.
     *
     * This includes destroyed handles, handles from a previous renderer
     * lifetime, and handles outside the current backend registry. Stale handles
     * never expose or dereference backend resources.
     */
    std::uint32_t staleHandleUseCount = 0;

    /** @brief Lifetime count of draw submissions rejected due to invalid or stale handles. */
    std::uint32_t destroyedHandleSubmissionCount = 0;

    /** @brief True when the latest submission requested and ran cascaded shadows. */
    bool terrainShadowsEnabled = false;

    /** @brief Active square shadow map resolution in texels for the latest shadow pass. */
    std::uint32_t shadowMapResolution = 0;

    /** @brief Shadow caster batches considered by the latest cascaded shadow pass. */
    std::uint32_t shadowCasterBatches = 0;

    /** @brief Backend draw calls submitted by the latest cascaded shadow pass. */
    std::uint32_t shadowPassDraws = 0;

    /** @brief Number of active cascade shadow render targets for the latest frame. */
    std::uint32_t shadowCascadeCount = 0;

    /** @brief Number of allocated valid cascade shadow render targets. */
    std::uint32_t shadowCascadeRenderTargetCount = 0;

    /** @brief Requested cascade count after public descriptor clamping for the latest frame. */
    std::uint32_t shadowRequestedCascadeCount = 0;

    /** @brief Requested shadow-map resolution after public descriptor clamping for the latest frame. */
    std::uint32_t shadowRequestedMapResolution = 0;

    /** @brief Non-zero when the latest shadow submission recreated or released backend resources. */
    std::uint32_t shadowResourceReconfigured = 0;

    /** @brief Lifetime count of successful backend shadow resource recreations. */
    std::uint32_t shadowResourceRecreateCount = 0;

    /** @brief Lifetime count of failed backend shadow resource allocation attempts. */
    std::uint32_t shadowResourceAllocationFailureCount = 0;

    /** @brief Non-zero when the latest shadow allocation attempt failed and shadows were skipped. */
    std::uint32_t shadowResourceAllocationFailed = 0;

    /** @brief All shadow caster batches submitted per cascade. */
    std::uint32_t shadowCasterBatchesByCascade[kMaxDirectionalShadowCascades] = {};

    /** @brief Static mesh shadow caster draws submitted per cascade. */
    std::uint32_t shadowStaticMeshCastersByCascade[kMaxDirectionalShadowCascades] = {};

    /** @brief Instanced mesh shadow caster batches submitted per cascade. */
    std::uint32_t shadowInstancedMeshCasterBatchesByCascade[kMaxDirectionalShadowCascades] = {};

    /** @brief Skinned mesh shadow caster draws submitted per cascade. */
    std::uint32_t shadowSkinnedMeshCastersByCascade[kMaxDirectionalShadowCascades] = {};

    /** @brief Terrain shadow caster batches submitted per cascade. */
    std::uint32_t shadowTerrainCasterBatchesByCascade[kMaxDirectionalShadowCascades] = {};

    /** @brief Static mesh forward draws that received CSM shadow data in the latest frame. */
    std::uint32_t shadowStaticMeshReceivers = 0;

    /** @brief Instanced mesh forward batches that received CSM shadow data in the latest frame. */
    std::uint32_t shadowInstancedMeshReceiverBatches = 0;

    /** @brief Skinned mesh forward draws that received CSM shadow data in the latest frame. */
    std::uint32_t shadowSkinnedMeshReceivers = 0;

    /** @brief Backend shadow depth draws submitted for skinned mesh casters. */
    std::uint32_t shadowSkinnedMeshPassDraws = 0;

    /** @brief Backend shadow pass draws submitted per cascade. */
    std::uint32_t shadowPassDrawsByCascade[kMaxDirectionalShadowCascades] = {};

    /** @brief Non-zero when the cascade has a valid backend-owned shadow render target. */
    std::uint32_t shadowCascadeRenderTargetValid[kMaxDirectionalShadowCascades] = {};

    /** @brief True when the latest submission requested and drew the sky gradient. */
    bool skyRendered = false;

    /** @brief True when linear distance fog was enabled for the latest forward pass. */
    bool fogEnabled = false;

    /** @brief Static mesh draws submitted with active fog state in the latest frame. */
    std::uint32_t foggedMeshDraws = 0;

    /** @brief Instanced mesh or terrain batches submitted with active fog state in the latest frame. */
    std::uint32_t foggedInstancedBatches = 0;

    /** @brief True when the latest submission enabled renderer-facing weather hooks. */
    bool weatherEnabled = false;

    /** @brief True when the latest weather plan enabled the wind hook. */
    bool weatherWindEnabled = false;

    /** @brief Effective normalized world-space wind direction used by the latest weather plan. */
    float weatherWindDirectionWorld[3] = {};

    /** @brief Effective wind speed in meters per second after validation and clamping. */
    float weatherWindSpeedMetersPerSecond = 0.0f;

    /** @brief True when the latest weather plan enabled precipitation state. */
    bool weatherPrecipitationEnabled = false;

    /** @brief Effective precipitation type from the latest weather plan. */
    PrecipitationType weatherPrecipitationType = PrecipitationType::None;

    /** @brief Effective precipitation intensity in `[0, 1]` after validation and clamping. */
    float weatherPrecipitationIntensity = 0.0f;

    /** @brief True when precipitation is expected to be represented by caller-owned particle batches. */
    bool weatherPrecipitationUsesParticleBatches = false;

    /** @brief True when the latest weather plan enabled wetness render state. */
    bool weatherWetnessEnabled = false;

    /** @brief Effective weather wetness amount in `[0, 1]`. */
    float weatherWetnessAmount = 0.0f;

    /** @brief Terrain batches submitted while terrain wetness was active. */
    std::uint32_t weatherTerrainWetnessDraws = 0;

    /** @brief Static, instanced, or skinned mesh draws submitted while mesh wetness was active. */
    std::uint32_t weatherMeshWetnessDraws = 0;

    /** @brief True when the latest weather plan adjusted the submitted fog settings. */
    bool weatherFogBlendEnabled = false;

    /** @brief Effective weather fog blend amount in `[0, 1]`. */
    float weatherFogBlendAmount = 0.0f;

    /** @brief Effective fog color after environment and weather blending. */
    float weatherEffectiveFogColorLinear[3] = {};

    /** @brief Effective fog start distance in meters after weather blending. */
    float weatherEffectiveFogStartMeters = 0.0f;

    /** @brief Effective fog end distance in meters after weather blending. */
    float weatherEffectiveFogEndMeters = 0.0f;

    /** @brief Count of weather descriptor values clamped by the latest CPU-side weather plan. */
    std::uint32_t weatherClampedValueCount = 0;

    /** @brief True when disabled or invalid weather produced neutral render state. */
    bool weatherNeutral = true;

    /** @brief Fade descriptors submitted with enabled fade state in the latest frame. */
    std::uint32_t structureFadeSubmittedCount = 0;

    /** @brief Enabled fade descriptors that affect visible rendering after clamping. */
    std::uint32_t structureFadeActiveCount = 0;

    /** @brief Enabled fade descriptors clamped or submitted as fully visible. */
    std::uint32_t structureFadeFullyVisibleCount = 0;

    /** @brief Enabled fade descriptors with visibility strictly between zero and one. */
    std::uint32_t structureFadePartiallyFadedCount = 0;

    /** @brief Enabled fade descriptors clamped or submitted as fully hidden. */
    std::uint32_t structureFadeFullyHiddenCount = 0;

    /** @brief Fade descriptors rejected by validation before backend submission. */
    std::uint32_t structureFadeInvalidCount = 0;

    /** @brief Forward draws or batches using alpha fade state in the latest frame. */
    std::uint32_t structureFadeAlphaDraws = 0;

    /** @brief Forward draws or batches using dithered fade state in the latest frame. */
    std::uint32_t structureFadeDitherDraws = 0;

    /** @brief Fade targets not supported by the first implementation. */
    std::uint32_t structureFadeUnsupportedTargetCount = 0;

    /** @brief True when the latest frame submitted selected-object outline work. */
    bool selectionOutlineEnabled = false;

    /** @brief Non-zero when the backend-owned selection mask target is valid. */
    std::uint32_t selectionMaskTargetValid = 0;

    /** @brief Static mesh draws selected for the latest outline mask pass. */
    std::uint32_t selectedStaticMeshDraws = 0;

    /** @brief Instanced mesh batches selected for the latest outline mask pass. */
    std::uint32_t selectedInstancedBatches = 0;

    /** @brief Instances covered by selected instanced batches in the latest outline mask pass. */
    std::uint32_t selectedInstances = 0;

    /** @brief Skinned mesh draws selected for the latest outline mask pass. */
    std::uint32_t selectedSkinnedMeshDraws = 0;

    /** @brief Backend mask draw submissions for selected objects. */
    std::uint32_t selectionMaskDraws = 0;

    /** @brief Backend fullscreen outline composite draws. */
    std::uint32_t selectionOutlineDraws = 0;

    /** @brief True when the latest frame requested and submitted SSAO work. */
    bool ssaoEnabled = false;

    /** @brief Non-zero when the backend-owned SSAO depth target was valid. */
    std::uint32_t ssaoDepthTargetValid = 0;

    /** @brief Non-zero when the backend-owned SSAO output target was valid. */
    std::uint32_t ssaoOutputTargetValid = 0;

    /** @brief Non-zero when backend-owned SSAO blur targets were valid and blur was active. */
    std::uint32_t ssaoBlurTargetValid = 0;

    /** @brief Active AO buffer width in pixels for the latest SSAO frame. */
    std::uint32_t ssaoAoWidth = 0;

    /** @brief Active AO buffer height in pixels for the latest SSAO frame. */
    std::uint32_t ssaoAoHeight = 0;

    /** @brief True when the latest SSAO frame used half-resolution AO targets. */
    bool ssaoHalfResolution = false;

    /** @brief True when the latest SSAO frame submitted separable blur passes. */
    bool ssaoBlurEnabled = false;

    /** @brief Geometry draws submitted to capture scene depth for SSAO. */
    std::uint32_t ssaoDepthPassDraws = 0;

    /** @brief Fullscreen draws submitted by the SSAO evaluation pass. */
    std::uint32_t ssaoPassDraws = 0;

    /** @brief Fullscreen draws submitted by the AO composite/debug pass. */
    std::uint32_t ssaoCompositeDraws = 0;

    /** @brief Fullscreen draws submitted by SSAO blur passes. */
    std::uint32_t ssaoBlurPassDraws = 0;

    /** @brief Non-zero when SSAO had a valid private depth input for the latest frame. */
    std::uint32_t ssaoInputDepthValid = 0;

    /** @brief True when the latest SSAO pass displayed its AO buffer directly. */
    bool ssaoDebugVisualized = false;

    /** @brief True when the latest submission requested decal planning. */
    bool decalsEnabled = false;

    /** @brief Number of decal descriptors submitted before planning clamps. */
    std::uint32_t decalSubmittedCount = 0;

    /** @brief Valid decal volumes accepted by the latest CPU-side decal plan. */
    std::uint32_t decalActiveCount = 0;

    /** @brief Valid decal volumes culled outside the active camera frustum. */
    std::uint32_t decalCulledCount = 0;

    /** @brief Decal descriptors rejected by validation or count clamping in the latest plan. */
    std::uint32_t decalRejectedCount = 0;

    /** @brief Decal descriptors rejected because transform, extent, color, or opacity data was invalid. */
    std::uint32_t decalInvalidDescriptorRejectCount = 0;

    /** @brief Decal descriptors ignored because the frame exceeded `kMaxFrameDecals`. */
    std::uint32_t decalMaxCountRejectedCount = 0;

    /** @brief Planned decals using fallback tint color because no texture handle was supplied. */
    std::uint32_t decalFallbackColorCount = 0;

    /** @brief True when decal planning culled against the submitted camera frustum. */
    bool decalFrustumCullingEnabled = false;

    /** @brief Planned decal projection depth limit in meters; zero means full volume depth. */
    float decalMaxProjectionDepthMeters = 0.0f;

    /** @brief Planned decal projection edge fade distance in meters. */
    float decalProjectionEdgeFadeMeters = 0.0f;

    /** @brief Planned decal debug volumes emitted through the debug line path. */
    std::uint32_t decalDebugVolumeCount = 0;

    /** @brief Decals actually submitted to the active projected decal pass. */
    std::uint32_t decalRenderedCount = 0;

    /** @brief Backend projected decal pass submissions in the latest frame. */
    std::uint32_t decalPassDraws = 0;

    /** @brief Non-zero when the backend-owned scene color target was valid for decals. */
    std::uint32_t decalSceneColorTargetValid = 0;

    /** @brief Non-zero when the backend-owned scene depth/capture target was valid for decals. */
    std::uint32_t decalSceneDepthTargetValid = 0;

    /** @brief Non-zero when a readable decal depth input was available to the backend. */
    std::uint32_t decalInputDepthValid = 0;

    /** @brief Non-zero when a compositable decal scene color target was available to the backend. */
    std::uint32_t decalInputColorValid = 0;

    /** @brief True when active decal projection was deferred for the latest planned decals. */
    bool decalProjectionDeferred = false;

    /** @brief True when the latest submission requested particle billboard rendering. */
    bool particlesEnabled = false;

    /** @brief Particle batches submitted before planning clamps. */
    std::uint32_t particleSubmittedBatchCount = 0;

    /** @brief Valid particle batches accepted by the latest CPU-side particle plan. */
    std::uint32_t particleAcceptedBatchCount = 0;

    /** @brief Particle batches rejected by validation or count clamping. */
    std::uint32_t particleRejectedBatchCount = 0;

    /** @brief Valid particle batches culled outside the submitted camera frustum. */
    std::uint32_t particleCulledBatchCount = 0;

    /** @brief Particle descriptors submitted before validation and planning clamps. */
    std::uint32_t particleSubmittedCount = 0;

    /** @brief Valid particles accepted by the latest CPU-side particle plan. */
    std::uint32_t particleAcceptedCount = 0;

    /** @brief Particles rejected by validation or count clamping. */
    std::uint32_t particleRejectedCount = 0;

    /** @brief Accepted particle estimate skipped because whole batches were frustum-culled. */
    std::uint32_t particleCulledCount = 0;

    /** @brief Accepted batches reordered by the latest particle render plan. */
    std::uint32_t particleSortedBatchCount = 0;

    /** @brief Accepted particles reordered inside batches by the latest particle render plan. */
    std::uint32_t particleSortedCount = 0;

    /** @brief Accepted batches using the backend-owned fallback white texture. */
    std::uint32_t particleFallbackTextureBatchCount = 0;

    /** @brief Backend particle draw submissions emitted in the latest frame. */
    std::uint32_t particleDrawCalls = 0;

    /** @brief Non-zero when the backend particle shader/resource path is valid. */
    std::uint32_t particleResourceValid = 0;

    /** @brief True when particle planning culled accepted batches against the camera frustum. */
    bool particleCullingEnabled = false;

    /** @brief True when soft-particle fade was requested for the latest submission. */
    bool particleSoftParticlesEnabled = false;

    /** @brief True when soft-particle fade used a valid backend-private depth input. */
    bool particleSoftParticlesActive = false;

    /** @brief Non-zero when a backend-private particle depth input was available. */
    std::uint32_t particleSoftParticleDepthInputValid = 0;

    /** @brief Effective soft-particle fade distance in meters after validation/clamping. */
    float particleSoftParticleFadeDistanceMeters = 0.0f;

    /** @brief True when the latest submission requested final color grading. */
    bool colorGradingEnabled = false;

    /** @brief True when the backend submitted the color grading fullscreen pass. */
    bool colorGradingPassSubmitted = false;

    /** @brief Non-zero when the backend-private scene color target was valid for grading. */
    std::uint32_t colorGradingSceneColorTargetValid = 0;

    /** @brief Non-zero when the color grading shader/uniform resource path was valid. */
    std::uint32_t colorGradingResourceValid = 0;

    /** @brief True when tonemapping was active in the latest color grading plan. */
    bool colorGradingTonemapEnabled = false;

    /** @brief Tonemap operator selected by the latest color grading plan. */
    TonemapOperator colorGradingTonemapOperator = TonemapOperator::None;

    /** @brief Effective exposure in EV stops after validation/clamping. */
    float colorGradingExposureStops = 0.0f;

    /** @brief Effective contrast multiplier after validation/clamping. */
    float colorGradingContrast = 1.0f;

    /** @brief Effective saturation multiplier after validation/clamping. */
    float colorGradingSaturation = 1.0f;

    /** @brief Effective gamma adjustment after validation/clamping. */
    float colorGradingGamma = 1.0f;

    /** @brief True when the latest color grading plan requested LUT contribution. */
    bool colorGradingLutEnabled = false;

    /** @brief True when a requested LUT was actively sampled by the backend. */
    bool colorGradingLutActive = false;

    /** @brief True when the current backend supports active LUT sampling for this path. */
    bool colorGradingLutSamplingSupported = false;

    /** @brief Number of requested LUT contributions that fell back to no LUT. */
    std::uint32_t colorGradingLutFallbackCount = 0;

    /** @brief Descriptor values clamped by the latest CPU-side color grading plan. */
    std::uint32_t colorGradingClampedValueCount = 0;

    /** @brief Debug isolation mode used by the latest color grading plan. */
    ColorGradingDebugMode colorGradingDebugMode = ColorGradingDebugMode::None;

    /** @brief Viewport width in pixels used by the latest backend-private scene/post plan. */
    std::uint32_t postViewportWidth = 0;

    /** @brief Viewport height in pixels used by the latest backend-private scene/post plan. */
    std::uint32_t postViewportHeight = 0;

    /** @brief True when the latest frame required an intermediate scene color/depth target. */
    bool postSceneTargetRequired = false;

    /** @brief True when the backend-private intermediate scene target was active this frame. */
    bool postSceneTargetActive = false;

    /**
     * @brief Diagnostic reason bitmask for the intermediate scene target.
     *
     * Bits are backend-neutral diagnostics: bit 0 SSAO, bit 1 decals, bit 2
     * soft particles, bit 3 reserved for older selection/outline diagnostics,
     * bit 4 color grading, and bit 5 an internal forced scene-target
     * submission path. Selection masks are owned separately and should not
     * require the intermediate scene target by themselves. The values are
     * copied from backend planning for the current frame and expose no backend
     * handles.
     */
    std::uint32_t postSceneTargetReasonMask = 0;

    /** @brief Non-zero when the backend-private scene color target was valid. */
    std::uint32_t postSceneColorTargetValid = 0;

    /** @brief Non-zero when the backend-private readable scene depth path was valid. */
    std::uint32_t postSceneDepthTargetValid = 0;

    /** @brief Scene color target width in pixels, or zero when inactive/invalid. */
    std::uint32_t postSceneColorWidth = 0;

    /** @brief Scene color target height in pixels, or zero when inactive/invalid. */
    std::uint32_t postSceneColorHeight = 0;

    /** @brief Readable scene depth target width in pixels, or zero when inactive/invalid. */
    std::uint32_t postSceneDepthWidth = 0;

    /** @brief Readable scene depth target height in pixels, or zero when inactive/invalid. */
    std::uint32_t postSceneDepthHeight = 0;

    /** @brief True when decals or soft particles required a readable scene depth input. */
    bool postReadableSceneDepthRequired = false;

    /** @brief Non-zero when the final scene present/fullscreen path was planned for submission. */
    std::uint32_t postFinalPresentSubmitted = 0;

    /**
     * @brief Backend-neutral final present mode for the latest frame.
     *
     * Values are `0` for direct swapchain rendering, `1` for a scene-present
     * fullscreen pass, and `2` for a color-graded scene-present fullscreen
     * pass.
     */
    std::uint32_t postPresentMode = 0;

    /** @brief Number of planned post-style passes after opaque scene rendering. */
    std::uint32_t postPassCount = 0;

    /** @brief Number of planned fullscreen post-style passes. */
    std::uint32_t postFullscreenPassCount = 0;

    /** @brief Number of requested post-style passes skipped or blocked by planning. */
    std::uint32_t postSkippedPassCount = 0;

    /**
     * @brief Backend-neutral bitmask explaining skipped or blocked post work.
     *
     * Bits are diagnostic only: bit 0 invalid viewport, bit 1 no active
     * decals, bit 2 no accepted particles, bit 3 no selected objects, bit 4
     * missing scene target, bit 5 missing readable depth, and bit 6 pass-list
     * limit. No backend handles or view IDs are exposed.
     */
    std::uint32_t postSkippedPassReasonMask = 0;

    /** @brief Number of missing or invalid backend-private resources found by post planning. */
    std::uint32_t postInvalidResourceCount = 0;

    /** @brief Non-zero when scene target resources were recreated during the latest frame. */
    std::uint32_t postSceneTargetReconfigured = 0;

    /** @brief Lifetime count of successful scene target resource recreations. */
    std::uint32_t postSceneTargetRecreateCount = 0;

    /** @brief Lifetime count of failed scene target resource allocation attempts. */
    std::uint32_t postSceneTargetAllocationFailureCount = 0;

    /** @brief Non-zero when the latest scene target allocation attempt failed. */
    std::uint32_t postSceneTargetAllocationFailed = 0;

    /** @brief Animated skinned mesh draws accepted in the latest submission. */
    std::uint32_t submittedAnimatedDraws = 0;

    /** @brief Animated skinned mesh draws submitted to the backend in the latest frame. */
    std::uint32_t renderedAnimatedDraws = 0;

    /** @brief Animated draws skipped by backend resource or palette validation in the latest frame. */
    std::uint32_t skippedAnimatedDraws = 0;

    /** @brief Static debug line vertices produced for skeletal debug visualization. */
    std::uint32_t animationDebugLineVertices = 0;
};

/**
 * @brief Backend-neutral platform window handles supplied by the host.
 *
 * The renderer never owns these pointers. If a future backend uses either
 * pointer, the pointed-to platform objects must outlive renderer shutdown. The
 * bgfx backend requires `nativeWindow` to identify the swapchain target.
 *
 * @note Thread safety: Populate and consume this descriptor on the renderer
 * owner thread unless a future backend documents otherwise.
 */
struct PlatformWindowDesc
{
    /**
     * @brief Optional platform display or connection handle.
     *
     * The exact native type is platform-specific and intentionally kept out of
     * the public API. The pointer is referenced only during backend
     * initialization; it is not owned or freed by the renderer.
     */
    void* nativeDisplay = nullptr;

    /**
     * @brief Optional platform window handle.
     *
     * The exact native type is platform-specific and intentionally kept out of
     * the public API. If provided, its lifetime must exceed the renderer backend
     * lifetime.
     */
    void* nativeWindow = nullptr;
};

/**
 * @brief Descriptor used to initialize renderer-owned backend state.
 *
 * Dimensions are physical backbuffer pixels, not logical window units. Width
 * and height must both be greater than zero. World-space renderer conventions
 * are meters, Y-up, and right-handed coordinates; this descriptor only covers
 * backend startup and does not transfer ownership of platform data.
 */
struct RendererInitDesc
{
    /**
     * @brief Platform-native window data observed by the backend.
     *
     * The renderer does not copy, own, or destroy the pointed-to objects. The
     * bgfx backend requires a non-null native window handle.
     */
    PlatformWindowDesc window;

    /** @brief Initial backbuffer width in physical pixels; zero is invalid. */
    std::uint32_t backbufferWidth = 0;

    /** @brief Initial backbuffer height in physical pixels; zero is invalid. */
    std::uint32_t backbufferHeight = 0;

    /**
     * @brief Enables backend debug behavior when supported.
     *
     * Future backend work may use this to enable lightweight renderer or backend
     * diagnostics. The current bgfx lifecycle slice does not expose debug text or
     * statistics yet.
     */
    bool enableDebug = false;

    /**
     * @brief Directory containing compiled renderer shader binaries.
     *
     * The pointer is borrowed only for the duration of `initialize`; the
     * renderer copies the path it needs. The current bgfx forward slice expects
     * compiled static, instanced, skinned, terrain, sky, shadow, and debug
     * shader binaries, including the particle billboard shader pair, in this
     * directory. The path is expressed in the host process filesystem encoding
     * and may be absolute or relative to the process working directory.
     */
    const char* shaderBinaryDirectory = nullptr;
};

/**
 * @brief Descriptor for resizing renderer-owned backbuffer state.
 *
 * Dimensions are physical backbuffer pixels. Resize is a lifecycle operation
 * between frames; callers should not issue it while a frame is active. The
 * descriptor is consumed during the call and no references are retained.
 */
struct RendererResizeDesc
{
    /** @brief New backbuffer width in physical pixels; zero is invalid. */
    std::uint32_t backbufferWidth = 0;

    /** @brief New backbuffer height in physical pixels; zero is invalid. */
    std::uint32_t backbufferHeight = 0;
};

/**
 * @brief Per-frame descriptor supplied by the host before rendering work.
 *
 * Dimensions are physical backbuffer pixels for the frame being submitted. The
 * renderer treats this data as call-local input and does not retain references
 * after `beginFrame` returns. `deltaSeconds` is optional timing metadata for
 * future renderer-owned time-dependent systems; zero is valid and means no time
 * advance should be inferred.
 */
struct FrameDesc
{
    /** @brief Current backbuffer width in physical pixels; zero is invalid. */
    std::uint32_t backbufferWidth = 0;

    /** @brief Current backbuffer height in physical pixels; zero is invalid. */
    std::uint32_t backbufferHeight = 0;

    /**
     * @brief Optional elapsed frame time in seconds.
     *
     * Must be finite and non-negative. The foundation renderer does not use this
     * value for animation or simulation.
     */
    double deltaSeconds = 0.0;
};

/**
 * @brief Engine-facing renderer lifecycle and frame boundary interface.
 *
 * The renderer accepts externally owned platform, resource, and per-frame data
 * through descriptors. Resource creation copies caller-provided CPU data into
 * renderer-owned backend resources, while per-frame submission reads draw data
 * only for the duration of the call.
 *
 * @note Thread safety: All calls must be made from the renderer owner thread.
 * The interface does not currently provide internal synchronization.
 */
class IRenderer
{
public:
    virtual ~IRenderer() = default;

    /**
     * @brief Initializes renderer-owned backend state.
     *
     * The descriptor is consumed only for the duration of the call. Platform
     * objects referenced by `desc.window` remain owned by the caller and, if
     * used by a future backend, must outlive `shutdown`.
     *
     * @param desc Backend-neutral initialization settings.
     * @return `RendererResult::Success` on success; `InvalidDescriptor` for
     * invalid dimensions or platform data; `AlreadyInitialized` if called twice;
     * `BackendFailure` if the internal backend cannot initialize.
     *
     * @pre No frame is active and the renderer has not already been initialized.
     * @post On success, `isInitialized()` returns true.
     */
    virtual RendererResult initialize(const RendererInitDesc& desc) = 0;

    /**
     * @brief Releases renderer-owned backend state.
     *
     * Shutdown is idempotent. If a frame is active, the renderer attempts to end
     * internal frame state before releasing backend resources. Future
     * renderer-owned resources must either be destroyed by callers before this
     * call or cleaned up deterministically by the implementation.
     *
     * @note Thread safety: Must be called from the renderer owner thread.
     * @post `isInitialized()` returns false.
     */
    virtual void shutdown() noexcept = 0;

    /**
     * @brief Reports whether initialization completed and shutdown has not run.
     *
     * @return True after a successful `initialize` call and before `shutdown`;
     * false otherwise.
     */
    virtual bool isInitialized() const noexcept = 0;

    /**
     * @brief Creates a renderer-owned mesh resource from CPU-side data.
     *
     * The renderer copies all vertex and index data needed by the backend before
     * returning. The caller may release or reuse the source buffers after this
     * call completes. The returned handle is owned by this renderer and must be
     * destroyed with `destroyMesh` or left for `shutdown` cleanup.
     *
     * @param desc Fixed-format mesh descriptor. Pointers must be valid for the
     * duration of the call.
     * @return A valid mesh handle on success; an invalid handle if the renderer
     * is not initialized, validation fails, or backend resource creation fails.
     *
     * @pre The renderer has been initialized.
     * @note Thread safety: Must be called from the renderer owner thread.
     * @see destroyMesh
     */
    virtual MeshHandle createMesh(const MeshDesc& desc) = 0;

    /**
     * @brief Destroys a renderer-owned mesh resource.
     *
     * Destroying an invalid, stale, or already destroyed handle is ignored. The
     * handle value is not modified by this call. Callers should not submit draw
     * items referencing a mesh after it is destroyed.
     *
     * @param handle Mesh handle previously returned by `createMesh`.
     *
     * @pre No frame is active.
     * @note Thread safety: Must be called from the renderer owner thread.
     * @see createMesh
     */
    virtual void destroyMesh(MeshHandle handle) noexcept = 0;

    /**
     * @brief Creates renderer-owned skeleton metadata for skinned mesh validation.
     *
     * The renderer copies joint hierarchy and inverse bind-pose data during the
     * call. The returned handle is backend-neutral metadata; it owns no engine
     * animation state and does not evaluate clips. Skeletons must outlive
     * skinned meshes and submissions that reference them.
     *
     * @param desc Skeleton joint hierarchy descriptor.
     * @return A valid skeleton handle on success; an invalid handle if the
     * renderer is not initialized, validation fails, or the joint limit is exceeded.
     *
     * @pre The renderer has been initialized and no frame is active.
     * @note Thread safety: Must be called from the renderer owner thread.
     * @see destroySkeleton
     */
    virtual SkeletonHandle createSkeleton(const SkeletonDesc& desc) = 0;

    /**
     * @brief Destroys renderer-owned skeleton metadata.
     *
     * Destroying an invalid, stale, or already destroyed handle is ignored.
     * Callers should destroy dependent skinned meshes first and must not submit
     * animated draws whose skinned mesh references a destroyed skeleton.
     *
     * @param handle Skeleton handle previously returned by `createSkeleton`.
     *
     * @pre No frame is active.
     * @note Thread safety: Must be called from the renderer owner thread.
     * @see createSkeleton
     */
    virtual void destroySkeleton(SkeletonHandle handle) noexcept = 0;

    /**
     * @brief Creates a renderer-owned skinned mesh resource.
     *
     * The renderer copies fixed-format skinned vertex and index data during the
     * call. `desc.skeleton` must refer to live skeleton metadata created by the
     * same renderer, and all vertex joint indices must be valid for that
     * skeleton. CPU pose palettes are not stored on the resource; they are
     * provided frame-locally through `AnimatedDrawItem`.
     *
     * @param desc Skinned mesh descriptor and owning skeleton reference.
     * @return A valid skinned mesh handle on success; an invalid handle on
     * validation, lifecycle, or backend resource creation failure.
     *
     * @pre The renderer has been initialized and no frame is active.
     * @note Thread safety: Must be called from the renderer owner thread.
     * @see destroySkinnedMesh
     */
    virtual SkinnedMeshHandle createSkinnedMesh(const SkinnedMeshDesc& desc) = 0;

    /**
     * @brief Destroys a renderer-owned skinned mesh resource.
     *
     * Destroying an invalid, stale, or already destroyed handle is ignored. The
     * handle value is not modified. Callers must not submit animated draws
     * referencing a skinned mesh after it is destroyed.
     *
     * @param handle Skinned mesh handle previously returned by `createSkinnedMesh`.
     *
     * @pre No frame is active.
     * @note Thread safety: Must be called from the renderer owner thread.
     * @see createSkinnedMesh
     */
    virtual void destroySkinnedMesh(SkinnedMeshHandle handle) noexcept = 0;

    /**
     * @brief Creates a renderer-owned 2D texture from CPU-side RGBA8 data.
     *
     * Pixel data is copied before the call returns. The returned handle is owned
     * by this renderer and must be destroyed with `destroyTexture` or left for
     * deterministic `shutdown` cleanup.
     *
     * @param desc Texture descriptor. `data` must point to tightly packed RGBA8
     * texels for the duration of the call.
     * @return A valid texture handle on success; an invalid handle if the
     * renderer is not initialized, validation fails, or backend creation fails.
     *
     * @pre The renderer has been initialized.
     * @note Thread safety: Must be called from the renderer owner thread.
     * @see destroyTexture
     */
    virtual TextureHandle createTexture(const TextureDesc& desc) = 0;

    /**
     * @brief Destroys a renderer-owned texture resource.
     *
     * Destroying an invalid, stale, or already destroyed handle is ignored.
     * Callers should not submit terrain materials or splat maps referencing a
     * texture after it is destroyed.
     *
     * @param handle Texture handle previously returned by `createTexture`.
     *
     * @pre No frame is active.
     * @note Thread safety: Must be called from the renderer owner thread.
     * @see createTexture
     */
    virtual void destroyTexture(TextureHandle handle) noexcept = 0;

    /**
     * @brief Creates a renderer-owned material for the basic forward pass.
     *
     * Descriptor values are copied before the function returns. The returned
     * handle is owned by this renderer and must be destroyed with
     * `destroyMaterial` or left for deterministic `shutdown` cleanup.
     *
     * @param desc Basic material descriptor using linear color values.
     * @return A valid material handle on success; an invalid handle if the
     * renderer is not initialized, validation fails, or backend creation fails.
     *
     * @pre The renderer has been initialized.
     * @note Thread safety: Must be called from the renderer owner thread.
     * @see destroyMaterial
     */
    virtual MaterialHandle createMaterial(const MaterialDesc& desc) = 0;

    /**
     * @brief Destroys a renderer-owned material resource.
     *
     * Destroying an invalid, stale, or already destroyed handle is ignored. The
     * handle value is not modified by this call. Callers should not submit draw
     * items referencing a material after it is destroyed.
     *
     * @param handle Material handle previously returned by `createMaterial`.
     *
     * @pre No frame is active.
     * @note Thread safety: Must be called from the renderer owner thread.
     * @see createMaterial
     */
    virtual void destroyMaterial(MaterialHandle handle) noexcept = 0;

    /**
     * @brief Creates a renderer-owned terrain chunk record.
     *
     * The renderer copies chunk bounds and LOD descriptors during the call.
     * Terrain chunks reference existing mesh and material handles; they do not
     * own or destroy those resources. Referenced mesh/material resources must
     * remain live until the chunk is destroyed.
     *
     * @param desc Terrain chunk bounds and sorted LOD descriptors.
     * @return A valid terrain chunk handle on success; an invalid handle if the
     * renderer is not initialized or descriptor validation fails.
     *
     * @pre The renderer has been initialized and no frame is active.
     * @note Thread safety: Must be called from the renderer owner thread.
     * @see destroyTerrainChunk
     */
    virtual TerrainChunkHandle createTerrainChunk(const TerrainChunkDesc& desc) = 0;

    /**
     * @brief Replaces a live terrain chunk descriptor without changing its handle.
     *
     * The renderer copies the new bounds, splat map handle, and LOD descriptors
     * into the existing chunk record. The chunk handle and generation remain
     * stable so engine-owned residency maps do not need to remove and recreate
     * their renderer-handle association for ordinary descriptor changes.
     *
     * Referenced mesh, material, and texture handles are still borrowed
     * renderer resources; they must remain live while the updated chunk can be
     * submitted. Invalid descriptors fail without modifying the existing chunk.
     * Invalid, stale, destroyed, or foreign chunk handles return
     * `RendererResult::InvalidArgument`.
     *
     * @param handle Terrain chunk handle previously returned by
     * `createTerrainChunk`.
     * @param desc Replacement chunk bounds, LOD descriptors, and splat map.
     * @return `Success` on update; `NotInitialized` before initialization;
     * `FrameAlreadyInProgress` during an active frame; `InvalidDescriptor` for
     * invalid replacement data; `InvalidArgument` for invalid or stale handles.
     *
     * @pre The renderer has been initialized and no frame is active.
     * @note Thread safety: Must be called from the renderer owner thread.
     * @see createTerrainChunk
     * @see destroyTerrainChunk
     */
    virtual RendererResult updateTerrainChunk(TerrainChunkHandle handle, const TerrainChunkDesc& desc) = 0;

    /**
     * @brief Destroys a renderer-owned terrain chunk record.
     *
     * Destroying an invalid, stale, or already destroyed handle is ignored. This
     * call does not destroy mesh or material resources referenced by the chunk.
     *
     * @param handle Terrain chunk handle previously returned by
     * `createTerrainChunk`.
     *
     * @pre No frame is active.
     * @note Thread safety: Must be called from the renderer owner thread.
     * @see createTerrainChunk
     */
    virtual void destroyTerrainChunk(TerrainChunkHandle handle) noexcept = 0;

    /**
     * @brief Resizes renderer-owned backbuffer state.
     *
     * For bgfx-backed rendering this maps to swapchain reset while preserving
     * the existing platform window. The current frame descriptor should match
     * the latest resize dimensions after this call succeeds.
     *
     * @param desc Physical-pixel backbuffer dimensions for subsequent frames.
     * @return `Success` on success; `NotInitialized` before initialization;
     * `FrameAlreadyInProgress` if called during an active frame;
     * `InvalidDescriptor` for zero dimensions; `BackendFailure` if the backend
     * cannot resize.
     *
     * @note Thread safety: Must be called from the renderer owner thread.
     * @pre The renderer has been initialized and no frame is active.
     */
    virtual RendererResult resize(const RendererResizeDesc& desc) = 0;

    /**
     * @brief Begins renderer work for a new frame.
     *
     * Resets per-frame renderer state and forwards frame dimensions to the
     * backend. Must be paired with `endFrame()` before another frame can begin.
     *
     * @param desc Frame dimensions and optional timing metadata. Values are
     * copied during the call.
     * @return `Success` on success; `NotInitialized`, `FrameAlreadyInProgress`,
     * `InvalidDescriptor`, or `BackendFailure` when the frame cannot begin.
     *
     * @pre The renderer has been successfully initialized.
     * @post On success, a frame is active until `endFrame` or `shutdown`.
     */
    virtual RendererResult beginFrame(const FrameDesc& desc) = 0;

    /**
     * @brief Submits stable render data for the active frame.
     *
     * The renderer reads the supplied packet during this call only. Draw item
     * and instanced draw handles must refer to live resources created by this
     * renderer; default, destroyed, or stale handles are rejected without
     * dereferencing backend resources and are reflected in `RendererStats`
     * diagnostics where practical.
     * Terrain submission, when present, performs CPU frustum culling,
     * deterministic distance LOD, and internal instanced batching before
     * reaching the backend. When `directionalShadow.enabled` is true, submitted
     * terrain chunks plus opt-in draw/instanced bounds are tested against each
     * active cascade light frustum and selected caster batches are rendered
     * into cascade shadow maps before the terrain forward pass. This path does
     * not perform sorting, animation, shadow atlasing, or post-processing.
     *
     * @param packet Frame-local camera, light, and draw data.
     * @return `Success` on success; `NotInitialized`, `FrameNotInProgress`,
     * `InvalidArgument`, or `BackendFailure` if data cannot be submitted.
     *
     * @pre `beginFrame` completed successfully for the current frame.
     * @note Thread safety: Must be called from the renderer owner thread.
     */
    virtual RendererResult submit(const RenderPacket& packet) = 0;

    /**
     * @brief Ends the current renderer frame.
     *
     * Finalizes the active frame and presents backend work. The current
     * implementation clears the default view and advances bgfx's frame.
     *
     * @return `Success` on success; `NotInitialized`, `FrameNotInProgress`, or
     * `BackendFailure` if no frame can be ended.
     *
     * @pre `beginFrame` completed successfully and no `endFrame` has yet matched
     * that frame.
     * @post No frame is active after this call returns.
     */
    virtual RendererResult endFrame() = 0;

    /**
     * @brief Returns lightweight renderer diagnostic counters.
     *
     * The returned value is a snapshot and can be read before, during, or after a
     * frame. Per-frame draw and pass counters describe the latest submitted
     * frame; live resource counts, approximate memory estimates, allocation
     * failure counts, and invalid/stale handle counters persist until renderer
     * shutdown. Memory values are CPU-side estimates and are not GPU memory
     * queries. No backend handles or view IDs are exposed.
     *
     * @return Current renderer statistics snapshot.
     *
     * @note Thread safety: Must be called from the renderer owner thread.
     */
    virtual RendererStats getStats() const noexcept = 0;

    /**
     * @brief Returns CPU-side terrain counters from the latest terrain submission.
     *
     * Stats are reset by renderer shutdown and updated when `submit` processes a
     * packet containing terrain data.
     *
     * @return Current terrain statistics snapshot.
     *
     * @note Thread safety: Must be called from the renderer owner thread.
     */
    virtual TerrainStats getTerrainStats() const noexcept = 0;

    /**
     * @brief Copies the latest terrain debug records into caller-owned storage.
     *
     * Debug records are produced only when `TerrainDebugOptions::captureChunkInfo`
     * is true for a submitted terrain packet. Chunk records expose per-chunk
     * culling state, selected LOD, and camera distance. Passing
     * `outItems == nullptr` or `maxItems == 0` returns the available record
     * count without copying.
     *
     * @param outItems Destination array owned by the caller, or null to query count.
     * @param maxItems Maximum number of records that can be written to `outItems`.
     * @return Number of available debug records; this may be greater than
     * `maxItems` when the destination array is too small.
     *
     * @note Thread safety: Must be called from the renderer owner thread.
     */
    virtual std::uint32_t copyTerrainDebugInfo(
        TerrainChunkDebugInfo* outItems,
        std::uint32_t maxItems) const noexcept = 0;

    /**
     * @brief Copies the latest terrain instance batch debug records.
     *
     * Batch debug records are produced only when
     * `TerrainDebugOptions::captureBatchInfo` is true for a submitted terrain
     * packet. Each record describes one backend-facing terrain instance batch,
     * including its shared mesh/material handles, selected LOD index, instance
     * count, and enclosing world-space bounds. The renderer copies records into
     * caller-owned storage and retains no reference to `outItems`.
     *
     * @param outItems Destination array owned by the caller, or null to query count.
     * @param maxItems Maximum number of records that can be written to `outItems`.
     * @return Number of available batch debug records; this may be greater than
     * `maxItems` when the destination array is too small.
     *
     * @note Thread safety: Must be called from the renderer owner thread.
     */
    virtual std::uint32_t copyTerrainBatchDebugInfo(
        TerrainBatchDebugInfo* outItems,
        std::uint32_t maxItems) const noexcept = 0;

    /**
     * @brief Copies the latest terrain shadow-caster debug records.
     *
     * Shadow-caster records are produced when terrain submission debug capture
     * or directional shadow debug overlays request caster diagnostics. They
     * describe the same renderer-facing chunk handles, bounds, LOD choices,
     * cascade indices, and light-frustum decisions used by cascaded shadow
     * caster selection. Passing `outItems == nullptr` or `maxItems == 0`
     * returns the available record count without copying.
     *
     * @param outItems Destination array owned by the caller, or null to query count.
     * @param maxItems Maximum number of records that can be written to `outItems`.
     * @return Number of available shadow-caster records; this may be greater
     * than `maxItems` when the destination array is too small.
     *
     * @note Thread safety: Must be called from the renderer owner thread.
     * @note Debug records are backend-neutral snapshots and expose no shadow-map,
     * framebuffer, render-target, texture, or view handles.
     */
    virtual std::uint32_t copyTerrainShadowCasterDebugInfo(
        TerrainChunkDebugInfo* outItems,
        std::uint32_t maxItems) const noexcept = 0;
};

/**
 * @brief Creates the default renderer implementation.
 *
 * The returned object owns its internal core renderer and backend wrapper. The
 * caller owns the returned pointer and must keep it alive for the desired
 * renderer lifetime. Destroying the object after initialization performs the
 * same cleanup as `shutdown`.
 *
 * @return A non-null renderer instance using the internal bgfx backend.
 *
 * @note Thread safety: Create, use, and destroy the renderer on the same owner
 * thread unless a future implementation documents broader guarantees.
 */
std::unique_ptr<IRenderer> createRenderer();
} // namespace full_renderer
