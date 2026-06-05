#include "renderer/bgfx/BgfxRenderDevice.hpp"

#include "renderer/scene/ColorGrading.hpp"
#include "renderer/scene/Decal.hpp"
#include "renderer/scene/Environment.hpp"
#include "renderer/scene/Fade.hpp"
#include "renderer/scene/Frustum.hpp"
#include "renderer/scene/MaterialPolicy.hpp"
#include "renderer/scene/Math.hpp"
#include "renderer/scene/Particle.hpp"
#include "renderer/scene/PostPass.hpp"
#include "renderer/scene/Shadow.hpp"
#include "renderer/scene/Ssao.hpp"
#include "renderer/scene/Weather.hpp"
#include "renderer/resources/ResourceLifetime.hpp"

#if FULL_RENDERER_ENABLE_BGFX
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace full_renderer::bgfx_backend
{
namespace
{
#if FULL_RENDERER_ENABLE_BGFX
constexpr bgfx::ViewId kShadowViewIdBase = 0;
constexpr bgfx::ViewId kSkyViewId = static_cast<bgfx::ViewId>(kShadowViewIdBase + kMaxDirectionalShadowCascades);
constexpr bgfx::ViewId kForwardViewId = static_cast<bgfx::ViewId>(kSkyViewId + 1);
constexpr bgfx::ViewId kSsaoDepthViewId = static_cast<bgfx::ViewId>(kForwardViewId + 1);
constexpr bgfx::ViewId kSsaoViewId = static_cast<bgfx::ViewId>(kSsaoDepthViewId + 1);
constexpr bgfx::ViewId kSsaoBlurHorizontalViewId = static_cast<bgfx::ViewId>(kSsaoViewId + 1);
constexpr bgfx::ViewId kSsaoBlurVerticalViewId = static_cast<bgfx::ViewId>(kSsaoBlurHorizontalViewId + 1);
constexpr bgfx::ViewId kSsaoCompositeViewId = static_cast<bgfx::ViewId>(kSsaoBlurVerticalViewId + 1);
constexpr bgfx::ViewId kDecalDepthViewId = static_cast<bgfx::ViewId>(kSsaoCompositeViewId + 1);
constexpr bgfx::ViewId kDecalViewId = static_cast<bgfx::ViewId>(kDecalDepthViewId + 1);
constexpr bgfx::ViewId kParticleViewId = static_cast<bgfx::ViewId>(kDecalViewId + 1);
constexpr bgfx::ViewId kScenePresentViewId = static_cast<bgfx::ViewId>(kParticleViewId + 1);
constexpr bgfx::ViewId kSelectionMaskViewId = static_cast<bgfx::ViewId>(kScenePresentViewId + 1);
constexpr bgfx::ViewId kOutlineCompositeViewId = static_cast<bgfx::ViewId>(kSelectionMaskViewId + 1);
constexpr bgfx::ViewId kDebugLineViewId = static_cast<bgfx::ViewId>(kOutlineCompositeViewId + 1);
static_assert(kShadowViewIdBase < kSkyViewId, "Shadow views must precede the sky pass.");
static_assert(kSkyViewId < kForwardViewId, "Sky must precede opaque forward rendering.");
static_assert(kForwardViewId < kSsaoDepthViewId, "Forward rendering must precede post-style passes.");
static_assert(kSsaoDepthViewId < kSsaoViewId, "SSAO view IDs must stay ordered.");
static_assert(kSsaoViewId < kSsaoBlurHorizontalViewId, "SSAO blur views must stay ordered.");
static_assert(kSsaoBlurHorizontalViewId < kSsaoBlurVerticalViewId, "SSAO blur views must stay ordered.");
static_assert(kSsaoBlurVerticalViewId < kSsaoCompositeViewId, "SSAO composite must follow blur views.");
static_assert(kSsaoCompositeViewId < kDecalDepthViewId, "Decals must follow SSAO composite.");
static_assert(kDecalDepthViewId < kDecalViewId, "Projected decals must follow decal depth capture.");
static_assert(kDecalViewId < kParticleViewId, "Particles must follow decals.");
static_assert(kParticleViewId < kScenePresentViewId, "Scene present must follow particles.");
static_assert(kScenePresentViewId < kSelectionMaskViewId, "Selection should remain after scene present.");
static_assert(kSelectionMaskViewId < kOutlineCompositeViewId, "Outline composite must follow selection mask.");
static_assert(kOutlineCompositeViewId < kDebugLineViewId, "Debug overlays must remain last before ImGui.");
static_assert(kDebugLineViewId < 255, "bgfx view IDs must fit in the supported view range.");
constexpr std::uint32_t kClearColorRgba = 0x1E3A5FFF;
constexpr std::uint32_t kResetFlags = BGFX_RESET_VSYNC;
#endif

bool hasValidDimensions(const std::uint32_t width, const std::uint32_t height) noexcept
{
    return width > 0 && height > 0;
}

bool hasValidFrameTime(const double deltaSeconds) noexcept
{
    return std::isfinite(deltaSeconds) && deltaSeconds >= 0.0;
}

scene::DecalRenderPlan buildCameraCulledDecalPlan(const RenderPacket& packet) noexcept
{
    if (packet.decals == nullptr || !packet.decals->enabled)
    {
        return scene::buildDecalRenderPlan(packet.decals);
    }

    float viewProjection[16] = {};
    scene::multiplyColumnMajor4x4(packet.view.projection, packet.view.view, viewProjection);
    const scene::Frustum frustum = scene::extractFrustumFromViewProjection(viewProjection);
    return scene::buildDecalRenderPlan(packet.decals, &frustum);
}

scene::ParticleRenderPlan buildPostParticlePlan(const RenderPacket& packet) noexcept
{
    if (packet.particles == nullptr || !packet.particles->enabled)
    {
        return scene::buildParticleRenderPlan(packet.particles);
    }

    float viewProjection[16] = {};
    scene::multiplyColumnMajor4x4(packet.view.projection, packet.view.view, viewProjection);
    const scene::Frustum frustum = scene::extractFrustumFromViewProjection(viewProjection);

    scene::ParticleRenderPlanInput input;
    input.cameraFrustum = &frustum;
    return scene::buildParticleRenderPlan(packet.particles, input);
}

bool colorGradingRequestsSceneTarget(const RenderPacket& packet) noexcept
{
    return packet.colorGrading.enabled && scene::isValidColorGradingDesc(packet.colorGrading);
}

void recordFadeStats(const scene::FadeRenderState& state, RendererStats& stats) noexcept
{
    if (!state.enabled)
    {
        return;
    }

    ++stats.structureFadeSubmittedCount;
    if (state.fullyVisible)
    {
        ++stats.structureFadeFullyVisibleCount;
    }
    else if (state.fullyHidden)
    {
        ++stats.structureFadeFullyHiddenCount;
    }
    else
    {
        ++stats.structureFadePartiallyFadedCount;
    }

    if (state.active)
    {
        ++stats.structureFadeActiveCount;
        if (state.alphaMode)
        {
            ++stats.structureFadeAlphaDraws;
        }
        else if (state.ditherMode)
        {
            ++stats.structureFadeDitherDraws;
        }
    }
}

void recordMaterialDrawStats(
    const MaterialDesc& material,
    const scene::FadeRenderState& fadeState,
    const bool transparentSorted,
    RendererStats& stats) noexcept
{
    const scene::MaterialRenderBucket bucket =
        scene::renderBucketForMaterialAndFade(material, fadeState);
    switch (bucket)
    {
    case scene::MaterialRenderBucket::Opaque:
        ++stats.materialOpaqueDraws;
        break;
    case scene::MaterialRenderBucket::AlphaTest:
        ++stats.materialAlphaTestDraws;
        break;
    case scene::MaterialRenderBucket::AlphaBlend:
        ++stats.materialAlphaBlendDraws;
        if (transparentSorted)
        {
            ++stats.transparentSortedDraws;
        }
        else
        {
            ++stats.transparentUnsortedDraws;
        }
        break;
    case scene::MaterialRenderBucket::Unsupported:
        ++stats.invalidMaterialCount;
        break;
    case scene::MaterialRenderBucket::Particle:
    case scene::MaterialRenderBucket::Decal:
    case scene::MaterialRenderBucket::SelectionMask:
    case scene::MaterialRenderBucket::ShadowDepth:
    case scene::MaterialRenderBucket::Debug:
        break;
    }

    if (fadeState.active && fadeState.ditherMode)
    {
        ++stats.materialDitherFadeDraws;
    }
}

void setFadeAndMaterialParams(
    const scene::FadeRenderState& fadeState,
    const MaterialDesc& material,
    float outParams[4]) noexcept
{
    outParams[0] = fadeState.params[0];
    outParams[1] = fadeState.params[1];
    outParams[2] = scene::materialAlphaModeToShaderValue(material.alphaMode);
    outParams[3] = material.alphaMode == MaterialAlphaMode::AlphaTest ?
        scene::clampMaterialAlphaCutoff(material.alphaCutoff) :
        0.0f;
}

void centerFromModelTranslation(const float model[16], float outCenter[3]) noexcept
{
    outCenter[0] = model[12];
    outCenter[1] = model[13];
    outCenter[2] = model[14];
}

void centerFromAabb(const Aabb& bounds, float outCenter[3]) noexcept
{
    const scene::Vec3 center = scene::aabbCenter(bounds);
    outCenter[0] = center.x;
    outCenter[1] = center.y;
    outCenter[2] = center.z;
}

void markShaderVariant(
    const scene::ShaderVariantKey& key,
    std::uint64_t& shaderVariantMask,
    RendererStats& stats) noexcept
{
    const std::uint32_t index = scene::shaderVariantStableIndex(key);
    if (index >= 64U)
    {
        ++stats.unsupportedShaderVariantRequestCount;
        return;
    }

    const std::uint64_t bit = std::uint64_t{1} << index;
    if ((shaderVariantMask & bit) == 0U)
    {
        shaderVariantMask |= bit;
        ++stats.shaderVariantCountInUse;
    }
}

std::string joinPath(const std::string& directory, const char* filename)
{
    if (directory.empty())
    {
        return filename;
    }

    const char last = directory[directory.size() - 1];
    if (last == '/' || last == '\\')
    {
        return directory + filename;
    }

    return directory + "/" + filename;
}

#if FULL_RENDERER_ENABLE_BGFX
std::uint64_t forwardMeshStateForMaterialAndFade(
    const MaterialDesc& material,
    const scene::FadeRenderState& fadeState) noexcept
{
    std::uint64_t state =
        BGFX_STATE_WRITE_RGB |
        BGFX_STATE_WRITE_A |
        BGFX_STATE_DEPTH_TEST_LESS |
        BGFX_STATE_MSAA;
    if (material.alphaMode == MaterialAlphaMode::AlphaBlend ||
        (fadeState.active && fadeState.alphaMode))
    {
        return state | BGFX_STATE_BLEND_ALPHA;
    }

    return state | BGFX_STATE_WRITE_Z;
}

void configureForwardView(const std::uint32_t width, const std::uint32_t height)
{
    bgfx::setViewName(kSkyViewId, "Sky");
    bgfx::setViewFrameBuffer(kSkyViewId, BGFX_INVALID_HANDLE);
    bgfx::setViewClear(kSkyViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, kClearColorRgba, 1.0f, 0);
    bgfx::setViewRect(kSkyViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));

    bgfx::setViewName(kForwardViewId, "Forward");
    bgfx::setViewFrameBuffer(kForwardViewId, BGFX_INVALID_HANDLE);
    bgfx::setViewClear(kForwardViewId, BGFX_CLEAR_NONE, kClearColorRgba, 1.0f, 0);
    bgfx::setViewRect(kForwardViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));

    bgfx::setViewName(kSsaoDepthViewId, "SSAO Depth Capture");
    bgfx::setViewMode(kSsaoDepthViewId, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(kSsaoDepthViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x00000000, 1.0f, 0);
    bgfx::setViewRect(kSsaoDepthViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));

    bgfx::setViewName(kSsaoViewId, "SSAO");
    bgfx::setViewMode(kSsaoViewId, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(kSsaoViewId, BGFX_CLEAR_COLOR, 0xffffffff, 1.0f, 0);
    bgfx::setViewRect(kSsaoViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));

    bgfx::setViewName(kSsaoBlurHorizontalViewId, "SSAO Blur H");
    bgfx::setViewMode(kSsaoBlurHorizontalViewId, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(kSsaoBlurHorizontalViewId, BGFX_CLEAR_COLOR, 0xffffffff, 1.0f, 0);
    bgfx::setViewRect(kSsaoBlurHorizontalViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));

    bgfx::setViewName(kSsaoBlurVerticalViewId, "SSAO Blur V");
    bgfx::setViewMode(kSsaoBlurVerticalViewId, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(kSsaoBlurVerticalViewId, BGFX_CLEAR_COLOR, 0xffffffff, 1.0f, 0);
    bgfx::setViewRect(kSsaoBlurVerticalViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));

    bgfx::setViewName(kSsaoCompositeViewId, "SSAO Composite");
    bgfx::setViewMode(kSsaoCompositeViewId, bgfx::ViewMode::Sequential);
    bgfx::setViewFrameBuffer(kSsaoCompositeViewId, BGFX_INVALID_HANDLE);
    bgfx::setViewClear(kSsaoCompositeViewId, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kSsaoCompositeViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));

    bgfx::setViewName(kDecalDepthViewId, "Decal Depth Capture");
    bgfx::setViewMode(kDecalDepthViewId, bgfx::ViewMode::Sequential);
    bgfx::setViewFrameBuffer(kDecalDepthViewId, BGFX_INVALID_HANDLE);
    bgfx::setViewClear(kDecalDepthViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x00000000, 1.0f, 0);
    bgfx::setViewRect(kDecalDepthViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));

    bgfx::setViewName(kDecalViewId, "Projected Decals");
    bgfx::setViewMode(kDecalViewId, bgfx::ViewMode::Sequential);
    bgfx::setViewFrameBuffer(kDecalViewId, BGFX_INVALID_HANDLE);
    bgfx::setViewClear(kDecalViewId, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kDecalViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));

    bgfx::setViewName(kParticleViewId, "Particles");
    bgfx::setViewMode(kParticleViewId, bgfx::ViewMode::Sequential);
    bgfx::setViewFrameBuffer(kParticleViewId, BGFX_INVALID_HANDLE);
    bgfx::setViewClear(kParticleViewId, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kParticleViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));

    bgfx::setViewName(kScenePresentViewId, "Scene Present");
    bgfx::setViewMode(kScenePresentViewId, bgfx::ViewMode::Sequential);
    bgfx::setViewFrameBuffer(kScenePresentViewId, BGFX_INVALID_HANDLE);
    bgfx::setViewClear(kScenePresentViewId, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kScenePresentViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));

    bgfx::setViewName(kSelectionMaskViewId, "Selection Mask");
    bgfx::setViewMode(kSelectionMaskViewId, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(kSelectionMaskViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x00000000, 1.0f, 0);
    bgfx::setViewRect(kSelectionMaskViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));

    bgfx::setViewName(kOutlineCompositeViewId, "Selection Outline");
    bgfx::setViewMode(kOutlineCompositeViewId, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(kOutlineCompositeViewId, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kOutlineCompositeViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));

    bgfx::setViewName(kDebugLineViewId, "Terrain Debug");
    bgfx::setViewClear(kDebugLineViewId, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    bgfx::setViewRect(kDebugLineViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));
}

bgfx::ViewId shadowViewId(const std::uint32_t cascadeIndex) noexcept
{
    return static_cast<bgfx::ViewId>(kShadowViewIdBase + cascadeIndex);
}

void configureShadowView(
    const std::uint32_t cascadeIndex,
    const std::uint32_t resolution,
    const bgfx::FrameBufferHandle frameBuffer)
{
    const bgfx::ViewId viewId = shadowViewId(cascadeIndex);
    bgfx::setViewName(viewId, "Terrain Shadow Cascade");
    bgfx::setViewFrameBuffer(viewId, frameBuffer);
    bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0xffffffff, 1.0f, 0);
    bgfx::setViewRect(viewId, 0, 0, static_cast<std::uint16_t>(resolution), static_cast<std::uint16_t>(resolution));
}

std::vector<char> readBinaryFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        return {};
    }

    const std::streamsize size = file.tellg();
    if (size <= 0)
    {
        return {};
    }

    std::vector<char> bytes(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(bytes.data(), size))
    {
        return {};
    }

    return bytes;
}

bgfx::ShaderHandle loadShader(const std::string& path)
{
    const std::vector<char> bytes = readBinaryFile(path);
    if (bytes.empty())
    {
        return BGFX_INVALID_HANDLE;
    }

    const bgfx::Memory* memory = bgfx::copy(bytes.data(), static_cast<std::uint32_t>(bytes.size()));
    return bgfx::createShader(memory);
}

bgfx::TextureHandle createSolidTexture(const std::uint8_t r, const std::uint8_t g, const std::uint8_t b, const std::uint8_t a)
{
    const std::uint8_t texel[4] = {r, g, b, a};
    return bgfx::createTexture2D(
        1,
        1,
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        0,
        bgfx::copy(texel, sizeof(texel)));
}

bool isValidUniform(const bgfx::UniformHandle handle) noexcept
{
    return bgfx::isValid(handle);
}

struct FullscreenVertex
{
    float position[3];
    float normal[3];
    float uv0[2];
    float color[4];
    float tangent[4];
};

struct ParticleVertex
{
    float position[3];
    float texcoord[2];
    float viewDepth = 0.0f;
    float color[4];
};

bool allocateFullscreenQuad(bgfx::TransientVertexBuffer& vertexBuffer, const bgfx::VertexLayout& layout) noexcept
{
    constexpr std::uint32_t kVertexCount = 6;
    if (bgfx::getAvailTransientVertexBuffer(kVertexCount, layout) < kVertexCount)
    {
        return false;
    }

    bgfx::allocTransientVertexBuffer(&vertexBuffer, kVertexCount, layout);
    auto* vertices = reinterpret_cast<FullscreenVertex*>(vertexBuffer.data);
    vertices[0] = {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}};
    vertices[1] = {{1.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}};
    vertices[2] = {{1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}};
    vertices[3] = {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}};
    vertices[4] = {{1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}};
    vertices[5] = {{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}};
    return true;
}

#endif

bool invertMatrix4x4(const float m[16], float invOut[16]) noexcept
{
    float inv[16] = {};
    inv[0] = m[5] * m[10] * m[15] -
        m[5] * m[11] * m[14] -
        m[9] * m[6] * m[15] +
        m[9] * m[7] * m[14] +
        m[13] * m[6] * m[11] -
        m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] +
        m[4] * m[11] * m[14] +
        m[8] * m[6] * m[15] -
        m[8] * m[7] * m[14] -
        m[12] * m[6] * m[11] +
        m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] -
        m[4] * m[11] * m[13] -
        m[8] * m[5] * m[15] +
        m[8] * m[7] * m[13] +
        m[12] * m[5] * m[11] -
        m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] +
        m[4] * m[10] * m[13] +
        m[8] * m[5] * m[14] -
        m[8] * m[6] * m[13] -
        m[12] * m[5] * m[10] +
        m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] +
        m[1] * m[11] * m[14] +
        m[9] * m[2] * m[15] -
        m[9] * m[3] * m[14] -
        m[13] * m[2] * m[11] +
        m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] -
        m[0] * m[11] * m[14] -
        m[8] * m[2] * m[15] +
        m[8] * m[3] * m[14] +
        m[12] * m[2] * m[11] -
        m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] +
        m[0] * m[11] * m[13] +
        m[8] * m[1] * m[15] -
        m[8] * m[3] * m[13] -
        m[12] * m[1] * m[11] +
        m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] -
        m[0] * m[10] * m[13] -
        m[8] * m[1] * m[14] +
        m[8] * m[2] * m[13] +
        m[12] * m[1] * m[10] -
        m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] -
        m[1] * m[7] * m[14] -
        m[5] * m[2] * m[15] +
        m[5] * m[3] * m[14] +
        m[13] * m[2] * m[7] -
        m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] +
        m[0] * m[7] * m[14] +
        m[4] * m[2] * m[15] -
        m[4] * m[3] * m[14] -
        m[12] * m[2] * m[7] +
        m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] -
        m[0] * m[7] * m[13] -
        m[4] * m[1] * m[15] +
        m[4] * m[3] * m[13] +
        m[12] * m[1] * m[7] -
        m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] +
        m[0] * m[6] * m[13] +
        m[4] * m[1] * m[14] -
        m[4] * m[2] * m[13] -
        m[12] * m[1] * m[6] +
        m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] +
        m[1] * m[7] * m[10] +
        m[5] * m[2] * m[11] -
        m[5] * m[3] * m[10] -
        m[9] * m[2] * m[7] +
        m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] -
        m[0] * m[7] * m[10] -
        m[4] * m[2] * m[11] +
        m[4] * m[3] * m[10] +
        m[8] * m[2] * m[7] -
        m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] +
        m[0] * m[7] * m[9] +
        m[4] * m[1] * m[11] -
        m[4] * m[3] * m[9] -
        m[8] * m[1] * m[7] +
        m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] -
        m[0] * m[6] * m[9] -
        m[4] * m[1] * m[10] +
        m[4] * m[2] * m[9] +
        m[8] * m[1] * m[6] -
        m[8] * m[2] * m[5];

    const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::abs(det) <= 0.000001f || !std::isfinite(det))
    {
        return false;
    }

    const float invDet = 1.0f / det;
    for (std::uint32_t index = 0; index < 16; ++index)
    {
        invOut[index] = inv[index] * invDet;
    }
    return true;
}

void extractProjectionDepthRange(const float projection[16], float& nearZ, float& farZ) noexcept
{
    nearZ = 0.0f;
    farZ = 0.0f;
    const float a = projection[10];
    const float b = projection[14];
    if (std::abs(a) <= 0.000001f || std::abs(a + 1.0f) <= 0.000001f)
    {
        return;
    }

    const float extractedNear = b / a;
    const float extractedFar = b / (a + 1.0f);
    if (std::isfinite(extractedNear) &&
        std::isfinite(extractedFar) &&
        extractedNear > 0.0f &&
        extractedFar > extractedNear)
    {
        nearZ = extractedNear;
        farZ = extractedFar;
    }
}

void normalize3(const float in[3], float out[3]) noexcept
{
    const float lengthSquared = in[0] * in[0] + in[1] * in[1] + in[2] * in[2];
    if (lengthSquared <= 0.000001f)
    {
        out[0] = 0.0f;
        out[1] = 1.0f;
        out[2] = 0.0f;
        return;
    }

    const float invLength = 1.0f / std::sqrt(lengthSquared);
    out[0] = in[0] * invLength;
    out[1] = in[1] * invLength;
    out[2] = in[2] * invLength;
}
} // namespace

BgfxRenderDevice::~BgfxRenderDevice()
{
    shutdown();
}

BgfxRenderDevice::MeshResource* BgfxRenderDevice::findMesh(const MeshHandle handle) noexcept
{
    if (!isValid(handle) || handle.id > meshes_.size())
    {
        return nullptr;
    }

    MeshResource& mesh = meshes_[handle.id - 1U];
    return mesh.active ? &mesh : nullptr;
}

const BgfxRenderDevice::MeshResource* BgfxRenderDevice::findMesh(const MeshHandle handle) const noexcept
{
    if (!isValid(handle) || handle.id > meshes_.size())
    {
        return nullptr;
    }

    const MeshResource& mesh = meshes_[handle.id - 1U];
    return mesh.active ? &mesh : nullptr;
}

BgfxRenderDevice::SkinnedMeshResource* BgfxRenderDevice::findSkinnedMesh(const SkinnedMeshHandle handle) noexcept
{
    if (!isValid(handle) || handle.id > skinnedMeshes_.size())
    {
        return nullptr;
    }

    SkinnedMeshResource& mesh = skinnedMeshes_[handle.id - 1U];
    return mesh.active ? &mesh : nullptr;
}

const BgfxRenderDevice::SkinnedMeshResource* BgfxRenderDevice::findSkinnedMesh(const SkinnedMeshHandle handle) const noexcept
{
    if (!isValid(handle) || handle.id > skinnedMeshes_.size())
    {
        return nullptr;
    }

    const SkinnedMeshResource& mesh = skinnedMeshes_[handle.id - 1U];
    return mesh.active ? &mesh : nullptr;
}

BgfxRenderDevice::TextureResource* BgfxRenderDevice::findTexture(const TextureHandle handle) noexcept
{
    if (!isValid(handle) || handle.id > textures_.size())
    {
        return nullptr;
    }

    TextureResource& texture = textures_[handle.id - 1U];
    return texture.active ? &texture : nullptr;
}

const BgfxRenderDevice::TextureResource* BgfxRenderDevice::findTexture(const TextureHandle handle) const noexcept
{
    if (!isValid(handle) || handle.id > textures_.size())
    {
        return nullptr;
    }

    const TextureResource& texture = textures_[handle.id - 1U];
    return texture.active ? &texture : nullptr;
}

BgfxRenderDevice::MaterialResource* BgfxRenderDevice::findMaterial(const MaterialHandle handle) noexcept
{
    if (!isValid(handle) || handle.id > materials_.size())
    {
        return nullptr;
    }

    MaterialResource& material = materials_[handle.id - 1U];
    return material.active ? &material : nullptr;
}

const BgfxRenderDevice::MaterialResource* BgfxRenderDevice::findMaterial(const MaterialHandle handle) const noexcept
{
    if (!isValid(handle) || handle.id > materials_.size())
    {
        return nullptr;
    }

    const MaterialResource& material = materials_[handle.id - 1U];
    return material.active ? &material : nullptr;
}

BgfxRenderDevice::ResourceHandleState BgfxRenderDevice::classifyMeshHandle(const MeshHandle handle) const noexcept
{
    if (!isValid(handle))
    {
        return ResourceHandleState::Invalid;
    }
    if (handle.id > meshes_.size())
    {
        return ResourceHandleState::Stale;
    }

    return meshes_[handle.id - 1U].active ? ResourceHandleState::Live : ResourceHandleState::Stale;
}

BgfxRenderDevice::ResourceHandleState BgfxRenderDevice::classifySkinnedMeshHandle(
    const SkinnedMeshHandle handle) const noexcept
{
    if (!isValid(handle))
    {
        return ResourceHandleState::Invalid;
    }
    if (handle.id > skinnedMeshes_.size())
    {
        return ResourceHandleState::Stale;
    }

    return skinnedMeshes_[handle.id - 1U].active ? ResourceHandleState::Live : ResourceHandleState::Stale;
}

BgfxRenderDevice::ResourceHandleState BgfxRenderDevice::classifyTextureHandle(
    const TextureHandle handle) const noexcept
{
    if (!isValid(handle))
    {
        return ResourceHandleState::Invalid;
    }
    if (handle.id > textures_.size())
    {
        return ResourceHandleState::Stale;
    }

    return textures_[handle.id - 1U].active ? ResourceHandleState::Live : ResourceHandleState::Stale;
}

BgfxRenderDevice::ResourceHandleState BgfxRenderDevice::classifyMaterialHandle(
    const MaterialHandle handle) const noexcept
{
    if (!isValid(handle))
    {
        return ResourceHandleState::Invalid;
    }
    if (handle.id > materials_.size())
    {
        return ResourceHandleState::Stale;
    }

    return materials_[handle.id - 1U].active ? ResourceHandleState::Live : ResourceHandleState::Stale;
}

void BgfxRenderDevice::recordHandleUse(
    const ResourceHandleState state,
    const bool submission) noexcept
{
    if (state == ResourceHandleState::Live)
    {
        return;
    }

    if (state == ResourceHandleState::Invalid)
    {
        ++invalidHandleUseCount_;
    }
    else
    {
        ++staleHandleUseCount_;
    }

    if (submission)
    {
        ++destroyedHandleSubmissionCount_;
    }

    stats_.invalidHandleUseCount = invalidHandleUseCount_;
    stats_.staleHandleUseCount = staleHandleUseCount_;
    stats_.destroyedHandleSubmissionCount = destroyedHandleSubmissionCount_;
}

bool BgfxRenderDevice::resolveMeshMaterial(
    const MeshHandle meshHandle,
    const MaterialHandle materialHandle,
    const MeshResource*& mesh,
    const MaterialResource*& material,
    const bool submission) noexcept
{
    mesh = nullptr;
    material = nullptr;
    const ResourceHandleState meshState = classifyMeshHandle(meshHandle);
    const ResourceHandleState materialState = classifyMaterialHandle(materialHandle);
    if (meshState != ResourceHandleState::Live || materialState != ResourceHandleState::Live)
    {
        recordHandleUse(meshState, submission);
        recordHandleUse(materialState, submission);
        return false;
    }

    mesh = findMesh(meshHandle);
    material = findMaterial(materialHandle);
    return mesh != nullptr && material != nullptr;
}

bool BgfxRenderDevice::resolveSkinnedMeshMaterial(
    const SkinnedMeshHandle meshHandle,
    const MaterialHandle materialHandle,
    const SkinnedMeshResource*& mesh,
    const MaterialResource*& material,
    const bool submission) noexcept
{
    mesh = nullptr;
    material = nullptr;
    const ResourceHandleState meshState = classifySkinnedMeshHandle(meshHandle);
    const ResourceHandleState materialState = classifyMaterialHandle(materialHandle);
    if (meshState != ResourceHandleState::Live || materialState != ResourceHandleState::Live)
    {
        recordHandleUse(meshState, submission);
        recordHandleUse(materialState, submission);
        return false;
    }

    mesh = findSkinnedMesh(meshHandle);
    material = findMaterial(materialHandle);
    return mesh != nullptr && material != nullptr;
}

const BgfxRenderDevice::TextureResource* BgfxRenderDevice::resolveTexture(
    const TextureHandle handle,
    const bool submission) noexcept
{
    const ResourceHandleState state = classifyTextureHandle(handle);
    if (state != ResourceHandleState::Live)
    {
        recordHandleUse(state, submission);
        return nullptr;
    }

    return findTexture(handle);
}

void BgfxRenderDevice::destroyAllResources() noexcept
{
#if FULL_RENDERER_ENABLE_BGFX
    for (MeshResource& mesh : meshes_)
    {
        if (bgfx::isValid(mesh.indexBuffer))
        {
            bgfx::destroy(mesh.indexBuffer);
            mesh.indexBuffer = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(mesh.vertexBuffer))
        {
            bgfx::destroy(mesh.vertexBuffer);
            mesh.vertexBuffer = BGFX_INVALID_HANDLE;
        }

        mesh.active = false;
    }
#else
    for (MeshResource& mesh : meshes_)
    {
        mesh.active = false;
    }
#endif

#if FULL_RENDERER_ENABLE_BGFX
    for (SkinnedMeshResource& mesh : skinnedMeshes_)
    {
        if (bgfx::isValid(mesh.indexBuffer))
        {
            bgfx::destroy(mesh.indexBuffer);
            mesh.indexBuffer = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(mesh.vertexBuffer))
        {
            bgfx::destroy(mesh.vertexBuffer);
            mesh.vertexBuffer = BGFX_INVALID_HANDLE;
        }

        mesh.active = false;
    }
#else
    for (SkinnedMeshResource& mesh : skinnedMeshes_)
    {
        mesh.active = false;
    }
#endif

#if FULL_RENDERER_ENABLE_BGFX
    for (TextureResource& texture : textures_)
    {
        if (bgfx::isValid(texture.texture))
        {
            bgfx::destroy(texture.texture);
            texture.texture = BGFX_INVALID_HANDLE;
        }
        texture.active = false;
    }
#else
    for (TextureResource& texture : textures_)
    {
        texture.active = false;
    }
#endif

    for (MaterialResource& material : materials_)
    {
        material.active = false;
    }

    meshes_.clear();
    skinnedMeshes_.clear();
    textures_.clear();
    materials_.clear();
    updateLiveResourceStats();
}

void BgfxRenderDevice::updateLiveResourceStats() noexcept
{
    std::uint32_t liveMeshes = 0;
    std::uint64_t meshBufferBytes = 0;
    for (const MeshResource& mesh : meshes_)
    {
        if (mesh.active)
        {
            ++liveMeshes;
            meshBufferBytes = resources::addSaturating(meshBufferBytes, mesh.estimatedBytes);
        }
    }

    std::uint32_t liveMaterials = 0;
    std::uint32_t liveBasicMaterials = 0;
    std::uint32_t liveTerrainSplatMaterials = 0;
    for (const MaterialResource& material : materials_)
    {
        if (material.active)
        {
            ++liveMaterials;
            if (material.desc.kind == MaterialKind::TerrainSplat)
            {
                ++liveTerrainSplatMaterials;
            }
            else
            {
                ++liveBasicMaterials;
            }
        }
    }

    std::uint32_t liveSkinnedMeshes = 0;
    std::uint64_t skinnedMeshBufferBytes = 0;
    for (const SkinnedMeshResource& mesh : skinnedMeshes_)
    {
        if (mesh.active)
        {
            ++liveSkinnedMeshes;
            skinnedMeshBufferBytes = resources::addSaturating(skinnedMeshBufferBytes, mesh.estimatedBytes);
        }
    }

    std::uint32_t liveTextures = 0;
    std::uint64_t textureBytes = 0;
    for (const TextureResource& texture : textures_)
    {
        if (texture.active)
        {
            ++liveTextures;
            textureBytes = resources::addSaturating(textureBytes, texture.estimatedBytes);
        }
    }

    stats_.liveMeshes = liveMeshes;
    stats_.liveSkinnedMeshes = liveSkinnedMeshes;
    stats_.liveTextures = liveTextures;
    stats_.liveMaterials = liveMaterials;
    stats_.materialBasicCount = liveBasicMaterials;
    stats_.materialTerrainSplatCount = liveTerrainSplatMaterials;
    stats_.meshBufferBytes = meshBufferBytes;
    stats_.skinnedMeshBufferBytes = skinnedMeshBufferBytes;
    stats_.textureBytes = textureBytes;
    stats_.materialBytes =
        static_cast<std::uint64_t>(liveMaterials) * static_cast<std::uint64_t>(sizeof(MaterialDesc));
    stats_.invalidHandleUseCount = invalidHandleUseCount_;
    stats_.staleHandleUseCount = staleHandleUseCount_;
    stats_.destroyedHandleSubmissionCount = destroyedHandleSubmissionCount_;

#if FULL_RENDERER_ENABLE_BGFX
    stats_.meshAllocationFailureCount = meshAllocationFailureCount_;
    stats_.skinnedMeshAllocationFailureCount = skinnedMeshAllocationFailureCount_;
    stats_.textureAllocationFailureCount = textureAllocationFailureCount_;
    stats_.selectionMaskResourceRecreateCount = selectionMaskResourceRecreateCount_;
    stats_.selectionMaskAllocationFailureCount = selectionMaskAllocationFailureCount_;
    stats_.ssaoResourceRecreateCount = ssaoResourceRecreateCount_;
    stats_.ssaoAllocationFailureCount = ssaoAllocationFailureCount_;

    const bool shadowResourcesValid =
        shadowCascadeResourceCount_ > 0 && hasValidShadowResources(shadowCascadeResourceCount_);
    stats_.shadowTargetBytes = shadowResourcesValid ?
        static_cast<std::uint64_t>(shadowCascadeResourceCount_) *
            (resources::estimateTexture2DBytes(
                 shadowMapResolution_,
                 shadowMapResolution_,
                 resources::ResourceMemoryFormat::R32F) +
                resources::estimateTexture2DBytes(
                    shadowMapResolution_,
                    shadowMapResolution_,
                    resources::ResourceMemoryFormat::D24)) :
        0;

    stats_.sceneTargetBytes = hasValidSceneTargetResource() ?
        resources::estimateTexture2DBytes(
            sceneTargetResource_.width,
            sceneTargetResource_.height,
            resources::ResourceMemoryFormat::Rgba8) +
            resources::estimateTexture2DBytes(
                sceneTargetResource_.width,
                sceneTargetResource_.height,
                resources::ResourceMemoryFormat::D24) +
            resources::estimateTexture2DBytes(
                sceneTargetResource_.width,
                sceneTargetResource_.height,
                resources::ResourceMemoryFormat::R32F) +
            resources::estimateTexture2DBytes(
                sceneTargetResource_.width,
                sceneTargetResource_.height,
                resources::ResourceMemoryFormat::D24) :
        0;

    const bool ssaoResourcesValid =
        ssaoResource_.width > 0 &&
        ssaoResource_.height > 0 &&
        ssaoResource_.aoWidth > 0 &&
        ssaoResource_.aoHeight > 0 &&
        bgfx::isValid(ssaoResource_.depthFrameBuffer) &&
        bgfx::isValid(ssaoResource_.aoFrameBuffer) &&
        bgfx::isValid(ssaoResource_.blurTempFrameBuffer) &&
        bgfx::isValid(ssaoResource_.blurredFrameBuffer);
    stats_.ssaoTargetBytes = ssaoResourcesValid ?
        resources::estimateTexture2DBytes(
            ssaoResource_.width,
            ssaoResource_.height,
            resources::ResourceMemoryFormat::R32F) +
            resources::estimateTexture2DBytes(
                ssaoResource_.width,
                ssaoResource_.height,
                resources::ResourceMemoryFormat::D24) +
            (3ULL * resources::estimateTexture2DBytes(
                ssaoResource_.aoWidth,
                ssaoResource_.aoHeight,
                resources::ResourceMemoryFormat::Rgba8)) :
        0;

    const bool selectionResourcesValid =
        selectionMaskResource_.width > 0 &&
        selectionMaskResource_.height > 0 &&
        bgfx::isValid(selectionMaskResource_.frameBuffer) &&
        bgfx::isValid(selectionMaskResource_.colorTexture) &&
        bgfx::isValid(selectionMaskResource_.depthTexture);
    stats_.selectionTargetBytes = selectionResourcesValid ?
        resources::estimateTexture2DBytes(
            selectionMaskResource_.width,
            selectionMaskResource_.height,
            resources::ResourceMemoryFormat::Rgba8) +
            resources::estimateTexture2DBytes(
                selectionMaskResource_.width,
                selectionMaskResource_.height,
                resources::ResourceMemoryFormat::D24) :
        0;

    stats_.fallbackResourceValid =
        bgfx::isValid(fallbackWhiteTexture_) &&
        bgfx::isValid(fallbackSplatTexture_) &&
        bgfx::isValid(fallbackNormalTexture_) ? 1U : 0U;
    stats_.fallbackTextureBytes = stats_.fallbackResourceValid != 0U ?
        3ULL * resources::estimateTexture2DBytes(1, 1, resources::ResourceMemoryFormat::Rgba8) :
        0;
#else
    stats_.meshAllocationFailureCount = 0;
    stats_.skinnedMeshAllocationFailureCount = 0;
    stats_.textureAllocationFailureCount = 0;
    stats_.selectionMaskResourceRecreateCount = 0;
    stats_.selectionMaskAllocationFailureCount = 0;
    stats_.ssaoResourceRecreateCount = 0;
    stats_.ssaoAllocationFailureCount = 0;
    stats_.shadowTargetBytes = 0;
    stats_.sceneTargetBytes = 0;
    stats_.ssaoTargetBytes = 0;
    stats_.selectionTargetBytes = 0;
    stats_.fallbackTextureBytes = 0;
    stats_.fallbackResourceValid = 0;
#endif

    std::uint64_t total = 0;
    total = resources::addSaturating(total, stats_.meshBufferBytes);
    total = resources::addSaturating(total, stats_.skinnedMeshBufferBytes);
    total = resources::addSaturating(total, stats_.textureBytes);
    total = resources::addSaturating(total, stats_.materialBytes);
    total = resources::addSaturating(total, stats_.shadowTargetBytes);
    total = resources::addSaturating(total, stats_.sceneTargetBytes);
    total = resources::addSaturating(total, stats_.ssaoTargetBytes);
    total = resources::addSaturating(total, stats_.selectionTargetBytes);
    total = resources::addSaturating(total, stats_.fallbackTextureBytes);
    stats_.totalEstimatedResourceBytes = total;
}

#if FULL_RENDERER_ENABLE_BGFX
void BgfxRenderDevice::destroyShadowResources() noexcept
{
    for (ShadowCascadeResource& resource : shadowCascadeResources_)
    {
        if (bgfx::isValid(resource.frameBuffer))
        {
            bgfx::destroy(resource.frameBuffer);
            resource.frameBuffer = BGFX_INVALID_HANDLE;
            resource.colorTexture = BGFX_INVALID_HANDLE;
            resource.depthTexture = BGFX_INVALID_HANDLE;
        }
        else
        {
            if (bgfx::isValid(resource.depthTexture))
            {
                bgfx::destroy(resource.depthTexture);
                resource.depthTexture = BGFX_INVALID_HANDLE;
            }
            if (bgfx::isValid(resource.colorTexture))
            {
                bgfx::destroy(resource.colorTexture);
                resource.colorTexture = BGFX_INVALID_HANDLE;
            }
        }
    }

    shadowMapResolution_ = 0;
    shadowCascadeResourceCount_ = 0;
    updateLiveResourceStats();
}

bool BgfxRenderDevice::hasValidShadowResources(const std::uint32_t cascadeCount) const noexcept
{
    if (cascadeCount == 0 ||
        cascadeCount > kMaxDirectionalShadowCascades ||
        shadowCascadeResourceCount_ < cascadeCount)
    {
        return false;
    }

    for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
    {
        const ShadowCascadeResource& resource = shadowCascadeResources_[cascadeIndex];
        if (!bgfx::isValid(resource.frameBuffer) ||
            !bgfx::isValid(resource.colorTexture) ||
            !bgfx::isValid(resource.depthTexture))
        {
            return false;
        }
    }

    return true;
}

bool BgfxRenderDevice::ensureShadowResources(const std::uint32_t resolution)
{
    return ensureShadowResources(resolution, 1);
}

bool BgfxRenderDevice::ensureShadowResources(const std::uint32_t resolution, const std::uint32_t cascadeCount)
{
    const std::uint32_t clampedCascadeCount = scene::clampShadowCascadeCount(cascadeCount);
    const DirectionalShadowDesc requestedDesc = [&]() {
        DirectionalShadowDesc desc;
        desc.enabled = true;
        desc.mapResolution = resolution;
        desc.cascadeCount = clampedCascadeCount;
        return desc;
    }();
    const scene::ShadowResourceReconfigurePlan plan = scene::planShadowResourceReconfiguration(
        requestedDesc,
        shadowMapResolution_,
        shadowCascadeResourceCount_,
        hasValidShadowResources(clampedCascadeCount));
    const bool alreadyValid = plan.action == scene::ShadowResourceReconfigureAction::None;
    if (alreadyValid)
    {
        for (std::uint32_t cascadeIndex = 0; cascadeIndex < clampedCascadeCount; ++cascadeIndex)
        {
            configureShadowView(cascadeIndex, resolution, shadowCascadeResources_[cascadeIndex].frameBuffer);
        }
        return true;
    }

    destroyShadowResources();

    constexpr std::uint64_t kSamplerFlags =
        BGFX_SAMPLER_U_CLAMP |
        BGFX_SAMPLER_V_CLAMP |
        BGFX_SAMPLER_MIN_POINT |
        BGFX_SAMPLER_MAG_POINT |
        BGFX_SAMPLER_MIP_POINT;
    for (std::uint32_t cascadeIndex = 0; cascadeIndex < clampedCascadeCount; ++cascadeIndex)
    {
        ShadowCascadeResource& resource = shadowCascadeResources_[cascadeIndex];
        resource.colorTexture = bgfx::createTexture2D(
            static_cast<std::uint16_t>(resolution),
            static_cast<std::uint16_t>(resolution),
            false,
            1,
            bgfx::TextureFormat::R32F,
            BGFX_TEXTURE_RT | kSamplerFlags);
        resource.depthTexture = bgfx::createTexture2D(
            static_cast<std::uint16_t>(resolution),
            static_cast<std::uint16_t>(resolution),
            false,
            1,
            bgfx::TextureFormat::D24,
            BGFX_TEXTURE_RT_WRITE_ONLY);
        if (!bgfx::isValid(resource.colorTexture) || !bgfx::isValid(resource.depthTexture))
        {
            destroyShadowResources();
            ++shadowResourceAllocationFailureCount_;
            updateLiveResourceStats();
            return false;
        }

        bgfx::Attachment attachments[2];
        attachments[0].init(resource.colorTexture);
        attachments[1].init(resource.depthTexture);
        resource.frameBuffer = bgfx::createFrameBuffer(2, attachments, true);
        if (!bgfx::isValid(resource.frameBuffer))
        {
            destroyShadowResources();
            ++shadowResourceAllocationFailureCount_;
            updateLiveResourceStats();
            return false;
        }
    }

    shadowMapResolution_ = resolution;
    shadowCascadeResourceCount_ = clampedCascadeCount;
    for (std::uint32_t cascadeIndex = 0; cascadeIndex < clampedCascadeCount; ++cascadeIndex)
    {
        configureShadowView(cascadeIndex, resolution, shadowCascadeResources_[cascadeIndex].frameBuffer);
    }
    ++shadowResourceRecreateCount_;
    updateLiveResourceStats();
    return true;
}

void BgfxRenderDevice::destroySelectionMaskResource() noexcept
{
    if (bgfx::isValid(selectionMaskResource_.frameBuffer))
    {
        bgfx::destroy(selectionMaskResource_.frameBuffer);
        selectionMaskResource_.frameBuffer = BGFX_INVALID_HANDLE;
        selectionMaskResource_.colorTexture = BGFX_INVALID_HANDLE;
        selectionMaskResource_.depthTexture = BGFX_INVALID_HANDLE;
    }
    else
    {
        if (bgfx::isValid(selectionMaskResource_.depthTexture))
        {
            bgfx::destroy(selectionMaskResource_.depthTexture);
            selectionMaskResource_.depthTexture = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(selectionMaskResource_.colorTexture))
        {
            bgfx::destroy(selectionMaskResource_.colorTexture);
            selectionMaskResource_.colorTexture = BGFX_INVALID_HANDLE;
        }
    }
    selectionMaskResource_.width = 0;
    selectionMaskResource_.height = 0;
    updateLiveResourceStats();
}

bool BgfxRenderDevice::ensureSelectionMaskResource(const std::uint32_t width, const std::uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return false;
    }

    const bool validExisting =
        selectionMaskResource_.width == width &&
        selectionMaskResource_.height == height &&
        bgfx::isValid(selectionMaskResource_.frameBuffer) &&
        bgfx::isValid(selectionMaskResource_.colorTexture) &&
        bgfx::isValid(selectionMaskResource_.depthTexture);
    if (validExisting)
    {
        bgfx::setViewFrameBuffer(kSelectionMaskViewId, selectionMaskResource_.frameBuffer);
        bgfx::setViewRect(kSelectionMaskViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));
        return true;
    }

    destroySelectionMaskResource();
    constexpr std::uint64_t kSamplerFlags =
        BGFX_SAMPLER_U_CLAMP |
        BGFX_SAMPLER_V_CLAMP |
        BGFX_SAMPLER_MIN_POINT |
        BGFX_SAMPLER_MAG_POINT |
        BGFX_SAMPLER_MIP_POINT;
    selectionMaskResource_.colorTexture = bgfx::createTexture2D(
        static_cast<std::uint16_t>(width),
        static_cast<std::uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | kSamplerFlags);
    selectionMaskResource_.depthTexture = bgfx::createTexture2D(
        static_cast<std::uint16_t>(width),
        static_cast<std::uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::D24,
        BGFX_TEXTURE_RT_WRITE_ONLY);
    if (!bgfx::isValid(selectionMaskResource_.colorTexture) ||
        !bgfx::isValid(selectionMaskResource_.depthTexture))
    {
        destroySelectionMaskResource();
        ++selectionMaskAllocationFailureCount_;
        updateLiveResourceStats();
        return false;
    }

    bgfx::Attachment attachments[2];
    attachments[0].init(selectionMaskResource_.colorTexture);
    attachments[1].init(selectionMaskResource_.depthTexture);
    selectionMaskResource_.frameBuffer = bgfx::createFrameBuffer(2, attachments, true);
    if (!bgfx::isValid(selectionMaskResource_.frameBuffer))
    {
        destroySelectionMaskResource();
        ++selectionMaskAllocationFailureCount_;
        updateLiveResourceStats();
        return false;
    }

    selectionMaskResource_.width = width;
    selectionMaskResource_.height = height;
    ++selectionMaskResourceRecreateCount_;
    bgfx::setViewFrameBuffer(kSelectionMaskViewId, selectionMaskResource_.frameBuffer);
    bgfx::setViewRect(kSelectionMaskViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));
    updateLiveResourceStats();
    return true;
}

void BgfxRenderDevice::destroySsaoResource() noexcept
{
    if (bgfx::isValid(ssaoResource_.depthFrameBuffer))
    {
        bgfx::destroy(ssaoResource_.depthFrameBuffer);
        ssaoResource_.depthFrameBuffer = BGFX_INVALID_HANDLE;
        ssaoResource_.depthColorTexture = BGFX_INVALID_HANDLE;
        ssaoResource_.depthTexture = BGFX_INVALID_HANDLE;
    }
    else
    {
        if (bgfx::isValid(ssaoResource_.depthTexture))
        {
            bgfx::destroy(ssaoResource_.depthTexture);
            ssaoResource_.depthTexture = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(ssaoResource_.depthColorTexture))
        {
            bgfx::destroy(ssaoResource_.depthColorTexture);
            ssaoResource_.depthColorTexture = BGFX_INVALID_HANDLE;
        }
    }

    if (bgfx::isValid(ssaoResource_.aoFrameBuffer))
    {
        bgfx::destroy(ssaoResource_.aoFrameBuffer);
        ssaoResource_.aoFrameBuffer = BGFX_INVALID_HANDLE;
        ssaoResource_.aoTexture = BGFX_INVALID_HANDLE;
    }
    else if (bgfx::isValid(ssaoResource_.aoTexture))
    {
        bgfx::destroy(ssaoResource_.aoTexture);
        ssaoResource_.aoTexture = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(ssaoResource_.blurTempFrameBuffer))
    {
        bgfx::destroy(ssaoResource_.blurTempFrameBuffer);
        ssaoResource_.blurTempFrameBuffer = BGFX_INVALID_HANDLE;
        ssaoResource_.blurTempTexture = BGFX_INVALID_HANDLE;
    }
    else if (bgfx::isValid(ssaoResource_.blurTempTexture))
    {
        bgfx::destroy(ssaoResource_.blurTempTexture);
        ssaoResource_.blurTempTexture = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(ssaoResource_.blurredFrameBuffer))
    {
        bgfx::destroy(ssaoResource_.blurredFrameBuffer);
        ssaoResource_.blurredFrameBuffer = BGFX_INVALID_HANDLE;
        ssaoResource_.blurredTexture = BGFX_INVALID_HANDLE;
    }
    else if (bgfx::isValid(ssaoResource_.blurredTexture))
    {
        bgfx::destroy(ssaoResource_.blurredTexture);
        ssaoResource_.blurredTexture = BGFX_INVALID_HANDLE;
    }

    ssaoResource_.width = 0;
    ssaoResource_.height = 0;
    ssaoResource_.aoWidth = 0;
    ssaoResource_.aoHeight = 0;
    updateLiveResourceStats();
}

bool BgfxRenderDevice::ensureSsaoResource(
    const std::uint32_t width,
    const std::uint32_t height,
    const std::uint32_t aoWidth,
    const std::uint32_t aoHeight)
{
    if (width == 0 || height == 0 || aoWidth == 0 || aoHeight == 0)
    {
        return false;
    }

    const bool validExisting =
        ssaoResource_.width == width &&
        ssaoResource_.height == height &&
        ssaoResource_.aoWidth == aoWidth &&
        ssaoResource_.aoHeight == aoHeight &&
        bgfx::isValid(ssaoResource_.depthFrameBuffer) &&
        bgfx::isValid(ssaoResource_.depthColorTexture) &&
        bgfx::isValid(ssaoResource_.depthTexture) &&
        bgfx::isValid(ssaoResource_.aoFrameBuffer) &&
        bgfx::isValid(ssaoResource_.aoTexture) &&
        bgfx::isValid(ssaoResource_.blurTempFrameBuffer) &&
        bgfx::isValid(ssaoResource_.blurTempTexture) &&
        bgfx::isValid(ssaoResource_.blurredFrameBuffer) &&
        bgfx::isValid(ssaoResource_.blurredTexture);
    if (validExisting)
    {
        bgfx::setViewFrameBuffer(kSsaoDepthViewId, ssaoResource_.depthFrameBuffer);
        bgfx::setViewRect(kSsaoDepthViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));
        bgfx::setViewFrameBuffer(kSsaoViewId, ssaoResource_.aoFrameBuffer);
        bgfx::setViewRect(kSsaoViewId, 0, 0, static_cast<std::uint16_t>(aoWidth), static_cast<std::uint16_t>(aoHeight));
        bgfx::setViewFrameBuffer(kSsaoBlurHorizontalViewId, ssaoResource_.blurTempFrameBuffer);
        bgfx::setViewRect(kSsaoBlurHorizontalViewId, 0, 0, static_cast<std::uint16_t>(aoWidth), static_cast<std::uint16_t>(aoHeight));
        bgfx::setViewFrameBuffer(kSsaoBlurVerticalViewId, ssaoResource_.blurredFrameBuffer);
        bgfx::setViewRect(kSsaoBlurVerticalViewId, 0, 0, static_cast<std::uint16_t>(aoWidth), static_cast<std::uint16_t>(aoHeight));
        return true;
    }

    destroySsaoResource();
    constexpr std::uint64_t kDepthSamplerFlags =
        BGFX_SAMPLER_U_CLAMP |
        BGFX_SAMPLER_V_CLAMP |
        BGFX_SAMPLER_MIN_POINT |
        BGFX_SAMPLER_MAG_POINT |
        BGFX_SAMPLER_MIP_POINT;
    constexpr std::uint64_t kAoSamplerFlags =
        BGFX_SAMPLER_U_CLAMP |
        BGFX_SAMPLER_V_CLAMP |
        BGFX_SAMPLER_MIP_POINT;
    ssaoResource_.depthColorTexture = bgfx::createTexture2D(
        static_cast<std::uint16_t>(width),
        static_cast<std::uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::R32F,
        BGFX_TEXTURE_RT | kDepthSamplerFlags);
    ssaoResource_.depthTexture = bgfx::createTexture2D(
        static_cast<std::uint16_t>(width),
        static_cast<std::uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::D24,
        BGFX_TEXTURE_RT_WRITE_ONLY);
    ssaoResource_.aoTexture = bgfx::createTexture2D(
        static_cast<std::uint16_t>(aoWidth),
        static_cast<std::uint16_t>(aoHeight),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | kAoSamplerFlags);
    ssaoResource_.blurTempTexture = bgfx::createTexture2D(
        static_cast<std::uint16_t>(aoWidth),
        static_cast<std::uint16_t>(aoHeight),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | kAoSamplerFlags);
    ssaoResource_.blurredTexture = bgfx::createTexture2D(
        static_cast<std::uint16_t>(aoWidth),
        static_cast<std::uint16_t>(aoHeight),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | kAoSamplerFlags);
    if (!bgfx::isValid(ssaoResource_.depthColorTexture) ||
        !bgfx::isValid(ssaoResource_.depthTexture) ||
        !bgfx::isValid(ssaoResource_.aoTexture) ||
        !bgfx::isValid(ssaoResource_.blurTempTexture) ||
        !bgfx::isValid(ssaoResource_.blurredTexture))
    {
        destroySsaoResource();
        ++ssaoAllocationFailureCount_;
        updateLiveResourceStats();
        return false;
    }

    bgfx::Attachment depthAttachments[2];
    depthAttachments[0].init(ssaoResource_.depthColorTexture);
    depthAttachments[1].init(ssaoResource_.depthTexture);
    ssaoResource_.depthFrameBuffer = bgfx::createFrameBuffer(2, depthAttachments, true);
    bgfx::Attachment aoAttachment;
    aoAttachment.init(ssaoResource_.aoTexture);
    ssaoResource_.aoFrameBuffer = bgfx::createFrameBuffer(1, &aoAttachment, true);
    bgfx::Attachment blurTempAttachment;
    blurTempAttachment.init(ssaoResource_.blurTempTexture);
    ssaoResource_.blurTempFrameBuffer = bgfx::createFrameBuffer(1, &blurTempAttachment, true);
    bgfx::Attachment blurredAttachment;
    blurredAttachment.init(ssaoResource_.blurredTexture);
    ssaoResource_.blurredFrameBuffer = bgfx::createFrameBuffer(1, &blurredAttachment, true);
    if (!bgfx::isValid(ssaoResource_.depthFrameBuffer) ||
        !bgfx::isValid(ssaoResource_.aoFrameBuffer) ||
        !bgfx::isValid(ssaoResource_.blurTempFrameBuffer) ||
        !bgfx::isValid(ssaoResource_.blurredFrameBuffer))
    {
        destroySsaoResource();
        ++ssaoAllocationFailureCount_;
        updateLiveResourceStats();
        return false;
    }

    ssaoResource_.width = width;
    ssaoResource_.height = height;
    ssaoResource_.aoWidth = aoWidth;
    ssaoResource_.aoHeight = aoHeight;
    bgfx::setViewFrameBuffer(kSsaoDepthViewId, ssaoResource_.depthFrameBuffer);
    bgfx::setViewRect(kSsaoDepthViewId, 0, 0, static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));
    bgfx::setViewFrameBuffer(kSsaoViewId, ssaoResource_.aoFrameBuffer);
    bgfx::setViewRect(kSsaoViewId, 0, 0, static_cast<std::uint16_t>(aoWidth), static_cast<std::uint16_t>(aoHeight));
    bgfx::setViewFrameBuffer(kSsaoBlurHorizontalViewId, ssaoResource_.blurTempFrameBuffer);
    bgfx::setViewRect(kSsaoBlurHorizontalViewId, 0, 0, static_cast<std::uint16_t>(aoWidth), static_cast<std::uint16_t>(aoHeight));
    bgfx::setViewFrameBuffer(kSsaoBlurVerticalViewId, ssaoResource_.blurredFrameBuffer);
    bgfx::setViewRect(kSsaoBlurVerticalViewId, 0, 0, static_cast<std::uint16_t>(aoWidth), static_cast<std::uint16_t>(aoHeight));
    ++ssaoResourceRecreateCount_;
    updateLiveResourceStats();
    return true;
}

void BgfxRenderDevice::destroySceneTargetResource() noexcept
{
    if (bgfx::isValid(sceneTargetResource_.frameBuffer))
    {
        bgfx::destroy(sceneTargetResource_.frameBuffer);
        sceneTargetResource_.frameBuffer = BGFX_INVALID_HANDLE;
        sceneTargetResource_.colorTexture = BGFX_INVALID_HANDLE;
        sceneTargetResource_.depthTexture = BGFX_INVALID_HANDLE;
    }
    else
    {
        if (bgfx::isValid(sceneTargetResource_.depthTexture))
        {
            bgfx::destroy(sceneTargetResource_.depthTexture);
            sceneTargetResource_.depthTexture = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(sceneTargetResource_.colorTexture))
        {
            bgfx::destroy(sceneTargetResource_.colorTexture);
            sceneTargetResource_.colorTexture = BGFX_INVALID_HANDLE;
        }
    }

    if (bgfx::isValid(sceneTargetResource_.depthCaptureFrameBuffer))
    {
        bgfx::destroy(sceneTargetResource_.depthCaptureFrameBuffer);
        sceneTargetResource_.depthCaptureFrameBuffer = BGFX_INVALID_HANDLE;
        sceneTargetResource_.depthColorTexture = BGFX_INVALID_HANDLE;
        sceneTargetResource_.depthCaptureDepthTexture = BGFX_INVALID_HANDLE;
    }
    else
    {
        if (bgfx::isValid(sceneTargetResource_.depthCaptureDepthTexture))
        {
            bgfx::destroy(sceneTargetResource_.depthCaptureDepthTexture);
            sceneTargetResource_.depthCaptureDepthTexture = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(sceneTargetResource_.depthColorTexture))
        {
            bgfx::destroy(sceneTargetResource_.depthColorTexture);
            sceneTargetResource_.depthColorTexture = BGFX_INVALID_HANDLE;
        }
    }

    sceneTargetResource_.width = 0;
    sceneTargetResource_.height = 0;
    updateLiveResourceStats();
}

bool BgfxRenderDevice::hasValidSceneTargetResource() const noexcept
{
    return sceneTargetResource_.width > 0 &&
        sceneTargetResource_.height > 0 &&
        bgfx::isValid(sceneTargetResource_.frameBuffer) &&
        bgfx::isValid(sceneTargetResource_.colorTexture) &&
        bgfx::isValid(sceneTargetResource_.depthTexture) &&
        bgfx::isValid(sceneTargetResource_.depthCaptureFrameBuffer) &&
        bgfx::isValid(sceneTargetResource_.depthColorTexture) &&
        bgfx::isValid(sceneTargetResource_.depthCaptureDepthTexture);
}

bool BgfxRenderDevice::ensureSceneTargetResource(const std::uint32_t width, const std::uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return false;
    }

    const bool validExisting =
        sceneTargetResource_.width == width &&
        sceneTargetResource_.height == height &&
        hasValidSceneTargetResource();
    if (validExisting)
    {
        bgfx::setViewFrameBuffer(kSkyViewId, sceneTargetResource_.frameBuffer);
        bgfx::setViewFrameBuffer(kForwardViewId, sceneTargetResource_.frameBuffer);
        bgfx::setViewFrameBuffer(kSsaoCompositeViewId, sceneTargetResource_.frameBuffer);
        bgfx::setViewFrameBuffer(kDecalDepthViewId, sceneTargetResource_.depthCaptureFrameBuffer);
        bgfx::setViewFrameBuffer(kDecalViewId, sceneTargetResource_.frameBuffer);
        bgfx::setViewFrameBuffer(kParticleViewId, sceneTargetResource_.frameBuffer);
        return true;
    }

    sceneTargetResourceReconfiguredThisFrame_ = true;
    destroySceneTargetResource();
    constexpr std::uint64_t kColorSamplerFlags =
        BGFX_SAMPLER_U_CLAMP |
        BGFX_SAMPLER_V_CLAMP |
        BGFX_SAMPLER_MIN_POINT |
        BGFX_SAMPLER_MAG_POINT |
        BGFX_SAMPLER_MIP_POINT;
    sceneTargetResource_.colorTexture = bgfx::createTexture2D(
        static_cast<std::uint16_t>(width),
        static_cast<std::uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | kColorSamplerFlags);
    sceneTargetResource_.depthTexture = bgfx::createTexture2D(
        static_cast<std::uint16_t>(width),
        static_cast<std::uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::D24,
        BGFX_TEXTURE_RT_WRITE_ONLY);
    sceneTargetResource_.depthColorTexture = bgfx::createTexture2D(
        static_cast<std::uint16_t>(width),
        static_cast<std::uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::R32F,
        BGFX_TEXTURE_RT | kColorSamplerFlags);
    sceneTargetResource_.depthCaptureDepthTexture = bgfx::createTexture2D(
        static_cast<std::uint16_t>(width),
        static_cast<std::uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::D24,
        BGFX_TEXTURE_RT_WRITE_ONLY);
    if (!bgfx::isValid(sceneTargetResource_.colorTexture) ||
        !bgfx::isValid(sceneTargetResource_.depthTexture) ||
        !bgfx::isValid(sceneTargetResource_.depthColorTexture) ||
        !bgfx::isValid(sceneTargetResource_.depthCaptureDepthTexture))
    {
        destroySceneTargetResource();
        sceneTargetResourceAllocationFailedThisFrame_ = true;
        ++sceneTargetResourceAllocationFailureCount_;
        updateLiveResourceStats();
        return false;
    }

    bgfx::Attachment sceneAttachments[2];
    sceneAttachments[0].init(sceneTargetResource_.colorTexture);
    sceneAttachments[1].init(sceneTargetResource_.depthTexture);
    sceneTargetResource_.frameBuffer = bgfx::createFrameBuffer(2, sceneAttachments, true);

    bgfx::Attachment depthAttachments[2];
    depthAttachments[0].init(sceneTargetResource_.depthColorTexture);
    depthAttachments[1].init(sceneTargetResource_.depthCaptureDepthTexture);
    sceneTargetResource_.depthCaptureFrameBuffer = bgfx::createFrameBuffer(2, depthAttachments, true);
    if (!bgfx::isValid(sceneTargetResource_.frameBuffer) ||
        !bgfx::isValid(sceneTargetResource_.depthCaptureFrameBuffer))
    {
        destroySceneTargetResource();
        sceneTargetResourceAllocationFailedThisFrame_ = true;
        ++sceneTargetResourceAllocationFailureCount_;
        updateLiveResourceStats();
        return false;
    }

    sceneTargetResource_.width = width;
    sceneTargetResource_.height = height;
    ++sceneTargetResourceRecreateCount_;
    bgfx::setViewFrameBuffer(kSkyViewId, sceneTargetResource_.frameBuffer);
    bgfx::setViewFrameBuffer(kForwardViewId, sceneTargetResource_.frameBuffer);
    bgfx::setViewFrameBuffer(kSsaoCompositeViewId, sceneTargetResource_.frameBuffer);
    bgfx::setViewFrameBuffer(kDecalDepthViewId, sceneTargetResource_.depthCaptureFrameBuffer);
    bgfx::setViewFrameBuffer(kDecalViewId, sceneTargetResource_.frameBuffer);
    bgfx::setViewFrameBuffer(kParticleViewId, sceneTargetResource_.frameBuffer);
    updateLiveResourceStats();
    return true;
}
#endif

RendererResult BgfxRenderDevice::initialize(const RendererInitDesc& desc)
{
    if (initialized_)
    {
        return RendererResult::AlreadyInitialized;
    }

    if (!hasValidDimensions(desc.backbufferWidth, desc.backbufferHeight))
    {
        return RendererResult::InvalidDescriptor;
    }

    if (desc.window.nativeWindow == nullptr)
    {
        return RendererResult::InvalidDescriptor;
    }

    if (desc.shaderBinaryDirectory == nullptr || desc.shaderBinaryDirectory[0] == '\0')
    {
        return RendererResult::InvalidDescriptor;
    }

    shaderBinaryDirectory_ = desc.shaderBinaryDirectory;

#if FULL_RENDERER_ENABLE_BGFX
    bgfx::Init init;
    init.type = bgfx::RendererType::Count;
    init.platformData.ndt = desc.window.nativeDisplay;
    init.platformData.nwh = desc.window.nativeWindow;
    init.resolution.width = desc.backbufferWidth;
    init.resolution.height = desc.backbufferHeight;
    init.resolution.reset = kResetFlags;

    if (!bgfx::init(init))
    {
        shaderBinaryDirectory_.clear();
        return RendererResult::BackendFailure;
    }

    initialized_ = true;

    meshVertexLayout_
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Float)
        .end();
    skinnedMeshVertexLayout_
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord2, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Float)
        .end();
    particleVertexLayout_
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 1, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
        .end();
    debugLineVertexLayout_
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
        .end();

    lightDirIntensityUniform_ = bgfx::createUniform("u_lightDirIntensity", bgfx::UniformType::Vec4);
    lightColorUniform_ = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
    materialColorUniform_ = bgfx::createUniform("u_materialColor", bgfx::UniformType::Vec4);
    basicBaseColorSampler_ = bgfx::createUniform("s_basicBaseColor", bgfx::UniformType::Sampler);
    basicNormalSampler_ = bgfx::createUniform("s_basicNormal", bgfx::UniformType::Sampler);
    terrainLayerColorUniform_ = bgfx::createUniform("u_terrainLayerColors", bgfx::UniformType::Vec4, kMaxTerrainMaterialLayers);
    terrainParamsUniform_ = bgfx::createUniform("u_terrainParams", bgfx::UniformType::Vec4);
    shadowViewProjUniform_ = bgfx::createUniform("u_shadowViewProj", bgfx::UniformType::Mat4, kMaxDirectionalShadowCascades);
    shadowParamsUniform_ = bgfx::createUniform("u_shadowParams", bgfx::UniformType::Vec4);
    shadowFilterParamsUniform_ = bgfx::createUniform("u_shadowFilterParams", bgfx::UniformType::Vec4);
    cascadeSplitsUniform_ = bgfx::createUniform("u_cascadeSplits", bgfx::UniformType::Vec4);
    environmentColorsUniform_ = bgfx::createUniform("u_environmentColors", bgfx::UniformType::Vec4, 4);
    fogParamsUniform_ = bgfx::createUniform("u_fogParams", bgfx::UniformType::Vec4);
    weatherParamsUniform_ = bgfx::createUniform("u_weatherParams", bgfx::UniformType::Vec4);
    weatherWindParamsUniform_ = bgfx::createUniform("u_weatherWindParams", bgfx::UniformType::Vec4);
    weatherPrecipitationParamsUniform_ =
        bgfx::createUniform("u_weatherPrecipitationParams", bgfx::UniformType::Vec4);
    fadeParamsUniform_ = bgfx::createUniform("u_fadeParams", bgfx::UniformType::Vec4);
    skinningPaletteUniform_ = bgfx::createUniform("u_skinningPalette", bgfx::UniformType::Mat4, kMaxSkinningJoints);
    layerSamplers_[0] = bgfx::createUniform("s_layer0", bgfx::UniformType::Sampler);
    layerSamplers_[1] = bgfx::createUniform("s_layer1", bgfx::UniformType::Sampler);
    layerSamplers_[2] = bgfx::createUniform("s_layer2", bgfx::UniformType::Sampler);
    layerSamplers_[3] = bgfx::createUniform("s_layer3", bgfx::UniformType::Sampler);
    normalSamplers_[0] = bgfx::createUniform("s_normal0", bgfx::UniformType::Sampler);
    normalSamplers_[1] = bgfx::createUniform("s_normal1", bgfx::UniformType::Sampler);
    normalSamplers_[2] = bgfx::createUniform("s_normal2", bgfx::UniformType::Sampler);
    normalSamplers_[3] = bgfx::createUniform("s_normal3", bgfx::UniformType::Sampler);
    splatSampler_ = bgfx::createUniform("s_splat", bgfx::UniformType::Sampler);
    shadowMapSamplers_[0] = bgfx::createUniform("s_shadowMap0", bgfx::UniformType::Sampler);
    shadowMapSamplers_[1] = bgfx::createUniform("s_shadowMap1", bgfx::UniformType::Sampler);
    shadowMapSamplers_[2] = bgfx::createUniform("s_shadowMap2", bgfx::UniformType::Sampler);
    shadowMapSamplers_[3] = bgfx::createUniform("s_shadowMap3", bgfx::UniformType::Sampler);
    outlineColorUniform_ = bgfx::createUniform("u_outlineColor", bgfx::UniformType::Vec4);
    outlineParamsUniform_ = bgfx::createUniform("u_outlineParams", bgfx::UniformType::Vec4);
    selectionMaskSampler_ = bgfx::createUniform("s_selectionMask", bgfx::UniformType::Sampler);
    ssaoDepthParamsUniform_ = bgfx::createUniform("u_ssaoDepthParams", bgfx::UniformType::Vec4);
    ssaoParamsUniform_ = bgfx::createUniform("u_ssaoParams", bgfx::UniformType::Vec4);
    ssaoDepthSampler_ = bgfx::createUniform("s_ssaoDepth", bgfx::UniformType::Sampler);
    ssaoSampler_ = bgfx::createUniform("s_ssao", bgfx::UniformType::Sampler);
    decalInvViewUniform_ = bgfx::createUniform("u_decalInvView", bgfx::UniformType::Mat4);
    decalInvProjUniform_ = bgfx::createUniform("u_decalInvProj", bgfx::UniformType::Mat4);
    decalWorldToLocalUniform_ = bgfx::createUniform("u_decalWorldToLocal", bgfx::UniformType::Mat4);
    decalColorOpacityUniform_ = bgfx::createUniform("u_decalColorOpacity", bgfx::UniformType::Vec4);
    decalDepthParamsUniform_ = bgfx::createUniform("u_decalDepthParams", bgfx::UniformType::Vec4);
    decalDepthSampler_ = bgfx::createUniform("s_decalDepth", bgfx::UniformType::Sampler);
    decalAlbedoSampler_ = bgfx::createUniform("s_decalAlbedo", bgfx::UniformType::Sampler);
    sceneColorSampler_ = bgfx::createUniform("s_sceneColor", bgfx::UniformType::Sampler);
    colorGradingParamsUniform_ = bgfx::createUniform("u_colorGradeParams", bgfx::UniformType::Vec4);
    colorGradingControlsUniform_ = bgfx::createUniform("u_colorGradeControls", bgfx::UniformType::Vec4);
    colorGradingLiftUniform_ = bgfx::createUniform("u_colorGradeLift", bgfx::UniformType::Vec4);
    colorGradingGainUniform_ = bgfx::createUniform("u_colorGradeGain", bgfx::UniformType::Vec4);
    particleSampler_ = bgfx::createUniform("s_particleTexture", bgfx::UniformType::Sampler);
    particleDepthSampler_ = bgfx::createUniform("s_particleDepth", bgfx::UniformType::Sampler);
    particleParamsUniform_ = bgfx::createUniform("u_particleParams", bgfx::UniformType::Vec4);

    const bgfx::ShaderHandle vertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_mesh.bin"));
    const bgfx::ShaderHandle instancedVertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_mesh_instanced.bin"));
    const bgfx::ShaderHandle skinnedVertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_skinned_mesh.bin"));
    const bgfx::ShaderHandle fragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_mesh.bin"));
    const bgfx::ShaderHandle skyVertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_sky.bin"));
    const bgfx::ShaderHandle skyFragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_sky.bin"));
    const bgfx::ShaderHandle terrainVertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_terrain_instanced.bin"));
    const bgfx::ShaderHandle terrainFragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_terrain_splat.bin"));
    const bgfx::ShaderHandle debugLineVertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_debug_line.bin"));
    const bgfx::ShaderHandle debugLineFragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_debug_line.bin"));
    const bgfx::ShaderHandle terrainShadowVertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_terrain_shadow_instanced.bin"));
    const bgfx::ShaderHandle skinnedShadowVertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_skinned_shadow.bin"));
    const bgfx::ShaderHandle shadowFragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_shadow_depth.bin"));
    const bgfx::ShaderHandle selectionVertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_selection_mesh.bin"));
    const bgfx::ShaderHandle selectionInstancedVertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_selection_instanced.bin"));
    const bgfx::ShaderHandle selectionSkinnedVertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_selection_skinned.bin"));
    const bgfx::ShaderHandle selectionFragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_selection_mask.bin"));
    const bgfx::ShaderHandle outlineVertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_outline_fullscreen.bin"));
    const bgfx::ShaderHandle outlineFragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_outline_composite.bin"));
    const bgfx::ShaderHandle ssaoDepthVertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_ssao_depth_mesh.bin"));
    const bgfx::ShaderHandle ssaoDepthInstancedVertexShader =
        loadShader(joinPath(shaderBinaryDirectory_, "vs_ssao_depth_instanced.bin"));
    const bgfx::ShaderHandle ssaoDepthSkinnedVertexShader =
        loadShader(joinPath(shaderBinaryDirectory_, "vs_ssao_depth_skinned.bin"));
    const bgfx::ShaderHandle ssaoDepthFragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_ssao_depth.bin"));
    const bgfx::ShaderHandle ssaoFullscreenVertexShader =
        loadShader(joinPath(shaderBinaryDirectory_, "vs_ssao_fullscreen.bin"));
    const bgfx::ShaderHandle ssaoFragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_ssao.bin"));
    const bgfx::ShaderHandle ssaoBlurFragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_ssao_blur.bin"));
    const bgfx::ShaderHandle ssaoCompositeFragmentShader =
        loadShader(joinPath(shaderBinaryDirectory_, "fs_ssao_composite.bin"));
    const bgfx::ShaderHandle decalFragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_decal_projected.bin"));
    const bgfx::ShaderHandle scenePresentFragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_scene_present.bin"));
    const bgfx::ShaderHandle colorGradingFragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_color_grade.bin"));
    const bgfx::ShaderHandle particleVertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_particles.bin"));
    const bgfx::ShaderHandle particleFragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_particles.bin"));
    if (!isValidUniform(lightDirIntensityUniform_) ||
        !isValidUniform(lightColorUniform_) ||
        !isValidUniform(materialColorUniform_) ||
        !isValidUniform(basicBaseColorSampler_) ||
        !isValidUniform(basicNormalSampler_) ||
        !isValidUniform(terrainLayerColorUniform_) ||
        !isValidUniform(terrainParamsUniform_) ||
        !isValidUniform(shadowViewProjUniform_) ||
        !isValidUniform(shadowParamsUniform_) ||
        !isValidUniform(shadowFilterParamsUniform_) ||
        !isValidUniform(cascadeSplitsUniform_) ||
        !isValidUniform(environmentColorsUniform_) ||
        !isValidUniform(fogParamsUniform_) ||
        !isValidUniform(weatherParamsUniform_) ||
        !isValidUniform(weatherWindParamsUniform_) ||
        !isValidUniform(weatherPrecipitationParamsUniform_) ||
        !isValidUniform(fadeParamsUniform_) ||
        !isValidUniform(skinningPaletteUniform_) ||
        !isValidUniform(layerSamplers_[0]) ||
        !isValidUniform(layerSamplers_[1]) ||
        !isValidUniform(layerSamplers_[2]) ||
        !isValidUniform(layerSamplers_[3]) ||
        !isValidUniform(normalSamplers_[0]) ||
        !isValidUniform(normalSamplers_[1]) ||
        !isValidUniform(normalSamplers_[2]) ||
        !isValidUniform(normalSamplers_[3]) ||
        !isValidUniform(splatSampler_) ||
        !isValidUniform(shadowMapSamplers_[0]) ||
        !isValidUniform(shadowMapSamplers_[1]) ||
        !isValidUniform(shadowMapSamplers_[2]) ||
        !isValidUniform(shadowMapSamplers_[3]) ||
        !isValidUniform(outlineColorUniform_) ||
        !isValidUniform(outlineParamsUniform_) ||
        !isValidUniform(selectionMaskSampler_) ||
        !isValidUniform(ssaoDepthParamsUniform_) ||
        !isValidUniform(ssaoParamsUniform_) ||
        !isValidUniform(ssaoDepthSampler_) ||
        !isValidUniform(ssaoSampler_) ||
        !isValidUniform(decalInvViewUniform_) ||
        !isValidUniform(decalInvProjUniform_) ||
        !isValidUniform(decalWorldToLocalUniform_) ||
        !isValidUniform(decalColorOpacityUniform_) ||
        !isValidUniform(decalDepthParamsUniform_) ||
        !isValidUniform(decalDepthSampler_) ||
        !isValidUniform(decalAlbedoSampler_) ||
        !isValidUniform(sceneColorSampler_) ||
        !isValidUniform(colorGradingParamsUniform_) ||
        !isValidUniform(colorGradingControlsUniform_) ||
        !isValidUniform(colorGradingLiftUniform_) ||
        !isValidUniform(colorGradingGainUniform_) ||
        !isValidUniform(particleSampler_) ||
        !isValidUniform(particleDepthSampler_) ||
        !isValidUniform(particleParamsUniform_) ||
        !bgfx::isValid(vertexShader) ||
        !bgfx::isValid(instancedVertexShader) ||
        !bgfx::isValid(skinnedVertexShader) ||
        !bgfx::isValid(fragmentShader) ||
        !bgfx::isValid(skyVertexShader) ||
        !bgfx::isValid(skyFragmentShader) ||
        !bgfx::isValid(terrainVertexShader) ||
        !bgfx::isValid(terrainFragmentShader) ||
        !bgfx::isValid(debugLineVertexShader) ||
        !bgfx::isValid(debugLineFragmentShader) ||
        !bgfx::isValid(terrainShadowVertexShader) ||
        !bgfx::isValid(skinnedShadowVertexShader) ||
        !bgfx::isValid(shadowFragmentShader) ||
        !bgfx::isValid(selectionVertexShader) ||
        !bgfx::isValid(selectionInstancedVertexShader) ||
        !bgfx::isValid(selectionSkinnedVertexShader) ||
        !bgfx::isValid(selectionFragmentShader) ||
        !bgfx::isValid(outlineVertexShader) ||
        !bgfx::isValid(outlineFragmentShader) ||
        !bgfx::isValid(ssaoDepthVertexShader) ||
        !bgfx::isValid(ssaoDepthInstancedVertexShader) ||
        !bgfx::isValid(ssaoDepthSkinnedVertexShader) ||
        !bgfx::isValid(ssaoDepthFragmentShader) ||
        !bgfx::isValid(ssaoFullscreenVertexShader) ||
        !bgfx::isValid(ssaoFragmentShader) ||
        !bgfx::isValid(ssaoBlurFragmentShader) ||
        !bgfx::isValid(ssaoCompositeFragmentShader) ||
        !bgfx::isValid(decalFragmentShader) ||
        !bgfx::isValid(scenePresentFragmentShader) ||
        !bgfx::isValid(colorGradingFragmentShader) ||
        !bgfx::isValid(particleVertexShader) ||
        !bgfx::isValid(particleFragmentShader))
    {
        if (bgfx::isValid(vertexShader))
        {
            bgfx::destroy(vertexShader);
        }
        if (bgfx::isValid(fragmentShader))
        {
            bgfx::destroy(fragmentShader);
        }
        if (bgfx::isValid(instancedVertexShader))
        {
            bgfx::destroy(instancedVertexShader);
        }
        if (bgfx::isValid(skinnedVertexShader))
        {
            bgfx::destroy(skinnedVertexShader);
        }
        if (bgfx::isValid(skyVertexShader))
        {
            bgfx::destroy(skyVertexShader);
        }
        if (bgfx::isValid(skyFragmentShader))
        {
            bgfx::destroy(skyFragmentShader);
        }
        if (bgfx::isValid(terrainVertexShader))
        {
            bgfx::destroy(terrainVertexShader);
        }
        if (bgfx::isValid(terrainFragmentShader))
        {
            bgfx::destroy(terrainFragmentShader);
        }
        if (bgfx::isValid(debugLineVertexShader))
        {
            bgfx::destroy(debugLineVertexShader);
        }
        if (bgfx::isValid(debugLineFragmentShader))
        {
            bgfx::destroy(debugLineFragmentShader);
        }
        if (bgfx::isValid(terrainShadowVertexShader))
        {
            bgfx::destroy(terrainShadowVertexShader);
        }
        if (bgfx::isValid(skinnedShadowVertexShader))
        {
            bgfx::destroy(skinnedShadowVertexShader);
        }
        if (bgfx::isValid(shadowFragmentShader))
        {
            bgfx::destroy(shadowFragmentShader);
        }
        if (bgfx::isValid(selectionVertexShader))
        {
            bgfx::destroy(selectionVertexShader);
        }
        if (bgfx::isValid(selectionInstancedVertexShader))
        {
            bgfx::destroy(selectionInstancedVertexShader);
        }
        if (bgfx::isValid(selectionSkinnedVertexShader))
        {
            bgfx::destroy(selectionSkinnedVertexShader);
        }
        if (bgfx::isValid(selectionFragmentShader))
        {
            bgfx::destroy(selectionFragmentShader);
        }
        if (bgfx::isValid(outlineVertexShader))
        {
            bgfx::destroy(outlineVertexShader);
        }
        if (bgfx::isValid(outlineFragmentShader))
        {
            bgfx::destroy(outlineFragmentShader);
        }
        if (bgfx::isValid(ssaoDepthVertexShader))
        {
            bgfx::destroy(ssaoDepthVertexShader);
        }
        if (bgfx::isValid(ssaoDepthInstancedVertexShader))
        {
            bgfx::destroy(ssaoDepthInstancedVertexShader);
        }
        if (bgfx::isValid(ssaoDepthSkinnedVertexShader))
        {
            bgfx::destroy(ssaoDepthSkinnedVertexShader);
        }
        if (bgfx::isValid(ssaoDepthFragmentShader))
        {
            bgfx::destroy(ssaoDepthFragmentShader);
        }
        if (bgfx::isValid(ssaoFullscreenVertexShader))
        {
            bgfx::destroy(ssaoFullscreenVertexShader);
        }
        if (bgfx::isValid(ssaoFragmentShader))
        {
            bgfx::destroy(ssaoFragmentShader);
        }
        if (bgfx::isValid(ssaoBlurFragmentShader))
        {
            bgfx::destroy(ssaoBlurFragmentShader);
        }
        if (bgfx::isValid(ssaoCompositeFragmentShader))
        {
            bgfx::destroy(ssaoCompositeFragmentShader);
        }
        if (bgfx::isValid(decalFragmentShader))
        {
            bgfx::destroy(decalFragmentShader);
        }
        if (bgfx::isValid(scenePresentFragmentShader))
        {
            bgfx::destroy(scenePresentFragmentShader);
        }
        if (bgfx::isValid(colorGradingFragmentShader))
        {
            bgfx::destroy(colorGradingFragmentShader);
        }
        if (bgfx::isValid(particleVertexShader))
        {
            bgfx::destroy(particleVertexShader);
        }
        if (bgfx::isValid(particleFragmentShader))
        {
            bgfx::destroy(particleFragmentShader);
        }
        shutdown();
        return RendererResult::BackendFailure;
    }

    forwardProgram_ = bgfx::createProgram(vertexShader, fragmentShader, false);
    instancedForwardProgram_ = bgfx::createProgram(instancedVertexShader, fragmentShader, false);
    skinnedForwardProgram_ = bgfx::createProgram(skinnedVertexShader, fragmentShader, true);
    skyProgram_ = bgfx::createProgram(skyVertexShader, skyFragmentShader, true);
    terrainSplatProgram_ = bgfx::createProgram(terrainVertexShader, terrainFragmentShader, true);
    debugLineProgram_ = bgfx::createProgram(debugLineVertexShader, debugLineFragmentShader, true);
    terrainShadowProgram_ = bgfx::createProgram(terrainShadowVertexShader, shadowFragmentShader, false);
    skinnedShadowProgram_ = bgfx::createProgram(skinnedShadowVertexShader, shadowFragmentShader, true);
    selectionMaskProgram_ = bgfx::createProgram(selectionVertexShader, selectionFragmentShader, false);
    selectionMaskInstancedProgram_ = bgfx::createProgram(selectionInstancedVertexShader, selectionFragmentShader, false);
    selectionMaskSkinnedProgram_ = bgfx::createProgram(selectionSkinnedVertexShader, selectionFragmentShader, true);
    outlineCompositeProgram_ = bgfx::createProgram(outlineVertexShader, outlineFragmentShader, true);
    ssaoDepthProgram_ = bgfx::createProgram(ssaoDepthVertexShader, ssaoDepthFragmentShader, false);
    ssaoDepthInstancedProgram_ = bgfx::createProgram(ssaoDepthInstancedVertexShader, ssaoDepthFragmentShader, false);
    ssaoDepthSkinnedProgram_ = bgfx::createProgram(ssaoDepthSkinnedVertexShader, ssaoDepthFragmentShader, true);
    ssaoProgram_ = bgfx::createProgram(ssaoFullscreenVertexShader, ssaoFragmentShader, false);
    ssaoBlurProgram_ = bgfx::createProgram(ssaoFullscreenVertexShader, ssaoBlurFragmentShader, false);
    ssaoCompositeProgram_ = bgfx::createProgram(ssaoFullscreenVertexShader, ssaoCompositeFragmentShader, false);
    decalProgram_ = bgfx::createProgram(ssaoFullscreenVertexShader, decalFragmentShader, false);
    scenePresentProgram_ = bgfx::createProgram(ssaoFullscreenVertexShader, scenePresentFragmentShader, false);
    colorGradingProgram_ = bgfx::createProgram(ssaoFullscreenVertexShader, colorGradingFragmentShader, true);
    particleProgram_ = bgfx::createProgram(particleVertexShader, particleFragmentShader, true);
    bgfx::destroy(vertexShader);
    bgfx::destroy(instancedVertexShader);
    bgfx::destroy(terrainShadowVertexShader);
    bgfx::destroy(selectionVertexShader);
    bgfx::destroy(selectionInstancedVertexShader);
    bgfx::destroy(ssaoDepthVertexShader);
    bgfx::destroy(ssaoDepthInstancedVertexShader);
    bgfx::destroy(ssaoFragmentShader);
    bgfx::destroy(ssaoBlurFragmentShader);
    bgfx::destroy(ssaoCompositeFragmentShader);
    bgfx::destroy(decalFragmentShader);
    bgfx::destroy(scenePresentFragmentShader);
    if (!bgfx::isValid(forwardProgram_) ||
        !bgfx::isValid(instancedForwardProgram_) ||
        !bgfx::isValid(skinnedForwardProgram_) ||
        !bgfx::isValid(skyProgram_) ||
        !bgfx::isValid(terrainSplatProgram_) ||
        !bgfx::isValid(debugLineProgram_) ||
        !bgfx::isValid(terrainShadowProgram_) ||
        !bgfx::isValid(skinnedShadowProgram_) ||
        !bgfx::isValid(selectionMaskProgram_) ||
        !bgfx::isValid(selectionMaskInstancedProgram_) ||
        !bgfx::isValid(selectionMaskSkinnedProgram_) ||
        !bgfx::isValid(outlineCompositeProgram_) ||
        !bgfx::isValid(ssaoDepthProgram_) ||
        !bgfx::isValid(ssaoDepthInstancedProgram_) ||
        !bgfx::isValid(ssaoDepthSkinnedProgram_) ||
        !bgfx::isValid(ssaoProgram_) ||
        !bgfx::isValid(ssaoBlurProgram_) ||
        !bgfx::isValid(ssaoCompositeProgram_) ||
        !bgfx::isValid(decalProgram_) ||
        !bgfx::isValid(scenePresentProgram_) ||
        !bgfx::isValid(colorGradingProgram_) ||
        !bgfx::isValid(particleProgram_))
    {
        shutdown();
        return RendererResult::BackendFailure;
    }

    fallbackWhiteTexture_ = createSolidTexture(255, 255, 255, 255);
    fallbackSplatTexture_ = createSolidTexture(255, 0, 0, 255);
    fallbackNormalTexture_ = createSolidTexture(128, 128, 255, 255);
    if (!bgfx::isValid(fallbackWhiteTexture_) ||
        !bgfx::isValid(fallbackSplatTexture_) ||
        !bgfx::isValid(fallbackNormalTexture_))
    {
        shutdown();
        return RendererResult::BackendFailure;
    }
#endif

    backbufferWidth_ = desc.backbufferWidth;
    backbufferHeight_ = desc.backbufferHeight;
#if FULL_RENDERER_ENABLE_BGFX
    configureForwardView(backbufferWidth_, backbufferHeight_);
#endif

    initialized_ = true;
    frameInProgress_ = false;
    invalidHandleUseCount_ = 0;
    staleHandleUseCount_ = 0;
    destroyedHandleSubmissionCount_ = 0;
#if FULL_RENDERER_ENABLE_BGFX
    meshAllocationFailureCount_ = 0;
    skinnedMeshAllocationFailureCount_ = 0;
    textureAllocationFailureCount_ = 0;
    selectionMaskResourceRecreateCount_ = 0;
    selectionMaskAllocationFailureCount_ = 0;
    ssaoResourceRecreateCount_ = 0;
    ssaoAllocationFailureCount_ = 0;
#endif
    stats_ = {};
    return RendererResult::Success;
}

void BgfxRenderDevice::shutdown() noexcept
{
    if (initialized_)
    {
        destroyAllResources();

#if FULL_RENDERER_ENABLE_BGFX
        destroyShadowResources();
        destroySelectionMaskResource();
        destroySsaoResource();
        destroySceneTargetResource();
        shadowResourceRecreateCount_ = 0;
        shadowResourceAllocationFailureCount_ = 0;

        if (bgfx::isValid(forwardProgram_))
        {
            bgfx::destroy(forwardProgram_);
            forwardProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(instancedForwardProgram_))
        {
            bgfx::destroy(instancedForwardProgram_);
            instancedForwardProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(skinnedForwardProgram_))
        {
            bgfx::destroy(skinnedForwardProgram_);
            skinnedForwardProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(skyProgram_))
        {
            bgfx::destroy(skyProgram_);
            skyProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(terrainSplatProgram_))
        {
            bgfx::destroy(terrainSplatProgram_);
            terrainSplatProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(debugLineProgram_))
        {
            bgfx::destroy(debugLineProgram_);
            debugLineProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(terrainShadowProgram_))
        {
            bgfx::destroy(terrainShadowProgram_);
            terrainShadowProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(skinnedShadowProgram_))
        {
            bgfx::destroy(skinnedShadowProgram_);
            skinnedShadowProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(selectionMaskProgram_))
        {
            bgfx::destroy(selectionMaskProgram_);
            selectionMaskProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(selectionMaskInstancedProgram_))
        {
            bgfx::destroy(selectionMaskInstancedProgram_);
            selectionMaskInstancedProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(selectionMaskSkinnedProgram_))
        {
            bgfx::destroy(selectionMaskSkinnedProgram_);
            selectionMaskSkinnedProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(outlineCompositeProgram_))
        {
            bgfx::destroy(outlineCompositeProgram_);
            outlineCompositeProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(ssaoDepthProgram_))
        {
            bgfx::destroy(ssaoDepthProgram_);
            ssaoDepthProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(ssaoDepthInstancedProgram_))
        {
            bgfx::destroy(ssaoDepthInstancedProgram_);
            ssaoDepthInstancedProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(ssaoDepthSkinnedProgram_))
        {
            bgfx::destroy(ssaoDepthSkinnedProgram_);
            ssaoDepthSkinnedProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(ssaoProgram_))
        {
            bgfx::destroy(ssaoProgram_);
            ssaoProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(ssaoBlurProgram_))
        {
            bgfx::destroy(ssaoBlurProgram_);
            ssaoBlurProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(ssaoCompositeProgram_))
        {
            bgfx::destroy(ssaoCompositeProgram_);
            ssaoCompositeProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(decalProgram_))
        {
            bgfx::destroy(decalProgram_);
            decalProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(scenePresentProgram_))
        {
            bgfx::destroy(scenePresentProgram_);
            scenePresentProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(colorGradingProgram_))
        {
            bgfx::destroy(colorGradingProgram_);
            colorGradingProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(particleProgram_))
        {
            bgfx::destroy(particleProgram_);
            particleProgram_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(fallbackSplatTexture_))
        {
            bgfx::destroy(fallbackSplatTexture_);
            fallbackSplatTexture_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(fallbackNormalTexture_))
        {
            bgfx::destroy(fallbackNormalTexture_);
            fallbackNormalTexture_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(fallbackWhiteTexture_))
        {
            bgfx::destroy(fallbackWhiteTexture_);
            fallbackWhiteTexture_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(splatSampler_))
        {
            bgfx::destroy(splatSampler_);
            splatSampler_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(selectionMaskSampler_))
        {
            bgfx::destroy(selectionMaskSampler_);
            selectionMaskSampler_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(ssaoSampler_))
        {
            bgfx::destroy(ssaoSampler_);
            ssaoSampler_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(ssaoDepthSampler_))
        {
            bgfx::destroy(ssaoDepthSampler_);
            ssaoDepthSampler_ = BGFX_INVALID_HANDLE;
        }

        for (bgfx::UniformHandle& sampler : shadowMapSamplers_)
        {
            if (bgfx::isValid(sampler))
            {
                bgfx::destroy(sampler);
                sampler = BGFX_INVALID_HANDLE;
            }
        }

        for (bgfx::UniformHandle& sampler : layerSamplers_)
        {
            if (bgfx::isValid(sampler))
            {
                bgfx::destroy(sampler);
                sampler = BGFX_INVALID_HANDLE;
            }
        }

        for (bgfx::UniformHandle& sampler : normalSamplers_)
        {
            if (bgfx::isValid(sampler))
            {
                bgfx::destroy(sampler);
                sampler = BGFX_INVALID_HANDLE;
            }
        }

        if (bgfx::isValid(terrainParamsUniform_))
        {
            bgfx::destroy(terrainParamsUniform_);
            terrainParamsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(shadowParamsUniform_))
        {
            bgfx::destroy(shadowParamsUniform_);
            shadowParamsUniform_ = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(shadowFilterParamsUniform_))
        {
            bgfx::destroy(shadowFilterParamsUniform_);
            shadowFilterParamsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(cascadeSplitsUniform_))
        {
            bgfx::destroy(cascadeSplitsUniform_);
            cascadeSplitsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(fogParamsUniform_))
        {
            bgfx::destroy(fogParamsUniform_);
            fogParamsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(weatherPrecipitationParamsUniform_))
        {
            bgfx::destroy(weatherPrecipitationParamsUniform_);
            weatherPrecipitationParamsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(weatherWindParamsUniform_))
        {
            bgfx::destroy(weatherWindParamsUniform_);
            weatherWindParamsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(weatherParamsUniform_))
        {
            bgfx::destroy(weatherParamsUniform_);
            weatherParamsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(fadeParamsUniform_))
        {
            bgfx::destroy(fadeParamsUniform_);
            fadeParamsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(skinningPaletteUniform_))
        {
            bgfx::destroy(skinningPaletteUniform_);
            skinningPaletteUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(outlineParamsUniform_))
        {
            bgfx::destroy(outlineParamsUniform_);
            outlineParamsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(outlineColorUniform_))
        {
            bgfx::destroy(outlineColorUniform_);
            outlineColorUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(ssaoParamsUniform_))
        {
            bgfx::destroy(ssaoParamsUniform_);
            ssaoParamsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(ssaoDepthParamsUniform_))
        {
            bgfx::destroy(ssaoDepthParamsUniform_);
            ssaoDepthParamsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(sceneColorSampler_))
        {
            bgfx::destroy(sceneColorSampler_);
            sceneColorSampler_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(colorGradingParamsUniform_))
        {
            bgfx::destroy(colorGradingParamsUniform_);
            colorGradingParamsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(colorGradingControlsUniform_))
        {
            bgfx::destroy(colorGradingControlsUniform_);
            colorGradingControlsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(colorGradingLiftUniform_))
        {
            bgfx::destroy(colorGradingLiftUniform_);
            colorGradingLiftUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(colorGradingGainUniform_))
        {
            bgfx::destroy(colorGradingGainUniform_);
            colorGradingGainUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(particleSampler_))
        {
            bgfx::destroy(particleSampler_);
            particleSampler_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(particleDepthSampler_))
        {
            bgfx::destroy(particleDepthSampler_);
            particleDepthSampler_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(particleParamsUniform_))
        {
            bgfx::destroy(particleParamsUniform_);
            particleParamsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(decalAlbedoSampler_))
        {
            bgfx::destroy(decalAlbedoSampler_);
            decalAlbedoSampler_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(decalDepthSampler_))
        {
            bgfx::destroy(decalDepthSampler_);
            decalDepthSampler_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(decalDepthParamsUniform_))
        {
            bgfx::destroy(decalDepthParamsUniform_);
            decalDepthParamsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(decalColorOpacityUniform_))
        {
            bgfx::destroy(decalColorOpacityUniform_);
            decalColorOpacityUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(decalWorldToLocalUniform_))
        {
            bgfx::destroy(decalWorldToLocalUniform_);
            decalWorldToLocalUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(decalInvProjUniform_))
        {
            bgfx::destroy(decalInvProjUniform_);
            decalInvProjUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(decalInvViewUniform_))
        {
            bgfx::destroy(decalInvViewUniform_);
            decalInvViewUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(environmentColorsUniform_))
        {
            bgfx::destroy(environmentColorsUniform_);
            environmentColorsUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(shadowViewProjUniform_))
        {
            bgfx::destroy(shadowViewProjUniform_);
            shadowViewProjUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(terrainLayerColorUniform_))
        {
            bgfx::destroy(terrainLayerColorUniform_);
            terrainLayerColorUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(materialColorUniform_))
        {
            bgfx::destroy(materialColorUniform_);
            materialColorUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(basicBaseColorSampler_))
        {
            bgfx::destroy(basicBaseColorSampler_);
            basicBaseColorSampler_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(basicNormalSampler_))
        {
            bgfx::destroy(basicNormalSampler_);
            basicNormalSampler_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(lightColorUniform_))
        {
            bgfx::destroy(lightColorUniform_);
            lightColorUniform_ = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(lightDirIntensityUniform_))
        {
            bgfx::destroy(lightDirIntensityUniform_);
            lightDirIntensityUniform_ = BGFX_INVALID_HANDLE;
        }

        bgfx::shutdown();
#endif
    }

    frameInProgress_ = false;
    initialized_ = false;
    backbufferWidth_ = 0;
    backbufferHeight_ = 0;
    shaderBinaryDirectory_.clear();
    invalidHandleUseCount_ = 0;
    staleHandleUseCount_ = 0;
    destroyedHandleSubmissionCount_ = 0;
#if FULL_RENDERER_ENABLE_BGFX
    meshAllocationFailureCount_ = 0;
    skinnedMeshAllocationFailureCount_ = 0;
    textureAllocationFailureCount_ = 0;
    selectionMaskResourceRecreateCount_ = 0;
    selectionMaskAllocationFailureCount_ = 0;
    ssaoResourceRecreateCount_ = 0;
    ssaoAllocationFailureCount_ = 0;
    sceneTargetResourceRecreateCount_ = 0;
    sceneTargetResourceAllocationFailureCount_ = 0;
#endif
    stats_ = {};
}

RendererResult BgfxRenderDevice::resize(const RendererResizeDesc& desc)
{
    if (!initialized_)
    {
        return RendererResult::NotInitialized;
    }

    if (frameInProgress_)
    {
        return RendererResult::FrameAlreadyInProgress;
    }

    if (!hasValidDimensions(desc.backbufferWidth, desc.backbufferHeight))
    {
        return RendererResult::InvalidDescriptor;
    }

#if FULL_RENDERER_ENABLE_BGFX
    bgfx::reset(desc.backbufferWidth, desc.backbufferHeight, kResetFlags);
    destroySelectionMaskResource();
    destroySsaoResource();
    destroySceneTargetResource();
#endif
    backbufferWidth_ = desc.backbufferWidth;
    backbufferHeight_ = desc.backbufferHeight;
#if FULL_RENDERER_ENABLE_BGFX
    configureForwardView(backbufferWidth_, backbufferHeight_);
#endif
    return RendererResult::Success;
}

MeshHandle BgfxRenderDevice::createMesh(const MeshDesc& desc)
{
    if (!initialized_)
    {
        return {};
    }

    MeshResource mesh;
#if FULL_RENDERER_ENABLE_BGFX
    const std::uint32_t vertexBytes = static_cast<std::uint32_t>(sizeof(MeshVertex) * desc.vertexCount);
    const std::uint32_t indexBytes = static_cast<std::uint32_t>(sizeof(std::uint16_t) * desc.indexCount);
    mesh.estimatedBytes = static_cast<std::uint64_t>(vertexBytes) + static_cast<std::uint64_t>(indexBytes);

    mesh.vertexBuffer = bgfx::createVertexBuffer(bgfx::copy(desc.vertices, vertexBytes), meshVertexLayout_);
    mesh.indexBuffer = bgfx::createIndexBuffer(bgfx::copy(desc.indices, indexBytes));
    if (!bgfx::isValid(mesh.vertexBuffer) || !bgfx::isValid(mesh.indexBuffer))
    {
        if (bgfx::isValid(mesh.indexBuffer))
        {
            bgfx::destroy(mesh.indexBuffer);
        }
        if (bgfx::isValid(mesh.vertexBuffer))
        {
            bgfx::destroy(mesh.vertexBuffer);
        }
        ++meshAllocationFailureCount_;
        updateLiveResourceStats();
        return {};
    }
#else
    mesh.estimatedBytes =
        resources::estimateBufferBytes(desc.vertexCount, static_cast<std::uint32_t>(sizeof(MeshVertex))) +
        resources::estimateBufferBytes(desc.indexCount, static_cast<std::uint32_t>(sizeof(std::uint16_t)));
#endif

    mesh.active = true;
    meshes_.push_back(mesh);
    updateLiveResourceStats();
    return MeshHandle{static_cast<std::uint32_t>(meshes_.size())};
}

void BgfxRenderDevice::destroyMesh(const MeshHandle handle) noexcept
{
    const ResourceHandleState state = classifyMeshHandle(handle);
    if (state != ResourceHandleState::Live)
    {
        recordHandleUse(state, false);
        return;
    }

    MeshResource* mesh = findMesh(handle);
#if FULL_RENDERER_ENABLE_BGFX
    if (bgfx::isValid(mesh->indexBuffer))
    {
        bgfx::destroy(mesh->indexBuffer);
        mesh->indexBuffer = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(mesh->vertexBuffer))
    {
        bgfx::destroy(mesh->vertexBuffer);
        mesh->vertexBuffer = BGFX_INVALID_HANDLE;
    }
#endif

    mesh->active = false;
    mesh->estimatedBytes = 0;
    updateLiveResourceStats();
}

SkinnedMeshHandle BgfxRenderDevice::createSkinnedMesh(const SkinnedMeshDesc& desc)
{
    if (!initialized_)
    {
        return {};
    }

    SkinnedMeshResource mesh;
#if FULL_RENDERER_ENABLE_BGFX
    const std::uint32_t vertexBytes = static_cast<std::uint32_t>(sizeof(SkinnedMeshVertex) * desc.vertexCount);
    const std::uint32_t indexBytes = static_cast<std::uint32_t>(sizeof(std::uint16_t) * desc.indexCount);
    mesh.estimatedBytes = static_cast<std::uint64_t>(vertexBytes) + static_cast<std::uint64_t>(indexBytes);

    mesh.vertexBuffer = bgfx::createVertexBuffer(bgfx::copy(desc.vertices, vertexBytes), skinnedMeshVertexLayout_);
    mesh.indexBuffer = bgfx::createIndexBuffer(bgfx::copy(desc.indices, indexBytes));
    if (!bgfx::isValid(mesh.vertexBuffer) || !bgfx::isValid(mesh.indexBuffer))
    {
        if (bgfx::isValid(mesh.indexBuffer))
        {
            bgfx::destroy(mesh.indexBuffer);
        }
        if (bgfx::isValid(mesh.vertexBuffer))
        {
            bgfx::destroy(mesh.vertexBuffer);
        }
        ++skinnedMeshAllocationFailureCount_;
        updateLiveResourceStats();
        return {};
    }
#else
    mesh.estimatedBytes =
        resources::estimateBufferBytes(desc.vertexCount, static_cast<std::uint32_t>(sizeof(SkinnedMeshVertex))) +
        resources::estimateBufferBytes(desc.indexCount, static_cast<std::uint32_t>(sizeof(std::uint16_t)));
#endif

    if (desc.sectionCount == 0)
    {
        SkinnedMeshSectionDesc section;
        section.firstIndex = 0;
        section.indexCount = desc.indexCount;
        mesh.sections.push_back(section);
    }
    else
    {
        mesh.sections.assign(desc.sections, desc.sections + desc.sectionCount);
    }
    mesh.active = true;
    skinnedMeshes_.push_back(mesh);
    updateLiveResourceStats();
    return SkinnedMeshHandle{static_cast<std::uint32_t>(skinnedMeshes_.size())};
}

void BgfxRenderDevice::destroySkinnedMesh(const SkinnedMeshHandle handle) noexcept
{
    const ResourceHandleState state = classifySkinnedMeshHandle(handle);
    if (state != ResourceHandleState::Live)
    {
        recordHandleUse(state, false);
        return;
    }

    SkinnedMeshResource* mesh = findSkinnedMesh(handle);
#if FULL_RENDERER_ENABLE_BGFX
    if (bgfx::isValid(mesh->indexBuffer))
    {
        bgfx::destroy(mesh->indexBuffer);
        mesh->indexBuffer = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(mesh->vertexBuffer))
    {
        bgfx::destroy(mesh->vertexBuffer);
        mesh->vertexBuffer = BGFX_INVALID_HANDLE;
    }
#endif

    mesh->active = false;
    mesh->estimatedBytes = 0;
    mesh->sections.clear();
    updateLiveResourceStats();
}

TextureHandle BgfxRenderDevice::createTexture(const TextureDesc& desc)
{
    if (!initialized_)
    {
        return {};
    }

    TextureResource texture;
    texture.semantic = desc.semantic;
    texture.colorSpace = desc.colorSpace;
    texture.mipCount = desc.mipCount;
    texture.compressed = desc.compressed;
    texture.estimatedBytes = resources::estimateTexture2DBytes(desc.width, desc.height, desc.format);
#if FULL_RENDERER_ENABLE_BGFX
    texture.texture = bgfx::createTexture2D(
        static_cast<std::uint16_t>(desc.width),
        static_cast<std::uint16_t>(desc.height),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        0,
        bgfx::copy(desc.data, desc.dataSizeBytes));
    if (!bgfx::isValid(texture.texture))
    {
        ++textureAllocationFailureCount_;
        updateLiveResourceStats();
        return {};
    }
#endif

    texture.active = true;
    textures_.push_back(texture);
    updateLiveResourceStats();
    return TextureHandle{static_cast<std::uint32_t>(textures_.size())};
}

void BgfxRenderDevice::destroyTexture(const TextureHandle handle) noexcept
{
    const ResourceHandleState state = classifyTextureHandle(handle);
    if (state != ResourceHandleState::Live)
    {
        recordHandleUse(state, false);
        return;
    }

    TextureResource* texture = findTexture(handle);
#if FULL_RENDERER_ENABLE_BGFX
    if (bgfx::isValid(texture->texture))
    {
        bgfx::destroy(texture->texture);
        texture->texture = BGFX_INVALID_HANDLE;
    }
#endif

    texture->active = false;
    texture->semantic = TextureSemantic::Color;
    texture->colorSpace = TextureColorSpace::Srgb;
    texture->mipCount = 1;
    texture->compressed = false;
    texture->estimatedBytes = 0;
    updateLiveResourceStats();
}

MaterialHandle BgfxRenderDevice::createMaterial(const MaterialDesc& desc)
{
    if (!initialized_)
    {
        return {};
    }

    if (desc.kind == MaterialKind::Basic)
    {
        const TextureHandle basicTextures[] = {
            desc.basicTextures.baseColor,
            desc.basicTextures.normal,
            desc.basicTextures.metallicRoughness,
            desc.basicTextures.occlusion,
            desc.basicTextures.emissive};
        for (const TextureHandle texture : basicTextures)
        {
            if (isValid(texture) && resolveTexture(texture, false) == nullptr)
            {
                return {};
            }
        }

        if (isValid(desc.basicTextures.normal))
        {
            const TextureResource* normalTexture = resolveTexture(desc.basicTextures.normal, false);
            if (normalTexture == nullptr ||
                normalTexture->semantic != TextureSemantic::NormalMap ||
                normalTexture->colorSpace != TextureColorSpace::EncodedNormal)
            {
                return {};
            }
        }
    }
    else if (desc.kind == MaterialKind::TerrainSplat)
    {
        for (const TerrainMaterialLayerDesc& layer : desc.terrain.layers)
        {
            if (isValid(layer.albedoTexture) && resolveTexture(layer.albedoTexture, false) == nullptr)
            {
                return {};
            }
            if (isValid(layer.normalTexture) && resolveTexture(layer.normalTexture, false) == nullptr)
            {
                return {};
            }
        }
    }

    MaterialResource material;
    material.desc = desc;
    material.estimatedBytes = static_cast<std::uint64_t>(sizeof(MaterialDesc));
    material.active = true;
    materials_.push_back(material);
    updateLiveResourceStats();
    return MaterialHandle{static_cast<std::uint32_t>(materials_.size())};
}

void BgfxRenderDevice::destroyMaterial(const MaterialHandle handle) noexcept
{
    const ResourceHandleState state = classifyMaterialHandle(handle);
    if (state != ResourceHandleState::Live)
    {
        recordHandleUse(state, false);
        return;
    }

    MaterialResource* material = findMaterial(handle);
    material->active = false;
    material->estimatedBytes = 0;
    updateLiveResourceStats();
}

#if FULL_RENDERER_ENABLE_BGFX
bool BgfxRenderDevice::prepareCsmForwardState(
    const RenderPacket& packet,
    float shadowViewProjMatrices[kMaxDirectionalShadowCascades][16],
    float cascadeSplits[4],
    std::uint32_t& outActiveCascadeCount,
    bool& outShadowsEnabled) const noexcept
{
    outActiveCascadeCount = 0;
    outShadowsEnabled = false;
    std::memset(shadowViewProjMatrices, 0, sizeof(float) * kMaxDirectionalShadowCascades * 16U);
    std::memset(cascadeSplits, 0, sizeof(float) * 4U);

    if (!packet.directionalShadow.enabled)
    {
        return true;
    }

    scene::DirectionalShadowCascadeSet cascadeSet;
    if (scene::clampShadowCascadeCount(packet.directionalShadow.cascadeCount) == 1U)
    {
        scene::DirectionalShadowSplit split;
        if (!scene::buildDirectionalShadowSplit(packet.directionalLight, packet.directionalShadow, split))
        {
            return false;
        }
        cascadeSet.cascadeCount = 1;
        cascadeSet.splits[0] = split;
        cascadeSet.splits[0].splitIndex = 0;
    }
    else if (!scene::buildDirectionalShadowCascadeSet(packet.directionalLight, packet.directionalShadow, packet.view, cascadeSet) ||
        cascadeSet.cascadeCount == 0)
    {
        return false;
    }

    for (std::uint32_t cascadeIndex = 0; cascadeIndex < kMaxDirectionalShadowCascades; ++cascadeIndex)
    {
        const std::uint32_t sourceIndex = std::min(cascadeIndex, cascadeSet.cascadeCount - 1U);
        for (int matrixIndex = 0; matrixIndex < 16; ++matrixIndex)
        {
            shadowViewProjMatrices[cascadeIndex][matrixIndex] =
                cascadeSet.splits[sourceIndex].matrices.viewProjection[matrixIndex];
        }
        cascadeSplits[cascadeIndex] = cascadeSet.splits[sourceIndex].farDistanceMeters;
    }

    outActiveCascadeCount = cascadeSet.cascadeCount;
    outShadowsEnabled = hasValidShadowResources(cascadeSet.cascadeCount);

    return true;
}

void BgfxRenderDevice::bindCsmForwardState(
    const RenderPacket& packet,
    const float shadowViewProjMatrices[kMaxDirectionalShadowCascades][16],
    const float cascadeSplits[4],
    const std::uint32_t activeCascadeCount,
    const bool shadowsEnabled) const noexcept
{
    for (std::uint32_t cascadeIndex = 0; cascadeIndex < kMaxDirectionalShadowCascades; ++cascadeIndex)
    {
        const bool hasCascadeTexture =
            shadowsEnabled &&
            cascadeIndex < shadowCascadeResourceCount_ &&
            bgfx::isValid(shadowCascadeResources_[cascadeIndex].colorTexture);
        bgfx::setTexture(
            static_cast<std::uint8_t>(5U + cascadeIndex),
            shadowMapSamplers_[cascadeIndex],
            hasCascadeTexture ? shadowCascadeResources_[cascadeIndex].colorTexture : fallbackWhiteTexture_);
    }

    const float shadowParams[4] = {
        shadowsEnabled ? static_cast<float>(activeCascadeCount) : 0.0f,
        scene::clampShadowDepthBias(packet.directionalShadow.depthBias),
        packet.directionalShadow.strength,
        0.0f};
    const std::uint32_t filterTapCount = scene::shadowFilterModeToTapCount(packet.directionalShadow.filterMode);
    const float filterMode = filterTapCount == 9U ? 2.0f : filterTapCount == 4U ? 1.0f : 0.0f;
    const float shadowFilterParams[4] = {
        shadowsEnabled && packet.directionalShadow.cascadeBlendEnabled ?
            scene::clampCascadeBlendFraction(packet.directionalShadow.cascadeBlendFraction) :
            0.0f,
        scene::clampShadowSlopeBias(packet.directionalShadow.slopeBias),
        filterMode,
        shadowMapResolution_ > 0 ? 1.0f / static_cast<float>(shadowMapResolution_) : 0.0f};

    bgfx::setUniform(shadowViewProjUniform_, shadowViewProjMatrices, kMaxDirectionalShadowCascades);
    bgfx::setUniform(shadowParamsUniform_, shadowParams);
    bgfx::setUniform(shadowFilterParamsUniform_, shadowFilterParams);
    bgfx::setUniform(cascadeSplitsUniform_, cascadeSplits);
}

void BgfxRenderDevice::bindEnvironmentState(const EnvironmentDesc& environment) const noexcept
{
    const scene::EnvironmentUniformPlan plan = scene::makeEnvironmentUniformPlan(environment);
    bgfx::setUniform(environmentColorsUniform_, plan.colors, 4);
    bgfx::setUniform(fogParamsUniform_, plan.fogParams);
}

void BgfxRenderDevice::bindWeatherState(const scene::WeatherRenderPlan& weatherPlan) const noexcept
{
    bgfx::setUniform(weatherParamsUniform_, weatherPlan.weatherParams);
    bgfx::setUniform(weatherWindParamsUniform_, weatherPlan.windParams);
    bgfx::setUniform(weatherPrecipitationParamsUniform_, weatherPlan.precipitationParams);
}

void BgfxRenderDevice::bindFadeState(
    const scene::FadeRenderState& fadeState,
    const MaterialDesc& material) const noexcept
{
    float params[4] = {};
    setFadeAndMaterialParams(fadeState, material, params);
    bgfx::setUniform(fadeParamsUniform_, params);
}

void BgfxRenderDevice::bindBasicMaterialTextureState(const MaterialDesc& material) noexcept
{
#if FULL_RENDERER_ENABLE_BGFX
    const TextureResource* baseColorTexture = isValid(material.basicTextures.baseColor) ?
        resolveTexture(material.basicTextures.baseColor, true) :
        nullptr;
    if (baseColorTexture == nullptr)
    {
        ++stats_.fallbackMaterialTextureCount;
    }

    bgfx::setTexture(
        0,
        basicBaseColorSampler_,
        baseColorTexture != nullptr ? baseColorTexture->texture : fallbackWhiteTexture_);

    const TextureResource* normalTexture = isValid(material.basicTextures.normal) ?
        resolveTexture(material.basicTextures.normal, true) :
        nullptr;
    if (normalTexture == nullptr)
    {
        ++stats_.fallbackMaterialTextureCount;
    }

    bgfx::setTexture(
        1,
        basicNormalSampler_,
        normalTexture != nullptr ? normalTexture->texture : fallbackNormalTexture_);
#else
    (void)material;
#endif
}

void BgfxRenderDevice::applyWeatherStats(const scene::WeatherRenderPlan& weatherPlan) noexcept
{
    stats_.weatherEnabled = weatherPlan.weatherEnabled;
    stats_.weatherWindEnabled = weatherPlan.windEnabled;
    stats_.weatherWindDirectionWorld[0] = weatherPlan.windParams[0];
    stats_.weatherWindDirectionWorld[1] = weatherPlan.windParams[1];
    stats_.weatherWindDirectionWorld[2] = weatherPlan.windParams[2];
    stats_.weatherWindSpeedMetersPerSecond = weatherPlan.windParams[3];
    stats_.weatherPrecipitationEnabled = weatherPlan.precipitationEnabled;
    stats_.weatherPrecipitationType = weatherPlan.precipitationType;
    stats_.weatherPrecipitationIntensity = weatherPlan.precipitationIntensity;
    stats_.weatherPrecipitationUsesParticleBatches = weatherPlan.precipitationUsesParticleBatches;
    stats_.weatherWetnessEnabled = weatherPlan.wetnessEnabled;
    stats_.weatherWetnessAmount = weatherPlan.wetnessAmount;
    stats_.weatherFogBlendEnabled = weatherPlan.fogBlendEnabled;
    stats_.weatherFogBlendAmount = weatherPlan.fogBlendAmount;
    stats_.weatherEffectiveFogColorLinear[0] = weatherPlan.environment.fogColorLinear[0];
    stats_.weatherEffectiveFogColorLinear[1] = weatherPlan.environment.fogColorLinear[1];
    stats_.weatherEffectiveFogColorLinear[2] = weatherPlan.environment.fogColorLinear[2];
    stats_.weatherEffectiveFogStartMeters = weatherPlan.environment.fogStartMeters;
    stats_.weatherEffectiveFogEndMeters = weatherPlan.environment.fogEndMeters;
    stats_.weatherClampedValueCount = weatherPlan.clampedValueCount;
    stats_.weatherNeutral = weatherPlan.neutralState;
}

void BgfxRenderDevice::applyColorGradingStats(const scene::ColorGradingRenderPlan& plan) noexcept
{
    stats_.colorGradingEnabled = plan.enabled;
    stats_.colorGradingPassSubmitted = plan.passSubmitted;
    stats_.colorGradingSceneColorTargetValid = plan.sceneColorTargetAvailable ? 1U : 0U;
    stats_.colorGradingTonemapEnabled = plan.tonemapEnabled;
    stats_.colorGradingTonemapOperator = plan.tonemapOperator;
    stats_.colorGradingExposureStops = plan.params[0] > 0.0f ? std::log2(plan.params[0]) : 0.0f;
    stats_.colorGradingContrast = plan.controls[0];
    stats_.colorGradingSaturation = plan.controls[1];
    stats_.colorGradingGamma = plan.controls[2];
    stats_.colorGradingLutEnabled = plan.lutRequested;
    stats_.colorGradingLutActive = plan.lutActive;
    stats_.colorGradingLutSamplingSupported = plan.lutSamplingSupported;
    stats_.colorGradingLutFallbackCount = plan.lutFallback ? 1U : 0U;
    stats_.colorGradingClampedValueCount = plan.clampedValueCount;
    stats_.colorGradingDebugMode = plan.debugMode;
}

scene::PostPassPlan BgfxRenderDevice::makePostPassPlanForPacket(
    const RenderPacket& packet,
    const scene::DecalRenderPlan& decalPlan,
    const scene::ParticleRenderPlan& particlePlan,
    const bool selectedObjects,
    const bool sceneTargetAvailable,
    const bool sceneDepthAvailable) const noexcept
{
    scene::PostPassPlanInput input;
    input.viewportWidth = backbufferWidth_;
    input.viewportHeight = backbufferHeight_;
    input.forceSceneTarget = forceSceneTargetForSubmit_;
    input.ssaoEnabled = packet.ssao.enabled;
    input.ssaoBlurEnabled = packet.ssao.blurEnabled;
    input.decalsEnabled = decalPlan.enabled;
    input.activeDecals = decalPlan.activeCount > 0U;
    input.particlesEnabled = particlePlan.enabled;
    input.acceptedParticles = particlePlan.acceptedParticleCount > 0U;
    input.softParticlesEnabled = particlePlan.softParticlesEnabled && particlePlan.softParticleFadeDistanceMeters > 0.0f;
    input.selectionOutlineEnabled = packet.selectionOutline.enabled;
    input.hasSelectedObjects = selectedObjects;
    input.colorGradingEnabled = colorGradingRequestsSceneTarget(packet);
    input.sceneTargetAvailable = sceneTargetAvailable;
    input.sceneDepthAvailable = sceneDepthAvailable;
    return scene::makePostPassPlan(input);
}

void BgfxRenderDevice::applyPostPassStats(const scene::PostPassPlan& plan) noexcept
{
    const bool validSceneTarget = sceneTargetActiveThisFrame_ && hasValidSceneTargetResource();
    stats_.postViewportWidth = plan.viewportWidth;
    stats_.postViewportHeight = plan.viewportHeight;
    stats_.postSceneTargetRequired = plan.sceneTargetRequired;
    stats_.postSceneTargetActive = validSceneTarget;
    stats_.postSceneTargetReasonMask = plan.sceneTargetReasonMask;
    stats_.postSceneColorTargetValid = validSceneTarget ? 1U : 0U;
    stats_.postSceneDepthTargetValid = validSceneTarget ? 1U : 0U;
    stats_.postSceneColorWidth = validSceneTarget ? sceneTargetResource_.width : 0U;
    stats_.postSceneColorHeight = validSceneTarget ? sceneTargetResource_.height : 0U;
    stats_.postSceneDepthWidth = validSceneTarget ? sceneTargetResource_.width : 0U;
    stats_.postSceneDepthHeight = validSceneTarget ? sceneTargetResource_.height : 0U;
    stats_.postReadableSceneDepthRequired = plan.readableSceneDepthRequired;
    stats_.postFinalPresentSubmitted = plan.finalPresentSubmitted ? 1U : 0U;
    stats_.postPresentMode = static_cast<std::uint32_t>(plan.presentMode);
    stats_.postPassCount = plan.passCount;
    stats_.postFullscreenPassCount = plan.fullscreenPassCount;
    stats_.postSkippedPassCount = plan.skippedPassCount;
    stats_.postSkippedPassReasonMask = plan.skippedPassReasonMask;
    stats_.postInvalidResourceCount = plan.invalidResourceCount;
    stats_.postSceneTargetReconfigured = sceneTargetResourceReconfiguredThisFrame_ ? 1U : 0U;
    stats_.postSceneTargetRecreateCount = sceneTargetResourceRecreateCount_;
    stats_.postSceneTargetAllocationFailureCount = sceneTargetResourceAllocationFailureCount_;
    stats_.postSceneTargetAllocationFailed = sceneTargetResourceAllocationFailedThisFrame_ ? 1U : 0U;
}

RendererResult BgfxRenderDevice::submitSky(const EnvironmentDesc& environment)
{
    if (!environment.skyEnabled)
    {
        return RendererResult::Success;
    }
    if (!bgfx::isValid(skyProgram_))
    {
        return RendererResult::BackendFailure;
    }

    bgfx::TransientVertexBuffer vertices;
    constexpr std::uint32_t kVertexCount = 6;
    if (bgfx::getAvailTransientVertexBuffer(kVertexCount, meshVertexLayout_) < kVertexCount)
    {
        return RendererResult::BackendFailure;
    }
    bgfx::allocTransientVertexBuffer(&vertices, kVertexCount, meshVertexLayout_);

    struct SkyVertex
    {
        float position[3];
        float normal[3];
        float uv0[2];
        float color[4];
        float tangent[4];
    };
    static_assert(sizeof(SkyVertex) == sizeof(MeshVertex), "Sky vertex must match mesh layout.");

    const float positions[kVertexCount][2] = {
        {-1.0f, -1.0f},
        {1.0f, -1.0f},
        {1.0f, 1.0f},
        {-1.0f, -1.0f},
        {1.0f, 1.0f},
        {-1.0f, 1.0f},
    };
    auto* skyVertices = reinterpret_cast<SkyVertex*>(vertices.data);
    for (std::uint32_t index = 0; index < kVertexCount; ++index)
    {
        skyVertices[index] = {
            {positions[index][0], positions[index][1], 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {1.0f, 0.0f, 0.0f, 1.0f}};
    }

    bindEnvironmentState(environment);
    bgfx::setVertexBuffer(0, &vertices);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
    bgfx::submit(kSkyViewId, skyProgram_);
    stats_.skyRendered = true;
    return RendererResult::Success;
}
#endif

RendererResult BgfxRenderDevice::submitSsao(
    const RenderPacket& packet,
    const core::InstancedDrawBatch* batches,
    const std::uint32_t batchCount)
{
    if (batchCount > 0 && batches == nullptr)
    {
        return RendererResult::InvalidArgument;
    }

    if (!packet.ssao.enabled)
    {
        return RendererResult::Success;
    }

    stats_.ssaoEnabled = true;

#if FULL_RENDERER_ENABLE_BGFX
    if (!bgfx::isValid(ssaoDepthProgram_) ||
        !bgfx::isValid(ssaoDepthInstancedProgram_) ||
        !bgfx::isValid(ssaoDepthSkinnedProgram_) ||
        !bgfx::isValid(ssaoProgram_) ||
        !bgfx::isValid(ssaoBlurProgram_) ||
        !bgfx::isValid(ssaoCompositeProgram_) ||
        !bgfx::isValid(ssaoDepthParamsUniform_) ||
        !bgfx::isValid(ssaoParamsUniform_) ||
        !bgfx::isValid(ssaoDepthSampler_) ||
        !bgfx::isValid(ssaoSampler_))
    {
        return RendererResult::BackendFailure;
    }

    const scene::SsaoUniformPlan plan = scene::makeSsaoUniformPlan(packet.ssao);
    scene::SsaoTargetDimensions aoDimensions =
        scene::makeSsaoTargetDimensions(backbufferWidth_, backbufferHeight_, plan.halfResolution);
    if (!ensureSsaoResource(backbufferWidth_, backbufferHeight_, aoDimensions.width, aoDimensions.height))
    {
        aoDimensions = scene::makeSsaoTargetDimensions(backbufferWidth_, backbufferHeight_, false);
    }
    if (!ensureSsaoResource(backbufferWidth_, backbufferHeight_, aoDimensions.width, aoDimensions.height))
    {
        stats_.ssaoDepthTargetValid = 0;
        stats_.ssaoOutputTargetValid = 0;
        stats_.ssaoBlurTargetValid = 0;
        stats_.ssaoInputDepthValid = 0;
        return RendererResult::Success;
    }

    stats_.ssaoDepthTargetValid = 1;
    stats_.ssaoOutputTargetValid = 1;
    stats_.ssaoBlurTargetValid = plan.blurEnabled ? 1U : 0U;
    stats_.ssaoAoWidth = aoDimensions.width;
    stats_.ssaoAoHeight = aoDimensions.height;
    stats_.ssaoHalfResolution = plan.halfResolution &&
        aoDimensions.width != backbufferWidth_ &&
        aoDimensions.height != backbufferHeight_;
    stats_.ssaoBlurEnabled = plan.blurEnabled;
    stats_.ssaoInputDepthValid = 1;

    bgfx::setViewFrameBuffer(kSsaoDepthViewId, ssaoResource_.depthFrameBuffer);
    bgfx::setViewRect(
        kSsaoDepthViewId,
        0,
        0,
        static_cast<std::uint16_t>(backbufferWidth_),
        static_cast<std::uint16_t>(backbufferHeight_));
    bgfx::setViewTransform(kSsaoDepthViewId, packet.view.view, packet.view.projection);

    float projectionNearZ = 0.0f;
    float projectionFarZ = 0.0f;
    extractProjectionDepthRange(packet.view.projection, projectionNearZ, projectionFarZ);
    const float depthParams[4] = {plan.inverseMaxDistanceMeters, projectionNearZ, projectionFarZ, 0.0f};
    constexpr std::uint64_t kDepthCaptureState =
        BGFX_STATE_WRITE_RGB |
        BGFX_STATE_WRITE_A |
        BGFX_STATE_WRITE_Z |
        BGFX_STATE_DEPTH_TEST_LESS |
        BGFX_STATE_MSAA;

    for (std::uint32_t index = 0; index < packet.drawItemCount; ++index)
    {
        const DrawItem& draw = packet.drawItems[index];
        const MeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveMeshMaterial(draw.mesh, draw.material, mesh, material, true))
        {
            return RendererResult::InvalidArgument;
        }

        bgfx::setUniform(ssaoDepthParamsUniform_, depthParams);
        bgfx::setTransform(draw.model);
        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        bgfx::setIndexBuffer(mesh->indexBuffer);
        bgfx::setState(kDepthCaptureState);
        bgfx::submit(kSsaoDepthViewId, ssaoDepthProgram_);
        ++stats_.ssaoDepthPassDraws;
    }

    for (std::uint32_t index = 0; index < packet.animatedDrawCount; ++index)
    {
        const AnimatedDrawItem& draw = packet.animatedDraws[index];
        const SkinnedMeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveSkinnedMeshMaterial(draw.mesh, draw.material, mesh, material, true) ||
            draw.palette.skinningMatrices == nullptr ||
            draw.palette.matrixCount == 0 ||
            draw.palette.matrixCount > kMaxSkinningJoints)
        {
            return RendererResult::InvalidArgument;
        }

        float palette[kMaxSkinningJoints][16] = {};
        for (std::uint32_t matrixIndex = 0; matrixIndex < draw.palette.matrixCount; ++matrixIndex)
        {
            std::memcpy(
                palette[matrixIndex],
                draw.palette.skinningMatrices + matrixIndex * 16U,
                sizeof(float) * 16U);
        }

        bgfx::setUniform(skinningPaletteUniform_, palette, kMaxSkinningJoints);
        bgfx::setUniform(ssaoDepthParamsUniform_, depthParams);
        bgfx::setTransform(draw.model);
        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        const SkinnedMeshSectionDesc& section = mesh->sections[draw.sectionIndex];
        bgfx::setIndexBuffer(mesh->indexBuffer, section.firstIndex, section.indexCount);
        bgfx::setState(kDepthCaptureState);
        bgfx::submit(kSsaoDepthViewId, ssaoDepthSkinnedProgram_);
        ++stats_.ssaoDepthPassDraws;
    }

    for (std::uint32_t batchIndex = 0; batchIndex < batchCount; ++batchIndex)
    {
        const core::InstancedDrawBatch& batch = batches[batchIndex];
        const MeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveMeshMaterial(batch.mesh, batch.material, mesh, material, true) ||
            batch.modelMatrices == nullptr ||
            batch.instanceCount == 0)
        {
            return RendererResult::InvalidArgument;
        }

        constexpr std::uint16_t kInstanceStride = sizeof(float) * 16U;
        const std::uint32_t availableInstances = bgfx::getAvailInstanceDataBuffer(batch.instanceCount, kInstanceStride);
        if (availableInstances < batch.instanceCount)
        {
            return RendererResult::BackendFailure;
        }

        bgfx::InstanceDataBuffer instanceData;
        bgfx::allocInstanceDataBuffer(&instanceData, batch.instanceCount, kInstanceStride);
        for (std::uint32_t instanceIndex = 0; instanceIndex < batch.instanceCount; ++instanceIndex)
        {
            const float* source = batch.modelMatrices + instanceIndex * 16U;
            float* destination = reinterpret_cast<float*>(instanceData.data + instanceIndex * kInstanceStride);
            for (std::uint32_t matrixIndex = 0; matrixIndex < 16U; ++matrixIndex)
            {
                destination[matrixIndex] = source[matrixIndex];
            }
        }

        bgfx::setUniform(ssaoDepthParamsUniform_, depthParams);
        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        bgfx::setIndexBuffer(mesh->indexBuffer);
        bgfx::setInstanceDataBuffer(&instanceData);
        bgfx::setState(kDepthCaptureState);
        bgfx::submit(kSsaoDepthViewId, ssaoDepthInstancedProgram_);
        ++stats_.ssaoDepthPassDraws;
    }

    bgfx::TransientVertexBuffer vertices;
    constexpr std::uint32_t kVertexCount = 6;
    if (bgfx::getAvailTransientVertexBuffer(kVertexCount, meshVertexLayout_) < kVertexCount)
    {
        return RendererResult::BackendFailure;
    }
    bgfx::allocTransientVertexBuffer(&vertices, kVertexCount, meshVertexLayout_);

    struct FullscreenVertex
    {
        float position[3];
        float normal[3];
        float uv0[2];
        float color[4];
        float tangent[4];
    };
    static_assert(sizeof(FullscreenVertex) == sizeof(MeshVertex), "Fullscreen vertex must match mesh layout.");

    const float positions[kVertexCount][2] = {
        {-1.0f, -1.0f},
        {1.0f, -1.0f},
        {1.0f, 1.0f},
        {-1.0f, -1.0f},
        {1.0f, 1.0f},
        {-1.0f, 1.0f},
    };
    auto* quadVertices = reinterpret_cast<FullscreenVertex*>(vertices.data);
    for (std::uint32_t index = 0; index < kVertexCount; ++index)
    {
        quadVertices[index] = {
            {positions[index][0], positions[index][1], 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {1.0f, 0.0f, 0.0f, 1.0f}};
    }

    const float texelX = aoDimensions.width > 0 ? 1.0f / static_cast<float>(aoDimensions.width) : 0.0f;
    const float texelY = aoDimensions.height > 0 ? 1.0f / static_cast<float>(aoDimensions.height) : 0.0f;
    const float resolutionScale = backbufferWidth_ > 0 ?
        static_cast<float>(aoDimensions.width) / static_cast<float>(backbufferWidth_) :
        1.0f;
    const float aoRadiusPixels = std::max(1.0f, plan.radiusPixels * resolutionScale);
    const float ssaoParams[4] = {texelX, texelY, aoRadiusPixels, plan.intensity};
    const float sampleMode = plan.sampleCount >= 8U ? 1.0f : 0.0f;
    const float aoParams[4] = {plan.biasNormalized, plan.power, sampleMode, 0.0f};

    bgfx::setViewFrameBuffer(kSsaoViewId, ssaoResource_.aoFrameBuffer);
    bgfx::setViewRect(
        kSsaoViewId,
        0,
        0,
        static_cast<std::uint16_t>(aoDimensions.width),
        static_cast<std::uint16_t>(aoDimensions.height));
    bgfx::setVertexBuffer(0, &vertices);
    bgfx::setTexture(13, ssaoDepthSampler_, ssaoResource_.depthColorTexture);
    bgfx::setUniform(ssaoParamsUniform_, ssaoParams);
    bgfx::setUniform(ssaoDepthParamsUniform_, aoParams);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
    bgfx::submit(kSsaoViewId, ssaoProgram_);
    ++stats_.ssaoPassDraws;

    bgfx::TextureHandle compositeSource = ssaoResource_.aoTexture;
    if (plan.blurEnabled)
    {
        if (bgfx::getAvailTransientVertexBuffer(kVertexCount, meshVertexLayout_) < kVertexCount)
        {
            return RendererResult::BackendFailure;
        }
        bgfx::allocTransientVertexBuffer(&vertices, kVertexCount, meshVertexLayout_);
        quadVertices = reinterpret_cast<FullscreenVertex*>(vertices.data);
        for (std::uint32_t index = 0; index < kVertexCount; ++index)
        {
            quadVertices[index] = {
                {positions[index][0], positions[index][1], 0.0f},
                {0.0f, 1.0f, 0.0f},
                {0.0f, 0.0f},
                {1.0f, 1.0f, 1.0f, 1.0f},
                {1.0f, 0.0f, 0.0f, 1.0f}};
        }

        const float horizontalBlurParams[4] = {texelX, texelY, plan.blurRadiusPixels, 0.0f};
        bgfx::setViewFrameBuffer(kSsaoBlurHorizontalViewId, ssaoResource_.blurTempFrameBuffer);
        bgfx::setViewRect(
            kSsaoBlurHorizontalViewId,
            0,
            0,
            static_cast<std::uint16_t>(aoDimensions.width),
            static_cast<std::uint16_t>(aoDimensions.height));
        bgfx::setVertexBuffer(0, &vertices);
        bgfx::setTexture(13, ssaoSampler_, ssaoResource_.aoTexture);
        bgfx::setUniform(ssaoParamsUniform_, horizontalBlurParams);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
        bgfx::submit(kSsaoBlurHorizontalViewId, ssaoBlurProgram_);
        ++stats_.ssaoBlurPassDraws;

        if (bgfx::getAvailTransientVertexBuffer(kVertexCount, meshVertexLayout_) < kVertexCount)
        {
            return RendererResult::BackendFailure;
        }
        bgfx::allocTransientVertexBuffer(&vertices, kVertexCount, meshVertexLayout_);
        quadVertices = reinterpret_cast<FullscreenVertex*>(vertices.data);
        for (std::uint32_t index = 0; index < kVertexCount; ++index)
        {
            quadVertices[index] = {
                {positions[index][0], positions[index][1], 0.0f},
                {0.0f, 1.0f, 0.0f},
                {0.0f, 0.0f},
                {1.0f, 1.0f, 1.0f, 1.0f},
                {1.0f, 0.0f, 0.0f, 1.0f}};
        }

        const float verticalBlurParams[4] = {texelX, texelY, plan.blurRadiusPixels, 1.0f};
        bgfx::setViewFrameBuffer(kSsaoBlurVerticalViewId, ssaoResource_.blurredFrameBuffer);
        bgfx::setViewRect(
            kSsaoBlurVerticalViewId,
            0,
            0,
            static_cast<std::uint16_t>(aoDimensions.width),
            static_cast<std::uint16_t>(aoDimensions.height));
        bgfx::setVertexBuffer(0, &vertices);
        bgfx::setTexture(13, ssaoSampler_, ssaoResource_.blurTempTexture);
        bgfx::setUniform(ssaoParamsUniform_, verticalBlurParams);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
        bgfx::submit(kSsaoBlurVerticalViewId, ssaoBlurProgram_);
        ++stats_.ssaoBlurPassDraws;
        compositeSource = ssaoResource_.blurredTexture;
    }

    if (bgfx::getAvailTransientVertexBuffer(kVertexCount, meshVertexLayout_) < kVertexCount)
    {
        return RendererResult::BackendFailure;
    }
    bgfx::allocTransientVertexBuffer(&vertices, kVertexCount, meshVertexLayout_);
    quadVertices = reinterpret_cast<FullscreenVertex*>(vertices.data);
    for (std::uint32_t index = 0; index < kVertexCount; ++index)
    {
        quadVertices[index] = {
            {positions[index][0], positions[index][1], 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {1.0f, 0.0f, 0.0f, 1.0f}};
    }

    const float compositeParams[4] = {0.0f, 0.0f, 0.0f, plan.debugVisualize ? 1.0f : 0.0f};
    const bgfx::FrameBufferHandle ssaoCompositeFrameBuffer =
        sceneTargetActiveThisFrame_ && hasValidSceneTargetResource() ?
            sceneTargetResource_.frameBuffer :
            bgfx::FrameBufferHandle{bgfx::kInvalidHandle};
    bgfx::setViewFrameBuffer(
        kSsaoCompositeViewId,
        ssaoCompositeFrameBuffer);
    bgfx::setViewRect(
        kSsaoCompositeViewId,
        0,
        0,
        static_cast<std::uint16_t>(backbufferWidth_),
        static_cast<std::uint16_t>(backbufferHeight_));
    bgfx::setVertexBuffer(0, &vertices);
    bgfx::setTexture(13, ssaoSampler_, compositeSource);
    bgfx::setUniform(ssaoDepthParamsUniform_, compositeParams);
    bgfx::setState(
        BGFX_STATE_WRITE_RGB |
        BGFX_STATE_WRITE_A |
        BGFX_STATE_BLEND_ALPHA |
        BGFX_STATE_MSAA);
    bgfx::submit(kSsaoCompositeViewId, ssaoCompositeProgram_);
    ++stats_.ssaoCompositeDraws;
    stats_.ssaoDebugVisualized = plan.debugVisualize;
#else
    stats_.ssaoDepthTargetValid = 1;
    stats_.ssaoOutputTargetValid = 1;
    stats_.ssaoInputDepthValid = 1;
    stats_.ssaoDepthPassDraws = packet.drawItemCount + packet.animatedDrawCount + batchCount;
    stats_.ssaoPassDraws = 1;
    stats_.ssaoBlurPassDraws = packet.ssao.blurEnabled ? 2 : 0;
    stats_.ssaoCompositeDraws = 1;
    stats_.ssaoBlurTargetValid = packet.ssao.blurEnabled ? 1U : 0U;
    stats_.ssaoAoWidth = backbufferWidth_;
    stats_.ssaoAoHeight = backbufferHeight_;
    stats_.ssaoHalfResolution = packet.ssao.halfResolution;
    stats_.ssaoBlurEnabled = packet.ssao.blurEnabled;
    stats_.ssaoDebugVisualized = packet.ssao.debugVisualize;
#endif
    return RendererResult::Success;
}

RendererResult BgfxRenderDevice::submitSceneDepthCapture(
    const RenderPacket& packet,
    const core::InstancedDrawBatch* batches,
    const std::uint32_t batchCount)
{
#if FULL_RENDERER_ENABLE_BGFX
    if (sceneDepthCapturedThisFrame_)
    {
        return RendererResult::Success;
    }
    if (!sceneTargetActiveThisFrame_ || !hasValidSceneTargetResource())
    {
        return RendererResult::Success;
    }
    if (!bgfx::isValid(ssaoDepthProgram_) ||
        !bgfx::isValid(ssaoDepthInstancedProgram_) ||
        !bgfx::isValid(ssaoDepthSkinnedProgram_) ||
        !bgfx::isValid(ssaoDepthParamsUniform_))
    {
        return RendererResult::BackendFailure;
    }

    bgfx::setViewFrameBuffer(kDecalDepthViewId, sceneTargetResource_.depthCaptureFrameBuffer);
    bgfx::setViewRect(
        kDecalDepthViewId,
        0,
        0,
        static_cast<std::uint16_t>(backbufferWidth_),
        static_cast<std::uint16_t>(backbufferHeight_));
    bgfx::setViewTransform(kDecalDepthViewId, packet.view.view, packet.view.projection);

    constexpr float kSceneDepthMaxDistanceMeters = 1000.0f;
    float projectionNearZ = 0.0f;
    float projectionFarZ = 0.0f;
    extractProjectionDepthRange(packet.view.projection, projectionNearZ, projectionFarZ);
    const float depthParams[4] = {
        1.0f / kSceneDepthMaxDistanceMeters,
        projectionNearZ,
        projectionFarZ,
        0.0f};
    constexpr std::uint64_t kDepthCaptureState =
        BGFX_STATE_WRITE_RGB |
        BGFX_STATE_WRITE_A |
        BGFX_STATE_WRITE_Z |
        BGFX_STATE_DEPTH_TEST_LESS |
        BGFX_STATE_MSAA;

    for (std::uint32_t index = 0; index < packet.drawItemCount; ++index)
    {
        const DrawItem& draw = packet.drawItems[index];
        const MeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveMeshMaterial(draw.mesh, draw.material, mesh, material, true))
        {
            return RendererResult::InvalidArgument;
        }

        bgfx::setUniform(ssaoDepthParamsUniform_, depthParams);
        bgfx::setTransform(draw.model);
        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        bgfx::setIndexBuffer(mesh->indexBuffer);
        bgfx::setState(kDepthCaptureState);
        bgfx::submit(kDecalDepthViewId, ssaoDepthProgram_);
    }

    for (std::uint32_t index = 0; index < packet.animatedDrawCount; ++index)
    {
        const AnimatedDrawItem& draw = packet.animatedDraws[index];
        const SkinnedMeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveSkinnedMeshMaterial(draw.mesh, draw.material, mesh, material, true) ||
            draw.palette.skinningMatrices == nullptr ||
            draw.palette.matrixCount == 0 ||
            draw.palette.matrixCount > kMaxSkinningJoints)
        {
            return RendererResult::InvalidArgument;
        }

        float palette[kMaxSkinningJoints][16] = {};
        for (std::uint32_t matrixIndex = 0; matrixIndex < draw.palette.matrixCount; ++matrixIndex)
        {
            std::memcpy(
                palette[matrixIndex],
                draw.palette.skinningMatrices + matrixIndex * 16U,
                sizeof(float) * 16U);
        }

        bgfx::setUniform(skinningPaletteUniform_, palette, kMaxSkinningJoints);
        bgfx::setUniform(ssaoDepthParamsUniform_, depthParams);
        bgfx::setTransform(draw.model);
        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        const SkinnedMeshSectionDesc& section = mesh->sections[draw.sectionIndex];
        bgfx::setIndexBuffer(mesh->indexBuffer, section.firstIndex, section.indexCount);
        bgfx::setState(kDepthCaptureState);
        bgfx::submit(kDecalDepthViewId, ssaoDepthSkinnedProgram_);
    }

    for (std::uint32_t batchIndex = 0; batchIndex < batchCount; ++batchIndex)
    {
        const core::InstancedDrawBatch& batch = batches[batchIndex];
        const MeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveMeshMaterial(batch.mesh, batch.material, mesh, material, true) ||
            batch.modelMatrices == nullptr ||
            batch.instanceCount == 0)
        {
            return RendererResult::InvalidArgument;
        }

        constexpr std::uint16_t kInstanceStride = sizeof(float) * 16U;
        const std::uint32_t availableInstances = bgfx::getAvailInstanceDataBuffer(batch.instanceCount, kInstanceStride);
        if (availableInstances < batch.instanceCount)
        {
            return RendererResult::BackendFailure;
        }

        bgfx::InstanceDataBuffer instanceData;
        bgfx::allocInstanceDataBuffer(&instanceData, batch.instanceCount, kInstanceStride);
        for (std::uint32_t instanceIndex = 0; instanceIndex < batch.instanceCount; ++instanceIndex)
        {
            const float* source = batch.modelMatrices + instanceIndex * 16U;
            float* destination = reinterpret_cast<float*>(instanceData.data + instanceIndex * kInstanceStride);
            for (std::uint32_t matrixIndex = 0; matrixIndex < 16U; ++matrixIndex)
            {
                destination[matrixIndex] = source[matrixIndex];
            }
        }

        bgfx::setUniform(ssaoDepthParamsUniform_, depthParams);
        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        bgfx::setIndexBuffer(mesh->indexBuffer);
        bgfx::setInstanceDataBuffer(&instanceData);
        bgfx::setState(kDepthCaptureState);
        bgfx::submit(kDecalDepthViewId, ssaoDepthInstancedProgram_);
    }

    sceneDepthCapturedThisFrame_ = true;
#else
    (void)packet;
    (void)batches;
    (void)batchCount;
#endif
    return RendererResult::Success;
}

RendererResult BgfxRenderDevice::submitDecals(
    const RenderPacket& packet,
    const core::InstancedDrawBatch* batches,
    const std::uint32_t batchCount)
{
    if (batchCount > 0 && batches == nullptr)
    {
        return RendererResult::InvalidArgument;
    }

    const scene::DecalRenderPlan plan = buildCameraCulledDecalPlan(packet);
    if (!plan.enabled)
    {
        return RendererResult::Success;
    }

    stats_.decalsEnabled = true;
    stats_.decalSubmittedCount = plan.submittedCount;
    stats_.decalActiveCount = plan.activeCount;
    stats_.decalCulledCount = plan.culledCount;
    stats_.decalRejectedCount = plan.rejectedCount;
    stats_.decalInvalidDescriptorRejectCount = plan.invalidDescriptorRejectCount;
    stats_.decalMaxCountRejectedCount = plan.maxCountRejectedCount;
    stats_.decalFallbackColorCount = plan.fallbackColorCount;
    stats_.decalFrustumCullingEnabled = plan.frustumCullingEnabled;
    stats_.decalMaxProjectionDepthMeters = plan.maxProjectionDepthMeters;
    stats_.decalProjectionEdgeFadeMeters = plan.projectionEdgeFadeMeters;
    stats_.decalDebugVolumeCount = plan.debugVolumeCount;
    stats_.decalInputDepthValid = 0;
    stats_.decalInputColorValid = 0;
    stats_.decalRenderedCount = 0;
    stats_.decalSceneColorTargetValid = 0;
    stats_.decalSceneDepthTargetValid = 0;
    stats_.decalPassDraws = 0;
    stats_.decalProjectionDeferred = plan.activeCount > 0;

#if FULL_RENDERER_ENABLE_BGFX
    if (plan.activeCount == 0)
    {
        stats_.decalProjectionDeferred = false;
        return RendererResult::Success;
    }

    if (!sceneTargetActiveThisFrame_ || !hasValidSceneTargetResource())
    {
        return RendererResult::Success;
    }

    if (!bgfx::isValid(decalProgram_) ||
        !bgfx::isValid(scenePresentProgram_) ||
        !bgfx::isValid(ssaoDepthProgram_) ||
        !bgfx::isValid(ssaoDepthInstancedProgram_) ||
        !bgfx::isValid(ssaoDepthSkinnedProgram_) ||
        !bgfx::isValid(ssaoDepthParamsUniform_) ||
        !bgfx::isValid(decalInvViewUniform_) ||
        !bgfx::isValid(decalInvProjUniform_) ||
        !bgfx::isValid(decalWorldToLocalUniform_) ||
        !bgfx::isValid(decalColorOpacityUniform_) ||
        !bgfx::isValid(decalDepthParamsUniform_) ||
        !bgfx::isValid(decalDepthSampler_) ||
        !bgfx::isValid(decalAlbedoSampler_))
    {
        return RendererResult::BackendFailure;
    }

    stats_.decalSceneColorTargetValid = 1;
    stats_.decalSceneDepthTargetValid = 1;
    stats_.decalInputDepthValid = 1;
    stats_.decalInputColorValid = 1;

    const RendererResult depthCaptureResult = submitSceneDepthCapture(packet, batches, batchCount);
    if (depthCaptureResult != RendererResult::Success)
    {
        return depthCaptureResult;
    }

    constexpr float kDecalDepthMaxDistanceMeters = 1000.0f;
    float inverseView[16] = {};
    float inverseProjection[16] = {};
    if (!invertMatrix4x4(packet.view.view, inverseView) ||
        !invertMatrix4x4(packet.view.projection, inverseProjection))
    {
        return RendererResult::Success;
    }

    bgfx::setViewRect(
        kDecalViewId,
        0,
        0,
        static_cast<std::uint16_t>(backbufferWidth_),
        static_cast<std::uint16_t>(backbufferHeight_));
    bgfx::setViewFrameBuffer(kDecalViewId, sceneTargetResource_.frameBuffer);

    for (std::uint32_t decalIndex = 0; decalIndex < plan.activeCount; ++decalIndex)
    {
        const scene::DecalRenderItem& item = plan.items[decalIndex];
        bgfx::TransientVertexBuffer vertices;
        if (!allocateFullscreenQuad(vertices, meshVertexLayout_))
        {
            return RendererResult::BackendFailure;
        }

        bgfx::TextureHandle albedoTexture = fallbackWhiteTexture_;
        if (isValid(item.desc.albedoTexture))
        {
            if (const TextureResource* texture = resolveTexture(item.desc.albedoTexture, true))
            {
                albedoTexture = texture->texture;
            }
            else
            {
                ++stats_.decalFallbackColorCount;
            }
        }

        const float colorOpacity[4] = {
            item.desc.tintColorLinear[0],
            item.desc.tintColorLinear[1],
            item.desc.tintColorLinear[2],
            item.desc.tintColorLinear[3] * item.desc.opacity};
        const float rawDepthLimit =
            plan.maxProjectionDepthMeters > 0.0f ?
                std::min(plan.maxProjectionDepthMeters, item.desc.halfExtentsMeters[1]) :
                item.desc.halfExtentsMeters[1];
        const float normalizedDepthLimit =
            item.desc.halfExtentsMeters[1] > 0.0f ?
                std::min(std::max(rawDepthLimit / item.desc.halfExtentsMeters[1], 0.001f), 1.0f) :
                1.0f;
        const float normalizedDepthFade =
            item.desc.halfExtentsMeters[1] > 0.0f && plan.projectionEdgeFadeMeters > 0.0f ?
                std::min(plan.projectionEdgeFadeMeters / item.desc.halfExtentsMeters[1], normalizedDepthLimit) :
                0.0f;
        const float decalDepthParams[4] = {
            kDecalDepthMaxDistanceMeters,
            normalizedDepthLimit,
            normalizedDepthFade,
            0.0f};

        bgfx::setVertexBuffer(0, &vertices);
        bgfx::setTexture(12, decalDepthSampler_, sceneTargetResource_.depthColorTexture);
        bgfx::setTexture(13, decalAlbedoSampler_, albedoTexture);
        bgfx::setUniform(decalInvViewUniform_, inverseView);
        bgfx::setUniform(decalInvProjUniform_, inverseProjection);
        bgfx::setUniform(decalWorldToLocalUniform_, item.worldToDecal);
        bgfx::setUniform(decalColorOpacityUniform_, colorOpacity);
        bgfx::setUniform(decalDepthParamsUniform_, decalDepthParams);
        bgfx::setState(
            BGFX_STATE_WRITE_RGB |
            BGFX_STATE_WRITE_A |
            BGFX_STATE_BLEND_ALPHA |
            BGFX_STATE_MSAA);
        bgfx::submit(kDecalViewId, decalProgram_);
        ++stats_.decalPassDraws;
        ++stats_.decalRenderedCount;
    }
    stats_.decalProjectionDeferred = stats_.decalRenderedCount == 0;
#else
    stats_.decalSceneColorTargetValid = plan.activeCount > 0 ? 1U : 0U;
    stats_.decalSceneDepthTargetValid = plan.activeCount > 0 ? 1U : 0U;
    stats_.decalInputDepthValid = plan.activeCount > 0 ? 1U : 0U;
    stats_.decalInputColorValid = plan.activeCount > 0 ? 1U : 0U;
    stats_.decalPassDraws = plan.activeCount;
    stats_.decalRenderedCount = plan.activeCount;
    stats_.decalProjectionDeferred = false;
#endif

    return RendererResult::Success;
}

RendererResult BgfxRenderDevice::submitParticles(
    const RenderPacket& packet,
    const core::InstancedDrawBatch* batches,
    const std::uint32_t batchCount)
{
    scene::Frustum frustum = {};
    float viewProjection[16] = {};
    scene::multiplyColumnMajor4x4(packet.view.projection, packet.view.view, viewProjection);
    frustum = scene::extractFrustumFromViewProjection(viewProjection);

    float inverseView[16] = {};
    float cameraPosition[3] = {};
    const bool hasCameraPosition = invertMatrix4x4(packet.view.view, inverseView);
    if (hasCameraPosition)
    {
        cameraPosition[0] = inverseView[12];
        cameraPosition[1] = inverseView[13];
        cameraPosition[2] = inverseView[14];
    }

    scene::ParticleRenderPlanInput input;
    input.cameraFrustum = &frustum;
    input.cameraPositionWorld = hasCameraPosition ? cameraPosition : nullptr;
    const scene::ParticleRenderPlan plan = scene::buildParticleRenderPlan(packet.particles, input);
    if (!plan.enabled)
    {
        return RendererResult::Success;
    }

    stats_.particlesEnabled = true;
    stats_.particleSubmittedBatchCount = plan.submittedBatchCount;
    stats_.particleAcceptedBatchCount = plan.acceptedBatchCount;
    stats_.particleRejectedBatchCount = plan.rejectedBatchCount;
    stats_.particleCulledBatchCount = plan.culledBatchCount;
    stats_.particleSubmittedCount = plan.submittedParticleCount;
    stats_.particleAcceptedCount = plan.acceptedParticleCount;
    stats_.particleRejectedCount = plan.rejectedParticleCount;
    stats_.particleCulledCount = plan.culledParticleCount;
    stats_.particleSortedBatchCount = plan.sortedBatchCount;
    stats_.particleSortedCount = plan.sortedParticleCount;
    stats_.transparentParticleSortedBatches = plan.sortedBatchCount;
    stats_.transparentParticleUnsortedBatches =
        plan.acceptedBatchCount >= plan.sortedBatchCount ? plan.acceptedBatchCount - plan.sortedBatchCount : 0U;
    stats_.particleFallbackTextureBatchCount = plan.fallbackTextureBatchCount;
    stats_.particleDrawCalls = 0;
    stats_.particleResourceValid = 0;
    stats_.particleCullingEnabled = plan.frustumCullingEnabled;
    stats_.particleSoftParticlesEnabled = plan.softParticlesEnabled;
    stats_.particleSoftParticleFadeDistanceMeters = plan.softParticleFadeDistanceMeters;

    if (plan.acceptedBatchCount == 0)
    {
        return RendererResult::Success;
    }

#if FULL_RENDERER_ENABLE_BGFX
    if (!bgfx::isValid(particleProgram_) ||
        !bgfx::isValid(particleSampler_) ||
        !bgfx::isValid(particleDepthSampler_) ||
        !bgfx::isValid(particleParamsUniform_) ||
        !bgfx::isValid(fallbackWhiteTexture_))
    {
        return RendererResult::BackendFailure;
    }
    stats_.particleResourceValid = 1;

    const bool softParticlesRequested =
        plan.softParticlesEnabled && plan.softParticleFadeDistanceMeters > 0.0f;
    if (softParticlesRequested && sceneTargetActiveThisFrame_ && hasValidSceneTargetResource())
    {
        const RendererResult depthResult = submitSceneDepthCapture(packet, batches, batchCount);
        if (depthResult != RendererResult::Success)
        {
            return depthResult;
        }
        stats_.particleSoftParticleDepthInputValid = sceneDepthCapturedThisFrame_ ? 1U : 0U;
        stats_.particleSoftParticlesActive = sceneDepthCapturedThisFrame_;
    }

    const float right[3] = {
        packet.view.view[0],
        packet.view.view[4],
        packet.view.view[8]};
    const float up[3] = {
        packet.view.view[1],
        packet.view.view[5],
        packet.view.view[9]};

    bgfx::FrameBufferHandle target = BGFX_INVALID_HANDLE;
    if (sceneTargetActiveThisFrame_ && hasValidSceneTargetResource())
    {
        target = sceneTargetResource_.frameBuffer;
    }
    bgfx::setViewFrameBuffer(kParticleViewId, target);
    bgfx::setViewRect(
        kParticleViewId,
        0,
        0,
        static_cast<std::uint16_t>(backbufferWidth_),
        static_cast<std::uint16_t>(backbufferHeight_));
    bgfx::setViewTransform(kParticleViewId, packet.view.view, packet.view.projection);
    const float particleParams[4] = {
        stats_.particleSoftParticlesActive ? 1.0f : 0.0f,
        plan.softParticleFadeDistanceMeters,
        backbufferWidth_ > 0 ? 1.0f / static_cast<float>(backbufferWidth_) : 0.0f,
        backbufferHeight_ > 0 ? 1.0f / static_cast<float>(backbufferHeight_) : 0.0f};

    for (std::uint32_t batchIndex = 0; batchIndex < plan.acceptedBatchCount; ++batchIndex)
    {
        const scene::ParticleRenderBatch& batch = plan.batches[batchIndex];
        const std::uint32_t vertexCount = batch.particleCount * 4U;
        const std::uint32_t indexCount = batch.particleCount * 6U;
        if (bgfx::getAvailTransientVertexBuffer(vertexCount, particleVertexLayout_) < vertexCount ||
            bgfx::getAvailTransientIndexBuffer(indexCount) < indexCount)
        {
            return RendererResult::BackendFailure;
        }

        bgfx::TransientVertexBuffer vertices;
        bgfx::TransientIndexBuffer indices;
        bgfx::allocTransientVertexBuffer(&vertices, vertexCount, particleVertexLayout_);
        bgfx::allocTransientIndexBuffer(&indices, indexCount);

        auto* vertexData = reinterpret_cast<ParticleVertex*>(vertices.data);
        auto* indexData = reinterpret_cast<std::uint16_t*>(indices.data);
        for (std::uint32_t particleIndex = 0; particleIndex < batch.particleCount; ++particleIndex)
        {
            const Particle& particle = plan.particles[batch.firstParticle + particleIndex];
            const float halfSize = particle.sizeMeters * 0.5f;
            const float c = std::cos(particle.rotationRadians);
            const float s = std::sin(particle.rotationRadians);
            const float corners[4][2] = {
                {-1.0f, -1.0f},
                {1.0f, -1.0f},
                {1.0f, 1.0f},
                {-1.0f, 1.0f},
            };
            const float uvs[4][2] = {
                {particle.uvRect[0], particle.uvRect[1]},
                {particle.uvRect[2], particle.uvRect[1]},
                {particle.uvRect[2], particle.uvRect[3]},
                {particle.uvRect[0], particle.uvRect[3]},
            };

            const std::uint32_t baseVertex = particleIndex * 4U;
            for (std::uint32_t cornerIndex = 0; cornerIndex < 4U; ++cornerIndex)
            {
                const float localX = corners[cornerIndex][0];
                const float localY = corners[cornerIndex][1];
                const float rotatedX = (localX * c - localY * s) * halfSize;
                const float rotatedY = (localX * s + localY * c) * halfSize;
                ParticleVertex& vertex = vertexData[baseVertex + cornerIndex];
                vertex.position[0] =
                    particle.positionWorld[0] + right[0] * rotatedX + up[0] * rotatedY;
                vertex.position[1] =
                    particle.positionWorld[1] + right[1] * rotatedX + up[1] * rotatedY;
                vertex.position[2] =
                    particle.positionWorld[2] + right[2] * rotatedX + up[2] * rotatedY;
                vertex.texcoord[0] = uvs[cornerIndex][0];
                vertex.texcoord[1] = uvs[cornerIndex][1];
                const float viewZ =
                    packet.view.view[2] * vertex.position[0] +
                    packet.view.view[6] * vertex.position[1] +
                    packet.view.view[10] * vertex.position[2] +
                    packet.view.view[14];
                vertex.viewDepth = std::max(-viewZ, 0.0f);
                vertex.color[0] = particle.colorLinear[0];
                vertex.color[1] = particle.colorLinear[1];
                vertex.color[2] = particle.colorLinear[2];
                vertex.color[3] = particle.colorLinear[3];
            }

            const std::uint32_t baseIndex = particleIndex * 6U;
            indexData[baseIndex + 0U] = static_cast<std::uint16_t>(baseVertex + 0U);
            indexData[baseIndex + 1U] = static_cast<std::uint16_t>(baseVertex + 1U);
            indexData[baseIndex + 2U] = static_cast<std::uint16_t>(baseVertex + 2U);
            indexData[baseIndex + 3U] = static_cast<std::uint16_t>(baseVertex + 0U);
            indexData[baseIndex + 4U] = static_cast<std::uint16_t>(baseVertex + 2U);
            indexData[baseIndex + 5U] = static_cast<std::uint16_t>(baseVertex + 3U);
        }

        bgfx::TextureHandle texture = fallbackWhiteTexture_;
        if (isValid(batch.desc.texture))
        {
            if (const TextureResource* resource = resolveTexture(batch.desc.texture, true))
            {
                texture = resource->texture;
            }
            else
            {
                ++stats_.particleFallbackTextureBatchCount;
            }
        }

        bgfx::setVertexBuffer(0, &vertices);
        bgfx::setIndexBuffer(&indices);
        bgfx::setTexture(15, particleSampler_, texture);
        const bgfx::TextureHandle depthTexture =
            stats_.particleSoftParticlesActive ? sceneTargetResource_.depthColorTexture : fallbackWhiteTexture_;
        const float particleDepthParams[4] = {
            particleParams[0],
            particleParams[1],
            particleParams[2],
            particleParams[3]};
        bgfx::setTexture(14, particleDepthSampler_, depthTexture);
        bgfx::setUniform(particleParamsUniform_, particleDepthParams);
        bgfx::setState(
            BGFX_STATE_WRITE_RGB |
            BGFX_STATE_WRITE_A |
            BGFX_STATE_DEPTH_TEST_LEQUAL |
            BGFX_STATE_BLEND_ALPHA |
            BGFX_STATE_MSAA);
        bgfx::submit(kParticleViewId, particleProgram_);
        ++stats_.particleDrawCalls;
    }
#else
    stats_.particleResourceValid = 1;
    stats_.particleDrawCalls = plan.drawCallCount;
#endif

    return RendererResult::Success;
}

RendererResult BgfxRenderDevice::submitScenePresent(const RenderPacket& packet)
{
#if FULL_RENDERER_ENABLE_BGFX
    const bool sceneTargetAvailable = sceneTargetActiveThisFrame_ && hasValidSceneTargetResource();
    const bool lutTextureAvailable =
        packet.colorGrading.lut.enabled &&
        isValid(packet.colorGrading.lut.texture) &&
        resolveTexture(packet.colorGrading.lut.texture, true) != nullptr;
    const scene::ColorGradingRenderPlan colorGradingPlan =
        scene::makeColorGradingRenderPlan(packet.colorGrading, sceneTargetAvailable, lutTextureAvailable, false);
    applyColorGradingStats(colorGradingPlan);

    if (!sceneTargetActiveThisFrame_)
    {
        return RendererResult::Success;
    }
    if (!hasValidSceneTargetResource())
    {
        return RendererResult::BackendFailure;
    }
    if (!bgfx::isValid(scenePresentProgram_) || !bgfx::isValid(sceneColorSampler_))
    {
        return RendererResult::BackendFailure;
    }
    const bool submitColorGrading = colorGradingPlan.enabled && colorGradingPlan.passSubmitted;
    if (submitColorGrading)
    {
        const bool colorGradingResourcesValid =
            bgfx::isValid(colorGradingProgram_) &&
            bgfx::isValid(colorGradingParamsUniform_) &&
            bgfx::isValid(colorGradingControlsUniform_) &&
            bgfx::isValid(colorGradingLiftUniform_) &&
            bgfx::isValid(colorGradingGainUniform_);
        if (!colorGradingResourcesValid)
        {
            return RendererResult::BackendFailure;
        }
        stats_.colorGradingResourceValid = 1;
    }

    bgfx::TransientVertexBuffer vertices;
    if (!allocateFullscreenQuad(vertices, meshVertexLayout_))
    {
        return RendererResult::BackendFailure;
    }

    bgfx::setViewFrameBuffer(kScenePresentViewId, BGFX_INVALID_HANDLE);
    bgfx::setViewRect(
        kScenePresentViewId,
        0,
        0,
        static_cast<std::uint16_t>(backbufferWidth_),
        static_cast<std::uint16_t>(backbufferHeight_));
    bgfx::setVertexBuffer(0, &vertices);
    bgfx::setTexture(12, sceneColorSampler_, sceneTargetResource_.colorTexture);
    if (submitColorGrading)
    {
        bgfx::setUniform(colorGradingParamsUniform_, colorGradingPlan.params);
        bgfx::setUniform(colorGradingControlsUniform_, colorGradingPlan.controls);
        bgfx::setUniform(colorGradingLiftUniform_, colorGradingPlan.lift);
        bgfx::setUniform(colorGradingGainUniform_, colorGradingPlan.gain);
    }
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
    bgfx::submit(kScenePresentViewId, submitColorGrading ? colorGradingProgram_ : scenePresentProgram_);
#endif
    return RendererResult::Success;
}

bool BgfxRenderDevice::hasSelectedObjects(
    const RenderPacket& packet,
    const core::InstancedDrawBatch* batches,
    const std::uint32_t batchCount) const noexcept
{
    if (!packet.selectionOutline.enabled)
    {
        return false;
    }

    for (std::uint32_t index = 0; index < packet.drawItemCount; ++index)
    {
        if (packet.drawItems[index].selected)
        {
            return true;
        }
    }

    for (std::uint32_t index = 0; index < packet.animatedDrawCount; ++index)
    {
        if (packet.animatedDraws[index].selected)
        {
            return true;
        }
    }

    for (std::uint32_t index = 0; index < batchCount; ++index)
    {
        if (batches[index].selected)
        {
            return true;
        }
    }

    return false;
}

RendererResult BgfxRenderDevice::submitSelectionOutline(
    const RenderPacket& packet,
    const core::InstancedDrawBatch* batches,
    const std::uint32_t batchCount)
{
    if (batchCount > 0 && batches == nullptr)
    {
        return RendererResult::InvalidArgument;
    }

    if (!hasSelectedObjects(packet, batches, batchCount))
    {
        return RendererResult::Success;
    }

    stats_.selectionOutlineEnabled = true;

#if FULL_RENDERER_ENABLE_BGFX
    if (!bgfx::isValid(selectionMaskProgram_) ||
        !bgfx::isValid(selectionMaskInstancedProgram_) ||
        !bgfx::isValid(selectionMaskSkinnedProgram_) ||
        !bgfx::isValid(outlineCompositeProgram_) ||
        !bgfx::isValid(selectionMaskSampler_) ||
        !bgfx::isValid(outlineColorUniform_) ||
        !bgfx::isValid(outlineParamsUniform_))
    {
        return RendererResult::BackendFailure;
    }

    if (!ensureSelectionMaskResource(backbufferWidth_, backbufferHeight_))
    {
        stats_.selectionMaskTargetValid = 0;
        return RendererResult::BackendFailure;
    }
    stats_.selectionMaskTargetValid = 1;

    bgfx::setViewFrameBuffer(kSelectionMaskViewId, selectionMaskResource_.frameBuffer);
    bgfx::setViewRect(
        kSelectionMaskViewId,
        0,
        0,
        static_cast<std::uint16_t>(backbufferWidth_),
        static_cast<std::uint16_t>(backbufferHeight_));
    bgfx::setViewTransform(kSelectionMaskViewId, packet.view.view, packet.view.projection);

    constexpr std::uint64_t kMaskDepthPrepassState =
        BGFX_STATE_WRITE_Z |
        BGFX_STATE_DEPTH_TEST_LESS |
        BGFX_STATE_MSAA;
    constexpr std::uint64_t kMaskState =
        BGFX_STATE_WRITE_RGB |
        BGFX_STATE_WRITE_A |
        BGFX_STATE_DEPTH_TEST_LEQUAL |
        BGFX_STATE_MSAA;

    for (std::uint32_t index = 0; index < packet.drawItemCount; ++index)
    {
        const DrawItem& draw = packet.drawItems[index];
        const MeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveMeshMaterial(draw.mesh, draw.material, mesh, material, true))
        {
            return RendererResult::InvalidArgument;
        }

        bgfx::setTransform(draw.model);
        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        bgfx::setIndexBuffer(mesh->indexBuffer);
        bgfx::setState(kMaskDepthPrepassState);
        bgfx::submit(kSelectionMaskViewId, selectionMaskProgram_);
    }

    for (std::uint32_t index = 0; index < packet.animatedDrawCount; ++index)
    {
        const AnimatedDrawItem& draw = packet.animatedDraws[index];
        const SkinnedMeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveSkinnedMeshMaterial(draw.mesh, draw.material, mesh, material, true) ||
            draw.palette.skinningMatrices == nullptr ||
            draw.palette.matrixCount == 0 ||
            draw.palette.matrixCount > kMaxSkinningJoints)
        {
            return RendererResult::InvalidArgument;
        }

        float palette[kMaxSkinningJoints][16] = {};
        for (std::uint32_t matrixIndex = 0; matrixIndex < draw.palette.matrixCount; ++matrixIndex)
        {
            std::memcpy(
                palette[matrixIndex],
                draw.palette.skinningMatrices + matrixIndex * 16U,
                sizeof(float) * 16U);
        }

        bgfx::setUniform(skinningPaletteUniform_, palette, kMaxSkinningJoints);
        bgfx::setTransform(draw.model);
        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        const SkinnedMeshSectionDesc& section = mesh->sections[draw.sectionIndex];
        bgfx::setIndexBuffer(mesh->indexBuffer, section.firstIndex, section.indexCount);
        bgfx::setState(kMaskDepthPrepassState);
        bgfx::submit(kSelectionMaskViewId, selectionMaskSkinnedProgram_);
    }

    for (std::uint32_t batchIndex = 0; batchIndex < batchCount; ++batchIndex)
    {
        const core::InstancedDrawBatch& batch = batches[batchIndex];
        const MeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveMeshMaterial(batch.mesh, batch.material, mesh, material, true) ||
            batch.modelMatrices == nullptr ||
            batch.instanceCount == 0)
        {
            return RendererResult::InvalidArgument;
        }

        constexpr std::uint16_t kInstanceStride = sizeof(float) * 16U;
        const std::uint32_t availableInstances = bgfx::getAvailInstanceDataBuffer(batch.instanceCount, kInstanceStride);
        if (availableInstances < batch.instanceCount)
        {
            return RendererResult::BackendFailure;
        }

        bgfx::InstanceDataBuffer instanceData;
        bgfx::allocInstanceDataBuffer(&instanceData, batch.instanceCount, kInstanceStride);
        for (std::uint32_t instanceIndex = 0; instanceIndex < batch.instanceCount; ++instanceIndex)
        {
            const float* source = batch.modelMatrices + instanceIndex * 16U;
            float* destination = reinterpret_cast<float*>(instanceData.data + instanceIndex * kInstanceStride);
            for (std::uint32_t matrixIndex = 0; matrixIndex < 16U; ++matrixIndex)
            {
                destination[matrixIndex] = source[matrixIndex];
            }
        }

        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        bgfx::setIndexBuffer(mesh->indexBuffer);
        bgfx::setInstanceDataBuffer(&instanceData);
        bgfx::setState(kMaskDepthPrepassState);
        bgfx::submit(kSelectionMaskViewId, selectionMaskInstancedProgram_);
    }

    for (std::uint32_t index = 0; index < packet.drawItemCount; ++index)
    {
        const DrawItem& draw = packet.drawItems[index];
        if (!draw.selected)
        {
            continue;
        }

        const MeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveMeshMaterial(draw.mesh, draw.material, mesh, material, true))
        {
            return RendererResult::InvalidArgument;
        }

        bgfx::setTransform(draw.model);
        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        bgfx::setIndexBuffer(mesh->indexBuffer);
        bgfx::setState(kMaskState);
        bgfx::submit(kSelectionMaskViewId, selectionMaskProgram_);
        ++stats_.selectedStaticMeshDraws;
        ++stats_.selectionMaskDraws;
    }

    for (std::uint32_t index = 0; index < packet.animatedDrawCount; ++index)
    {
        const AnimatedDrawItem& draw = packet.animatedDraws[index];
        if (!draw.selected)
        {
            continue;
        }

        const SkinnedMeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveSkinnedMeshMaterial(draw.mesh, draw.material, mesh, material, true) ||
            draw.palette.skinningMatrices == nullptr ||
            draw.palette.matrixCount == 0 ||
            draw.palette.matrixCount > kMaxSkinningJoints)
        {
            return RendererResult::InvalidArgument;
        }

        float palette[kMaxSkinningJoints][16] = {};
        for (std::uint32_t matrixIndex = 0; matrixIndex < draw.palette.matrixCount; ++matrixIndex)
        {
            std::memcpy(
                palette[matrixIndex],
                draw.palette.skinningMatrices + matrixIndex * 16U,
                sizeof(float) * 16U);
        }

        bgfx::setUniform(skinningPaletteUniform_, palette, kMaxSkinningJoints);
        bgfx::setTransform(draw.model);
        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        const SkinnedMeshSectionDesc& section = mesh->sections[draw.sectionIndex];
        bgfx::setIndexBuffer(mesh->indexBuffer, section.firstIndex, section.indexCount);
        bgfx::setState(kMaskState);
        bgfx::submit(kSelectionMaskViewId, selectionMaskSkinnedProgram_);
        ++stats_.selectedSkinnedMeshDraws;
        ++stats_.selectionMaskDraws;
    }

    for (std::uint32_t batchIndex = 0; batchIndex < batchCount; ++batchIndex)
    {
        const core::InstancedDrawBatch& batch = batches[batchIndex];
        if (!batch.selected)
        {
            continue;
        }

        const MeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveMeshMaterial(batch.mesh, batch.material, mesh, material, true) ||
            batch.modelMatrices == nullptr ||
            batch.instanceCount == 0)
        {
            return RendererResult::InvalidArgument;
        }

        constexpr std::uint16_t kInstanceStride = sizeof(float) * 16U;
        const std::uint32_t availableInstances = bgfx::getAvailInstanceDataBuffer(batch.instanceCount, kInstanceStride);
        if (availableInstances < batch.instanceCount)
        {
            return RendererResult::BackendFailure;
        }

        bgfx::InstanceDataBuffer instanceData;
        bgfx::allocInstanceDataBuffer(&instanceData, batch.instanceCount, kInstanceStride);
        for (std::uint32_t instanceIndex = 0; instanceIndex < batch.instanceCount; ++instanceIndex)
        {
            const float* source = batch.modelMatrices + instanceIndex * 16U;
            float* destination = reinterpret_cast<float*>(instanceData.data + instanceIndex * kInstanceStride);
            for (std::uint32_t matrixIndex = 0; matrixIndex < 16U; ++matrixIndex)
            {
                destination[matrixIndex] = source[matrixIndex];
            }
        }

        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        bgfx::setIndexBuffer(mesh->indexBuffer);
        bgfx::setInstanceDataBuffer(&instanceData);
        bgfx::setState(kMaskState);
        bgfx::submit(kSelectionMaskViewId, selectionMaskInstancedProgram_);
        ++stats_.selectedInstancedBatches;
        stats_.selectedInstances += batch.instanceCount;
        ++stats_.selectionMaskDraws;
    }

    bgfx::TransientVertexBuffer vertices;
    constexpr std::uint32_t kVertexCount = 6;
    if (bgfx::getAvailTransientVertexBuffer(kVertexCount, meshVertexLayout_) < kVertexCount)
    {
        return RendererResult::BackendFailure;
    }
    bgfx::allocTransientVertexBuffer(&vertices, kVertexCount, meshVertexLayout_);

    struct FullscreenVertex
    {
        float position[3];
        float normal[3];
        float uv0[2];
        float color[4];
        float tangent[4];
    };
    static_assert(sizeof(FullscreenVertex) == sizeof(MeshVertex), "Fullscreen vertex must match mesh layout.");

    const float positions[kVertexCount][2] = {
        {-1.0f, -1.0f},
        {1.0f, -1.0f},
        {1.0f, 1.0f},
        {-1.0f, -1.0f},
        {1.0f, 1.0f},
        {-1.0f, 1.0f},
    };
    auto* quadVertices = reinterpret_cast<FullscreenVertex*>(vertices.data);
    for (std::uint32_t index = 0; index < kVertexCount; ++index)
    {
        quadVertices[index] = {
            {positions[index][0], positions[index][1], 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {1.0f, 0.0f, 0.0f, 1.0f}};
    }

    const float outlineColor[4] = {
        packet.selectionOutline.colorLinear[0],
        packet.selectionOutline.colorLinear[1],
        packet.selectionOutline.colorLinear[2],
        packet.selectionOutline.colorLinear[3]};
    const float thickness = std::min(std::max(packet.selectionOutline.thicknessPixels, 0.0f), 8.0f);
    const float outlineParams[4] = {
        backbufferWidth_ > 0 ? 1.0f / static_cast<float>(backbufferWidth_) : 0.0f,
        backbufferHeight_ > 0 ? 1.0f / static_cast<float>(backbufferHeight_) : 0.0f,
        thickness,
        0.0f};

    bgfx::setViewFrameBuffer(kOutlineCompositeViewId, BGFX_INVALID_HANDLE);
    bgfx::setViewRect(
        kOutlineCompositeViewId,
        0,
        0,
        static_cast<std::uint16_t>(backbufferWidth_),
        static_cast<std::uint16_t>(backbufferHeight_));
    bgfx::setVertexBuffer(0, &vertices);
    bgfx::setTexture(14, selectionMaskSampler_, selectionMaskResource_.colorTexture);
    bgfx::setUniform(outlineColorUniform_, outlineColor);
    bgfx::setUniform(outlineParamsUniform_, outlineParams);
    bgfx::setState(
        BGFX_STATE_WRITE_RGB |
        BGFX_STATE_WRITE_A |
        BGFX_STATE_BLEND_ALPHA |
        BGFX_STATE_MSAA);
    bgfx::submit(kOutlineCompositeViewId, outlineCompositeProgram_);
    ++stats_.selectionOutlineDraws;
#else
    for (std::uint32_t index = 0; index < packet.drawItemCount; ++index)
    {
        if (packet.drawItems[index].selected)
        {
            ++stats_.selectedStaticMeshDraws;
            ++stats_.selectionMaskDraws;
        }
    }
    for (std::uint32_t index = 0; index < packet.animatedDrawCount; ++index)
    {
        if (packet.animatedDraws[index].selected)
        {
            ++stats_.selectedSkinnedMeshDraws;
            ++stats_.selectionMaskDraws;
        }
    }
    for (std::uint32_t index = 0; index < batchCount; ++index)
    {
        if (batches[index].selected)
        {
            ++stats_.selectedInstancedBatches;
            stats_.selectedInstances += batches[index].instanceCount;
            ++stats_.selectionMaskDraws;
        }
    }
    stats_.selectionMaskTargetValid = 1;
    stats_.selectionOutlineDraws = 1;
#endif
    return RendererResult::Success;
}

RendererResult BgfxRenderDevice::beginFrame(const FrameDesc& desc)
{
    if (!initialized_)
    {
        return RendererResult::NotInitialized;
    }

    if (frameInProgress_)
    {
        return RendererResult::FrameAlreadyInProgress;
    }

    if (!hasValidDimensions(desc.backbufferWidth, desc.backbufferHeight) ||
        !hasValidFrameTime(desc.deltaSeconds))
    {
        return RendererResult::InvalidDescriptor;
    }

    backbufferWidth_ = desc.backbufferWidth;
    backbufferHeight_ = desc.backbufferHeight;
#if FULL_RENDERER_ENABLE_BGFX
    configureForwardView(backbufferWidth_, backbufferHeight_);
    bgfx::touch(kSkyViewId);
    bgfx::touch(kForwardViewId);
    bgfx::touch(kSsaoDepthViewId);
    bgfx::touch(kSsaoViewId);
    bgfx::touch(kSsaoBlurHorizontalViewId);
    bgfx::touch(kSsaoBlurVerticalViewId);
    bgfx::touch(kSsaoCompositeViewId);
    bgfx::touch(kDecalDepthViewId);
    bgfx::touch(kDecalViewId);
    bgfx::touch(kParticleViewId);
    bgfx::touch(kScenePresentViewId);
    bgfx::touch(kSelectionMaskViewId);
    bgfx::touch(kOutlineCompositeViewId);
    sceneTargetActiveThisFrame_ = false;
    sceneDepthCapturedThisFrame_ = false;
    forceSceneTargetForSubmit_ = false;
    sceneTargetResourceReconfiguredThisFrame_ = false;
    sceneTargetResourceAllocationFailedThisFrame_ = false;
    suppressPostScenePasses_ = false;
#endif
    shaderVariantMaskThisFrame_ = 0;

    stats_.submittedDraws = 0;
    stats_.renderedDraws = 0;
    stats_.materialOpaqueDraws = 0;
    stats_.materialAlphaTestDraws = 0;
    stats_.materialAlphaBlendDraws = 0;
    stats_.transparentSortedDraws = 0;
    stats_.transparentUnsortedDraws = 0;
    stats_.transparentParticleSortedBatches = 0;
    stats_.transparentParticleUnsortedBatches = 0;
    stats_.materialDitherFadeDraws = 0;
    stats_.invalidMaterialCount = 0;
    stats_.fallbackMaterialCount = 0;
    stats_.fallbackMaterialTextureCount = 0;
    stats_.unsupportedShaderVariantRequestCount = 0;
    stats_.shaderVariantCountInUse = 0;
    stats_.alphaMaterialShadowUnsupportedCount = 0;
    stats_.materialColorSpaceWarningCount = 0;
    stats_.terrainShadowsEnabled = false;
    stats_.shadowCasterBatches = 0;
    stats_.shadowPassDraws = 0;
    stats_.shadowStaticMeshReceivers = 0;
    stats_.shadowInstancedMeshReceiverBatches = 0;
    stats_.shadowSkinnedMeshReceivers = 0;
    stats_.shadowSkinnedMeshPassDraws = 0;
    stats_.shadowMapResolution =
#if FULL_RENDERER_ENABLE_BGFX
        shadowMapResolution_;
#else
        0;
#endif
    stats_.shadowRequestedCascadeCount = 0;
    stats_.shadowRequestedMapResolution = 0;
    stats_.shadowResourceReconfigured = 0;
    stats_.shadowResourceRecreateCount =
#if FULL_RENDERER_ENABLE_BGFX
        shadowResourceRecreateCount_;
#else
        0;
#endif
    stats_.shadowResourceAllocationFailureCount =
#if FULL_RENDERER_ENABLE_BGFX
        shadowResourceAllocationFailureCount_;
#else
        0;
#endif
    stats_.shadowResourceAllocationFailed = 0;
    stats_.shadowCascadeCount = 0;
    stats_.skyRendered = false;
    stats_.fogEnabled = false;
    stats_.foggedMeshDraws = 0;
    stats_.foggedInstancedBatches = 0;
    stats_.weatherEnabled = false;
    stats_.weatherWindEnabled = false;
    stats_.weatherWindDirectionWorld[0] = 0.0f;
    stats_.weatherWindDirectionWorld[1] = 0.0f;
    stats_.weatherWindDirectionWorld[2] = 0.0f;
    stats_.weatherWindSpeedMetersPerSecond = 0.0f;
    stats_.weatherPrecipitationEnabled = false;
    stats_.weatherPrecipitationType = PrecipitationType::None;
    stats_.weatherPrecipitationIntensity = 0.0f;
    stats_.weatherPrecipitationUsesParticleBatches = false;
    stats_.weatherWetnessEnabled = false;
    stats_.weatherWetnessAmount = 0.0f;
    stats_.weatherTerrainWetnessDraws = 0;
    stats_.weatherMeshWetnessDraws = 0;
    stats_.weatherFogBlendEnabled = false;
    stats_.weatherFogBlendAmount = 0.0f;
    stats_.weatherEffectiveFogColorLinear[0] = 0.0f;
    stats_.weatherEffectiveFogColorLinear[1] = 0.0f;
    stats_.weatherEffectiveFogColorLinear[2] = 0.0f;
    stats_.weatherEffectiveFogStartMeters = 0.0f;
    stats_.weatherEffectiveFogEndMeters = 0.0f;
    stats_.weatherClampedValueCount = 0;
    stats_.weatherNeutral = true;
    stats_.structureFadeSubmittedCount = 0;
    stats_.structureFadeActiveCount = 0;
    stats_.structureFadeFullyVisibleCount = 0;
    stats_.structureFadePartiallyFadedCount = 0;
    stats_.structureFadeFullyHiddenCount = 0;
    stats_.structureFadeInvalidCount = 0;
    stats_.structureFadeAlphaDraws = 0;
    stats_.structureFadeDitherDraws = 0;
    stats_.structureFadeUnsupportedTargetCount = 0;
    stats_.selectionOutlineEnabled = false;
    stats_.selectionMaskTargetValid = 0;
    stats_.selectedStaticMeshDraws = 0;
    stats_.selectedInstancedBatches = 0;
    stats_.selectedInstances = 0;
    stats_.selectedSkinnedMeshDraws = 0;
    stats_.selectionMaskDraws = 0;
    stats_.selectionOutlineDraws = 0;
    stats_.ssaoEnabled = false;
    stats_.ssaoDepthTargetValid = 0;
    stats_.ssaoOutputTargetValid = 0;
    stats_.ssaoBlurTargetValid = 0;
    stats_.ssaoAoWidth = 0;
    stats_.ssaoAoHeight = 0;
    stats_.ssaoHalfResolution = false;
    stats_.ssaoBlurEnabled = false;
    stats_.ssaoDepthPassDraws = 0;
    stats_.ssaoPassDraws = 0;
    stats_.ssaoCompositeDraws = 0;
    stats_.ssaoBlurPassDraws = 0;
    stats_.ssaoInputDepthValid = 0;
    stats_.ssaoDebugVisualized = false;
    stats_.decalsEnabled = false;
    stats_.decalSubmittedCount = 0;
    stats_.decalActiveCount = 0;
    stats_.decalCulledCount = 0;
    stats_.decalRejectedCount = 0;
    stats_.decalInvalidDescriptorRejectCount = 0;
    stats_.decalMaxCountRejectedCount = 0;
    stats_.decalFallbackColorCount = 0;
    stats_.decalFrustumCullingEnabled = false;
    stats_.decalMaxProjectionDepthMeters = 0.0f;
    stats_.decalProjectionEdgeFadeMeters = 0.0f;
    stats_.decalDebugVolumeCount = 0;
    stats_.decalPassDraws = 0;
    stats_.decalRenderedCount = 0;
    stats_.decalInputDepthValid = 0;
    stats_.decalInputColorValid = 0;
    stats_.decalSceneColorTargetValid = 0;
    stats_.decalSceneDepthTargetValid = 0;
    stats_.decalProjectionDeferred = false;
    stats_.particlesEnabled = false;
    stats_.particleSubmittedBatchCount = 0;
    stats_.particleAcceptedBatchCount = 0;
    stats_.particleRejectedBatchCount = 0;
    stats_.particleCulledBatchCount = 0;
    stats_.particleSubmittedCount = 0;
    stats_.particleAcceptedCount = 0;
    stats_.particleRejectedCount = 0;
    stats_.particleCulledCount = 0;
    stats_.particleSortedBatchCount = 0;
    stats_.particleSortedCount = 0;
    stats_.particleFallbackTextureBatchCount = 0;
    stats_.particleDrawCalls = 0;
    stats_.particleResourceValid = 0;
    stats_.particleCullingEnabled = false;
    stats_.particleSoftParticlesEnabled = false;
    stats_.particleSoftParticlesActive = false;
    stats_.particleSoftParticleDepthInputValid = 0;
    stats_.particleSoftParticleFadeDistanceMeters = 0.0f;
    stats_.colorGradingEnabled = false;
    stats_.colorGradingPassSubmitted = false;
    stats_.colorGradingSceneColorTargetValid = 0;
    stats_.colorGradingResourceValid = 0;
    stats_.colorGradingTonemapEnabled = false;
    stats_.colorGradingTonemapOperator = TonemapOperator::None;
    stats_.colorGradingExposureStops = 0.0f;
    stats_.colorGradingContrast = 1.0f;
    stats_.colorGradingSaturation = 1.0f;
    stats_.colorGradingGamma = 1.0f;
    stats_.colorGradingLutEnabled = false;
    stats_.colorGradingLutActive = false;
    stats_.colorGradingLutSamplingSupported = false;
    stats_.colorGradingLutFallbackCount = 0;
    stats_.colorGradingClampedValueCount = 0;
    stats_.colorGradingDebugMode = ColorGradingDebugMode::None;
    stats_.postViewportWidth = backbufferWidth_;
    stats_.postViewportHeight = backbufferHeight_;
    stats_.postSceneTargetRequired = false;
    stats_.postSceneTargetActive = false;
    stats_.postSceneTargetReasonMask = 0;
    stats_.postSceneColorTargetValid = 0;
    stats_.postSceneDepthTargetValid = 0;
    stats_.postSceneColorWidth = 0;
    stats_.postSceneColorHeight = 0;
    stats_.postSceneDepthWidth = 0;
    stats_.postSceneDepthHeight = 0;
    stats_.postReadableSceneDepthRequired = false;
    stats_.postFinalPresentSubmitted = 0;
    stats_.postPresentMode = 0;
    stats_.postPassCount = 0;
    stats_.postFullscreenPassCount = 0;
    stats_.postSkippedPassCount = 0;
    stats_.postSkippedPassReasonMask = 0;
    stats_.postInvalidResourceCount = 0;
    stats_.postSceneTargetReconfigured = 0;
    stats_.postSceneTargetRecreateCount =
#if FULL_RENDERER_ENABLE_BGFX
        sceneTargetResourceRecreateCount_;
#else
        0;
#endif
    stats_.postSceneTargetAllocationFailureCount =
#if FULL_RENDERER_ENABLE_BGFX
        sceneTargetResourceAllocationFailureCount_;
#else
        0;
#endif
    stats_.postSceneTargetAllocationFailed = 0;
    stats_.submittedAnimatedDraws = 0;
    stats_.renderedAnimatedDraws = 0;
    stats_.skippedAnimatedDraws = 0;
    stats_.animationDebugLineVertices = 0;
    stats_.shadowCascadeRenderTargetCount =
#if FULL_RENDERER_ENABLE_BGFX
        shadowCascadeResourceCount_;
#else
        0;
#endif
    for (std::uint32_t cascadeIndex = 0; cascadeIndex < kMaxDirectionalShadowCascades; ++cascadeIndex)
    {
        stats_.shadowCasterBatchesByCascade[cascadeIndex] = 0;
        stats_.shadowStaticMeshCastersByCascade[cascadeIndex] = 0;
        stats_.shadowInstancedMeshCasterBatchesByCascade[cascadeIndex] = 0;
        stats_.shadowSkinnedMeshCastersByCascade[cascadeIndex] = 0;
        stats_.shadowTerrainCasterBatchesByCascade[cascadeIndex] = 0;
        stats_.shadowPassDrawsByCascade[cascadeIndex] = 0;
#if FULL_RENDERER_ENABLE_BGFX
        stats_.shadowCascadeRenderTargetValid[cascadeIndex] =
            cascadeIndex < shadowCascadeResourceCount_ &&
            bgfx::isValid(shadowCascadeResources_[cascadeIndex].frameBuffer) ? 1U : 0U;
#else
        stats_.shadowCascadeRenderTargetValid[cascadeIndex] = 0;
#endif
    }
    updateLiveResourceStats();
    frameInProgress_ = true;
    return RendererResult::Success;
}

RendererResult BgfxRenderDevice::submit(const RenderPacket& packet)
{
    if (!initialized_)
    {
        return RendererResult::NotInitialized;
    }

    if (!frameInProgress_)
    {
        return RendererResult::FrameNotInProgress;
    }

    const scene::WeatherRenderPlan weatherPlan =
        scene::makeWeatherRenderPlan(packet.weather, packet.environment);
    const EnvironmentDesc& effectiveEnvironment = weatherPlan.environment;

    bool meshShadowsEnabled = false;

#if FULL_RENDERER_ENABLE_BGFX
    if (!bgfx::isValid(forwardProgram_))
    {
        return RendererResult::BackendFailure;
    }

    if (!packet.directionalShadow.enabled && shadowCascadeResourceCount_ > 0)
    {
        destroyShadowResources();
        stats_.shadowResourceReconfigured = 1;
        stats_.shadowMapResolution = 0;
        stats_.shadowCascadeRenderTargetCount = 0;
        for (std::uint32_t cascadeIndex = 0; cascadeIndex < kMaxDirectionalShadowCascades; ++cascadeIndex)
        {
            stats_.shadowCascadeRenderTargetValid[cascadeIndex] = 0;
        }
    }

    applyWeatherStats(weatherPlan);

    const scene::DecalRenderPlan decalScenePlan = buildCameraCulledDecalPlan(packet);
    const scene::ParticleRenderPlan particleScenePlan = buildPostParticlePlan(packet);
    const bool selectedObjects = hasSelectedObjects(packet, nullptr, 0);
    const scene::PostPassPlan preResourcePostPlan =
        makePostPassPlanForPacket(packet, decalScenePlan, particleScenePlan, selectedObjects, false, false);
    const bool shouldUseSceneTarget = preResourcePostPlan.sceneTargetRequired;
    if (shouldUseSceneTarget && ensureSceneTargetResource(backbufferWidth_, backbufferHeight_))
    {
        sceneTargetActiveThisFrame_ = true;
        stats_.decalSceneColorTargetValid = 1;
        stats_.decalSceneDepthTargetValid = 1;
        bgfx::setViewFrameBuffer(kSkyViewId, sceneTargetResource_.frameBuffer);
        bgfx::setViewFrameBuffer(kForwardViewId, sceneTargetResource_.frameBuffer);
        bgfx::setViewFrameBuffer(kParticleViewId, sceneTargetResource_.frameBuffer);
        bgfx::touch(kSkyViewId);
    }
    else
    {
        sceneTargetActiveThisFrame_ = false;
        bgfx::setViewFrameBuffer(kSkyViewId, BGFX_INVALID_HANDLE);
        bgfx::setViewFrameBuffer(kForwardViewId, BGFX_INVALID_HANDLE);
        bgfx::setViewFrameBuffer(kParticleViewId, BGFX_INVALID_HANDLE);
    }
    applyPostPassStats(makePostPassPlanForPacket(
        packet,
        decalScenePlan,
        particleScenePlan,
        selectedObjects,
        sceneTargetActiveThisFrame_ && hasValidSceneTargetResource(),
        sceneTargetActiveThisFrame_ && hasValidSceneTargetResource()));

    bgfx::setViewTransform(kForwardViewId, packet.view.view, packet.view.projection);

    bindEnvironmentState(effectiveEnvironment);
    bindWeatherState(weatherPlan);
    const RendererResult skyResult = submitSky(effectiveEnvironment);
    if (skyResult != RendererResult::Success)
    {
        return skyResult;
    }
    stats_.fogEnabled = effectiveEnvironment.fogEnabled;

    float lightDirection[3] = {};
    normalize3(packet.directionalLight.directionWorld, lightDirection);
    const float lightDirIntensity[4] = {
        lightDirection[0],
        lightDirection[1],
        lightDirection[2],
        packet.directionalLight.intensity};
    const float lightColor[4] = {
        packet.directionalLight.colorLinear[0],
        packet.directionalLight.colorLinear[1],
        packet.directionalLight.colorLinear[2],
        1.0f};

    bgfx::setUniform(lightDirIntensityUniform_, lightDirIntensity);
    bgfx::setUniform(lightColorUniform_, lightColor);

    float shadowViewProjMatrices[kMaxDirectionalShadowCascades][16] = {};
    float cascadeSplits[4] = {};
    std::uint32_t activeShadowCascadeCount = 0;
    if (!prepareCsmForwardState(
            packet,
            shadowViewProjMatrices,
            cascadeSplits,
            activeShadowCascadeCount,
            meshShadowsEnabled))
    {
        return RendererResult::BackendFailure;
    }
#endif

    stats_.submittedDraws += packet.drawItemCount;

    float cameraPositionWorld[3] = {};
    const bool hasCameraPosition =
        scene::extractCameraPositionFromViewMatrix(packet.view.view, cameraPositionWorld);
    std::vector<scene::TransparentSortItem> transparentStaticDraws;
    transparentStaticDraws.reserve(packet.drawItemCount);
    for (std::uint32_t index = 0; index < packet.drawItemCount; ++index)
    {
        const DrawItem& drawItem = packet.drawItems[index];
        const MaterialResource* material = findMaterial(drawItem.material);
        if (material == nullptr)
        {
            continue;
        }
        const scene::FadeRenderState fadeState = scene::makeFadeRenderState(drawItem.fade);
        if (!scene::isTransparentRenderBucket(
                scene::renderBucketForMaterialAndFade(material->desc, fadeState)))
        {
            continue;
        }

        scene::TransparentSortItem sortItem;
        sortItem.submissionIndex = index;
        sortItem.stableOrder = index;
        float centerWorld[3] = {};
        centerFromModelTranslation(drawItem.model, centerWorld);
        sortItem.validDistance = hasCameraPosition &&
            scene::computeSortDistanceSquared(centerWorld, cameraPositionWorld, sortItem.distanceSquared);
        transparentStaticDraws.push_back(sortItem);
    }
    scene::stableSortTransparentBackToFront(
        transparentStaticDraws.empty() ? nullptr : transparentStaticDraws.data(),
        static_cast<std::uint32_t>(transparentStaticDraws.size()));

    const auto renderStaticDraw = [&](const std::uint32_t index, const bool transparentSorted) -> RendererResult {
        const DrawItem& drawItem = packet.drawItems[index];
        const MeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveMeshMaterial(drawItem.mesh, drawItem.material, mesh, material, true))
        {
            return RendererResult::InvalidArgument;
        }

        const scene::FadeRenderState fadeState = scene::makeFadeRenderState(drawItem.fade);
        recordFadeStats(fadeState, stats_);
        recordMaterialDrawStats(material->desc, fadeState, transparentSorted, stats_);
        markShaderVariant(
            scene::makeForwardShaderVariantKey(
                material->desc,
                scene::ShaderVariantPass::ForwardStatic,
                effectiveEnvironment.fogEnabled,
                meshShadowsEnabled && scene::materialReceivesCsmByPolicy(material->desc),
                fadeState),
            shaderVariantMaskThisFrame_,
            stats_);
#if FULL_RENDERER_ENABLE_BGFX
        const float materialColor[4] = {
            material->desc.baseColorLinear[0],
            material->desc.baseColorLinear[1],
            material->desc.baseColorLinear[2],
            material->desc.lit ? material->desc.baseColorLinear[3] : -material->desc.baseColorLinear[3]};

        bgfx::setUniform(materialColorUniform_, materialColor);
        bindEnvironmentState(effectiveEnvironment);
        bindWeatherState(weatherPlan);
        bindFadeState(fadeState, material->desc);
        bindBasicMaterialTextureState(material->desc);
        bindCsmForwardState(
            packet,
            shadowViewProjMatrices,
            cascadeSplits,
            activeShadowCascadeCount,
            meshShadowsEnabled && material->desc.lit);
        bgfx::setTransform(drawItem.model);
        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        bgfx::setIndexBuffer(mesh->indexBuffer);
        bgfx::setState(forwardMeshStateForMaterialAndFade(material->desc, fadeState));
        bgfx::submit(kForwardViewId, forwardProgram_);
#endif
        if (packet.directionalShadow.enabled && material->desc.lit)
        {
            ++stats_.shadowStaticMeshReceivers;
        }
        if (effectiveEnvironment.fogEnabled)
        {
            ++stats_.foggedMeshDraws;
        }
        if (weatherPlan.meshWetnessActive)
        {
            ++stats_.weatherMeshWetnessDraws;
        }
        ++stats_.renderedDraws;
        return RendererResult::Success;
    };

    for (std::uint32_t index = 0; index < packet.drawItemCount; ++index)
    {
        const MaterialResource* material = findMaterial(packet.drawItems[index].material);
        const scene::FadeRenderState fadeState = scene::makeFadeRenderState(packet.drawItems[index].fade);
        if (material != nullptr &&
            scene::isTransparentRenderBucket(
                scene::renderBucketForMaterialAndFade(material->desc, fadeState)))
        {
            continue;
        }
        const RendererResult drawResult = renderStaticDraw(index, false);
        if (drawResult != RendererResult::Success)
        {
            return drawResult;
        }
    }

    for (const scene::TransparentSortItem& sortItem : transparentStaticDraws)
    {
        const RendererResult drawResult = renderStaticDraw(sortItem.submissionIndex, true);
        if (drawResult != RendererResult::Success)
        {
            return drawResult;
        }
    }

    if (suppressPostScenePasses_)
    {
        return RendererResult::Success;
    }

    const RendererResult ssaoResult = submitSsao(packet, nullptr, 0);
    if (ssaoResult != RendererResult::Success)
    {
        return ssaoResult;
    }

    const RendererResult decalResult = submitDecals(packet, nullptr, 0);
    if (decalResult != RendererResult::Success)
    {
        return decalResult;
    }

    const RendererResult particleResult = submitParticles(packet, nullptr, 0);
    if (particleResult != RendererResult::Success)
    {
        return particleResult;
    }

    const RendererResult presentResult = submitScenePresent(packet);
    if (presentResult != RendererResult::Success)
    {
        return presentResult;
    }

#if FULL_RENDERER_ENABLE_BGFX
    applyPostPassStats(makePostPassPlanForPacket(
        packet,
        decalScenePlan,
        particleScenePlan,
        selectedObjects,
        sceneTargetActiveThisFrame_ && hasValidSceneTargetResource(),
        sceneTargetActiveThisFrame_ && hasValidSceneTargetResource()));
#endif

    return submitSelectionOutline(packet, nullptr, 0);
}

RendererResult BgfxRenderDevice::submitInstanced(
    const RenderPacket& packet,
    const core::InstancedDrawBatch* batches,
    const std::uint32_t batchCount,
    const core::InstancedDrawBatch* shadowBatches,
    const std::uint32_t shadowBatchCount,
    const core::SkinnedShadowDrawBatch* skinnedShadowBatches,
    const std::uint32_t skinnedShadowBatchCount)
{
    if (batchCount > 0 && batches == nullptr)
    {
        return RendererResult::InvalidArgument;
    }
    if (shadowBatchCount > 0 && shadowBatches == nullptr)
    {
        return RendererResult::InvalidArgument;
    }
    if (skinnedShadowBatchCount > 0 && skinnedShadowBatches == nullptr)
    {
        return RendererResult::InvalidArgument;
    }

#if FULL_RENDERER_ENABLE_BGFX
    if (!bgfx::isValid(instancedForwardProgram_) ||
        !bgfx::isValid(skinnedForwardProgram_) ||
        (skinnedShadowBatchCount > 0 && !bgfx::isValid(skinnedShadowProgram_)))
    {
        return RendererResult::BackendFailure;
    }
#endif

    stats_.submittedDraws += batchCount;
    stats_.submittedAnimatedDraws += packet.animatedDrawCount;

    bool terrainShadowsEnabled = false;

#if FULL_RENDERER_ENABLE_BGFX
    float shadowViewProjMatrices[kMaxDirectionalShadowCascades][16] = {};
    float cascadeSplits[4] = {};
    std::uint32_t activeShadowCascadeCount = 0;
    if (packet.directionalShadow.enabled)
    {
        const std::uint32_t shadowResolution = scene::clampShadowMapResolution(packet.directionalShadow.mapResolution);
        const scene::ShadowResourceReconfigurePlan resourcePlan = scene::planShadowResourceReconfiguration(
            packet.directionalShadow,
            shadowMapResolution_,
            shadowCascadeResourceCount_,
            hasValidShadowResources(scene::clampShadowCascadeCount(packet.directionalShadow.cascadeCount)));
        stats_.shadowRequestedCascadeCount = resourcePlan.requested.cascadeCount;
        stats_.shadowRequestedMapResolution = resourcePlan.requested.mapResolution;

        scene::DirectionalShadowCascadeSet cascadeSet;
        if (scene::clampShadowCascadeCount(packet.directionalShadow.cascadeCount) == 1U)
        {
            scene::DirectionalShadowSplit split;
            if (!scene::buildDirectionalShadowSplit(packet.directionalLight, packet.directionalShadow, split))
            {
                return RendererResult::BackendFailure;
            }
            cascadeSet.cascadeCount = 1;
            cascadeSet.splits[0] = split;
            cascadeSet.splits[0].splitIndex = 0;
        }
        else if (!scene::buildDirectionalShadowCascadeSet(packet.directionalLight, packet.directionalShadow, packet.view, cascadeSet) ||
            cascadeSet.cascadeCount == 0)
        {
            return RendererResult::BackendFailure;
        }

        if (resourcePlan.action == scene::ShadowResourceReconfigureAction::Recreate)
        {
            stats_.shadowResourceReconfigured = 1;
        }

        const bool shadowResourcesReady = ensureShadowResources(shadowResolution, cascadeSet.cascadeCount);
        stats_.shadowResourceRecreateCount = shadowResourceRecreateCount_;
        stats_.shadowResourceAllocationFailureCount = shadowResourceAllocationFailureCount_;
        if (!shadowResourcesReady)
        {
            stats_.shadowResourceAllocationFailed = 1;
            stats_.terrainShadowsEnabled = false;
            stats_.shadowCascadeCount = 0;
            stats_.shadowCascadeRenderTargetCount = 0;
            stats_.shadowMapResolution = 0;
        }
        else if (!bgfx::isValid(terrainShadowProgram_))
        {
            return RendererResult::BackendFailure;
        }

        if (shadowResourcesReady)
        {
            for (std::uint32_t cascadeIndex = 0; cascadeIndex < kMaxDirectionalShadowCascades; ++cascadeIndex)
            {
                const std::uint32_t sourceIndex = std::min(cascadeIndex, cascadeSet.cascadeCount - 1U);
                for (int matrixIndex = 0; matrixIndex < 16; ++matrixIndex)
                {
                    shadowViewProjMatrices[cascadeIndex][matrixIndex] =
                        cascadeSet.splits[sourceIndex].matrices.viewProjection[matrixIndex];
                }
                cascadeSplits[cascadeIndex] = cascadeSet.splits[sourceIndex].farDistanceMeters;
            }
            activeShadowCascadeCount = cascadeSet.cascadeCount;
            terrainShadowsEnabled = true;
            stats_.terrainShadowsEnabled = true;
            stats_.shadowMapResolution = shadowResolution;
            stats_.shadowCascadeCount = cascadeSet.cascadeCount;
            stats_.shadowCascadeRenderTargetCount = shadowCascadeResourceCount_;
            stats_.shadowCasterBatches = shadowBatchCount + skinnedShadowBatchCount;
            for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeSet.cascadeCount; ++cascadeIndex)
            {
                stats_.shadowCascadeRenderTargetValid[cascadeIndex] =
                    bgfx::isValid(shadowCascadeResources_[cascadeIndex].frameBuffer) ? 1U : 0U;
                bgfx::setViewTransform(
                    shadowViewId(cascadeIndex),
                    cascadeSet.splits[cascadeIndex].matrices.view,
                    cascadeSet.splits[cascadeIndex].matrices.projection);

                for (std::uint32_t batchIndex = 0; batchIndex < shadowBatchCount; ++batchIndex)
                {
                    const core::InstancedDrawBatch& batch = shadowBatches[batchIndex];
                    if (batch.shadowCascadeIndex != cascadeIndex)
                    {
                        continue;
                    }

                    const MeshResource* mesh = nullptr;
                    const MaterialResource* material = nullptr;
                    if (!resolveMeshMaterial(batch.mesh, batch.material, mesh, material, true) ||
                        batch.modelMatrices == nullptr ||
                        batch.instanceCount == 0)
                    {
                        return RendererResult::InvalidArgument;
                    }
                    if (!scene::materialCastsDirectionalShadowByPolicy(material->desc))
                    {
                        ++stats_.alphaMaterialShadowUnsupportedCount;
                        continue;
                    }

                    constexpr std::uint16_t kInstanceStride = sizeof(float) * 16U;
                    const std::uint32_t availableInstances = bgfx::getAvailInstanceDataBuffer(batch.instanceCount, kInstanceStride);
                    if (availableInstances < batch.instanceCount)
                    {
                        return RendererResult::BackendFailure;
                    }

                    bgfx::InstanceDataBuffer instanceData;
                    bgfx::allocInstanceDataBuffer(&instanceData, batch.instanceCount, kInstanceStride);
                    for (std::uint32_t instanceIndex = 0; instanceIndex < batch.instanceCount; ++instanceIndex)
                    {
                        const float* source = batch.modelMatrices + instanceIndex * 16U;
                        float* destination = reinterpret_cast<float*>(instanceData.data + instanceIndex * kInstanceStride);
                        for (std::uint32_t matrixIndex = 0; matrixIndex < 16U; ++matrixIndex)
                        {
                            destination[matrixIndex] = source[matrixIndex];
                        }
                    }

                    bgfx::setVertexBuffer(0, mesh->vertexBuffer);
                    bgfx::setIndexBuffer(mesh->indexBuffer);
                    bgfx::setInstanceDataBuffer(&instanceData);
                    bgfx::setState(
                        BGFX_STATE_WRITE_RGB |
                        BGFX_STATE_WRITE_Z |
                        BGFX_STATE_DEPTH_TEST_LESS |
                        BGFX_STATE_CULL_CW);
                    bgfx::submit(shadowViewId(cascadeIndex), terrainShadowProgram_);
                    ++stats_.shadowPassDraws;
                    ++stats_.shadowPassDrawsByCascade[cascadeIndex];
                    ++stats_.shadowCasterBatchesByCascade[cascadeIndex];
                    if (batch.shadowCasterKind == core::ShadowCasterKind::StaticMesh)
                    {
                        ++stats_.shadowStaticMeshCastersByCascade[cascadeIndex];
                    }
                    else if (batch.shadowCasterKind == core::ShadowCasterKind::InstancedMesh)
                    {
                        ++stats_.shadowInstancedMeshCasterBatchesByCascade[cascadeIndex];
                    }
                    else if (batch.shadowCasterKind == core::ShadowCasterKind::Terrain)
                    {
                        ++stats_.shadowTerrainCasterBatchesByCascade[cascadeIndex];
                    }
                }

                for (std::uint32_t batchIndex = 0; batchIndex < skinnedShadowBatchCount; ++batchIndex)
                {
                    const core::SkinnedShadowDrawBatch& batch = skinnedShadowBatches[batchIndex];
                    if (batch.shadowCascadeIndex != cascadeIndex)
                    {
                        continue;
                    }

                    const SkinnedMeshResource* mesh = nullptr;
                    const MaterialResource* material = nullptr;
                    if (!resolveSkinnedMeshMaterial(batch.mesh, batch.material, mesh, material, true) ||
                        batch.model == nullptr ||
                        batch.palette.skinningMatrices == nullptr ||
                        batch.palette.matrixCount == 0 ||
                        batch.palette.matrixCount > kMaxSkinningJoints)
                    {
                        return RendererResult::InvalidArgument;
                    }
                    if (!scene::materialCastsDirectionalShadowByPolicy(material->desc))
                    {
                        ++stats_.alphaMaterialShadowUnsupportedCount;
                        continue;
                    }

                    float palette[kMaxSkinningJoints][16] = {};
                    for (std::uint32_t matrixIndex = 0; matrixIndex < batch.palette.matrixCount; ++matrixIndex)
                    {
                        std::memcpy(
                            palette[matrixIndex],
                            batch.palette.skinningMatrices + matrixIndex * 16U,
                            sizeof(float) * 16U);
                    }

                    bgfx::setUniform(skinningPaletteUniform_, palette, kMaxSkinningJoints);
                    bgfx::setTransform(batch.model);
                    bgfx::setVertexBuffer(0, mesh->vertexBuffer);
                    const SkinnedMeshSectionDesc& section = mesh->sections[batch.sectionIndex];
                    bgfx::setIndexBuffer(mesh->indexBuffer, section.firstIndex, section.indexCount);
                    bgfx::setState(
                        BGFX_STATE_WRITE_RGB |
                        BGFX_STATE_WRITE_Z |
                        BGFX_STATE_DEPTH_TEST_LESS |
                        BGFX_STATE_CULL_CW);
                    bgfx::submit(shadowViewId(cascadeIndex), skinnedShadowProgram_);
                    ++stats_.shadowPassDraws;
                    ++stats_.shadowPassDrawsByCascade[cascadeIndex];
                    ++stats_.shadowCasterBatchesByCascade[cascadeIndex];
                    ++stats_.shadowSkinnedMeshCastersByCascade[cascadeIndex];
                    ++stats_.shadowSkinnedMeshPassDraws;
                }
            }
        }
    }
    else if (shadowCascadeResourceCount_ > 0)
    {
        destroyShadowResources();
        stats_.shadowResourceReconfigured = 1;
        stats_.shadowMapResolution = 0;
        stats_.shadowCascadeRenderTargetCount = 0;
        for (std::uint32_t cascadeIndex = 0; cascadeIndex < kMaxDirectionalShadowCascades; ++cascadeIndex)
        {
            stats_.shadowCascadeRenderTargetValid[cascadeIndex] = 0;
        }
    }
#endif

    RenderPacket forwardPacket = packet;
    forwardPacket.selectionOutline.enabled = false;
    forwardPacket.ssao.enabled = false;
    forwardPacket.decals = nullptr;
#if FULL_RENDERER_ENABLE_BGFX
    const scene::DecalRenderPlan decalScenePlan = buildCameraCulledDecalPlan(packet);
    const scene::ParticleRenderPlan particleScenePlan = buildPostParticlePlan(packet);
    const bool selectedObjects = hasSelectedObjects(packet, batches, batchCount);
    const scene::PostPassPlan preResourcePostPlan =
        makePostPassPlanForPacket(packet, decalScenePlan, particleScenePlan, selectedObjects, false, false);
    forceSceneTargetForSubmit_ = preResourcePostPlan.sceneTargetRequired;
    suppressPostScenePasses_ = true;
#endif
    const RendererResult drawResult = submit(forwardPacket);
#if FULL_RENDERER_ENABLE_BGFX
    suppressPostScenePasses_ = false;
    forceSceneTargetForSubmit_ = false;
#endif
    if (drawResult != RendererResult::Success)
    {
        return drawResult;
    }

    const scene::WeatherRenderPlan weatherPlan =
        scene::makeWeatherRenderPlan(packet.weather, packet.environment);
    const EnvironmentDesc& effectiveEnvironment = weatherPlan.environment;
#if FULL_RENDERER_ENABLE_BGFX
    applyWeatherStats(weatherPlan);
#endif

    float cameraPositionWorld[3] = {};
    const bool hasCameraPosition =
        scene::extractCameraPositionFromViewMatrix(packet.view.view, cameraPositionWorld);
    std::vector<scene::TransparentSortItem> transparentSkinnedDraws;
    transparentSkinnedDraws.reserve(packet.animatedDrawCount);
    for (std::uint32_t drawIndex = 0; drawIndex < packet.animatedDrawCount; ++drawIndex)
    {
        const AnimatedDrawItem& draw = packet.animatedDraws[drawIndex];
        const MaterialResource* material = findMaterial(draw.material);
        if (material == nullptr)
        {
            continue;
        }
        const scene::FadeRenderState fadeState = scene::makeFadeRenderState(draw.fade);
        if (!scene::isTransparentRenderBucket(
                scene::renderBucketForMaterialAndFade(material->desc, fadeState)))
        {
            continue;
        }

        scene::TransparentSortItem sortItem;
        sortItem.submissionIndex = drawIndex;
        sortItem.stableOrder = drawIndex;
        float centerWorld[3] = {};
        centerFromModelTranslation(draw.model, centerWorld);
        sortItem.validDistance = hasCameraPosition &&
            scene::computeSortDistanceSquared(centerWorld, cameraPositionWorld, sortItem.distanceSquared);
        transparentSkinnedDraws.push_back(sortItem);
    }
    scene::stableSortTransparentBackToFront(
        transparentSkinnedDraws.empty() ? nullptr : transparentSkinnedDraws.data(),
        static_cast<std::uint32_t>(transparentSkinnedDraws.size()));

    const auto renderSkinnedDraw = [&](const std::uint32_t drawIndex, const bool transparentSorted) -> RendererResult {
        const AnimatedDrawItem& draw = packet.animatedDraws[drawIndex];
        const SkinnedMeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveSkinnedMeshMaterial(draw.mesh, draw.material, mesh, material, true) ||
            draw.palette.skinningMatrices == nullptr ||
            draw.palette.matrixCount == 0 ||
            draw.palette.matrixCount > kMaxSkinningJoints)
        {
            ++stats_.skippedAnimatedDraws;
            return RendererResult::InvalidArgument;
        }

        const scene::FadeRenderState fadeState = scene::makeFadeRenderState(draw.fade);
        recordFadeStats(fadeState, stats_);
        recordMaterialDrawStats(material->desc, fadeState, transparentSorted, stats_);
        markShaderVariant(
            scene::makeForwardShaderVariantKey(
                material->desc,
                scene::ShaderVariantPass::ForwardSkinned,
                effectiveEnvironment.fogEnabled,
                terrainShadowsEnabled &&
                    scene::materialReceivesCsmByPolicy(material->desc) &&
                    draw.receivesShadow,
                fadeState),
            shaderVariantMaskThisFrame_,
            stats_);
#if FULL_RENDERER_ENABLE_BGFX
        const float materialColor[4] = {
            material->desc.baseColorLinear[0],
            material->desc.baseColorLinear[1],
            material->desc.baseColorLinear[2],
            material->desc.lit ? material->desc.baseColorLinear[3] : -material->desc.baseColorLinear[3]};
        float palette[kMaxSkinningJoints][16] = {};
        for (std::uint32_t matrixIndex = 0; matrixIndex < draw.palette.matrixCount; ++matrixIndex)
        {
            std::memcpy(
                palette[matrixIndex],
                draw.palette.skinningMatrices + matrixIndex * 16U,
                sizeof(float) * 16U);
        }

        bgfx::setUniform(materialColorUniform_, materialColor);
        bgfx::setUniform(skinningPaletteUniform_, palette, kMaxSkinningJoints);
        bindEnvironmentState(effectiveEnvironment);
        bindWeatherState(weatherPlan);
        bindFadeState(fadeState, material->desc);
        bindBasicMaterialTextureState(material->desc);
        bindCsmForwardState(
            packet,
            shadowViewProjMatrices,
            cascadeSplits,
            activeShadowCascadeCount,
            terrainShadowsEnabled && material->desc.lit && draw.receivesShadow);
        bgfx::setTransform(draw.model);
        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        const SkinnedMeshSectionDesc& section = mesh->sections[draw.sectionIndex];
        bgfx::setIndexBuffer(mesh->indexBuffer, section.firstIndex, section.indexCount);
        bgfx::setState(forwardMeshStateForMaterialAndFade(material->desc, fadeState));
        bgfx::submit(kForwardViewId, skinnedForwardProgram_);
#endif
        if (packet.directionalShadow.enabled && material->desc.lit && draw.receivesShadow)
        {
            ++stats_.shadowSkinnedMeshReceivers;
        }
        if (effectiveEnvironment.fogEnabled)
        {
            ++stats_.foggedMeshDraws;
        }
        if (weatherPlan.meshWetnessActive)
        {
            ++stats_.weatherMeshWetnessDraws;
        }
        ++stats_.renderedAnimatedDraws;
        ++stats_.renderedDraws;
        return RendererResult::Success;
    };

    for (std::uint32_t drawIndex = 0; drawIndex < packet.animatedDrawCount; ++drawIndex)
    {
        const MaterialResource* material = findMaterial(packet.animatedDraws[drawIndex].material);
        const scene::FadeRenderState fadeState = scene::makeFadeRenderState(packet.animatedDraws[drawIndex].fade);
        if (material != nullptr &&
            scene::isTransparentRenderBucket(
                scene::renderBucketForMaterialAndFade(material->desc, fadeState)))
        {
            continue;
        }
        const RendererResult skinnedResult = renderSkinnedDraw(drawIndex, false);
        if (skinnedResult != RendererResult::Success)
        {
            return skinnedResult;
        }
    }

    for (const scene::TransparentSortItem& sortItem : transparentSkinnedDraws)
    {
        const RendererResult skinnedResult = renderSkinnedDraw(sortItem.submissionIndex, true);
        if (skinnedResult != RendererResult::Success)
        {
            return skinnedResult;
        }
    }

    std::vector<scene::TransparentSortItem> transparentInstancedBatches;
    transparentInstancedBatches.reserve(batchCount);
    for (std::uint32_t batchIndex = 0; batchIndex < batchCount; ++batchIndex)
    {
        const core::InstancedDrawBatch& batch = batches[batchIndex];
        const MaterialResource* material = findMaterial(batch.material);
        if (material == nullptr || material->desc.kind == MaterialKind::TerrainSplat)
        {
            continue;
        }

        const scene::FadeRenderState fadeState = scene::makeFadeRenderState(batch.fade);
        if (!scene::isTransparentRenderBucket(
                scene::renderBucketForMaterialAndFade(material->desc, fadeState)))
        {
            continue;
        }

        scene::TransparentSortItem sortItem;
        sortItem.submissionIndex = batchIndex;
        sortItem.stableOrder = batchIndex;
        float centerWorld[3] = {};
        centerFromAabb(batch.bounds, centerWorld);
        sortItem.validDistance = hasCameraPosition &&
            scene::computeSortDistanceSquared(centerWorld, cameraPositionWorld, sortItem.distanceSquared);
        transparentInstancedBatches.push_back(sortItem);
    }
    scene::stableSortTransparentBackToFront(
        transparentInstancedBatches.empty() ? nullptr : transparentInstancedBatches.data(),
        static_cast<std::uint32_t>(transparentInstancedBatches.size()));

    const auto renderInstancedBatch = [&](
        const std::uint32_t batchIndex,
        const bool transparentSorted) -> RendererResult {
        const core::InstancedDrawBatch& batch = batches[batchIndex];
        const MeshResource* mesh = nullptr;
        const MaterialResource* material = nullptr;
        if (!resolveMeshMaterial(batch.mesh, batch.material, mesh, material, true) ||
            batch.modelMatrices == nullptr ||
            batch.instanceCount == 0)
        {
            return RendererResult::InvalidArgument;
        }

        scene::FadeRenderState fadeState;
        if (material->desc.kind == MaterialKind::TerrainSplat)
        {
            if (batch.fade.enabled)
            {
                ++stats_.structureFadeUnsupportedTargetCount;
            }
        }
        else
        {
            fadeState = scene::makeFadeRenderState(batch.fade);
            recordFadeStats(fadeState, stats_);
        }
        recordMaterialDrawStats(material->desc, fadeState, transparentSorted, stats_);
        markShaderVariant(
            scene::makeForwardShaderVariantKey(
                material->desc,
                material->desc.kind == MaterialKind::TerrainSplat ?
                    scene::ShaderVariantPass::TerrainSplat :
                    scene::ShaderVariantPass::ForwardInstanced,
                effectiveEnvironment.fogEnabled,
                terrainShadowsEnabled && scene::materialReceivesCsmByPolicy(material->desc),
                fadeState),
            shaderVariantMaskThisFrame_,
            stats_);

#if FULL_RENDERER_ENABLE_BGFX
        constexpr std::uint16_t kInstanceStride = sizeof(float) * 16U;
        const std::uint32_t availableInstances = bgfx::getAvailInstanceDataBuffer(batch.instanceCount, kInstanceStride);
        if (availableInstances < batch.instanceCount)
        {
            return RendererResult::BackendFailure;
        }

        bgfx::InstanceDataBuffer instanceData;
        bgfx::allocInstanceDataBuffer(&instanceData, batch.instanceCount, kInstanceStride);
        for (std::uint32_t instanceIndex = 0; instanceIndex < batch.instanceCount; ++instanceIndex)
        {
            const float* source = batch.modelMatrices + instanceIndex * 16U;
            float* destination = reinterpret_cast<float*>(instanceData.data + instanceIndex * kInstanceStride);
            for (std::uint32_t matrixIndex = 0; matrixIndex < 16U; ++matrixIndex)
            {
                destination[matrixIndex] = source[matrixIndex];
            }
        }

        bgfx::ProgramHandle program = instancedForwardProgram_;
        if (material->desc.kind == MaterialKind::TerrainSplat)
        {
            if (!bgfx::isValid(terrainSplatProgram_))
            {
                return RendererResult::BackendFailure;
            }

            program = terrainSplatProgram_;
            float layerColors[kMaxTerrainMaterialLayers][4] = {};
            for (std::uint32_t layerIndex = 0; layerIndex < kMaxTerrainMaterialLayers; ++layerIndex)
            {
                const TerrainMaterialLayerDesc& layer = material->desc.terrain.layers[layerIndex];
                layerColors[layerIndex][0] = layer.fallbackColorLinear[0];
                layerColors[layerIndex][1] = layer.fallbackColorLinear[1];
                layerColors[layerIndex][2] = layer.fallbackColorLinear[2];
                layerColors[layerIndex][3] = layer.fallbackColorLinear[3];

                const TextureResource* layerTexture = isValid(layer.albedoTexture) ?
                    resolveTexture(layer.albedoTexture, true) :
                    nullptr;
                if (layerTexture == nullptr)
                {
                    ++stats_.fallbackMaterialTextureCount;
                }
                const bgfx::TextureHandle textureHandle =
                    layerTexture != nullptr ? layerTexture->texture : fallbackWhiteTexture_;
                bgfx::setTexture(
                    static_cast<std::uint8_t>(layerIndex),
                    layerSamplers_[layerIndex],
                    textureHandle);

                const TextureResource* normalTexture = isValid(layer.normalTexture) ?
                    resolveTexture(layer.normalTexture, true) :
                    nullptr;
                if (normalTexture == nullptr)
                {
                    ++stats_.fallbackMaterialTextureCount;
                }
                const bgfx::TextureHandle normalTextureHandle =
                    normalTexture != nullptr ? normalTexture->texture : fallbackNormalTexture_;
                bgfx::setTexture(
                    static_cast<std::uint8_t>(9U + layerIndex),
                    normalSamplers_[layerIndex],
                    normalTextureHandle);
            }

            const TextureResource* splatTexture = isValid(batch.splatMap) ?
                resolveTexture(batch.splatMap, true) :
                nullptr;
            if (splatTexture == nullptr)
            {
                ++stats_.fallbackMaterialTextureCount;
            }
            bgfx::setTexture(
                4,
                splatSampler_,
                splatTexture != nullptr ? splatTexture->texture : fallbackSplatTexture_);
            for (std::uint32_t cascadeIndex = 0; cascadeIndex < kMaxDirectionalShadowCascades; ++cascadeIndex)
            {
                const bool hasCascadeTexture =
                    terrainShadowsEnabled &&
                    cascadeIndex < shadowCascadeResourceCount_ &&
                    bgfx::isValid(shadowCascadeResources_[cascadeIndex].colorTexture);
                bgfx::setTexture(
                    static_cast<std::uint8_t>(5U + cascadeIndex),
                    shadowMapSamplers_[cascadeIndex],
                    hasCascadeTexture ? shadowCascadeResources_[cascadeIndex].colorTexture : fallbackWhiteTexture_);
            }
            const float terrainParams[4] = {
                material->desc.terrain.uvScale,
                material->desc.terrain.normalMapStrength,
                material->desc.terrain.flipNormalMapY ? 1.0f : 0.0f,
                0.0f};
            const float shadowParams[4] = {
                terrainShadowsEnabled ? static_cast<float>(activeShadowCascadeCount) : 0.0f,
                scene::clampShadowDepthBias(packet.directionalShadow.depthBias),
                packet.directionalShadow.strength,
                0.0f};
            const std::uint32_t filterTapCount = scene::shadowFilterModeToTapCount(packet.directionalShadow.filterMode);
            const float filterMode = filterTapCount == 9U ? 2.0f : filterTapCount == 4U ? 1.0f : 0.0f;
            const float shadowFilterParams[4] = {
                terrainShadowsEnabled && packet.directionalShadow.cascadeBlendEnabled ?
                    scene::clampCascadeBlendFraction(packet.directionalShadow.cascadeBlendFraction) :
                    0.0f,
                scene::clampShadowSlopeBias(packet.directionalShadow.slopeBias),
                filterMode,
                shadowMapResolution_ > 0 ? 1.0f / static_cast<float>(shadowMapResolution_) : 0.0f};
            bgfx::setUniform(terrainLayerColorUniform_, layerColors, kMaxTerrainMaterialLayers);
            bgfx::setUniform(terrainParamsUniform_, terrainParams);
            bindEnvironmentState(effectiveEnvironment);
            bindWeatherState(weatherPlan);
            bgfx::setUniform(shadowViewProjUniform_, shadowViewProjMatrices, kMaxDirectionalShadowCascades);
            bgfx::setUniform(shadowParamsUniform_, shadowParams);
            bgfx::setUniform(shadowFilterParamsUniform_, shadowFilterParams);
            bgfx::setUniform(cascadeSplitsUniform_, cascadeSplits);
        }
        else
        {
            const float materialColor[4] = {
                material->desc.baseColorLinear[0],
                material->desc.baseColorLinear[1],
                material->desc.baseColorLinear[2],
                material->desc.lit ? material->desc.baseColorLinear[3] : -material->desc.baseColorLinear[3]};
            bgfx::setUniform(materialColorUniform_, materialColor);
            bindEnvironmentState(effectiveEnvironment);
            bindWeatherState(weatherPlan);
            bindFadeState(fadeState, material->desc);
            bindBasicMaterialTextureState(material->desc);
            bindCsmForwardState(
                packet,
                shadowViewProjMatrices,
                cascadeSplits,
                activeShadowCascadeCount,
                terrainShadowsEnabled && material->desc.lit);
        }

        bgfx::setVertexBuffer(0, mesh->vertexBuffer);
        bgfx::setIndexBuffer(mesh->indexBuffer);
        bgfx::setInstanceDataBuffer(&instanceData);
        bgfx::setState(forwardMeshStateForMaterialAndFade(material->desc, fadeState));
        bgfx::submit(kForwardViewId, program);
#endif
        if (packet.directionalShadow.enabled &&
            material->desc.kind != MaterialKind::TerrainSplat &&
            material->desc.lit)
        {
            ++stats_.shadowInstancedMeshReceiverBatches;
        }
        if (effectiveEnvironment.fogEnabled)
        {
            ++stats_.foggedInstancedBatches;
        }
        if (weatherPlan.terrainWetnessActive && material->desc.kind == MaterialKind::TerrainSplat)
        {
            ++stats_.weatherTerrainWetnessDraws;
        }
        else if (weatherPlan.meshWetnessActive && material->desc.kind != MaterialKind::TerrainSplat)
        {
            ++stats_.weatherMeshWetnessDraws;
        }
        ++stats_.renderedDraws;
        return RendererResult::Success;
    };

    for (std::uint32_t batchIndex = 0; batchIndex < batchCount; ++batchIndex)
    {
        const MaterialResource* material = findMaterial(batches[batchIndex].material);
        const scene::FadeRenderState fadeState = scene::makeFadeRenderState(batches[batchIndex].fade);
        if (material != nullptr &&
            material->desc.kind != MaterialKind::TerrainSplat &&
            scene::isTransparentRenderBucket(
                scene::renderBucketForMaterialAndFade(material->desc, fadeState)))
        {
            continue;
        }
        const RendererResult batchResult = renderInstancedBatch(batchIndex, false);
        if (batchResult != RendererResult::Success)
        {
            return batchResult;
        }
    }

    for (const scene::TransparentSortItem& sortItem : transparentInstancedBatches)
    {
        const RendererResult batchResult = renderInstancedBatch(sortItem.submissionIndex, true);
        if (batchResult != RendererResult::Success)
        {
            return batchResult;
        }
    }

    const RendererResult ssaoResult = submitSsao(packet, batches, batchCount);
    if (ssaoResult != RendererResult::Success)
    {
        return ssaoResult;
    }

    const RendererResult decalResult = submitDecals(packet, batches, batchCount);
    if (decalResult != RendererResult::Success)
    {
        return decalResult;
    }

    const RendererResult particleResult = submitParticles(packet, batches, batchCount);
    if (particleResult != RendererResult::Success)
    {
        return particleResult;
    }

    const RendererResult presentResult = submitScenePresent(packet);
    if (presentResult != RendererResult::Success)
    {
        return presentResult;
    }

#if FULL_RENDERER_ENABLE_BGFX
    applyPostPassStats(makePostPassPlanForPacket(
        packet,
        decalScenePlan,
        particleScenePlan,
        selectedObjects,
        sceneTargetActiveThisFrame_ && hasValidSceneTargetResource(),
        sceneTargetActiveThisFrame_ && hasValidSceneTargetResource()));
#endif

    return submitSelectionOutline(packet, batches, batchCount);
}

RendererResult BgfxRenderDevice::submitDebugLines(
    const RenderViewDesc& view,
    const debug::DebugLineVertex* lines,
    const std::uint32_t lineVertexCount)
{
    if (!initialized_)
    {
        return RendererResult::NotInitialized;
    }

    if (!frameInProgress_)
    {
        return RendererResult::FrameNotInProgress;
    }

    if (lineVertexCount == 0)
    {
        return RendererResult::Success;
    }

    if (lines == nullptr || (lineVertexCount % 2U) != 0U)
    {
        return RendererResult::InvalidArgument;
    }

#if FULL_RENDERER_ENABLE_BGFX
    if (!bgfx::isValid(debugLineProgram_))
    {
        return RendererResult::BackendFailure;
    }

    bgfx::setViewTransform(kDebugLineViewId, view.view, view.projection);
    bgfx::setViewRect(
        kDebugLineViewId,
        0,
        0,
        static_cast<std::uint16_t>(backbufferWidth_),
        static_cast<std::uint16_t>(backbufferHeight_));

    const std::uint32_t availableVertices = bgfx::getAvailTransientVertexBuffer(
        lineVertexCount,
        debugLineVertexLayout_);
    if (availableVertices < lineVertexCount)
    {
        return RendererResult::BackendFailure;
    }

    bgfx::TransientVertexBuffer vertexBuffer;
    bgfx::allocTransientVertexBuffer(&vertexBuffer, lineVertexCount, debugLineVertexLayout_);
    std::memcpy(
        vertexBuffer.data,
        lines,
        static_cast<std::size_t>(lineVertexCount) * sizeof(debug::DebugLineVertex));
    bgfx::setVertexBuffer(0, &vertexBuffer);
    bgfx::setState(
        BGFX_STATE_WRITE_RGB |
        BGFX_STATE_WRITE_A |
        BGFX_STATE_DEPTH_TEST_LEQUAL |
        BGFX_STATE_PT_LINES |
        BGFX_STATE_MSAA);
    bgfx::submit(kDebugLineViewId, debugLineProgram_);
#endif
    ++stats_.submittedDraws;
    ++stats_.renderedDraws;
    return RendererResult::Success;
}

RendererResult BgfxRenderDevice::endFrame()
{
    if (!initialized_)
    {
        return RendererResult::NotInitialized;
    }

    if (!frameInProgress_)
    {
        return RendererResult::FrameNotInProgress;
    }

#if FULL_RENDERER_ENABLE_BGFX
    bgfx::frame();
#endif
    ++stats_.frameIndex;
    frameInProgress_ = false;
    return RendererResult::Success;
}

RendererStats BgfxRenderDevice::getStats() const noexcept
{
    return stats_;
}

ShadowPreviewTextureInfo BgfxRenderDevice::getShadowPreviewTexture(const std::uint32_t cascadeIndex) const noexcept
{
    ShadowPreviewTextureInfo info;
#if FULL_RENDERER_ENABLE_BGFX
    info.resolution = shadowMapResolution_;
    info.cascadeCount = shadowCascadeResourceCount_;
    if (cascadeIndex >= shadowCascadeResourceCount_ ||
        !hasValidShadowResources(shadowCascadeResourceCount_))
    {
        return info;
    }

    info.texture = shadowCascadeResources_[cascadeIndex].colorTexture;
    info.valid = bgfx::isValid(info.texture);
#else
    (void)cascadeIndex;
#endif
    return info;
}
} // namespace full_renderer::bgfx_backend
