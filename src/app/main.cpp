#include "full_renderer/Renderer.hpp"

#include "engine/renderer_integration/ChunkTerrainHandleMap.hpp"
#include "engine/renderer_integration/TerrainChunkRequests.hpp"
#include "engine/renderer_integration/TerrainDescriptorBuilder.hpp"
#include "engine/renderer_integration/TerrainIntegrationDiagnostics.hpp"
#include "engine/renderer_integration/TerrainLifecyclePlan.hpp"
#include "engine/renderer_integration/TerrainRendererCommands.hpp"
#include "engine/renderer_integration/TerrainRenderPrep.hpp"
#include "engine/renderer_integration/TerrainResourceCatalog.hpp"
#include "engine/renderer_integration/TerrainSubmissionAdapter.hpp"
#include "engine/renderer_integration/WorldRenderSnapshot.hpp"
#include "engine/world/WorldChunkCatalog.hpp"
#include "engine/world/WorldChunkRegistry.hpp"
#include "engine/world/WorldOrigin.hpp"
#include "engine/world/WorldResidencyRequests.hpp"
#include "engine_bridge/StreamingSeam.hpp"
#if FULL_RENDERER_ENABLE_DEBUG_UI
#include "renderer/bgfx/BgfxRenderDevice.hpp"
#include "renderer/bgfx/ImguiBgfxRenderer.hpp"
#endif
#include "renderer/debug/CsmDebug.hpp"
#include "renderer/debug/FrameBudget.hpp"
#include "renderer/debug/LongSessionChurn.hpp"
#include "renderer/debug/Validation.hpp"
#include "renderer/core/Renderer.hpp"
#include "renderer/scene/ColorGrading.hpp"
#include "renderer/scene/Decal.hpp"
#include "renderer/scene/Environment.hpp"
#include "renderer/terrain/TerrainGridMesh.hpp"
#include "renderer/scene/Shadow.hpp"
#include "renderer/scene/Ssao.hpp"
#include "renderer/scene/Weather.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#if FULL_RENDERER_ENABLE_DEBUG_UI
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#ifndef FULL_RENDERER_SAMPLE_SHADER_DIR
#define FULL_RENDERER_SAMPLE_SHADER_DIR "shaders/dx11"
#endif

namespace
{
struct SdlWindowDeleter
{
    void operator()(SDL_Window* window) const noexcept
    {
        if (window != nullptr)
        {
            SDL_DestroyWindow(window);
        }
    }
};

using SdlWindowPtr = std::unique_ptr<SDL_Window, SdlWindowDeleter>;

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Vec3 operator+(const Vec3 lhs, const Vec3 rhs) noexcept
{
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 operator-(const Vec3 lhs, const Vec3 rhs) noexcept
{
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vec3 operator*(const Vec3 lhs, const float scalar) noexcept
{
    return {lhs.x * scalar, lhs.y * scalar, lhs.z * scalar};
}

float dot(const Vec3 lhs, const Vec3 rhs) noexcept
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(const Vec3 lhs, const Vec3 rhs) noexcept
{
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x};
}

Vec3 normalize(const Vec3 value) noexcept
{
    const float lengthSquared = dot(value, value);
    if (lengthSquared <= 0.000001f)
    {
        return {0.0f, 0.0f, 0.0f};
    }

    const float invLength = 1.0f / std::sqrt(lengthSquared);
    return value * invLength;
}

void setIdentity(float out[16]) noexcept
{
    for (int index = 0; index < 16; ++index)
    {
        out[index] = 0.0f;
    }

    out[0] = 1.0f;
    out[5] = 1.0f;
    out[10] = 1.0f;
    out[15] = 1.0f;
}

void makeTranslation(float out[16], const Vec3 translation) noexcept
{
    setIdentity(out);
    out[12] = translation.x;
    out[13] = translation.y;
    out[14] = translation.z;
}

void makeScaleTranslation(float out[16], const Vec3 scale, const Vec3 translation) noexcept
{
    setIdentity(out);
    out[0] = scale.x;
    out[5] = scale.y;
    out[10] = scale.z;
    out[12] = translation.x;
    out[13] = translation.y;
    out[14] = translation.z;
}

void makeRotationZTranslation(float out[16], const float radians, const Vec3 translation) noexcept
{
    setIdentity(out);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    out[0] = c;
    out[1] = s;
    out[4] = -s;
    out[5] = c;
    out[12] = translation.x;
    out[13] = translation.y;
    out[14] = translation.z;
}

void multiplyMatrices(const float a[16], const float b[16], float out[16]) noexcept
{
    float result[16] = {};
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            result[column * 4 + row] =
                a[0 * 4 + row] * b[column * 4 + 0] +
                a[1 * 4 + row] * b[column * 4 + 1] +
                a[2 * 4 + row] * b[column * 4 + 2] +
                a[3 * 4 + row] * b[column * 4 + 3];
        }
    }
    std::memcpy(out, result, sizeof(result));
}

void makeLookAt(float out[16], const Vec3 eye, const Vec3 target, const Vec3 up) noexcept
{
    const Vec3 zAxis = normalize(eye - target);
    const Vec3 xAxis = normalize(cross(up, zAxis));
    const Vec3 yAxis = cross(zAxis, xAxis);

    out[0] = xAxis.x;
    out[1] = yAxis.x;
    out[2] = zAxis.x;
    out[3] = 0.0f;
    out[4] = xAxis.y;
    out[5] = yAxis.y;
    out[6] = zAxis.y;
    out[7] = 0.0f;
    out[8] = xAxis.z;
    out[9] = yAxis.z;
    out[10] = zAxis.z;
    out[11] = 0.0f;
    out[12] = -dot(xAxis, eye);
    out[13] = -dot(yAxis, eye);
    out[14] = -dot(zAxis, eye);
    out[15] = 1.0f;
}

void makePerspective(float out[16], const float fovYRadians, const float aspect, const float nearZ, const float farZ) noexcept
{
    for (int index = 0; index < 16; ++index)
    {
        out[index] = 0.0f;
    }

    const float f = 1.0f / std::tan(fovYRadians * 0.5f);
    out[0] = f / aspect;
    out[5] = f;
    out[10] = farZ / (nearZ - farZ);
    out[11] = -1.0f;
    out[14] = (nearZ * farZ) / (nearZ - farZ);
}

bool queryWindowSize(SDL_Window* window, std::uint32_t& width, std::uint32_t& height)
{
    int pixelWidth = 0;
    int pixelHeight = 0;
    if (!SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight))
    {
        std::cerr << "Failed to query SDL window pixel size: " << SDL_GetError() << '\n';
        return false;
    }

    if (pixelWidth <= 0 || pixelHeight <= 0)
    {
        std::cerr << "SDL returned an invalid window pixel size.\n";
        return false;
    }

    width = static_cast<std::uint32_t>(pixelWidth);
    height = static_cast<std::uint32_t>(pixelHeight);
    return true;
}

bool fillPlatformWindowDesc(SDL_Window* window, full_renderer::PlatformWindowDesc& desc)
{
    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    if (properties == 0)
    {
        std::cerr << "Failed to query SDL window properties: " << SDL_GetError() << '\n';
        return false;
    }

#if defined(_WIN32)
    desc.nativeDisplay = nullptr;
    desc.nativeWindow = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(__APPLE__)
    desc.nativeDisplay = nullptr;
    desc.nativeWindow = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#elif defined(__linux__)
    const char* videoDriver = SDL_GetCurrentVideoDriver();
    if (videoDriver != nullptr && std::strcmp(videoDriver, "wayland") == 0)
    {
        desc.nativeDisplay = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        desc.nativeWindow = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    }
    else
    {
        desc.nativeDisplay = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        const Sint64 x11Window = SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        desc.nativeWindow = reinterpret_cast<void*>(static_cast<std::uintptr_t>(x11Window));
    }
#else
    (void)properties;
    desc.nativeDisplay = nullptr;
    desc.nativeWindow = nullptr;
#endif

    if (desc.nativeWindow == nullptr)
    {
        std::cerr << "SDL did not expose a native window handle for this platform.\n";
        return false;
    }

    return true;
}

const char* resultName(const full_renderer::RendererResult result)
{
    switch (result)
    {
    case full_renderer::RendererResult::Success:
        return "Success";
    case full_renderer::RendererResult::InvalidDescriptor:
        return "InvalidDescriptor";
    case full_renderer::RendererResult::InvalidArgument:
        return "InvalidArgument";
    case full_renderer::RendererResult::AlreadyInitialized:
        return "AlreadyInitialized";
    case full_renderer::RendererResult::NotInitialized:
        return "NotInitialized";
    case full_renderer::RendererResult::FrameAlreadyInProgress:
        return "FrameAlreadyInProgress";
    case full_renderer::RendererResult::FrameNotInProgress:
        return "FrameNotInProgress";
    case full_renderer::RendererResult::BackendFailure:
        return "BackendFailure";
    case full_renderer::RendererResult::UnsupportedPlatform:
        return "UnsupportedPlatform";
    }

    return "Unknown";
}

const char* splitModeName(const full_renderer::ShadowCascadeSplitMode mode)
{
    switch (mode)
    {
    case full_renderer::ShadowCascadeSplitMode::Uniform:
        return "Uniform";
    case full_renderer::ShadowCascadeSplitMode::Logarithmic:
        return "Logarithmic";
    case full_renderer::ShadowCascadeSplitMode::Practical:
        return "Practical";
    }

    return "Unknown";
}

constexpr std::array<full_renderer::MeshVertex, 24> kCubeVertices = {{
    {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.95f, 0.20f, 0.18f, 1.0f}},
    {{1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.95f, 0.20f, 0.18f, 1.0f}},
    {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.95f, 0.20f, 0.18f, 1.0f}},
    {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.95f, 0.20f, 0.18f, 1.0f}},

    {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.18f, 0.46f, 0.95f, 1.0f}},
    {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.18f, 0.46f, 0.95f, 1.0f}},
    {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.18f, 0.46f, 0.95f, 1.0f}},
    {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.18f, 0.46f, 0.95f, 1.0f}},

    {{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.25f, 0.85f, 0.38f, 1.0f}},
    {{-1.0f, -1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.25f, 0.85f, 0.38f, 1.0f}},
    {{-1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.25f, 0.85f, 0.38f, 1.0f}},
    {{-1.0f, 1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.25f, 0.85f, 0.38f, 1.0f}},

    {{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.95f, 0.78f, 0.22f, 1.0f}},
    {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.95f, 0.78f, 0.22f, 1.0f}},
    {{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.95f, 0.78f, 0.22f, 1.0f}},
    {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.95f, 0.78f, 0.22f, 1.0f}},

    {{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.82f, 0.32f, 0.95f, 1.0f}},
    {{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.82f, 0.32f, 0.95f, 1.0f}},
    {{1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.82f, 0.32f, 0.95f, 1.0f}},
    {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.82f, 0.32f, 0.95f, 1.0f}},

    {{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.95f, 0.45f, 0.18f, 1.0f}},
    {{1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.95f, 0.45f, 0.18f, 1.0f}},
    {{1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.95f, 0.45f, 0.18f, 1.0f}},
    {{-1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.95f, 0.45f, 0.18f, 1.0f}},
}};

constexpr std::array<std::uint16_t, 36> kCubeIndices = {{
    0, 1, 2, 0, 2, 3,
    4, 5, 6, 4, 6, 7,
    8, 9, 10, 8, 10, 11,
    12, 13, 14, 12, 14, 15,
    16, 17, 18, 16, 18, 19,
    20, 21, 22, 20, 22, 23,
}};

full_renderer::Aabb makeBounds(const float minX, const float minY, const float minZ, const float maxX, const float maxY, const float maxZ)
{
    full_renderer::Aabb bounds;
    bounds.min[0] = minX;
    bounds.min[1] = minY;
    bounds.min[2] = minZ;
    bounds.max[0] = maxX;
    bounds.max[1] = maxY;
    bounds.max[2] = maxZ;
    return bounds;
}

full_renderer::Aabb makeCubeBounds(const Vec3 center, const Vec3 scale)
{
    return makeBounds(
        center.x - scale.x,
        center.y - scale.y,
        center.z - scale.z,
        center.x + scale.x,
        center.y + scale.y,
        center.z + scale.z);
}

full_renderer::MeshHandle createTerrainMesh(
    full_renderer::IRenderer& renderer,
    const float size,
    const std::uint32_t subdivisions,
    const float skirtDepthMeters)
{
    const std::uint32_t verticesPerSide = subdivisions + 1U;
    std::vector<float> heights(static_cast<std::size_t>(verticesPerSide) * verticesPerSide);
    for (std::uint32_t z = 0; z < verticesPerSide; ++z)
    {
        const float v = static_cast<float>(z) / static_cast<float>(subdivisions);
        for (std::uint32_t x = 0; x < verticesPerSide; ++x)
        {
            const float u = static_cast<float>(x) / static_cast<float>(subdivisions);
            const float rolling = 0.18f * std::sin(u * 6.283185307f) * std::cos(v * 6.283185307f);
            const float ridge = 0.22f * std::sin((u + v) * 3.1415926535f);
            heights[static_cast<std::size_t>(z) * verticesPerSide + x] = 0.28f + rolling + ridge;
        }
    }

    const full_renderer::terrain::TerrainGridMeshData meshData =
        full_renderer::terrain::generateTerrainGridMeshWithHeights(
            subdivisions,
            size,
            heights.data(),
            static_cast<std::uint32_t>(heights.size()),
            full_renderer::terrain::TerrainGridMeshOptions{skirtDepthMeters});
    return renderer.createMesh(meshData.meshDesc());
}

full_renderer::TextureHandle createCheckerTexture(
    full_renderer::IRenderer& renderer,
    const std::array<std::uint8_t, 4> a,
    const std::array<std::uint8_t, 4> b)
{
    constexpr std::uint32_t kSize = 8;
    std::array<std::uint8_t, kSize * kSize * 4U> pixels = {};
    for (std::uint32_t y = 0; y < kSize; ++y)
    {
        for (std::uint32_t x = 0; x < kSize; ++x)
        {
            const std::array<std::uint8_t, 4>& color = (((x / 2U) + (y / 2U)) % 2U) == 0U ? a : b;
            const std::uint32_t offset = (y * kSize + x) * 4U;
            pixels[offset + 0U] = color[0];
            pixels[offset + 1U] = color[1];
            pixels[offset + 2U] = color[2];
            pixels[offset + 3U] = color[3];
        }
    }

    full_renderer::TextureDesc desc;
    desc.width = kSize;
    desc.height = kSize;
    desc.format = full_renderer::TextureFormat::Rgba8;
    desc.semantic = full_renderer::TextureSemantic::NormalMap;
    desc.colorSpace = full_renderer::TextureColorSpace::EncodedNormal;
    desc.data = pixels.data();
    desc.dataSizeBytes = static_cast<std::uint32_t>(pixels.size());
    return renderer.createTexture(desc);
}

full_renderer::TextureHandle createTerrainNormalTexture(
    full_renderer::IRenderer& renderer,
    const std::uint32_t layerIndex)
{
    constexpr std::uint32_t kSize = 16;
    std::array<std::uint8_t, kSize * kSize * 4U> pixels = {};
    for (std::uint32_t y = 0; y < kSize; ++y)
    {
        for (std::uint32_t x = 0; x < kSize; ++x)
        {
            const float u = static_cast<float>(x) / static_cast<float>(kSize - 1U);
            const float v = static_cast<float>(y) / static_cast<float>(kSize - 1U);
            const float waveX = std::sin((u * (2.0f + static_cast<float>(layerIndex)) + v) * 6.283185307f);
            const float waveY = std::cos((v * (2.5f + static_cast<float>(layerIndex)) - u) * 6.283185307f);
            const float strength = 0.10f + 0.04f * static_cast<float>(layerIndex);
            float nx = waveX * strength;
            float ny = waveY * strength;
            float nz = 1.0f;
            const float inverseLength = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
            nx *= inverseLength;
            ny *= inverseLength;
            nz *= inverseLength;

            const std::uint32_t offset = (y * kSize + x) * 4U;
            pixels[offset + 0U] = static_cast<std::uint8_t>(std::clamp(nx * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f);
            pixels[offset + 1U] = static_cast<std::uint8_t>(std::clamp(ny * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f);
            pixels[offset + 2U] = static_cast<std::uint8_t>(std::clamp(nz * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f);
            pixels[offset + 3U] = 255;
        }
    }

    full_renderer::TextureDesc desc;
    desc.width = kSize;
    desc.height = kSize;
    desc.format = full_renderer::TextureFormat::Rgba8;
    desc.semantic = full_renderer::TextureSemantic::TerrainSplat;
    desc.colorSpace = full_renderer::TextureColorSpace::Linear;
    desc.data = pixels.data();
    desc.dataSizeBytes = static_cast<std::uint32_t>(pixels.size());
    return renderer.createTexture(desc);
}

full_renderer::TextureHandle createSplatTexture(full_renderer::IRenderer& renderer)
{
    constexpr std::uint32_t kSize = 16;
    std::array<std::uint8_t, kSize * kSize * 4U> pixels = {};
    for (std::uint32_t y = 0; y < kSize; ++y)
    {
        for (std::uint32_t x = 0; x < kSize; ++x)
        {
            const float u = static_cast<float>(x) / static_cast<float>(kSize - 1U);
            const float v = static_cast<float>(y) / static_cast<float>(kSize - 1U);
            const float grass = std::max(0.0f, 1.0f - u - 0.35f * v);
            const float dirt = std::max(0.0f, u * (1.0f - v));
            const float rock = std::max(0.0f, v * 0.85f);
            const float sand = std::max(0.0f, (1.0f - u) * v * 0.65f);
            const float sum = std::max(0.0001f, grass + dirt + rock + sand);
            const std::uint32_t offset = (y * kSize + x) * 4U;
            pixels[offset + 0U] = static_cast<std::uint8_t>(255.0f * grass / sum);
            pixels[offset + 1U] = static_cast<std::uint8_t>(255.0f * dirt / sum);
            pixels[offset + 2U] = static_cast<std::uint8_t>(255.0f * rock / sum);
            pixels[offset + 3U] = static_cast<std::uint8_t>(255.0f * sand / sum);
        }
    }

    full_renderer::TextureDesc desc;
    desc.width = kSize;
    desc.height = kSize;
    desc.format = full_renderer::TextureFormat::Rgba8;
    desc.data = pixels.data();
    desc.dataSizeBytes = static_cast<std::uint32_t>(pixels.size());
    return renderer.createTexture(desc);
}

full_renderer::TextureHandle createSampleDecalTexture(
    full_renderer::IRenderer& renderer,
    const std::array<std::uint8_t, 4> centerColor,
    const std::array<std::uint8_t, 4> edgeColor)
{
    constexpr std::uint32_t kSize = 32;
    std::array<std::uint8_t, kSize * kSize * 4U> pixels = {};
    for (std::uint32_t y = 0; y < kSize; ++y)
    {
        for (std::uint32_t x = 0; x < kSize; ++x)
        {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(kSize) * 2.0f - 1.0f;
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(kSize) * 2.0f - 1.0f;
            const float radius = std::sqrt(u * u + v * v);
            const float ring = std::abs(radius - 0.45f);
            const float inner = std::clamp(1.0f - radius * 1.35f, 0.0f, 1.0f);
            const float stripe = (std::abs(u) < 0.08f || std::abs(v) < 0.08f) ? 0.35f : 0.0f;
            const float alpha = std::clamp(1.0f - ring * 4.5f + stripe + inner * 0.25f, 0.0f, 1.0f);
            const float colorMix = std::clamp(inner + stripe, 0.0f, 1.0f);
            const std::uint32_t offset = (y * kSize + x) * 4U;
            for (std::uint32_t channel = 0; channel < 3U; ++channel)
            {
                pixels[offset + channel] = static_cast<std::uint8_t>(
                    static_cast<float>(edgeColor[channel]) * (1.0f - colorMix) +
                    static_cast<float>(centerColor[channel]) * colorMix);
            }
            pixels[offset + 3U] = static_cast<std::uint8_t>(255.0f * alpha);
        }
    }

    full_renderer::TextureDesc desc;
    desc.width = kSize;
    desc.height = kSize;
    desc.format = full_renderer::TextureFormat::Rgba8;
    desc.data = pixels.data();
    desc.dataSizeBytes = static_cast<std::uint32_t>(pixels.size());
    return renderer.createTexture(desc);
}

full_renderer::TextureHandle createSampleParticleTexture(full_renderer::IRenderer& renderer)
{
    constexpr std::uint32_t kSize = 32;
    std::array<std::uint8_t, kSize * kSize * 4U> pixels = {};
    for (std::uint32_t y = 0; y < kSize; ++y)
    {
        for (std::uint32_t x = 0; x < kSize; ++x)
        {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(kSize) * 2.0f - 1.0f;
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(kSize) * 2.0f - 1.0f;
            const float radius = std::sqrt(u * u + v * v);
            const float alpha = std::clamp(1.0f - radius, 0.0f, 1.0f);
            const float softAlpha = alpha * alpha * (3.0f - 2.0f * alpha);
            const std::uint32_t offset = (y * kSize + x) * 4U;
            pixels[offset + 0U] = 220;
            pixels[offset + 1U] = 230;
            pixels[offset + 2U] = 236;
            pixels[offset + 3U] = static_cast<std::uint8_t>(255.0f * softAlpha);
        }
    }

    full_renderer::TextureDesc desc;
    desc.width = kSize;
    desc.height = kSize;
    desc.format = full_renderer::TextureFormat::Rgba8;
    desc.data = pixels.data();
    desc.dataSizeBytes = static_cast<std::uint32_t>(pixels.size());
    return renderer.createTexture(desc);
}

full_renderer::WeatherDesc makeSampleWeatherDesc()
{
    full_renderer::WeatherDesc weather = full_renderer::scene::makeDefaultWeatherDesc();
    weather.enabled = true;
    weather.wind.enabled = true;
    weather.wind.directionWorld[0] = 0.85f;
    weather.wind.directionWorld[1] = 0.0f;
    weather.wind.directionWorld[2] = -0.35f;
    weather.wind.speedMetersPerSecond = 4.5f;
    weather.wind.gustStrength = 0.15f;
    weather.precipitation.enabled = true;
    weather.precipitation.type = full_renderer::PrecipitationType::Rain;
    weather.precipitation.intensity = 0.35f;
    weather.precipitation.directionWorld[0] = 0.18f;
    weather.precipitation.directionWorld[1] = -1.0f;
    weather.precipitation.directionWorld[2] = 0.08f;
    weather.precipitation.particleTintLinear[0] = 0.68f;
    weather.precipitation.particleTintLinear[1] = 0.76f;
    weather.precipitation.particleTintLinear[2] = 0.88f;
    weather.precipitation.particleTintLinear[3] = 0.42f;
    weather.precipitation.particleSizeScale = 1.0f;
    weather.precipitation.particleAlphaScale = 0.9f;
    weather.wetness.enabled = true;
    weather.wetness.amount = 0.28f;
    weather.wetness.darkeningAmount = 0.16f;
    weather.wetness.terrainWetnessEnabled = true;
    weather.wetness.meshWetnessEnabled = true;
    weather.fogBlend.enabled = true;
    weather.fogBlend.weatherFogColorLinear[0] = 0.50f;
    weather.fogBlend.weatherFogColorLinear[1] = 0.56f;
    weather.fogBlend.weatherFogColorLinear[2] = 0.60f;
    weather.fogBlend.blendAmount = 0.22f;
    weather.fogBlend.fogDistanceScale = 0.78f;
    return weather;
}

const char* precipitationTypeName(const full_renderer::PrecipitationType type) noexcept
{
    switch (type)
    {
    case full_renderer::PrecipitationType::Rain:
        return "rain";
    case full_renderer::PrecipitationType::Snow:
        return "snow";
    case full_renderer::PrecipitationType::Dust:
        return "dust";
    case full_renderer::PrecipitationType::None:
    default:
        return "none";
    }
}

const char* tonemapOperatorName(const full_renderer::TonemapOperator operatorType) noexcept
{
    switch (operatorType)
    {
    case full_renderer::TonemapOperator::Reinhard:
        return "Reinhard";
    case full_renderer::TonemapOperator::AcesApproximation:
        return "ACES approximation";
    case full_renderer::TonemapOperator::None:
    default:
        return "None";
    }
}

const char* colorGradingDebugModeName(const full_renderer::ColorGradingDebugMode mode) noexcept
{
    switch (mode)
    {
    case full_renderer::ColorGradingDebugMode::TonemapOnly:
        return "Tonemap only";
    case full_renderer::ColorGradingDebugMode::GradingOnly:
        return "Grading only";
    case full_renderer::ColorGradingDebugMode::None:
    default:
        return "None";
    }
}

const char* postPresentModeName(const std::uint32_t mode) noexcept
{
    switch (mode)
    {
    case 1:
        return "Scene present";
    case 2:
        return "Color graded scene present";
    case 0:
    default:
        return "Direct swapchain";
    }
}

struct SampleValidationState
{
    std::uint32_t activePresetIndex = 0;
    std::uint32_t activeBookmarkIndex = 0;
};

struct SampleOpenWorldChurnState
{
    bool enabled = false;
    bool running = false;
    bool heavy = false;
    bool materialFallbackChurn = true;
    bool decalParticleChurn = true;
    bool resizeChurn = false;
    bool optionalPassToggleChurn = true;
    bool engineStreamingSeam = false;
    bool largeWorldOriginShift = false;
    int seed = 0x5eed1234;
    int targetFrameCount = 600;
    int simulatedFrame = 0;
    int chunkRadius = 5;
    int churnRate = 3;
    int originShiftPeriod = 64;
    double originWorld[3] = {1000000.0, 0.0, -1000000.0};
    full_renderer::debug::LongSessionChurnSummary lastSummary = {};
    full_renderer::engine_bridge::EngineStreamingChurnSummary seamSummary = {};
};

struct SampleEngineTerrainPipelineState
{
    full_engine::WorldRenderSnapshot snapshot = {};
    full_engine::TerrainRenderPrep prep = {};
    full_engine::TerrainLifecyclePlan lifecycle = {};
    full_engine::TerrainRendererCommandList commands = {};
    full_engine::TerrainDescriptorBuildResult descriptors = {};
    full_engine::TerrainSubmissionResult submission = {};
};

struct SampleTerrainChunkState
{
    full_engine::ChunkId id = {};
    full_engine::WorldChunkDesc worldDesc = {};
    full_engine::TerrainChunkResourceDesc resourceDesc = {};
    bool setupRegistered = true;
    bool resident = true;
};

struct SampleTerrainResidencyControls
{
    int selectedChunkX = 0;
    int selectedChunkZ = 0;
    int batchRingRadius = 0;
    bool reloadCenterAfterUnload = false;
};

full_engine::WorldBounds toEngineWorldBounds(const full_renderer::Aabb& bounds) noexcept
{
    full_engine::WorldBounds worldBounds;
    worldBounds.min.x = bounds.min[0];
    worldBounds.min.y = bounds.min[1];
    worldBounds.min.z = bounds.min[2];
    worldBounds.max.x = bounds.max[0];
    worldBounds.max.y = bounds.max[1];
    worldBounds.max.z = bounds.max[2];
    return worldBounds;
}

void refreshTerrainSubmitHandles(
    const full_engine::ChunkTerrainHandleMap& handleMap,
    std::vector<full_renderer::TerrainChunkHandle>& terrainChunks)
{
    terrainChunks.clear();
    for (const full_engine::ChunkTerrainHandleRecord& record : handleMap.records())
    {
        terrainChunks.push_back(record.handle);
    }
}

void destroyMappedTerrainChunks(
    full_renderer::IRenderer& renderer,
    full_engine::ChunkTerrainHandleMap& handleMap)
{
    for (const full_engine::ChunkTerrainHandleRecord& record : handleMap.records())
    {
        renderer.destroyTerrainChunk(record.handle);
    }
    handleMap.clear();
}

void removeSampleTerrainSetup(
    full_engine::WorldChunkRegistry& registry,
    full_engine::WorldChunkCatalog& worldCatalog,
    full_engine::TerrainResourceCatalog& resources,
    std::vector<SampleTerrainChunkState>& chunks)
{
    full_engine::TerrainChunkRequestQueue requests;
    for (const SampleTerrainChunkState& chunk : chunks)
    {
        if (chunk.setupRegistered)
        {
            requests.pushRemove(chunk.id);
        }
    }

    const full_engine::TerrainChunkRequestApplyResult result = requests.applyTo(registry, worldCatalog, resources);
    for (const full_engine::TerrainChunkRequestRecord& record : result.records)
    {
        switch (record.status)
        {
        case full_engine::TerrainChunkRequestStatus::Applied:
        case full_engine::TerrainChunkRequestStatus::NotFound:
            break;
        case full_engine::TerrainChunkRequestStatus::PartialFailure:
            std::cerr << "Sample terrain setup cleanup repaired drift for chunk ("
                      << record.request.id.x << ", " << record.request.id.y << ", " << record.request.id.z << ").\n";
            break;
        case full_engine::TerrainChunkRequestStatus::InvalidArgument:
        case full_engine::TerrainChunkRequestStatus::AlreadySatisfied:
            std::cerr << "Sample terrain setup cleanup returned unexpected status for chunk ("
                      << record.request.id.x << ", " << record.request.id.y << ", " << record.request.id.z << ").\n";
            break;
        }
    }
    chunks.clear();
}

SampleTerrainChunkState* findSampleTerrainChunk(
    std::vector<SampleTerrainChunkState>& chunks,
    const full_engine::ChunkId& id) noexcept
{
    for (SampleTerrainChunkState& chunk : chunks)
    {
        if (chunk.id == id)
        {
            return &chunk;
        }
    }
    return nullptr;
}

const SampleTerrainChunkState* findSampleTerrainChunk(
    const std::vector<SampleTerrainChunkState>& chunks,
    const full_engine::ChunkId& id) noexcept
{
    for (const SampleTerrainChunkState& chunk : chunks)
    {
        if (chunk.id == id)
        {
            return &chunk;
        }
    }
    return nullptr;
}

std::size_t countResidentSampleTerrainChunks(const std::vector<SampleTerrainChunkState>& chunks) noexcept
{
    std::size_t count = 0;
    for (const SampleTerrainChunkState& chunk : chunks)
    {
        if (chunk.setupRegistered && chunk.resident)
        {
            ++count;
        }
    }
    return count;
}

std::size_t countRegisteredSampleTerrainChunks(const std::vector<SampleTerrainChunkState>& chunks) noexcept
{
    std::size_t count = 0;
    for (const SampleTerrainChunkState& chunk : chunks)
    {
        if (chunk.setupRegistered)
        {
            ++count;
        }
    }
    return count;
}

bool isTerrainSetupRegistered(
    const full_engine::WorldChunkRegistry& registry,
    const full_engine::WorldChunkCatalog& worldCatalog,
    const full_engine::TerrainResourceCatalog& resources,
    const full_engine::ChunkId& id)
{
    return registry.contains(id) && worldCatalog.contains(id) && resources.contains(id);
}

void mirrorSampleTerrainSetupState(
    const full_engine::WorldChunkRegistry& registry,
    const full_engine::WorldChunkCatalog& worldCatalog,
    const full_engine::TerrainResourceCatalog& resources,
    SampleTerrainChunkState& chunk)
{
    chunk.setupRegistered = isTerrainSetupRegistered(registry, worldCatalog, resources, chunk.id);
    if (!chunk.setupRegistered)
    {
        chunk.resident = false;
        return;
    }

    const full_engine::WorldChunkInfo* info = registry.findChunk(chunk.id);
    chunk.resident = info != nullptr && info->residency == full_engine::ChunkResidencyState::Resident;
}

bool applySampleTerrainSetupRequests(
    full_engine::TerrainChunkRequestQueue& setupRequests,
    full_engine::WorldChunkRegistry& registry,
    full_engine::WorldChunkCatalog& worldCatalog,
    full_engine::TerrainResourceCatalog& resources,
    std::vector<SampleTerrainChunkState>& chunks,
    full_engine::WorldChunkResidencyRequestQueue& residencyRequests,
    full_engine::TerrainSetupRequestDiagnostics& diagnostics)
{
    if (setupRequests.requestCount() == 0)
    {
        return true;
    }

    const full_engine::TerrainChunkRequestApplyResult result = setupRequests.applyTo(registry, worldCatalog, resources);
    setupRequests.clear();
    diagnostics = full_engine::makeTerrainSetupRequestDiagnostics(result);

    for (const full_engine::TerrainChunkRequestRecord& record : result.records)
    {
        SampleTerrainChunkState* chunk = findSampleTerrainChunk(chunks, record.request.id);
        if (chunk == nullptr)
        {
            continue;
        }

        switch (record.status)
        {
        case full_engine::TerrainChunkRequestStatus::Applied:
        case full_engine::TerrainChunkRequestStatus::AlreadySatisfied:
            if (record.request.type == full_engine::TerrainChunkRequestType::Add)
            {
                chunk->setupRegistered = true;
                if (chunk->resident)
                {
                    residencyRequests.push(chunk->id, full_engine::WorldChunkResidencyRequestType::MakeResident);
                }
            }
            else
            {
                chunk->setupRegistered = false;
                chunk->resident = false;
            }
            break;
        case full_engine::TerrainChunkRequestStatus::NotFound:
            if (record.request.type == full_engine::TerrainChunkRequestType::Remove)
            {
                chunk->setupRegistered = false;
                chunk->resident = false;
            }
            break;
        case full_engine::TerrainChunkRequestStatus::PartialFailure:
            std::cerr << "Sample terrain setup request repaired drift for chunk ("
                      << record.request.id.x << ", " << record.request.id.y << ", " << record.request.id.z << ").\n";
            mirrorSampleTerrainSetupState(registry, worldCatalog, resources, *chunk);
            if (record.request.type == full_engine::TerrainChunkRequestType::Add &&
                chunk->setupRegistered &&
                chunk->resident)
            {
                residencyRequests.push(chunk->id, full_engine::WorldChunkResidencyRequestType::MakeResident);
            }
            break;
        case full_engine::TerrainChunkRequestStatus::InvalidArgument:
            std::cerr << "Sample terrain setup request failed for chunk ("
                      << record.request.id.x << ", " << record.request.id.y << ", " << record.request.id.z << ").\n";
            return false;
        }
    }

    return true;
}

bool runSampleTerrainPipeline(
    full_renderer::IRenderer& renderer,
    const full_engine::WorldChunkRegistry& registry,
    const full_engine::WorldChunkCatalog& catalog,
    const full_engine::TerrainResourceCatalog& resources,
    full_engine::ChunkTerrainHandleMap& handles,
    SampleEngineTerrainPipelineState& pipeline,
    const full_engine::WorldRenderSnapshotOptions& snapshotOptions,
    full_engine::TerrainPipelineDiagnostics& diagnostics,
    const full_engine::TerrainLifecyclePlanOptions& lifecycleOptions = {})
{
    const std::vector<full_engine::WorldChunkDesc> chunkDescs = catalog.descs();

    pipeline.snapshot = full_engine::buildWorldRenderSnapshot(
        registry,
        chunkDescs.data(),
        chunkDescs.size(),
        snapshotOptions);
    pipeline.prep = full_engine::prepareTerrainRenderChunks(pipeline.snapshot);
    pipeline.lifecycle = full_engine::planTerrainLifecycle(pipeline.prep, handles, lifecycleOptions);
    pipeline.commands = full_engine::buildTerrainRendererCommands(pipeline.lifecycle);
    pipeline.descriptors = full_engine::buildTerrainDescriptors(pipeline.commands, resources);
    pipeline.submission = full_engine::submitTerrainCommands(renderer, pipeline.descriptors, pipeline.commands, handles);
    diagnostics = full_engine::makeTerrainPipelineDiagnostics(
        pipeline.snapshot,
        pipeline.prep,
        pipeline.lifecycle,
        pipeline.commands,
        pipeline.descriptors,
        pipeline.submission,
        handles.mappedCount());

    return pipeline.submission.summary.rendererFailedCount == 0 &&
        pipeline.submission.summary.handleMapFailedCount == 0;
}

void mirrorSampleTerrainResidencyApplyResult(
    const full_engine::WorldChunkResidencyApplyResult& result,
    std::vector<SampleTerrainChunkState>& chunks)
{
    for (const full_engine::WorldChunkResidencyRequestRecord& record : result.records)
    {
        if (record.status != full_engine::WorldChunkResidencyRequestStatus::Applied &&
            record.status != full_engine::WorldChunkResidencyRequestStatus::AlreadySatisfied)
        {
            continue;
        }

        if (SampleTerrainChunkState* chunk = findSampleTerrainChunk(chunks, record.request.id))
        {
            chunk->resident = record.finalState == full_engine::ChunkResidencyState::Resident;
        }
    }
}

full_renderer::debug::LongSessionChurnOptions makeOpenWorldChurnOptions(
    const SampleOpenWorldChurnState& state) noexcept
{
    full_renderer::debug::LongSessionChurnOptions options =
        state.heavy ? full_renderer::debug::makeHeavyLongSessionChurnOptions() :
            full_renderer::debug::makeDefaultLongSessionChurnOptions();
    options.seed = static_cast<std::uint32_t>(state.seed);
    options.frameCount = static_cast<std::uint32_t>(std::max(1, state.targetFrameCount));
    options.terrainSlotCount = static_cast<std::uint32_t>(std::max(16, state.chunkRadius * state.chunkRadius * 4));
    options.maxResidentTerrainChunks = static_cast<std::uint32_t>(std::max(4, state.chunkRadius * state.chunkRadius));
    options.maxResidentTerrainChunks = std::min(options.maxResidentTerrainChunks, options.terrainSlotCount);
    options.chunkCreatesPerFrame = static_cast<std::uint32_t>(std::max(1, state.churnRate));
    options.chunkDestroysPerFrame = static_cast<std::uint32_t>(std::max(1, state.churnRate - 1));
    options.materialFallbackChurnEnabled = state.materialFallbackChurn;
    options.decalParticleChurnEnabled = state.decalParticleChurn;
    options.resizeChurnEnabled = state.resizeChurn;
    options.optionalPassToggleChurnEnabled = state.optionalPassToggleChurn;
    return options;
}

full_renderer::engine_bridge::EngineStreamingChurnOptions makeEngineStreamingChurnOptions(
    const SampleOpenWorldChurnState& state) noexcept
{
    full_renderer::engine_bridge::EngineStreamingChurnOptions options;
    options.churn = makeOpenWorldChurnOptions(state);
    options.origin.mode = state.largeWorldOriginShift ?
        full_renderer::engine_bridge::OriginMode::CameraRelative :
        full_renderer::engine_bridge::OriginMode::AbsoluteRenderSpace;
    options.origin.originWorld[0] = state.originWorld[0];
    options.origin.originWorld[1] = state.originWorld[1];
    options.origin.originWorld[2] = state.originWorld[2];
    options.origin.cameraWorld[0] = state.originWorld[0] + 32.0;
    options.origin.cameraWorld[1] = state.originWorld[1] + 8.0;
    options.origin.cameraWorld[2] = state.originWorld[2] + 16.0;
    options.shiftOriginAtInterval = state.largeWorldOriginShift;
    options.originShiftPeriod = static_cast<std::uint32_t>(std::max(1, state.originShiftPeriod));
    return options;
}

void applyCameraBookmark(
    const full_renderer::debug::ValidationCameraBookmark& bookmark,
    Vec3& cameraPosition,
    Vec3& cameraTarget,
    float& cameraFovYRadians,
    float& cameraNearMeters,
    float& cameraFarMeters) noexcept
{
    cameraPosition = {
        bookmark.positionWorld[0],
        bookmark.positionWorld[1],
        bookmark.positionWorld[2]};
    cameraTarget = {
        bookmark.targetWorld[0],
        bookmark.targetWorld[1],
        bookmark.targetWorld[2]};
    cameraFovYRadians = bookmark.fovYRadians;
    cameraNearMeters = bookmark.nearMeters;
    cameraFarMeters = bookmark.farMeters;
}

void applyValidationPreset(
    const full_renderer::debug::ValidationPreset& preset,
    full_renderer::TerrainDebugOptions& terrainDebugOptions,
    full_renderer::AnimationDebugOptions& animationDebugOptions,
    full_renderer::EnvironmentDesc& environment,
    full_renderer::WeatherDesc& weather,
    full_renderer::SsaoDesc& ssao,
    full_renderer::ColorGradingDesc& colorGrading,
    full_renderer::DecalSubmitDesc& decalSubmit,
    full_renderer::ParticleSubmitDesc& particleSubmit,
    full_renderer::SelectionOutlineDesc& selectionOutline,
    bool& sampleParticleEmissionEnabled,
    int& sampleParticleCount,
    bool& sampleWeatherPrecipitationEnabled,
    int& samplePrecipitationParticleBudget,
    bool& selectStaticMeshes,
    bool& selectInstancedBatch,
    bool& selectSkinnedMesh,
    bool& sampleStructureFadeEnabled,
    float& sampleStructureFadeVisibility,
    full_renderer::FadeMode& sampleStructureFadeMode,
    bool& fadeStaticStructure,
    bool& fadeInstancedStructure,
    bool& fadeSkinnedStructure,
    int& sampleStaticDrawCount,
    int& sampleInstancedInstanceCount,
    int& sampleSkinnedDrawCount,
    int& sampleDecalCount,
    SampleOpenWorldChurnState& openWorldChurnState) noexcept
{
    const full_renderer::debug::ValidationFeatureState& features = preset.features;

    terrainDebugOptions.drawChunkBounds = features.debugChunkBounds;
    terrainDebugOptions.drawCombinedOverlay = features.debugCombinedOverlay;
    terrainDebugOptions.drawLodOverlay = features.terrainLodOverlay;
    terrainDebugOptions.drawMaterialOverlay = features.terrainMaterialOverlay;
    terrainDebugOptions.drawSplatFallbackOverlay = features.terrainSplatFallbackOverlay;

    animationDebugOptions.drawBounds = features.animationDebugBounds;
    animationDebugOptions.drawSkeletons = features.animationDebugSkeletons;

    environment = full_renderer::scene::makeDefaultOpenWorldEnvironmentDesc();
    weather = features.weatherEnabled ? makeSampleWeatherDesc() : full_renderer::scene::makeDefaultWeatherDesc();
    weather.enabled = features.weatherEnabled;
    sampleWeatherPrecipitationEnabled = features.sampleWeatherPrecipitationEnabled;
    samplePrecipitationParticleBudget = features.sampleWeatherPrecipitationEnabled ? 160 : 0;

    ssao = full_renderer::scene::makeDefaultSsaoDesc();
    ssao.enabled = features.ssaoEnabled;

    colorGrading = full_renderer::scene::makeDefaultColorGradingDesc();
    colorGrading.enabled = features.colorGradingEnabled;
    colorGrading.tonemap.enabled = features.colorGradingEnabled;

    decalSubmit.enabled = features.projectedDecalsEnabled;
    decalSubmit.debugDrawVolumes = features.decalDebugVolumes;
    decalSubmit.cullAgainstViewFrustum = features.decalFrustumCullingEnabled;
    decalSubmit.maxProjectionDepthMeters = 0.0f;
    decalSubmit.projectionEdgeFadeMeters = 0.0f;

    particleSubmit.enabled = features.particlesEnabled;
    particleSubmit.cullAgainstViewFrustum = true;
    particleSubmit.sortMode = full_renderer::ParticleSortMode::SubmissionOrder;
    particleSubmit.softParticlesEnabled = features.softParticlesEnabled;
    particleSubmit.softParticleFadeDistanceMeters = 0.75f;
    sampleParticleEmissionEnabled = features.sampleParticleEmissionEnabled;
    sampleParticleCount = features.sampleParticleEmissionEnabled ? 96 : 0;

    selectionOutline.enabled = features.selectionOutlineEnabled;
    selectionOutline.thicknessPixels = 3.0f;
    selectionOutline.colorLinear[0] = 1.0f;
    selectionOutline.colorLinear[1] = 0.55f;
    selectionOutline.colorLinear[2] = 0.10f;
    selectionOutline.colorLinear[3] = 0.95f;
    selectStaticMeshes = features.selectStaticMesh;
    selectInstancedBatch = features.selectInstancedBatch;
    selectSkinnedMesh = features.selectSkinnedMesh;

    sampleStructureFadeEnabled = features.structureFadeEnabled;
    sampleStructureFadeVisibility = features.structureFadeEnabled ? 0.45f : 1.0f;
    sampleStructureFadeMode = full_renderer::FadeMode::Dithered;
    fadeStaticStructure = features.structureFadeEnabled;
    fadeInstancedStructure = features.structureFadeEnabled;
    fadeSkinnedStructure = false;
    sampleStaticDrawCount = static_cast<int>(features.sampleStaticDrawCount);
    sampleInstancedInstanceCount = static_cast<int>(features.sampleInstancedInstanceCount);
    sampleSkinnedDrawCount = static_cast<int>(features.sampleSkinnedDrawCount);
    sampleDecalCount = static_cast<int>(features.sampleDecalCount);
    if (features.sampleParticleEmissionEnabled)
    {
        sampleParticleCount = static_cast<int>(features.sampleParticleCount);
    }

    openWorldChurnState.enabled = features.openWorldChurnEnabled;
    openWorldChurnState.running = false;
    openWorldChurnState.heavy = features.openWorldChurnHeavy;
    openWorldChurnState.materialFallbackChurn = features.openWorldMaterialFallbackChurn;
    openWorldChurnState.decalParticleChurn = features.openWorldDecalParticleChurn;
    openWorldChurnState.resizeChurn = features.openWorldResizeChurn;
    openWorldChurnState.optionalPassToggleChurn = features.openWorldOptionalPassChurn;
    openWorldChurnState.engineStreamingSeam = features.engineStreamingSeamEnabled;
    openWorldChurnState.largeWorldOriginShift = features.largeWorldOriginShiftEnabled;
    openWorldChurnState.originWorld[0] = features.largeWorldOriginMeters[0] != 0.0f ?
        static_cast<double>(features.largeWorldOriginMeters[0]) :
        1000000.0;
    openWorldChurnState.originWorld[1] = static_cast<double>(features.largeWorldOriginMeters[1]);
    openWorldChurnState.originWorld[2] = features.largeWorldOriginMeters[2] != 0.0f ?
        static_cast<double>(features.largeWorldOriginMeters[2]) :
        -1000000.0;
    openWorldChurnState.targetFrameCount =
        features.openWorldChurnFrameCount > 0U ? static_cast<int>(features.openWorldChurnFrameCount) : 600;
    openWorldChurnState.chunkRadius =
        features.openWorldChurnChunkRadius > 0U ? static_cast<int>(features.openWorldChurnChunkRadius) : 5;
    openWorldChurnState.churnRate =
        features.openWorldChurnRate > 0U ? static_cast<int>(features.openWorldChurnRate) : 3;
    openWorldChurnState.simulatedFrame = 0;
    openWorldChurnState.lastSummary = {};
    openWorldChurnState.seamSummary = {};
}

#if FULL_RENDERER_ENABLE_DEBUG_UI
void drawCullingCategoryRow(const char* label, const full_renderer::CullingCategoryStats& stats)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%u", stats.submittedCount);
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("%u", stats.visibleCount);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%u", stats.frustumCulledCount);
    ImGui::TableSetColumnIndex(4);
    ImGui::Text("%u", stats.invalidResourceCount);
    ImGui::TableSetColumnIndex(5);
    ImGui::Text("%u", stats.drawSubmissionCount);
    ImGui::TableSetColumnIndex(6);
    ImGui::Text("%u / %u", stats.shadowCasterCount, stats.offCameraShadowCasterCount);
}

void drawTerrainDiagnosticsPanel(
    full_renderer::IRenderer& renderer,
    full_renderer::core::Renderer* concreteRenderer,
    full_renderer::bgfx_backend::ImguiBgfxRenderer& imguiRenderer,
    full_renderer::TerrainDebugOptions& options,
    full_renderer::AnimationDebugOptions& animationDebug,
    full_renderer::EnvironmentDesc& environment,
    full_renderer::WeatherDesc& weather,
    full_renderer::SsaoDesc& ssao,
    full_renderer::ColorGradingDesc& colorGrading,
    full_renderer::DecalSubmitDesc& decalSubmit,
    full_renderer::ParticleSubmitDesc& particleSubmit,
    full_renderer::DirectionalShadowDesc& shadow,
    full_renderer::SelectionOutlineDesc& selectionOutline,
    bool& sampleParticleEmissionEnabled,
    int& sampleParticleCount,
    bool& sampleWeatherPrecipitationEnabled,
    int& samplePrecipitationParticleBudget,
    std::uint32_t samplePrecipitationParticleCount,
    bool& selectStaticMeshes,
    bool& selectInstancedBatch,
    bool& selectSkinnedMesh,
    bool& sampleStructureFadeEnabled,
    float& sampleStructureFadeVisibility,
    full_renderer::FadeMode& sampleStructureFadeMode,
    bool& fadeStaticStructure,
    bool& fadeInstancedStructure,
    bool& fadeSkinnedStructure,
    int& sampleStaticDrawCount,
    int& sampleInstancedInstanceCount,
    int& sampleSkinnedDrawCount,
    int& sampleDecalCount,
    SampleValidationState& validationState,
    SampleOpenWorldChurnState& openWorldChurnState,
    const full_engine::TerrainIntegrationDiagnostics& terrainDiagnostics,
    std::vector<SampleTerrainChunkState>& sampleTerrainChunks,
    full_engine::WorldChunkResidencyRequestQueue& terrainResidencyRequests,
    full_engine::TerrainChunkRequestQueue& terrainSetupRequests,
    SampleTerrainResidencyControls& terrainResidencyControls,
    int terrainGridRadius,
    bool& engineTerrainPipelineDirty,
    Vec3& cameraPosition,
    Vec3& cameraTarget,
    float& cameraFovYRadians,
    float& cameraNearMeters,
    float& cameraFarMeters,
    const full_renderer::DirectionalShadowDesc& submittedShadow,
    const full_renderer::TerrainMaterialDesc& terrainMaterial,
    const bool terrainSkirtsEnabled,
    const float terrainSkirtDepthMeters,
    const full_renderer::DirectionalLightDesc& submittedLight,
    const full_renderer::RenderViewDesc& submittedView,
    const std::uint32_t viewportWidth,
    const std::uint32_t viewportHeight,
    full_renderer::debug::ShadowMapPreviewSettings& shadowPreview,
    bool& showDebugUi)
{
    if (!showDebugUi)
    {
        return;
    }

    const full_renderer::TerrainStats stats = renderer.getTerrainStats();
    ImGui::Begin("Terrain Diagnostics", &showDebugUi);
    ImGui::Checkbox("Chunk bounds", &options.drawChunkBounds);
    ImGui::Checkbox("LOD overlay", &options.drawLodOverlay);
    ImGui::Checkbox("Material overlay", &options.drawMaterialOverlay);
    ImGui::Checkbox("Splat fallback overlay", &options.drawSplatFallbackOverlay);
    ImGui::Checkbox("Combined diagnostics", &options.drawCombinedOverlay);
    ImGui::Separator();
    ImGui::Text("Chunks: %u visible / %u submitted", stats.visibleChunks, stats.submittedChunks);
    ImGui::Text("Culled: %u", stats.culledChunks);
    ImGui::Text("Residency: %u resident, %u slots, %u inactive",
        stats.residentChunks,
        stats.allocatedChunkSlots,
        stats.nonResidentChunks);
    ImGui::Text("Chunk churn: +%u, -%u, reused %u",
        stats.chunksCreatedSinceLastSubmit,
        stats.chunksDestroyedSinceLastSubmit,
        stats.chunkSlotsReusedSinceLastSubmit);
    ImGui::Text("Lifetime churn: created %u, destroyed %u, reused %u",
        stats.totalChunkCreateCount,
        stats.totalChunkDestroyCount,
        stats.totalChunkSlotReuseCount);
    ImGui::Text("Invalid/stale submitted handles: %u / %u",
        stats.invalidHandleChunks,
        stats.staleHandleChunks);
    ImGui::Text("Terrain draws/batches: %u", stats.terrainDraws);
    ImGui::Text("Splat residency: %u resident, %u fallback",
        stats.splatMapResidentChunks,
        stats.splatMapFallbackChunks);
    ImGui::Text("LOD fallback chunks: %u", stats.lodFallbackChunks);
    ImGui::Text("Shadow casters: %u total, %u off-camera",
        stats.shadowCasterChunks,
        stats.offCameraShadowCasterChunks);
    ImGui::TextUnformatted("Origin policy: caller-rebased float world coordinates");
    ImGui::TextUnformatted("Layer texture fallbacks: backend-local");

    if (ImGui::CollapsingHeader("Engine terrain pipeline"))
    {
        const full_engine::TerrainPipelineDiagnostics& pipeline = terrainDiagnostics.pipeline;
        const full_engine::TerrainSetupRequestDiagnostics& terrainSetupDiagnostics = terrainDiagnostics.setupRequests;
        const full_engine::TerrainResidencyRequestDiagnostics& terrainResidencyDiagnostics =
            terrainDiagnostics.residencyRequests;

        ImGui::Text("Handle map: %llu", static_cast<unsigned long long>(pipeline.handleCount));
        ImGui::Text(
            "Snapshot: ready %llu, not resident %llu, missing %llu, invalid %llu, out of range %llu",
            static_cast<unsigned long long>(pipeline.snapshotReadyCount),
            static_cast<unsigned long long>(pipeline.snapshotNotResidentCount),
            static_cast<unsigned long long>(pipeline.snapshotMissingChunkCount),
            static_cast<unsigned long long>(pipeline.snapshotInvalidBoundsCount),
            static_cast<unsigned long long>(pipeline.snapshotOutOfRangeCount));
        ImGui::Text(
            "Prep: ready %llu, skipped %llu",
            static_cast<unsigned long long>(pipeline.prep.readyCount),
            static_cast<unsigned long long>(
                pipeline.prep.skippedNotResidentCount +
                pipeline.prep.skippedMissingChunkCount +
                pipeline.prep.skippedInvalidBoundsCount +
                pipeline.prep.skippedOutOfRangeCount));
        ImGui::Text(
            "Lifecycle: create %llu, keep %llu, update %llu, release %llu",
            static_cast<unsigned long long>(pipeline.lifecycle.createCount),
            static_cast<unsigned long long>(pipeline.lifecycle.keepCount),
            static_cast<unsigned long long>(pipeline.lifecycle.updateCount),
            static_cast<unsigned long long>(pipeline.lifecycle.releaseCount));
        ImGui::Text(
            "Commands: create %llu, keep %llu, update %llu, destroy %llu",
            static_cast<unsigned long long>(pipeline.commands.createCount),
            static_cast<unsigned long long>(pipeline.commands.keepCount),
            static_cast<unsigned long long>(pipeline.commands.updateCount),
            static_cast<unsigned long long>(pipeline.commands.destroyCount));
        ImGui::Text(
            "Descriptors: ready %llu, missing %llu, invalid %llu, ignored %llu",
            static_cast<unsigned long long>(pipeline.descriptors.readyCount),
            static_cast<unsigned long long>(pipeline.descriptors.missingResourcesCount),
            static_cast<unsigned long long>(pipeline.descriptors.invalidResourcesCount),
            static_cast<unsigned long long>(pipeline.descriptors.ignoredCount));
        ImGui::Text(
            "Submission: created %llu, updated %llu, destroyed %llu, kept %llu, skipped %llu, failures %llu",
            static_cast<unsigned long long>(pipeline.submission.createdCount),
            static_cast<unsigned long long>(pipeline.submission.updatedCount),
            static_cast<unsigned long long>(pipeline.submission.destroyedCount),
            static_cast<unsigned long long>(pipeline.submission.keptCount),
            static_cast<unsigned long long>(pipeline.submission.skippedCount),
            static_cast<unsigned long long>(
                pipeline.submission.rendererFailedCount + pipeline.submission.handleMapFailedCount));

        ImGui::Separator();
        ImGui::Text(
            "Sample setup: %llu / %llu registered, %llu desired resident",
            static_cast<unsigned long long>(countRegisteredSampleTerrainChunks(sampleTerrainChunks)),
            static_cast<unsigned long long>(sampleTerrainChunks.size()),
            static_cast<unsigned long long>(countResidentSampleTerrainChunks(sampleTerrainChunks)));
        ImGui::Text(
            "Queued setup/residency requests: %llu / %llu",
            static_cast<unsigned long long>(terrainSetupRequests.requestCount()),
            static_cast<unsigned long long>(terrainResidencyRequests.requestCount()));
        ImGui::Text(
            "Last setup requests: total %llu, add %llu, remove %llu",
            static_cast<unsigned long long>(terrainSetupDiagnostics.requestCount),
            static_cast<unsigned long long>(terrainSetupDiagnostics.addCount),
            static_cast<unsigned long long>(terrainSetupDiagnostics.removeCount));
        ImGui::Text(
            "Last setup results: applied %llu, satisfied %llu, missing %llu, invalid %llu, partial %llu",
            static_cast<unsigned long long>(terrainSetupDiagnostics.summary.appliedCount),
            static_cast<unsigned long long>(terrainSetupDiagnostics.summary.alreadySatisfiedCount),
            static_cast<unsigned long long>(terrainSetupDiagnostics.summary.notFoundCount),
            static_cast<unsigned long long>(terrainSetupDiagnostics.summary.invalidArgumentCount),
            static_cast<unsigned long long>(terrainSetupDiagnostics.summary.partialFailureCount));
        ImGui::Text(
            "Last residency requests: total %llu, resident %llu, unloaded %llu",
            static_cast<unsigned long long>(terrainResidencyDiagnostics.requestCount),
            static_cast<unsigned long long>(terrainResidencyDiagnostics.makeResidentCount),
            static_cast<unsigned long long>(terrainResidencyDiagnostics.makeUnloadedCount));
        ImGui::Text(
            "Last residency results: applied %llu, satisfied %llu, missing %llu, invalid %llu",
            static_cast<unsigned long long>(terrainResidencyDiagnostics.summary.appliedCount),
            static_cast<unsigned long long>(terrainResidencyDiagnostics.summary.alreadySatisfiedCount),
            static_cast<unsigned long long>(terrainResidencyDiagnostics.summary.notFoundCount),
            static_cast<unsigned long long>(terrainResidencyDiagnostics.summary.invalidTransitionCount));
        ImGui::InputInt("Chunk X", &terrainResidencyControls.selectedChunkX);
        ImGui::InputInt("Chunk Z", &terrainResidencyControls.selectedChunkZ);
        terrainResidencyControls.selectedChunkX =
            std::max(-terrainGridRadius, std::min(terrainGridRadius, terrainResidencyControls.selectedChunkX));
        terrainResidencyControls.selectedChunkZ =
            std::max(-terrainGridRadius, std::min(terrainGridRadius, terrainResidencyControls.selectedChunkZ));

        const full_engine::ChunkId selectedChunkId = {
            static_cast<std::int32_t>(terrainResidencyControls.selectedChunkX),
            0,
            static_cast<std::int32_t>(terrainResidencyControls.selectedChunkZ)};
        SampleTerrainChunkState* selectedChunk = findSampleTerrainChunk(sampleTerrainChunks, selectedChunkId);
        const bool selectedRegistered = selectedChunk != nullptr && selectedChunk->setupRegistered;
        ImGui::Text(
            "Selected chunk: %s / %s",
            selectedRegistered ? "registered" : "unregistered",
            selectedRegistered && selectedChunk->resident ? "resident" : "unloaded");
        if (ImGui::Button("Add Setup") && selectedChunk != nullptr && !selectedChunk->setupRegistered)
        {
            selectedChunk->resident = true;
            terrainSetupRequests.pushAdd(selectedChunk->worldDesc, selectedChunk->resourceDesc);
            engineTerrainPipelineDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove Setup") && selectedRegistered)
        {
            selectedChunk->resident = false;
            terrainSetupRequests.pushRemove(selectedChunkId);
            engineTerrainPipelineDirty = true;
        }
        ImGui::InputInt("Ring Radius", &terrainResidencyControls.batchRingRadius);
        terrainResidencyControls.batchRingRadius =
            std::max(0, std::min(terrainGridRadius, terrainResidencyControls.batchRingRadius));
        if (ImGui::Button("Remove Ring"))
        {
            bool queuedAny = false;
            for (SampleTerrainChunkState& chunk : sampleTerrainChunks)
            {
                const int ringRadius = std::max(std::abs(chunk.id.x), std::abs(chunk.id.z));
                if (ringRadius == terrainResidencyControls.batchRingRadius && chunk.setupRegistered)
                {
                    chunk.resident = false;
                    terrainSetupRequests.pushRemove(chunk.id);
                    queuedAny = true;
                }
            }
            engineTerrainPipelineDirty = engineTerrainPipelineDirty || queuedAny;
        }
        ImGui::SameLine();
        if (ImGui::Button("Restore Ring"))
        {
            bool queuedAny = false;
            for (SampleTerrainChunkState& chunk : sampleTerrainChunks)
            {
                const int ringRadius = std::max(std::abs(chunk.id.x), std::abs(chunk.id.z));
                if (ringRadius == terrainResidencyControls.batchRingRadius && !chunk.setupRegistered)
                {
                    chunk.resident = true;
                    terrainSetupRequests.pushAdd(chunk.worldDesc, chunk.resourceDesc);
                    queuedAny = true;
                }
            }
            engineTerrainPipelineDirty = engineTerrainPipelineDirty || queuedAny;
        }
        if (ImGui::Button("Make Resident") && selectedRegistered)
        {
            selectedChunk->resident = true;
            terrainResidencyRequests.push(selectedChunkId, full_engine::WorldChunkResidencyRequestType::MakeResident);
            engineTerrainPipelineDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Make Unloaded") && selectedRegistered)
        {
            selectedChunk->resident = false;
            terrainResidencyRequests.push(selectedChunkId, full_engine::WorldChunkResidencyRequestType::MakeUnloaded);
            engineTerrainPipelineDirty = true;
        }
        if (ImGui::Button("Reload Center"))
        {
            const full_engine::ChunkId centerChunkId = {};
            if (SampleTerrainChunkState* centerChunk = findSampleTerrainChunk(sampleTerrainChunks, centerChunkId))
            {
                if (centerChunk->setupRegistered)
                {
                    centerChunk->resident = false;
                    terrainResidencyRequests.push(centerChunkId, full_engine::WorldChunkResidencyRequestType::MakeUnloaded);
                    terrainResidencyControls.reloadCenterAfterUnload = true;
                    engineTerrainPipelineDirty = true;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset All Resident"))
        {
            for (SampleTerrainChunkState& chunk : sampleTerrainChunks)
            {
                if (chunk.setupRegistered)
                {
                    chunk.resident = true;
                    terrainResidencyRequests.push(
                        chunk.id,
                        full_engine::WorldChunkResidencyRequestType::MakeResident);
                }
            }
            engineTerrainPipelineDirty = true;
        }
    }

    if (ImGui::BeginTable("Terrain LOD Counts", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("LOD");
        ImGui::TableSetupColumn("Visible chunks");
        ImGui::TableSetupColumn("Batches");
        ImGui::TableHeadersRow();
        for (std::uint32_t lodIndex = 0; lodIndex < full_renderer::kMaxTerrainLodLevels; ++lodIndex)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", lodIndex);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", stats.visibleChunksByLod[lodIndex]);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", stats.terrainBatchesByLod[lodIndex]);
        }
        ImGui::EndTable();
    }

    const std::uint32_t chunkDebugCount = renderer.copyTerrainDebugInfo(nullptr, 0);
    const std::uint32_t batchDebugCount = renderer.copyTerrainBatchDebugInfo(nullptr, 0);
    ImGui::Separator();
    ImGui::Text("Chunk debug records: %u", chunkDebugCount);
    ImGui::Text("Batch debug records: %u", batchDebugCount);
    if (ImGui::CollapsingHeader("Terrain Surface"))
    {
        ImGui::Text("Geometric normals: generated from chunk-local grid triangles");
        ImGui::Text("Layer normal maps: enabled");
        ImGui::Text("Normal strength: %.2f", terrainMaterial.normalMapStrength);
        ImGui::Text("Flip normal-map Y: %s", terrainMaterial.flipNormalMapY ? "yes" : "no");
        ImGui::Text("Skirts: %s", terrainSkirtsEnabled ? "enabled" : "disabled");
        ImGui::Text("Skirt depth: %.2f m", terrainSkirtDepthMeters);
        ImGui::TextUnformatted("Chunk debug bounds include skirt depth in the sample.");
        ImGui::TextUnformatted("Neighbor-aware edge normals: deferred");
        ImGui::TextUnformatted("Runtime material editing requires recreating the terrain material.");
    }
    if (ImGui::CollapsingHeader("Animation"))
    {
        ImGui::Checkbox("Draw skeletons", &animationDebug.drawSkeletons);
        ImGui::Checkbox("Draw skinned bounds", &animationDebug.drawBounds);
        const full_renderer::RendererStats rendererStats = renderer.getStats();
        ImGui::Text("Skeletons: %u live", rendererStats.liveSkeletons);
        ImGui::Text("Skinned meshes: %u live", rendererStats.liveSkinnedMeshes);
        ImGui::Text(
            "Animated draws: %u submitted, %u rendered, %u skipped",
            rendererStats.submittedAnimatedDraws,
            rendererStats.renderedAnimatedDraws,
            rendererStats.skippedAnimatedDraws);
        ImGui::Text(
            "Skinned shadows: %u pass draws, %u receivers",
            rendererStats.shadowSkinnedMeshPassDraws,
            rendererStats.shadowSkinnedMeshReceivers);
        ImGui::Text("Animation debug line vertices: %u", rendererStats.animationDebugLineVertices);
        ImGui::TextUnformatted("Sample pose palettes are generated by the app and submitted frame-locally.");
        ImGui::TextUnformatted("Shadow-map preview remains UI-only/deferred.");
    }
    if (ImGui::CollapsingHeader("Selection / Outline", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Outline enabled", &selectionOutline.enabled);
        ImGui::ColorEdit4("Outline color", selectionOutline.colorLinear);
        ImGui::SliderFloat("Thickness", &selectionOutline.thicknessPixels, 0.0f, 8.0f, "%.1f px");
        ImGui::Checkbox("Select static mesh", &selectStaticMeshes);
        ImGui::Checkbox("Select instanced batch", &selectInstancedBatch);
        ImGui::Checkbox("Select skinned mesh", &selectSkinnedMesh);
        const full_renderer::RendererStats rendererStats = renderer.getStats();
        ImGui::Text("Selected static meshes: %u", rendererStats.selectedStaticMeshDraws);
        ImGui::Text("Selected instanced batches: %u (%u instances)",
            rendererStats.selectedInstancedBatches,
            rendererStats.selectedInstances);
        ImGui::Text("Selected skinned meshes: %u", rendererStats.selectedSkinnedMeshDraws);
        ImGui::Text("Mask target: %s", rendererStats.selectionMaskTargetValid ? "valid" : "inactive");
        ImGui::Text("Mask draws: %u, outline passes: %u",
            rendererStats.selectionMaskDraws,
            rendererStats.selectionOutlineDraws);
        ImGui::TextUnformatted("Depth behavior: submitted scene depth prepass; through-wall outlines are deferred.");
        ImGui::TextUnformatted("Instancing selection is batch-level in this milestone.");
    }
    if (ImGui::CollapsingHeader("Structure Fade", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Reset fade defaults"))
        {
            sampleStructureFadeEnabled = true;
            sampleStructureFadeVisibility = 0.45f;
            sampleStructureFadeMode = full_renderer::FadeMode::Dithered;
            fadeStaticStructure = true;
            fadeInstancedStructure = true;
            fadeSkinnedStructure = false;
        }
        ImGui::Checkbox("Fade enabled", &sampleStructureFadeEnabled);
        ImGui::SliderFloat("Visibility", &sampleStructureFadeVisibility, 0.0f, 1.0f, "%.2f");
        int fadeMode = sampleStructureFadeMode == full_renderer::FadeMode::Alpha ? 1 :
            sampleStructureFadeMode == full_renderer::FadeMode::Dithered ? 2 :
            0;
        const char* fadeModeLabels[] = {"None", "Alpha", "Dithered"};
        if (ImGui::Combo("Fade mode", &fadeMode, fadeModeLabels, 3))
        {
            sampleStructureFadeMode = fadeMode == 1 ? full_renderer::FadeMode::Alpha :
                fadeMode == 2 ? full_renderer::FadeMode::Dithered :
                full_renderer::FadeMode::None;
        }
        ImGui::Checkbox("Fade static structure", &fadeStaticStructure);
        ImGui::Checkbox("Fade instanced batch", &fadeInstancedStructure);
        ImGui::Checkbox("Fade skinned mesh", &fadeSkinnedStructure);
        const full_renderer::RendererStats rendererStats = renderer.getStats();
        ImGui::Text("Fade descriptors: %u submitted, %u active",
            rendererStats.structureFadeSubmittedCount,
            rendererStats.structureFadeActiveCount);
        ImGui::Text("Visible/partial/hidden: %u / %u / %u",
            rendererStats.structureFadeFullyVisibleCount,
            rendererStats.structureFadePartiallyFadedCount,
            rendererStats.structureFadeFullyHiddenCount);
        ImGui::Text("Draw modes: %u alpha, %u dither",
            rendererStats.structureFadeAlphaDraws,
            rendererStats.structureFadeDitherDraws);
        ImGui::Text("Invalid fade descriptors: %u", rendererStats.structureFadeInvalidCount);
        ImGui::Text("Unsupported fade targets: %u", rendererStats.structureFadeUnsupportedTargetCount);
        ImGui::TextUnformatted("Fade decisions are sample-owned; no obstruction or building logic runs in the renderer.");
        ImGui::TextUnformatted("Dither uses main-pass depth writes; alpha fade is stable-sorted within each draw family.");
        ImGui::TextUnformatted("Faded structures keep existing full shadow casting in this milestone.");
    }
    if (ImGui::CollapsingHeader("Materials / Shaders / Transparency"))
    {
        const full_renderer::RendererStats rendererStats = renderer.getStats();
        ImGui::Text("Live materials: %u basic, %u terrain splat",
            rendererStats.materialBasicCount,
            rendererStats.materialTerrainSplatCount);
        ImGui::Text("Draw buckets: %u opaque, %u alpha-test, %u alpha-blend",
            rendererStats.materialOpaqueDraws,
            rendererStats.materialAlphaTestDraws,
            rendererStats.materialAlphaBlendDraws);
        ImGui::Text("Transparent sorting: %u sorted, %u submission-order",
            rendererStats.transparentSortedDraws,
            rendererStats.transparentUnsortedDraws);
        ImGui::Text("Particle transparency: %u sorted batches, %u unsorted batches",
            rendererStats.transparentParticleSortedBatches,
            rendererStats.transparentParticleUnsortedBatches);
        ImGui::Text("Dither fade draws: %u", rendererStats.materialDitherFadeDraws);
        ImGui::Text("Fallbacks: %u material, %u material textures",
            rendererStats.fallbackMaterialCount,
            rendererStats.fallbackMaterialTextureCount);
        ImGui::Text("Invalid materials: %u", rendererStats.invalidMaterialCount);
        ImGui::Text("Shader variants observed: %u", rendererStats.shaderVariantCountInUse);
        ImGui::Text("Unsupported variants: %u", rendererStats.unsupportedShaderVariantRequestCount);
        ImGui::Text("Unsupported alpha shadow casters: %u", rendererStats.alphaMaterialShadowUnsupportedCount);
        ImGui::TextUnformatted("Policy: opaque/alpha-test first, projected decals, alpha blend/fade within draw families, particles, then color/outline/debug.");
        ImGui::TextUnformatted("Asset policy: colors are linear in public descriptors; color textures are sRGB metadata, splats/scalars are linear, normal maps are encoded normals.");
        ImGui::TextUnformatted("Runtime uploads remain uncompressed single-mip RGBA8; importer mips/compression are documented future work.");
        ImGui::TextUnformatted("OIT, full cross-family transparent sorting, and alpha-clipped shadow variants are deferred.");
    }
    if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Reset sky/fog defaults"))
        {
            environment = full_renderer::scene::makeDefaultOpenWorldEnvironmentDesc();
        }
        ImGui::Checkbox("Sky", &environment.skyEnabled);
        ImGui::ColorEdit3("Zenith", environment.skyZenithColorLinear);
        ImGui::ColorEdit3("Horizon", environment.skyHorizonColorLinear);
        ImGui::ColorEdit3("Lower sky", environment.skyGroundColorLinear);
        ImGui::Checkbox("Distance fog", &environment.fogEnabled);
        ImGui::ColorEdit3("Fog color (terrain/mesh)", environment.fogColorLinear);
        constexpr float kMinimumFogUiRangeMeters = 0.1f;
        const bool fogStartChanged =
            ImGui::SliderFloat("Fog start", &environment.fogStartMeters, 0.0f, 180.0f, "%.1f m");
        const bool fogEndChanged =
            ImGui::SliderFloat("Fog end", &environment.fogEndMeters, 1.0f, 240.0f, "%.1f m");
        if (fogStartChanged && environment.fogEndMeters <= environment.fogStartMeters + kMinimumFogUiRangeMeters)
        {
            environment.fogEndMeters = environment.fogStartMeters + kMinimumFogUiRangeMeters;
        }
        if (fogEndChanged && environment.fogStartMeters >= environment.fogEndMeters - kMinimumFogUiRangeMeters)
        {
            environment.fogStartMeters = std::max(0.0f, environment.fogEndMeters - kMinimumFogUiRangeMeters);
        }

        const full_renderer::RendererStats rendererStats = renderer.getStats();
        ImGui::Text(
            "Fogged draws: %u mesh, %u instanced/terrain batches",
            rendererStats.foggedMeshDraws,
            rendererStats.foggedInstancedBatches);
        ImGui::Text("Sky rendered: %s", rendererStats.skyRendered ? "yes" : "no");
    }
    if (ImGui::CollapsingHeader("Weather", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Clear"))
        {
            weather = full_renderer::scene::makeDefaultWeatherDesc();
            sampleWeatherPrecipitationEnabled = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Rain"))
        {
            weather = makeSampleWeatherDesc();
            weather.precipitation.type = full_renderer::PrecipitationType::Rain;
            sampleWeatherPrecipitationEnabled = true;
            samplePrecipitationParticleBudget = 160;
        }
        ImGui::SameLine();
        if (ImGui::Button("Snow"))
        {
            weather = makeSampleWeatherDesc();
            weather.precipitation.type = full_renderer::PrecipitationType::Snow;
            weather.precipitation.intensity = 0.30f;
            weather.precipitation.particleTintLinear[0] = 0.86f;
            weather.precipitation.particleTintLinear[1] = 0.90f;
            weather.precipitation.particleTintLinear[2] = 0.96f;
            weather.precipitation.particleTintLinear[3] = 0.55f;
            weather.precipitation.particleSizeScale = 1.8f;
            weather.wind.speedMetersPerSecond = 2.0f;
            weather.wetness.amount = 0.12f;
            weather.fogBlend.blendAmount = 0.18f;
            sampleWeatherPrecipitationEnabled = true;
            samplePrecipitationParticleBudget = 128;
        }
        ImGui::SameLine();
        if (ImGui::Button("Dust"))
        {
            weather = makeSampleWeatherDesc();
            weather.precipitation.type = full_renderer::PrecipitationType::Dust;
            weather.precipitation.intensity = 0.25f;
            weather.precipitation.particleTintLinear[0] = 0.80f;
            weather.precipitation.particleTintLinear[1] = 0.65f;
            weather.precipitation.particleTintLinear[2] = 0.42f;
            weather.precipitation.particleTintLinear[3] = 0.28f;
            weather.precipitation.particleSizeScale = 2.2f;
            weather.wind.speedMetersPerSecond = 7.0f;
            weather.wetness.enabled = false;
            weather.fogBlend.blendAmount = 0.35f;
            weather.fogBlend.fogDistanceScale = 0.65f;
            sampleWeatherPrecipitationEnabled = true;
            samplePrecipitationParticleBudget = 96;
        }
        ImGui::Checkbox("Weather hooks", &weather.enabled);
        ImGui::Checkbox("Wind", &weather.wind.enabled);
        ImGui::DragFloat3("Wind direction", weather.wind.directionWorld, 0.02f, -1.0f, 1.0f);
        ImGui::SliderFloat("Wind speed", &weather.wind.speedMetersPerSecond, 0.0f, 24.0f, "%.1f m/s");
        ImGui::SliderFloat("Gust strength", &weather.wind.gustStrength, 0.0f, 1.0f, "%.2f");

        ImGui::Separator();
        ImGui::Checkbox("Precipitation", &weather.precipitation.enabled);
        int precipitationType = weather.precipitation.type == full_renderer::PrecipitationType::Rain ? 1 :
            weather.precipitation.type == full_renderer::PrecipitationType::Snow ? 2 :
            weather.precipitation.type == full_renderer::PrecipitationType::Dust ? 3 :
            0;
        const char* precipitationLabels[] = {"None", "Rain", "Snow", "Dust"};
        if (ImGui::Combo("Precipitation type", &precipitationType, precipitationLabels, 4))
        {
            weather.precipitation.type = precipitationType == 1 ? full_renderer::PrecipitationType::Rain :
                precipitationType == 2 ? full_renderer::PrecipitationType::Snow :
                precipitationType == 3 ? full_renderer::PrecipitationType::Dust :
                full_renderer::PrecipitationType::None;
        }
        ImGui::SliderFloat("Precipitation intensity", &weather.precipitation.intensity, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat3("Precipitation direction", weather.precipitation.directionWorld, 0.02f, -1.0f, 1.0f);
        ImGui::ColorEdit4("Precipitation tint", weather.precipitation.particleTintLinear);
        ImGui::SliderFloat("Particle size scale", &weather.precipitation.particleSizeScale, 0.0f, 4.0f, "%.2f");
        ImGui::SliderFloat("Particle alpha scale", &weather.precipitation.particleAlphaScale, 0.0f, 1.0f, "%.2f");
        ImGui::Checkbox("Sample precipitation particles", &sampleWeatherPrecipitationEnabled);
        ImGui::SliderInt("Precipitation particle budget", &samplePrecipitationParticleBudget, 0, 256);
        ImGui::Text("Sample precipitation particles submitted: %u", samplePrecipitationParticleCount);

        ImGui::Separator();
        ImGui::Checkbox("Wetness", &weather.wetness.enabled);
        ImGui::SliderFloat("Wetness amount", &weather.wetness.amount, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Wetness darkening", &weather.wetness.darkeningAmount, 0.0f, 0.6f, "%.2f");
        ImGui::Checkbox("Terrain wetness", &weather.wetness.terrainWetnessEnabled);
        ImGui::Checkbox("Mesh wetness", &weather.wetness.meshWetnessEnabled);

        ImGui::Separator();
        ImGui::Checkbox("Fog blend", &weather.fogBlend.enabled);
        ImGui::ColorEdit3("Weather fog color", weather.fogBlend.weatherFogColorLinear);
        ImGui::SliderFloat("Fog blend amount", &weather.fogBlend.blendAmount, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Fog distance scale", &weather.fogBlend.fogDistanceScale, 0.05f, 2.0f, "%.2f");

        const full_renderer::RendererStats rendererStats = renderer.getStats();
        ImGui::Text("Weather state: %s (%s)",
            rendererStats.weatherEnabled ? "enabled" : "disabled",
            rendererStats.weatherNeutral ? "neutral" : "active");
        ImGui::Text("Wind: %s dir %.2f %.2f %.2f at %.1f m/s",
            rendererStats.weatherWindEnabled ? "enabled" : "disabled",
            rendererStats.weatherWindDirectionWorld[0],
            rendererStats.weatherWindDirectionWorld[1],
            rendererStats.weatherWindDirectionWorld[2],
            rendererStats.weatherWindSpeedMetersPerSecond);
        ImGui::Text("Precipitation: %s %s intensity %.2f",
            rendererStats.weatherPrecipitationEnabled ? "enabled" : "disabled",
            precipitationTypeName(rendererStats.weatherPrecipitationType),
            rendererStats.weatherPrecipitationIntensity);
        ImGui::Text("Particles expected: %s",
            rendererStats.weatherPrecipitationUsesParticleBatches ? "sample/engine-owned batches" : "none");
        ImGui::Text("Wetness: %s amount %.2f",
            rendererStats.weatherWetnessEnabled ? "enabled" : "disabled",
            rendererStats.weatherWetnessAmount);
        ImGui::Text("Wetness draws: %u terrain, %u mesh",
            rendererStats.weatherTerrainWetnessDraws,
            rendererStats.weatherMeshWetnessDraws);
        ImGui::Text("Fog blend: %s amount %.2f",
            rendererStats.weatherFogBlendEnabled ? "enabled" : "disabled",
            rendererStats.weatherFogBlendAmount);
        ImGui::Text("Effective fog: %.2f %.2f %.2f, %.1f-%.1f m",
            rendererStats.weatherEffectiveFogColorLinear[0],
            rendererStats.weatherEffectiveFogColorLinear[1],
            rendererStats.weatherEffectiveFogColorLinear[2],
            rendererStats.weatherEffectiveFogStartMeters,
            rendererStats.weatherEffectiveFogEndMeters);
        ImGui::Text("Clamped weather values: %u", rendererStats.weatherClampedValueCount);
        ImGui::TextUnformatted("Weather simulation and precipitation emission are sample-owned.");
    }
    if (ImGui::CollapsingHeader("SSAO"))
    {
        if (ImGui::Button("Reset SSAO defaults"))
        {
            ssao = full_renderer::scene::makeDefaultSsaoDesc();
        }
        ImGui::Checkbox("SSAO enabled", &ssao.enabled);
        ImGui::Checkbox("Half resolution", &ssao.halfResolution);
        ImGui::Checkbox("Blur", &ssao.blurEnabled);
        ImGui::Checkbox("Visualize AO buffer", &ssao.debugVisualize);
        ImGui::SliderFloat("Radius", &ssao.radiusMeters, 0.05f, 8.0f, "%.2f m");
        ImGui::SliderFloat("Blur radius", &ssao.blurRadiusPixels, 0.0f, 4.0f, "%.1f px");
        ImGui::SliderFloat("Intensity", &ssao.intensity, 0.0f, 4.0f, "%.2f");
        ImGui::SliderFloat("Bias", &ssao.biasMeters, 0.0f, 0.5f, "%.3f m");
        ImGui::SliderFloat("Power", &ssao.power, 0.1f, 8.0f, "%.2f");
        ImGui::SliderFloat("Max distance", &ssao.maxDistanceMeters, 1.0f, 180.0f, "%.1f m");
        int sampleCount = ssao.sampleCount >= 8U ? 1 : 0;
        if (ImGui::Combo("Samples", &sampleCount, "4\0" "8\0" "\0"))
        {
            ssao.sampleCount = sampleCount == 0 ? 4U : 8U;
        }
        const full_renderer::RendererStats rendererStats = renderer.getStats();
        ImGui::Text("Viewport: %u x %u", viewportWidth, viewportHeight);
        ImGui::Text("AO resolution: %u x %u", rendererStats.ssaoAoWidth, rendererStats.ssaoAoHeight);
        ImGui::Text("Half resolution active: %s", rendererStats.ssaoHalfResolution ? "yes" : "no");
        ImGui::Text("Depth target: %s", rendererStats.ssaoDepthTargetValid ? "valid" : "inactive");
        ImGui::Text("AO target: %s", rendererStats.ssaoOutputTargetValid ? "valid" : "inactive");
        ImGui::Text("Blur target: %s", rendererStats.ssaoBlurTargetValid ? "valid" : "inactive");
        ImGui::Text("Input depth: %s", rendererStats.ssaoInputDepthValid ? "available" : "unavailable");
        ImGui::Text(
            "Passes: %u depth draws, %u AO, %u blur, %u composite",
            rendererStats.ssaoDepthPassDraws,
            rendererStats.ssaoPassDraws,
            rendererStats.ssaoBlurPassDraws,
            rendererStats.ssaoCompositeDraws);
        ImGui::Text("Blur active: %s", rendererStats.ssaoBlurEnabled ? "yes" : "no");
        ImGui::Text("Composite mode: %s", rendererStats.ssaoDebugVisualized ? "AO debug" : "scene darkening");
        ImGui::TextUnformatted("Depth-only SSAO; blur is simple/non-depth-aware.");
    }
    if (ImGui::CollapsingHeader("Decals"))
    {
        if (ImGui::Button("Reset decal defaults"))
        {
            decalSubmit.enabled = true;
            decalSubmit.debugDrawVolumes = true;
            decalSubmit.cullAgainstViewFrustum = true;
            decalSubmit.maxProjectionDepthMeters = 0.0f;
            decalSubmit.projectionEdgeFadeMeters = 0.0f;
        }
        ImGui::Checkbox("Projected decals", &decalSubmit.enabled);
        ImGui::Checkbox("Draw decal volumes", &decalSubmit.debugDrawVolumes);
        ImGui::Checkbox("Frustum culling", &decalSubmit.cullAgainstViewFrustum);
        ImGui::SliderFloat("Max projection depth", &decalSubmit.maxProjectionDepthMeters, 0.0f, 3.0f, "%.2f m");
        ImGui::SliderFloat("Depth edge fade", &decalSubmit.projectionEdgeFadeMeters, 0.0f, 1.0f, "%.2f m");
        decalSubmit.maxProjectionDepthMeters =
            full_renderer::scene::clampDecalProjectionDepthMeters(decalSubmit.maxProjectionDepthMeters);
        decalSubmit.projectionEdgeFadeMeters =
            full_renderer::scene::clampDecalProjectionEdgeFadeMeters(decalSubmit.projectionEdgeFadeMeters);
        const full_renderer::RendererStats rendererStats = renderer.getStats();
        ImGui::Text("Submitted decals: %u", rendererStats.decalSubmittedCount);
        ImGui::Text("Active decals: %u", rendererStats.decalActiveCount);
        ImGui::Text("Frustum culled decals: %u", rendererStats.decalCulledCount);
        ImGui::Text("Rendered decals: %u", rendererStats.decalRenderedCount);
        ImGui::Text("Rejected decals: %u", rendererStats.decalRejectedCount);
        ImGui::Text("Invalid descriptors: %u", rendererStats.decalInvalidDescriptorRejectCount);
        ImGui::Text("Max-count rejects: %u", rendererStats.decalMaxCountRejectedCount);
        ImGui::Text("Fallback color decals: %u", rendererStats.decalFallbackColorCount);
        ImGui::Text("Debug volumes: %u", rendererStats.decalDebugVolumeCount);
        ImGui::Text("Pass draws: %u", rendererStats.decalPassDraws);
        ImGui::Text("Frustum culling active: %s", rendererStats.decalFrustumCullingEnabled ? "yes" : "no");
        ImGui::Text(
            "Depth tuning: %.2f m max, %.2f m fade",
            rendererStats.decalMaxProjectionDepthMeters,
            rendererStats.decalProjectionEdgeFadeMeters);
        ImGui::Text("Scene color target: %s", rendererStats.decalSceneColorTargetValid ? "valid" : "inactive");
        ImGui::Text("Scene depth target: %s", rendererStats.decalSceneDepthTargetValid ? "valid" : "inactive");
        ImGui::Text("Depth input: %s", rendererStats.decalInputDepthValid ? "available" : "unavailable");
        ImGui::Text("Color input: %s", rendererStats.decalInputColorValid ? "available" : "unavailable");
        ImGui::Text("Projected decals: %s",
            rendererStats.decalProjectionDeferred ? "deferred/no scene input" :
            rendererStats.decalRenderedCount > 0 ? "active-visible" : "inactive");
        ImGui::TextUnformatted("Sample decals use generated RGBA textures where creation succeeds, with color fallback intact.");
        ImGui::TextUnformatted("Albedo/tint decals composite after SSAO using backend-private scene color/depth targets.");
    }
    if (ImGui::CollapsingHeader("Particles"))
    {
        if (ImGui::Button("Reset particle defaults"))
        {
            particleSubmit.enabled = true;
            particleSubmit.cullAgainstViewFrustum = true;
            particleSubmit.sortMode = full_renderer::ParticleSortMode::SubmissionOrder;
            particleSubmit.softParticlesEnabled = true;
            particleSubmit.softParticleFadeDistanceMeters = 0.75f;
            sampleParticleEmissionEnabled = true;
            sampleParticleCount = 96;
        }
        ImGui::Checkbox("Particle rendering", &particleSubmit.enabled);
        ImGui::Checkbox("Sample emission", &sampleParticleEmissionEnabled);
        ImGui::Checkbox("Cull particle batches", &particleSubmit.cullAgainstViewFrustum);
        int particleSortMode = particleSubmit.sortMode == full_renderer::ParticleSortMode::BatchDistanceBackToFront ?
            1 :
            (particleSubmit.sortMode == full_renderer::ParticleSortMode::ParticleDistanceBackToFront ? 2 : 0);
        const char* particleSortLabels[] = {"Submission order", "Batch back-to-front", "Particle back-to-front"};
        if (ImGui::Combo("Sort mode", &particleSortMode, particleSortLabels, 3))
        {
            particleSubmit.sortMode = particleSortMode == 1 ?
                full_renderer::ParticleSortMode::BatchDistanceBackToFront :
                (particleSortMode == 2 ?
                        full_renderer::ParticleSortMode::ParticleDistanceBackToFront :
                        full_renderer::ParticleSortMode::SubmissionOrder);
        }
        ImGui::Checkbox("Soft particles", &particleSubmit.softParticlesEnabled);
        ImGui::SliderFloat(
            "Soft fade distance",
            &particleSubmit.softParticleFadeDistanceMeters,
            0.0f,
            4.0f,
            "%.2f m");
        ImGui::SliderInt("Sample particle count", &sampleParticleCount, 0, 256);
        const full_renderer::RendererStats rendererStats = renderer.getStats();
        ImGui::Text("Batches: %u submitted, %u accepted, %u rejected",
            rendererStats.particleSubmittedBatchCount,
            rendererStats.particleAcceptedBatchCount,
            rendererStats.particleRejectedBatchCount);
        ImGui::Text("Culled batches/particles: %u / %u",
            rendererStats.particleCulledBatchCount,
            rendererStats.particleCulledCount);
        ImGui::Text("Sorted batches/particles: %u / %u",
            rendererStats.particleSortedBatchCount,
            rendererStats.particleSortedCount);
        ImGui::Text("Particles: %u submitted, %u accepted, %u rejected",
            rendererStats.particleSubmittedCount,
            rendererStats.particleAcceptedCount,
            rendererStats.particleRejectedCount);
        ImGui::Text("Soft particles: %s (%s depth, %.2f m fade)",
            rendererStats.particleSoftParticlesEnabled ? "enabled" : "disabled",
            rendererStats.particleSoftParticlesActive ? "active" : "inactive",
            rendererStats.particleSoftParticleFadeDistanceMeters);
        ImGui::Text("Depth input: %s",
            rendererStats.particleSoftParticleDepthInputValid ? "available" : "unavailable");
        ImGui::Text("Fallback texture batches: %u", rendererStats.particleFallbackTextureBatchCount);
        ImGui::Text("Draw calls: %u", rendererStats.particleDrawCalls);
        ImGui::Text("Particle resources: %s", rendererStats.particleResourceValid ? "valid" : "inactive");
        ImGui::TextUnformatted("Sample emission/update is app-owned; the renderer only draws frame-local billboards.");
        ImGui::TextUnformatted("Soft fade uses backend-private scene depth when available; particles do not cast or receive shadows.");
    }
    if (ImGui::CollapsingHeader("Color Grading"))
    {
        if (ImGui::Button("Neutral color"))
        {
            colorGrading = full_renderer::scene::makeDefaultColorGradingDesc();
            colorGrading.enabled = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("High contrast"))
        {
            colorGrading = full_renderer::scene::makeDefaultColorGradingDesc();
            colorGrading.enabled = true;
            colorGrading.tonemap.enabled = true;
            colorGrading.tonemap.operatorType = full_renderer::TonemapOperator::AcesApproximation;
            colorGrading.tonemap.exposureStops = 0.25f;
            colorGrading.contrast = 1.35f;
            colorGrading.saturation = 1.08f;
            colorGrading.gamma = 1.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Desaturated"))
        {
            colorGrading = full_renderer::scene::makeDefaultColorGradingDesc();
            colorGrading.enabled = true;
            colorGrading.tonemap.enabled = true;
            colorGrading.tonemap.operatorType = full_renderer::TonemapOperator::Reinhard;
            colorGrading.tonemap.exposureStops = 0.0f;
            colorGrading.contrast = 1.05f;
            colorGrading.saturation = 0.35f;
        }
        ImGui::Checkbox("Color grading", &colorGrading.enabled);
        ImGui::Checkbox("Tonemap", &colorGrading.tonemap.enabled);
        int tonemapOperator = colorGrading.tonemap.operatorType == full_renderer::TonemapOperator::Reinhard ? 1 :
            colorGrading.tonemap.operatorType == full_renderer::TonemapOperator::AcesApproximation ? 2 :
            0;
        const char* tonemapLabels[] = {"None", "Reinhard", "ACES approximation"};
        if (ImGui::Combo("Tonemap operator", &tonemapOperator, tonemapLabels, 3))
        {
            colorGrading.tonemap.operatorType = tonemapOperator == 1 ?
                full_renderer::TonemapOperator::Reinhard :
                (tonemapOperator == 2 ?
                        full_renderer::TonemapOperator::AcesApproximation :
                        full_renderer::TonemapOperator::None);
        }
        ImGui::SliderFloat("Exposure", &colorGrading.tonemap.exposureStops, -4.0f, 4.0f, "%.2f EV");
        ImGui::SliderFloat("Contrast", &colorGrading.contrast, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Saturation", &colorGrading.saturation, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Gamma", &colorGrading.gamma, 0.4f, 2.5f, "%.2f");
        ImGui::DragFloat3("Lift", colorGrading.liftLinear, 0.005f, -0.25f, 0.25f, "%.3f");
        ImGui::DragFloat3("Gain", colorGrading.gainLinear, 0.01f, 0.0f, 2.0f, "%.2f");
        ImGui::Checkbox("LUT intent", &colorGrading.lut.enabled);
        ImGui::SliderFloat("LUT strength", &colorGrading.lut.strength, 0.0f, 1.0f, "%.2f");
        int debugMode = colorGrading.debugMode == full_renderer::ColorGradingDebugMode::TonemapOnly ? 1 :
            colorGrading.debugMode == full_renderer::ColorGradingDebugMode::GradingOnly ? 2 :
            0;
        const char* debugModeLabels[] = {"None", "Tonemap only", "Grading only"};
        if (ImGui::Combo("Debug output", &debugMode, debugModeLabels, 3))
        {
            colorGrading.debugMode = debugMode == 1 ?
                full_renderer::ColorGradingDebugMode::TonemapOnly :
                (debugMode == 2 ?
                        full_renderer::ColorGradingDebugMode::GradingOnly :
                        full_renderer::ColorGradingDebugMode::None);
        }

        const full_renderer::RendererStats rendererStats = renderer.getStats();
        ImGui::Text("Color grading: %s", rendererStats.colorGradingEnabled ? "enabled" : "disabled");
        ImGui::Text("Final pass: %s", rendererStats.colorGradingPassSubmitted ? "submitted" : "not submitted");
        ImGui::Text("Scene color target: %s",
            rendererStats.colorGradingSceneColorTargetValid ? "valid" : "inactive");
        ImGui::Text("Resources: %s", rendererStats.colorGradingResourceValid ? "valid" : "inactive");
        ImGui::Text("Tonemap: %s %s",
            rendererStats.colorGradingTonemapEnabled ? "enabled" : "disabled",
            tonemapOperatorName(rendererStats.colorGradingTonemapOperator));
        ImGui::Text("Exposure/contrast/saturation/gamma: %.2f / %.2f / %.2f / %.2f",
            rendererStats.colorGradingExposureStops,
            rendererStats.colorGradingContrast,
            rendererStats.colorGradingSaturation,
            rendererStats.colorGradingGamma);
        ImGui::Text("LUT: %s active %s supported %s fallback %u",
            rendererStats.colorGradingLutEnabled ? "requested" : "disabled",
            rendererStats.colorGradingLutActive ? "yes" : "no",
            rendererStats.colorGradingLutSamplingSupported ? "yes" : "no",
            rendererStats.colorGradingLutFallbackCount);
        ImGui::Text("Debug mode: %s", colorGradingDebugModeName(rendererStats.colorGradingDebugMode));
        ImGui::Text("Clamped grading values: %u", rendererStats.colorGradingClampedValueCount);
        ImGui::TextUnformatted("Color grading runs before outlines and debug UI; LUT sampling is a deferred extension point.");
    }
    if (ImGui::CollapsingHeader("Scene / Post Passes"))
    {
        const full_renderer::RendererStats rendererStats = renderer.getStats();
        ImGui::Text(
            "Viewport: %u x %u",
            rendererStats.postViewportWidth,
            rendererStats.postViewportHeight);
        ImGui::Text("Intermediate scene target: %s (%s)",
            rendererStats.postSceneTargetRequired ? "required" : "not required",
            rendererStats.postSceneTargetActive ? "active" : "inactive");
        ImGui::Text(
            "Scene color: %s %u x %u",
            rendererStats.postSceneColorTargetValid ? "valid" : "invalid",
            rendererStats.postSceneColorWidth,
            rendererStats.postSceneColorHeight);
        ImGui::Text(
            "Readable depth: %s %u x %u",
            rendererStats.postSceneDepthTargetValid ? "valid" : "invalid",
            rendererStats.postSceneDepthWidth,
            rendererStats.postSceneDepthHeight);
        ImGui::Text("Readable depth required: %s",
            rendererStats.postReadableSceneDepthRequired ? "yes" : "no");
        ImGui::Text("Final present: %s (%s)",
            rendererStats.postFinalPresentSubmitted ? "ready" : "blocked",
            postPresentModeName(rendererStats.postPresentMode));
        ImGui::Text("Passes: %u planned, %u fullscreen, %u skipped",
            rendererStats.postPassCount,
            rendererStats.postFullscreenPassCount,
            rendererStats.postSkippedPassCount);
        ImGui::Text("Skipped reason mask: 0x%02x", rendererStats.postSkippedPassReasonMask);
        ImGui::Text("Invalid post resources: %u", rendererStats.postInvalidResourceCount);
        ImGui::Text("Scene target reason mask: 0x%02x", rendererStats.postSceneTargetReasonMask);
        ImGui::Text("Scene target recreate: %u this frame, %u lifetime",
            rendererStats.postSceneTargetReconfigured,
            rendererStats.postSceneTargetRecreateCount);
        ImGui::Text("Scene target allocation failures: %u lifetime, %u this frame",
            rendererStats.postSceneTargetAllocationFailureCount,
            rendererStats.postSceneTargetAllocationFailed);
        ImGui::Text("SSAO: %s, decals: %s, particles: %s",
            rendererStats.ssaoPassDraws > 0 ? "submitted" : "skipped",
            rendererStats.decalPassDraws > 0 ? "submitted" : "skipped",
            rendererStats.particleDrawCalls > 0 ? "submitted" : "skipped");
        ImGui::Text("Color grading: %s, outline: %s",
            rendererStats.colorGradingPassSubmitted ? "submitted" : "skipped",
            rendererStats.selectionOutlineDraws > 0 ? "submitted" : "skipped");
        ImGui::TextUnformatted("Order: SSAO, decals, particles, color/scene present, selection outline, debug UI.");
        ImGui::TextUnformatted("Selection masks are separate; hard particles do not require scene depth.");
        ImGui::TextUnformatted("View IDs remain backend-private and are compile-time sanity checked.");
    }
    if (ImGui::CollapsingHeader("Resource Lifetime"))
    {
        const full_renderer::RendererStats rendererStats = renderer.getStats();
        const auto mib = [](const std::uint64_t bytes) {
            return static_cast<double>(bytes) / (1024.0 * 1024.0);
        };
        ImGui::Text("Live resources: meshes %u, skinned %u, textures %u, materials %u, skeletons %u",
            rendererStats.liveMeshes,
            rendererStats.liveSkinnedMeshes,
            rendererStats.liveTextures,
            rendererStats.liveMaterials,
            rendererStats.liveSkeletons);
        ImGui::Text("Estimated total memory: %.2f MiB", mib(rendererStats.totalEstimatedResourceBytes));
        ImGui::Text("Mesh buffers: %.2f MiB static, %.2f MiB skinned",
            mib(rendererStats.meshBufferBytes),
            mib(rendererStats.skinnedMeshBufferBytes));
        ImGui::Text("Textures/materials: %.2f MiB textures, %.2f MiB material descriptors",
            mib(rendererStats.textureBytes),
            mib(rendererStats.materialBytes));
        ImGui::Text("Targets: %.2f MiB shadow, %.2f MiB scene, %.2f MiB SSAO, %.2f MiB selection",
            mib(rendererStats.shadowTargetBytes),
            mib(rendererStats.sceneTargetBytes),
            mib(rendererStats.ssaoTargetBytes),
            mib(rendererStats.selectionTargetBytes));
        ImGui::Text("Fallback textures: %s, %.2f MiB",
            rendererStats.fallbackResourceValid ? "valid" : "inactive",
            mib(rendererStats.fallbackTextureBytes));
        ImGui::Separator();
        ImGui::Text("Allocation failures: mesh %u, skinned %u, texture %u, material %u",
            rendererStats.meshAllocationFailureCount,
            rendererStats.skinnedMeshAllocationFailureCount,
            rendererStats.textureAllocationFailureCount,
            rendererStats.materialAllocationFailureCount);
        ImGui::Text("Target allocation failures: shadow %u, scene %u, selection %u, SSAO %u",
            rendererStats.shadowResourceAllocationFailureCount,
            rendererStats.postSceneTargetAllocationFailureCount,
            rendererStats.selectionMaskAllocationFailureCount,
            rendererStats.ssaoAllocationFailureCount);
        ImGui::Text("Recreates: shadow %u, scene %u, selection %u, SSAO %u",
            rendererStats.shadowResourceRecreateCount,
            rendererStats.postSceneTargetRecreateCount,
            rendererStats.selectionMaskResourceRecreateCount,
            rendererStats.ssaoResourceRecreateCount);
        ImGui::Text("Handle diagnostics: invalid %u, stale %u, rejected submissions %u",
            rendererStats.invalidHandleUseCount,
            rendererStats.staleHandleUseCount,
            rendererStats.destroyedHandleSubmissionCount);
        ImGui::Text("Viewport targets: scene %u x %u, depth %u x %u",
            rendererStats.postSceneColorWidth,
            rendererStats.postSceneColorHeight,
            rendererStats.postSceneDepthWidth,
            rendererStats.postSceneDepthHeight);
        ImGui::TextUnformatted("Memory values are CPU-side estimates; no GPU memory query or backend handles are exposed.");
    }
    if (ImGui::CollapsingHeader("Frame Budget"))
    {
        static bool holdFrameBudgetSnapshot = false;
        static full_renderer::RendererStats heldRendererStats;
        static full_renderer::TerrainStats heldTerrainStats;
        static int cpuBudgetUs = 8000;
        static int stagedBudgetKiB = 4096;
        static int terrainChurnBudget = 128;
        static int drawSubmissionBudget = 2000;
        static int particleBudget = 4096;
        static int decalBudget = static_cast<int>(full_renderer::kMaxFrameDecals);
        static int skinnedBudget = 128;

        if (!holdFrameBudgetSnapshot)
        {
            heldRendererStats = renderer.getStats();
            heldTerrainStats = stats;
        }

        if (ImGui::Button(holdFrameBudgetSnapshot ? "Resume frame budget" : "Hold frame budget"))
        {
            holdFrameBudgetSnapshot = !holdFrameBudgetSnapshot;
            if (holdFrameBudgetSnapshot)
            {
                heldRendererStats = renderer.getStats();
                heldTerrainStats = stats;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset budget thresholds"))
        {
            cpuBudgetUs = 8000;
            stagedBudgetKiB = 4096;
            terrainChurnBudget = 128;
            drawSubmissionBudget = 2000;
            particleBudget = 4096;
            decalBudget = static_cast<int>(full_renderer::kMaxFrameDecals);
            skinnedBudget = 128;
        }

        full_renderer::debug::FrameBudgetThresholds thresholds;
        thresholds.totalCpuMicroseconds = static_cast<std::uint64_t>(std::max(cpuBudgetUs, 0));
        thresholds.totalStagedBytes = static_cast<std::uint64_t>(std::max(stagedBudgetKiB, 0)) * 1024ULL;
        thresholds.terrainChunkChurn = static_cast<std::uint32_t>(std::max(terrainChurnBudget, 0));
        thresholds.drawSubmissionCount = static_cast<std::uint32_t>(std::max(drawSubmissionBudget, 0));
        thresholds.particleCount = static_cast<std::uint32_t>(std::max(particleBudget, 0));
        thresholds.decalCount = static_cast<std::uint32_t>(std::max(decalBudget, 0));
        thresholds.skinnedDrawCount = static_cast<std::uint32_t>(std::max(skinnedBudget, 0));
        const full_renderer::debug::FrameBudgetEvaluation budgetEvaluation =
            full_renderer::debug::evaluateFrameBudget(heldRendererStats, heldTerrainStats, thresholds);
        const full_renderer::FrameBudgetStats& budget = heldRendererStats.frameBudget;

        ImGui::Text("Active preset: %s",
            full_renderer::debug::validationPresetByIndex(validationState.activePresetIndex) != nullptr ?
                full_renderer::debug::validationPresetByIndex(validationState.activePresetIndex)->name :
                "Unknown");
        ImGui::Text("Active camera: %s",
            full_renderer::debug::validationCameraBookmarkByIndex(validationState.activeBookmarkIndex) != nullptr ?
                full_renderer::debug::validationCameraBookmarkByIndex(validationState.activeBookmarkIndex)->name :
                "Unknown");
        ImGui::Text("CPU planning: %.3f ms (%u warning%s)",
            static_cast<double>(budget.totalCpuMicroseconds) / 1000.0,
            budgetEvaluation.warningCount,
            budgetEvaluation.warningCount == 1U ? "" : "s");
        ImGui::Text("Submitted/visible/culled: %u / %u / %u",
            budget.totalSubmittedRenderables,
            budget.totalVisibleRenderables,
            budget.totalCulledRenderables);
        ImGui::Text("Draw/pass estimate: %u", budget.totalDrawSubmissionEstimate);
        ImGui::Text("Staged bytes: %.2f KiB in %u estimated staging group%s",
            static_cast<double>(budget.totalStagedBytes) / 1024.0,
            budget.frameAllocationEstimateCount,
            budget.frameAllocationEstimateCount == 1U ? "" : "s");
        ImGui::Text("Resource churn: terrain +%u, -%u, reused %u; target recreates %u",
            budget.terrainChunksCreatedThisFrame,
            budget.terrainChunksDestroyedThisFrame,
            budget.terrainChunkSlotsReusedThisFrame,
            budget.renderTargetRecreateCount);
        ImGui::Text("Post passes: submitted %u, skipped %u",
            budget.postPassSubmittedCount,
            budget.postPassSkippedCount);

        if (budgetEvaluation.warningMask != 0U)
        {
            ImGui::Text("Warnings: %s%s%s%s%s%s%s",
                (budgetEvaluation.warningMask & full_renderer::debug::kFrameBudgetWarningCpuTime) != 0U ? "CPU " : "",
                (budgetEvaluation.warningMask & full_renderer::debug::kFrameBudgetWarningStagedBytes) != 0U ? "StagedBytes " : "",
                (budgetEvaluation.warningMask & full_renderer::debug::kFrameBudgetWarningTerrainChurn) != 0U ? "TerrainChurn " : "",
                (budgetEvaluation.warningMask & full_renderer::debug::kFrameBudgetWarningDrawSubmissions) != 0U ? "Draws " : "",
                (budgetEvaluation.warningMask & full_renderer::debug::kFrameBudgetWarningParticles) != 0U ? "Particles " : "",
                (budgetEvaluation.warningMask & full_renderer::debug::kFrameBudgetWarningDecals) != 0U ? "Decals " : "",
                (budgetEvaluation.warningMask & full_renderer::debug::kFrameBudgetWarningSkinnedDraws) != 0U ? "Skinned " : "");
        }

        if (ImGui::TreeNode("Budget thresholds"))
        {
            ImGui::InputInt("CPU budget us", &cpuBudgetUs);
            ImGui::InputInt("Staged budget KiB", &stagedBudgetKiB);
            ImGui::InputInt("Terrain churn budget", &terrainChurnBudget);
            ImGui::InputInt("Draw/pass budget", &drawSubmissionBudget);
            ImGui::InputInt("Particle descriptor budget", &particleBudget);
            ImGui::InputInt("Decal descriptor budget", &decalBudget);
            ImGui::InputInt("Skinned draw budget", &skinnedBudget);
            ImGui::TreePop();
        }

        if (ImGui::BeginTable("CPU Stage Timings", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Stage");
            ImGui::TableSetupColumn("us");
            ImGui::TableSetupColumn("ms");
            ImGui::TableHeadersRow();
            for (std::uint32_t stageIndex = 0; stageIndex < full_renderer::kFrameBudgetStageCount; ++stageIndex)
            {
                if ((budget.recordedStageMask & (1U << stageIndex)) == 0U)
                {
                    continue;
                }
                const auto stage = static_cast<full_renderer::FrameBudgetStage>(stageIndex);
                const std::uint64_t microseconds = budget.stageCpuMicroseconds[stageIndex];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(full_renderer::debug::frameBudgetStageName(stage));
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%llu", static_cast<unsigned long long>(microseconds));
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f", static_cast<double>(microseconds) / 1000.0);
            }
            ImGui::EndTable();
        }

        if (ImGui::BeginTable("Staged Byte Estimates", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Category");
            ImGui::TableSetupColumn("KiB");
            ImGui::TableHeadersRow();
            const auto row = [](const char* label, const std::uint64_t bytes) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(label);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2f", static_cast<double>(bytes) / 1024.0);
            };
            row("Static draws", budget.staticDrawStagedBytes);
            row("Instancing", budget.instanceStagedBytes);
            row("Skinned palettes", budget.skinnedPaletteStagedBytes);
            row("Decals", budget.decalStagedBytes);
            row("Particles", budget.particleStagedBytes);
            row("Debug draw", budget.debugDrawStagedBytes);
            ImGui::EndTable();
        }
        ImGui::TextUnformatted("CPU timings and staged bytes are approximate diagnostics, not GPU timings or exact allocator accounting.");
    }
    if (ImGui::CollapsingHeader("Visual Regression"))
    {
        const full_renderer::RendererStats rendererStats = renderer.getStats();
        if (ImGui::BeginCombo(
                "Validation preset",
                full_renderer::debug::validationPresetByIndex(validationState.activePresetIndex) != nullptr ?
                    full_renderer::debug::validationPresetByIndex(validationState.activePresetIndex)->name :
                    "Unknown"))
        {
            for (std::uint32_t index = 0;
                 index < full_renderer::debug::validationPresetCount();
                 ++index)
            {
                const full_renderer::debug::ValidationPreset* preset =
                    full_renderer::debug::validationPresetByIndex(index);
                if (preset == nullptr)
                {
                    continue;
                }
                const bool selected = index == validationState.activePresetIndex;
                if (ImGui::Selectable(preset->name, selected))
                {
                    validationState.activePresetIndex = index;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply preset"))
        {
            const full_renderer::debug::ValidationPreset* preset =
                full_renderer::debug::validationPresetByIndex(validationState.activePresetIndex);
            if (preset != nullptr)
            {
                applyValidationPreset(
                    *preset,
                    options,
                    animationDebug,
                    environment,
                    weather,
                    ssao,
                    colorGrading,
                    decalSubmit,
                    particleSubmit,
                    selectionOutline,
                    sampleParticleEmissionEnabled,
                    sampleParticleCount,
                    sampleWeatherPrecipitationEnabled,
                    samplePrecipitationParticleBudget,
                    selectStaticMeshes,
                    selectInstancedBatch,
                    selectSkinnedMesh,
                    sampleStructureFadeEnabled,
                    sampleStructureFadeVisibility,
                    sampleStructureFadeMode,
                    fadeStaticStructure,
                    fadeInstancedStructure,
                    fadeSkinnedStructure,
                    sampleStaticDrawCount,
                    sampleInstancedInstanceCount,
                    sampleSkinnedDrawCount,
                    sampleDecalCount,
                    openWorldChurnState);
                validationState.activeBookmarkIndex = static_cast<std::uint32_t>(preset->cameraBookmark);
                if (const full_renderer::debug::ValidationCameraBookmark* bookmark =
                        full_renderer::debug::validationCameraBookmark(preset->cameraBookmark))
                {
                    applyCameraBookmark(
                        *bookmark,
                        cameraPosition,
                        cameraTarget,
                        cameraFovYRadians,
                        cameraNearMeters,
                        cameraFarMeters);
                }
            }
        }

        if (ImGui::BeginCombo(
                "Camera bookmark",
                full_renderer::debug::validationCameraBookmarkByIndex(validationState.activeBookmarkIndex) != nullptr ?
                    full_renderer::debug::validationCameraBookmarkByIndex(validationState.activeBookmarkIndex)->name :
                    "Unknown"))
        {
            for (std::uint32_t index = 0;
                 index < full_renderer::debug::validationCameraBookmarkCount();
                 ++index)
            {
                const full_renderer::debug::ValidationCameraBookmark* bookmark =
                    full_renderer::debug::validationCameraBookmarkByIndex(index);
                if (bookmark == nullptr)
                {
                    continue;
                }
                const bool selected = index == validationState.activeBookmarkIndex;
                if (ImGui::Selectable(bookmark->name, selected))
                {
                    validationState.activeBookmarkIndex = index;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply camera"))
        {
            if (const full_renderer::debug::ValidationCameraBookmark* bookmark =
                    full_renderer::debug::validationCameraBookmarkByIndex(validationState.activeBookmarkIndex))
            {
                applyCameraBookmark(
                    *bookmark,
                    cameraPosition,
                    cameraTarget,
                    cameraFovYRadians,
                    cameraNearMeters,
                    cameraFarMeters);
            }
        }

        if (ImGui::Button("Reset validation scene"))
        {
            validationState.activePresetIndex =
                static_cast<std::uint32_t>(full_renderer::debug::ValidationPresetId::CombinedPostPasses);
            const full_renderer::debug::ValidationPreset* preset =
                full_renderer::debug::validationPresetByIndex(validationState.activePresetIndex);
            if (preset != nullptr)
            {
                applyValidationPreset(
                    *preset,
                    options,
                    animationDebug,
                    environment,
                    weather,
                    ssao,
                    colorGrading,
                    decalSubmit,
                    particleSubmit,
                    selectionOutline,
                    sampleParticleEmissionEnabled,
                    sampleParticleCount,
                    sampleWeatherPrecipitationEnabled,
                    samplePrecipitationParticleBudget,
                    selectStaticMeshes,
                    selectInstancedBatch,
                    selectSkinnedMesh,
                    sampleStructureFadeEnabled,
                    sampleStructureFadeVisibility,
                    sampleStructureFadeMode,
                    fadeStaticStructure,
                    fadeInstancedStructure,
                    fadeSkinnedStructure,
                    sampleStaticDrawCount,
                    sampleInstancedInstanceCount,
                    sampleSkinnedDrawCount,
                    sampleDecalCount,
                    openWorldChurnState);
                validationState.activeBookmarkIndex = static_cast<std::uint32_t>(preset->cameraBookmark);
                if (const full_renderer::debug::ValidationCameraBookmark* bookmark =
                        full_renderer::debug::validationCameraBookmark(preset->cameraBookmark))
                {
                    applyCameraBookmark(
                        *bookmark,
                        cameraPosition,
                        cameraTarget,
                        cameraFovYRadians,
                        cameraNearMeters,
                        cameraFarMeters);
                }
            }
        }

        ImGui::Text("Active preset: %s",
            full_renderer::debug::validationPresetByIndex(validationState.activePresetIndex) != nullptr ?
                full_renderer::debug::validationPresetByIndex(validationState.activePresetIndex)->name :
                "Unknown");
        ImGui::Text("Active camera: %s",
            full_renderer::debug::validationCameraBookmarkByIndex(validationState.activeBookmarkIndex) != nullptr ?
                full_renderer::debug::validationCameraBookmarkByIndex(validationState.activeBookmarkIndex)->name :
                "Unknown");
        ImGui::Text("Camera pos: %.2f %.2f %.2f -> %.2f %.2f %.2f",
            cameraPosition.x,
            cameraPosition.y,
            cameraPosition.z,
            cameraTarget.x,
            cameraTarget.y,
            cameraTarget.z);
        ImGui::Text("Projection: %.1f deg, near %.2f m, far %.1f m",
            cameraFovYRadians * 180.0f / 3.1415926535f,
            cameraNearMeters,
            cameraFarMeters);
        ImGui::Text("Outline validation: %s, mask %s, draws %u",
            selectionOutline.enabled ? "enabled" : "disabled",
            rendererStats.selectionMaskTargetValid ? "valid" : "inactive",
            rendererStats.selectionMaskDraws);
        ImGui::Text("Selected static/instanced/skinned: %u / %u / %u",
            rendererStats.selectedStaticMeshDraws,
            rendererStats.selectedInstancedBatches,
            rendererStats.selectedSkinnedMeshDraws);
        ImGui::Text("Decal validation: %u submitted, %u active, %u rendered, %u culled, depth %s",
            rendererStats.decalSubmittedCount,
            rendererStats.decalActiveCount,
            rendererStats.decalRenderedCount,
            rendererStats.decalCulledCount,
            rendererStats.decalInputDepthValid ? "valid" : "inactive");
        ImGui::Text("Combined post status: final %s, scene target %s, skipped 0x%02x",
            rendererStats.postFinalPresentSubmitted ? "ready" : "blocked",
            rendererStats.postSceneTargetActive ? "active" : "inactive",
            rendererStats.postSkippedPassReasonMask);
        ImGui::TextUnformatted("F2 cycles camera bookmarks. F3 cycles validation presets.");
    }
    if (ImGui::CollapsingHeader("Large Scene / Culling Diagnostics"))
    {
        const full_renderer::RendererStats rendererStats = renderer.getStats();
        ImGui::Text("Active preset: %s",
            full_renderer::debug::validationPresetByIndex(validationState.activePresetIndex) != nullptr ?
                full_renderer::debug::validationPresetByIndex(validationState.activePresetIndex)->name :
                "Unknown");
        ImGui::Text("Active camera: %s",
            full_renderer::debug::validationCameraBookmarkByIndex(validationState.activeBookmarkIndex) != nullptr ?
                full_renderer::debug::validationCameraBookmarkByIndex(validationState.activeBookmarkIndex)->name :
                "Unknown");
        ImGui::SliderInt("Static draw stress count", &sampleStaticDrawCount, 0, 128);
        ImGui::SliderInt("Instanced instance stress count", &sampleInstancedInstanceCount, 0, 512);
        ImGui::SliderInt("Skinned draw stress count", &sampleSkinnedDrawCount, 0, 32);
        ImGui::SliderInt("Decal stress count", &sampleDecalCount, 0, static_cast<int>(full_renderer::kMaxFrameDecals));
        if (ImGui::BeginTable("Shared Culling Counts", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Category");
            ImGui::TableSetupColumn("Submitted");
            ImGui::TableSetupColumn("Visible");
            ImGui::TableSetupColumn("Culled");
            ImGui::TableSetupColumn("Invalid");
            ImGui::TableSetupColumn("Draws");
            ImGui::TableSetupColumn("Shadow / offcam");
            ImGui::TableHeadersRow();
            full_renderer::CullingCategoryStats terrainCategory;
            terrainCategory.submittedCount = stats.submittedChunks;
            terrainCategory.residentCount = stats.residentChunks;
            terrainCategory.visibleCount = stats.visibleChunks;
            terrainCategory.frustumCulledCount = stats.culledChunks;
            terrainCategory.invalidResourceCount = stats.invalidHandleChunks + stats.staleHandleChunks;
            terrainCategory.drawSubmissionCount = stats.terrainDraws;
            terrainCategory.shadowCasterCount = stats.shadowCasterChunks;
            terrainCategory.offCameraShadowCasterCount = stats.offCameraShadowCasterChunks;
            drawCullingCategoryRow("Terrain", terrainCategory);
            drawCullingCategoryRow("Static", rendererStats.staticMeshCulling);
            drawCullingCategoryRow("Instanced", rendererStats.instancedMeshCulling);
            drawCullingCategoryRow("Skinned", rendererStats.skinnedMeshCulling);
            full_renderer::CullingCategoryStats decalCategory;
            decalCategory.submittedCount = rendererStats.decalSubmittedCount;
            decalCategory.visibleCount = rendererStats.decalActiveCount;
            decalCategory.frustumCulledCount = rendererStats.decalCulledCount;
            decalCategory.invalidResourceCount = rendererStats.decalRejectedCount;
            decalCategory.drawSubmissionCount = rendererStats.decalPassDraws;
            decalCategory.fallbackResourceCount = rendererStats.decalFallbackColorCount;
            drawCullingCategoryRow("Decals", decalCategory);
            full_renderer::CullingCategoryStats particleCategory;
            particleCategory.submittedCount = rendererStats.particleSubmittedBatchCount;
            particleCategory.visibleCount = rendererStats.particleAcceptedBatchCount;
            particleCategory.frustumCulledCount = rendererStats.particleCulledBatchCount;
            particleCategory.invalidResourceCount = rendererStats.particleRejectedBatchCount;
            particleCategory.drawSubmissionCount = rendererStats.particleDrawCalls;
            particleCategory.fallbackResourceCount = rendererStats.particleFallbackTextureBatchCount;
            drawCullingCategoryRow("Particles", particleCategory);
            ImGui::EndTable();
        }

        ImGui::Text("Instances: submitted %u, particle submitted/rendered %u / %u",
            rendererStats.instancedMeshCulling.submittedCount,
            rendererStats.particleSubmittedCount,
            rendererStats.particleAcceptedCount);
        ImGui::Text("Fallbacks: terrain splat %u, decal color %u, particle texture batches %u",
            stats.splatMapFallbackChunks,
            rendererStats.decalFallbackColorCount,
            rendererStats.particleFallbackTextureBatchCount);
        ImGui::Text("Optional passes: SSAO %s, decals %s, particles %s, color %s, outline %s",
            rendererStats.ssaoEnabled ? "on" : "off",
            rendererStats.decalsEnabled ? "on" : "off",
            rendererStats.particlesEnabled ? "on" : "off",
            rendererStats.colorGradingEnabled ? "on" : "off",
            rendererStats.selectionOutlineEnabled ? "on" : "off");
        if (ImGui::BeginTable("Shadow Caster Cascades", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Cascade");
            ImGui::TableSetupColumn("All");
            ImGui::TableSetupColumn("Terrain");
            ImGui::TableSetupColumn("Static");
            ImGui::TableSetupColumn("Instanced");
            ImGui::TableSetupColumn("Skinned");
            ImGui::TableHeadersRow();
            for (std::uint32_t cascadeIndex = 0; cascadeIndex < full_renderer::kMaxDirectionalShadowCascades; ++cascadeIndex)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", cascadeIndex);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", rendererStats.shadowCasterBatchesByCascade[cascadeIndex]);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%u", rendererStats.shadowTerrainCasterBatchesByCascade[cascadeIndex]);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%u", rendererStats.shadowStaticMeshCastersByCascade[cascadeIndex]);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%u", rendererStats.shadowInstancedMeshCasterBatchesByCascade[cascadeIndex]);
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%u", rendererStats.shadowSkinnedMeshCastersByCascade[cascadeIndex]);
            }
            ImGui::EndTable();
        }
        ImGui::TextUnformatted("Static/instanced/skinned frustum counts are diagnostic unless their public descriptors require camera-culling bounds.");
    }
    if (ImGui::CollapsingHeader("Open World Churn / Long Session"))
    {
        ImGui::Text("Active churn preset: %s",
            openWorldChurnState.enabled ? "enabled by validation preset" : "manual");
        ImGui::Checkbox("Material fallback churn", &openWorldChurnState.materialFallbackChurn);
        ImGui::Checkbox("Decal/particle churn", &openWorldChurnState.decalParticleChurn);
        ImGui::Checkbox("Optional pass toggle churn", &openWorldChurnState.optionalPassToggleChurn);
        ImGui::Checkbox("Resize/post target churn", &openWorldChurnState.resizeChurn);
        ImGui::Checkbox("Engine streaming seam", &openWorldChurnState.engineStreamingSeam);
        ImGui::Checkbox("Camera-relative large-world origin", &openWorldChurnState.largeWorldOriginShift);
        ImGui::Checkbox("Heavy/manual mode", &openWorldChurnState.heavy);
        ImGui::InputInt("Seed", &openWorldChurnState.seed);
        ImGui::SliderInt("Target frames", &openWorldChurnState.targetFrameCount, 1, openWorldChurnState.heavy ? 10000 : 2000);
        ImGui::SliderInt("Chunk radius", &openWorldChurnState.chunkRadius, 2, openWorldChurnState.heavy ? 16 : 10);
        ImGui::SliderInt("Churn rate", &openWorldChurnState.churnRate, 1, openWorldChurnState.heavy ? 12 : 6);
        ImGui::SliderInt("Origin shift interval", &openWorldChurnState.originShiftPeriod, 1, 512);
        ImGui::InputDouble("Origin X", &openWorldChurnState.originWorld[0]);
        ImGui::InputDouble("Origin Z", &openWorldChurnState.originWorld[2]);

        if (ImGui::Button(openWorldChurnState.running ? "Stop churn" : "Start churn"))
        {
            openWorldChurnState.running = !openWorldChurnState.running;
            if (openWorldChurnState.running && openWorldChurnState.simulatedFrame <= 0)
            {
                openWorldChurnState.simulatedFrame = 1;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Run summary"))
        {
            openWorldChurnState.simulatedFrame = std::max(1, openWorldChurnState.targetFrameCount);
            full_renderer::debug::LongSessionChurnOptions churnOptions =
                makeOpenWorldChurnOptions(openWorldChurnState);
            churnOptions.frameCount = static_cast<std::uint32_t>(openWorldChurnState.simulatedFrame);
            openWorldChurnState.lastSummary =
                full_renderer::debug::runLongSessionChurnSimulation(churnOptions);
            if (openWorldChurnState.engineStreamingSeam)
            {
                full_renderer::engine_bridge::EngineStreamingChurnOptions seamOptions =
                    makeEngineStreamingChurnOptions(openWorldChurnState);
                seamOptions.churn.frameCount = static_cast<std::uint32_t>(openWorldChurnState.simulatedFrame);
                openWorldChurnState.seamSummary =
                    full_renderer::engine_bridge::runEngineStreamingSeamChurnSimulation(seamOptions);
            }
            openWorldChurnState.running = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset churn"))
        {
            openWorldChurnState.running = false;
            openWorldChurnState.simulatedFrame = 0;
            openWorldChurnState.lastSummary = {};
            openWorldChurnState.seamSummary = {};
        }

        if (openWorldChurnState.running)
        {
            const int step = openWorldChurnState.heavy ? 250 : 1;
            openWorldChurnState.simulatedFrame =
                std::min(std::max(1, openWorldChurnState.targetFrameCount), openWorldChurnState.simulatedFrame + step);
            full_renderer::debug::LongSessionChurnOptions churnOptions =
                makeOpenWorldChurnOptions(openWorldChurnState);
            churnOptions.frameCount = static_cast<std::uint32_t>(openWorldChurnState.simulatedFrame);
            openWorldChurnState.lastSummary =
                full_renderer::debug::runLongSessionChurnSimulation(churnOptions);
            if (openWorldChurnState.engineStreamingSeam)
            {
                full_renderer::engine_bridge::EngineStreamingChurnOptions seamOptions =
                    makeEngineStreamingChurnOptions(openWorldChurnState);
                seamOptions.churn.frameCount = static_cast<std::uint32_t>(openWorldChurnState.simulatedFrame);
                openWorldChurnState.seamSummary =
                    full_renderer::engine_bridge::runEngineStreamingSeamChurnSimulation(seamOptions);
            }
            if (openWorldChurnState.simulatedFrame >= openWorldChurnState.targetFrameCount)
            {
                openWorldChurnState.running = false;
            }
        }

        const full_renderer::debug::LongSessionChurnSummary& churn = openWorldChurnState.lastSummary;
        ImGui::Text("Simulated frames: %u / %d, seed 0x%08x",
            churn.simulatedFrames,
            openWorldChurnState.targetFrameCount,
            static_cast<std::uint32_t>(openWorldChurnState.seed));
        ImGui::Text("Chunks live before reset: %u, peak %u, created/destroyed/reused %u / %u / %u",
            churn.residentTerrainChunksBeforeReset,
            churn.peakResidentTerrainChunks,
            churn.totalTerrainChunksCreated,
            churn.totalTerrainChunksDestroyed,
            churn.totalTerrainChunkSlotsReused);
        ImGui::Text("Stale/invalid attempts: %u / %u",
            churn.totalStaleChunkHandleAttempts,
            churn.totalInvalidHandleAttempts);
        ImGui::Text("Fallbacks material/texture/LOD: %u / %u / %u",
            churn.totalMaterialFallbacks,
            churn.totalTextureFallbacks,
            churn.totalLodFallbacks);
        ImGui::Text("Decals submitted/rendered/culled: %u / %u / %u",
            churn.totalDecalsSubmitted,
            churn.totalDecalsRendered,
            churn.totalDecalsCulled);
        ImGui::Text("Particle batches submitted/rendered/culled: %u / %u / %u",
            churn.totalParticleBatchesSubmitted,
            churn.totalParticleBatchesRendered,
            churn.totalParticleBatchesCulled);
        ImGui::Text("Skinned palette submissions/rejections: %u / %u",
            churn.totalSkinnedPaletteSubmissions,
            churn.totalSkinnedPaletteRejections);
        ImGui::Text("Resize events scene/post/shadow recreates: %u / %u / %u / %u",
            churn.totalResizeEvents,
            churn.totalSceneTargetRecreates,
            churn.totalPostTargetRecreates,
            churn.totalShadowTargetRecreates);
        ImGui::Text("Tracked resources before reset/after reset/peak: %u / %u / %u",
            churn.trackedLiveResourcesBeforeReset,
            churn.finalTrackedLiveResourcesAfterReset,
            churn.peakTrackedLiveResources);
        ImGui::Text("Staged bytes total %.2f KiB, peak live %.2f KiB",
            static_cast<double>(churn.totalStagedBytes) / 1024.0,
            static_cast<double>(churn.peakEstimatedLiveBytes) / 1024.0);
        ImGui::Text("Frame-data checks/failures: %u / %u, final-present invalid frames: %u",
            churn.frameDataLifetimeChecks,
            churn.frameDataLifetimeFailures,
            churn.finalPresentInvalidFrames);
        ImGui::Text("Growth warnings: %u, hash 0x%016llx",
            churn.unboundedGrowthWarnings,
            static_cast<unsigned long long>(churn.deterministicHash));
        if (openWorldChurnState.engineStreamingSeam)
        {
            const full_renderer::engine_bridge::EngineStreamingChurnSummary& seam = openWorldChurnState.seamSummary;
            ImGui::Separator();
            ImGui::Text("Engine seam mappings live/reset/created/destroyed/reused: %u / %u / %u / %u / %u",
                seam.finalStats.mappingCount,
                seam.resetFinalMappingCount,
                seam.finalStats.totalMappingsCreated,
                seam.finalStats.totalMappingsDestroyed,
                seam.finalStats.totalMappingsReused);
            ImGui::Text("Engine seam stale/material fallback/origin shifts: %u / %u / %u",
                seam.finalStats.staleMappingAttempts,
                seam.finalStats.materialFallbackMappingCount,
                seam.originShiftCount);
            ImGui::Text("Origin mode: %s, origin %.1f %.1f %.1f",
                seam.finalStats.originMode == full_renderer::engine_bridge::OriginMode::CameraRelative ?
                    "camera-relative" :
                    "absolute",
                seam.finalStats.originWorld[0],
                seam.finalStats.originWorld[1],
                seam.finalStats.originWorld[2]);
            ImGui::Text("Camera world %.1f %.1f %.1f -> renderer %.2f %.2f %.2f",
                seam.finalStats.cameraWorld[0],
                seam.finalStats.cameraWorld[1],
                seam.finalStats.cameraWorld[2],
                seam.finalStats.cameraRendererRelative[0],
                seam.finalStats.cameraRendererRelative[1],
                seam.finalStats.cameraRendererRelative[2]);
            ImGui::Text("Large-coordinate warnings: %u, NaN/Inf rejects: %u, seam hash 0x%016llx",
                seam.finalStats.largeCoordinateWarningCount,
                seam.finalStats.nanOrInfRejectionCount,
                static_cast<unsigned long long>(seam.deterministicHash));
        }
        ImGui::TextUnformatted("This is a deterministic sample/test churn harness; it does not perform async streaming, file IO, or engine world simulation.");
    }
    if (ImGui::CollapsingHeader("Directional Shadow", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextUnformatted("CSM Controls");
        ImGui::Separator();
        if (ImGui::Button("Reset CSM validation defaults"))
        {
            shadow = full_renderer::debug::makeDefaultCsmValidationShadowDesc();
            shadowPreview = full_renderer::debug::makeDefaultShadowMapPreviewSettings();
        }
        ImGui::SameLine();
        if (ImGui::Button("Validation overlays"))
        {
            full_renderer::debug::applyCsmValidationDebugOverlayPreset(options, shadow);
        }
        ImGui::Checkbox("Terrain shadows", &shadow.enabled);

        ImGui::Spacing();
        ImGui::TextUnformatted("Cascade Configuration");
        ImGui::Separator();
        int cascadeCount = static_cast<int>(shadow.cascadeCount);
        if (ImGui::SliderInt("Cascade count", &cascadeCount, 1, static_cast<int>(full_renderer::kMaxDirectionalShadowCascades)))
        {
            shadow.cascadeCount = static_cast<std::uint32_t>(cascadeCount);
        }
        int splitMode = shadow.cascadeSplitMode == full_renderer::ShadowCascadeSplitMode::Uniform ? 0 :
            shadow.cascadeSplitMode == full_renderer::ShadowCascadeSplitMode::Logarithmic ? 1 :
            2;
        if (ImGui::Combo("Split mode", &splitMode, "Uniform\0" "Logarithmic\0" "Practical\0" "\0"))
        {
            shadow.cascadeSplitMode = splitMode == 0 ? full_renderer::ShadowCascadeSplitMode::Uniform :
                splitMode == 1 ? full_renderer::ShadowCascadeSplitMode::Logarithmic :
                full_renderer::ShadowCascadeSplitMode::Practical;
        }
        ImGui::SliderFloat("Split lambda", &shadow.cascadeSplitLambda, 0.0f, 1.0f);
        ImGui::SliderFloat("Shadow distance", &shadow.cascadeShadowDistanceMeters, 8.0f, 200.0f);
        ImGui::SliderFloat("Camera near", &shadow.cascadeCameraNearMeters, 0.01f, 5.0f, "%.3f");
        ImGui::SliderFloat("Camera far", &shadow.cascadeCameraFarMeters, 10.0f, 250.0f);
        ImGui::Checkbox("Stable cascades", &shadow.stableCascadeProjection);
        ImGui::Checkbox("Cascade blend bands", &shadow.cascadeBlendEnabled);
        ImGui::SliderFloat("Blend fraction", &shadow.cascadeBlendFraction, 0.0f, 0.5f, "%.3f");

        ImGui::Spacing();
        ImGui::TextUnformatted("Bias And Filtering");
        ImGui::Separator();
        ImGui::SliderFloat("Depth bias", &shadow.depthBias, 0.0f, 0.02f, "%.5f");
        ImGui::SliderFloat("Slope bias", &shadow.slopeBias, 0.0f, 0.02f, "%.5f");
        ImGui::SliderFloat("Strength", &shadow.strength, 0.0f, 1.0f);
        int filterMode = shadow.filterMode == full_renderer::ShadowFilterMode::Nearest ? 0 :
            shadow.filterMode == full_renderer::ShadowFilterMode::Pcf2x2 ? 1 :
            2;
        if (ImGui::Combo("Filter", &filterMode, "Nearest\0" "PCF 2x2\0" "PCF 3x3\0" "\0"))
        {
            shadow.filterMode = filterMode == 0 ? full_renderer::ShadowFilterMode::Nearest :
                filterMode == 1 ? full_renderer::ShadowFilterMode::Pcf2x2 :
                full_renderer::ShadowFilterMode::Pcf3x3;
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Resource And Overlay Controls");
        ImGui::Separator();
        int resolutionIndex = shadow.mapResolution <= 512U ? 0 :
            shadow.mapResolution <= 1024U ? 1 :
            shadow.mapResolution <= 2048U ? 2 :
            3;
        if (ImGui::Combo("Resolution", &resolutionIndex, "512\0" "1024\0" "2048\0" "4096\0" "\0"))
        {
            const std::uint32_t values[] = {512U, 1024U, 2048U, 4096U};
            shadow.mapResolution = values[static_cast<std::uint32_t>(resolutionIndex)];
        }
        ImGui::Checkbox("Light bounds", &shadow.debugDrawLightBounds);
        ImGui::Checkbox("Shadow casters", &shadow.debugDrawShadowCasters);
        ImGui::Checkbox("Cascade frusta", &shadow.debugDrawCascadeFrusta);
        ImGui::Checkbox("Cascade casters", &shadow.debugDrawCascadeCasters);
        ImGui::Checkbox("Shadow map preview", &shadowPreview.enabled);
        int previewSize = static_cast<int>(shadowPreview.previewSize);
        if (ImGui::SliderInt("Preview size", &previewSize, 128, 512))
        {
            shadowPreview.previewSize = static_cast<std::uint32_t>(previewSize);
        }
        ImGui::SliderFloat("Preview black depth", &shadowPreview.blackDepth, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Preview white depth", &shadowPreview.whiteDepth, 0.0f, 1.0f, "%.3f");
        ImGui::Checkbox("Invert preview", &shadowPreview.invert);

        const full_renderer::RendererStats rendererStats = renderer.getStats();
        ImGui::Spacing();
        ImGui::TextUnformatted("Resource Status");
        ImGui::Separator();
        ImGui::Text("Shadow map: %u", rendererStats.shadowMapResolution);
        ImGui::Text("Requested resources: %u cascades @ %u",
            rendererStats.shadowRequestedCascadeCount,
            rendererStats.shadowRequestedMapResolution);
        ImGui::Text("Active resources: %u targets @ %u",
            rendererStats.shadowCascadeRenderTargetCount,
            rendererStats.shadowMapResolution);
        ImGui::Text("Resource reconfigured this frame: %s",
            rendererStats.shadowResourceReconfigured != 0U ? "yes" : "no");
        ImGui::Text("Resource recreates: %u, allocation failures: %u%s",
            rendererStats.shadowResourceRecreateCount,
            rendererStats.shadowResourceAllocationFailureCount,
            rendererStats.shadowResourceAllocationFailed != 0U ? " (latest failed)" : "");
        ImGui::Text("Caster batches: %u", rendererStats.shadowCasterBatches);
        ImGui::Text("Shadow draws: %u", rendererStats.shadowPassDraws);
        ImGui::Text("Mesh receivers: %u static, %u instanced",
            rendererStats.shadowStaticMeshReceivers,
            rendererStats.shadowInstancedMeshReceiverBatches);
        ImGui::Text("Caster chunks: %u", stats.shadowCasterChunks);
        ImGui::Text("Off-camera casters: %u", stats.offCameraShadowCasterChunks);
        ImGui::Text("Light-frustum rejects: %u", stats.shadowRejectedChunks);
        ImGui::Text("Invalid shadow resources: %u", stats.shadowInvalidResourceChunks);
        ImGui::Text("CSM configured: %u cascades, %s", 
            full_renderer::scene::clampShadowCascadeCount(shadow.cascadeCount),
            splitModeName(shadow.cascadeSplitMode));
        ImGui::Text("Cascade blending: %s, %.3f",
            shadow.cascadeBlendEnabled ? "on" : "off",
            full_renderer::scene::clampCascadeBlendFraction(shadow.cascadeBlendFraction));
        ImGui::Text("Filter taps: %u", full_renderer::scene::shadowFilterModeToTapCount(shadow.filterMode));
        ImGui::TextUnformatted("Rendering: all active cascades sampled by terrain");
        ImGui::Spacing();
        ImGui::TextUnformatted("Per-Cascade Stats");
        ImGui::Separator();
        if (ImGui::BeginTable("Cascade Caster Counts", 13, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Cascade");
            ImGui::TableSetupColumn("Target");
            ImGui::TableSetupColumn("Casters");
            ImGui::TableSetupColumn("Terrain");
            ImGui::TableSetupColumn("Mesh");
            ImGui::TableSetupColumn("Inst");
            ImGui::TableSetupColumn("Skin");
            ImGui::TableSetupColumn("Off-camera");
            ImGui::TableSetupColumn("Rejected");
            ImGui::TableSetupColumn("Invalid");
            ImGui::TableSetupColumn("Draws");
            ImGui::TableSetupColumn("LOD0");
            ImGui::TableSetupColumn("LOD1+");
            ImGui::TableHeadersRow();
            for (std::uint32_t cascadeIndex = 0; cascadeIndex < stats.shadowCascadeCount; ++cascadeIndex)
            {
                std::uint32_t lowerLods = 0;
                for (std::uint32_t lodIndex = 1; lodIndex < full_renderer::kMaxTerrainLodLevels; ++lodIndex)
                {
                    lowerLods += stats.shadowCascadeCasterChunksByLod[cascadeIndex][lodIndex];
                }
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u active", cascadeIndex);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(rendererStats.shadowCascadeRenderTargetValid[cascadeIndex] ? "valid" : "missing");
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%u", stats.shadowCascadeCasterChunks[cascadeIndex]);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%u", rendererStats.shadowTerrainCasterBatchesByCascade[cascadeIndex]);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%u", rendererStats.shadowStaticMeshCastersByCascade[cascadeIndex]);
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%u", rendererStats.shadowInstancedMeshCasterBatchesByCascade[cascadeIndex]);
                ImGui::TableSetColumnIndex(6);
                ImGui::Text("%u", rendererStats.shadowSkinnedMeshCastersByCascade[cascadeIndex]);
                ImGui::TableSetColumnIndex(7);
                ImGui::Text("%u", stats.shadowCascadeOffCameraCasterChunks[cascadeIndex]);
                ImGui::TableSetColumnIndex(8);
                ImGui::Text("%u", stats.shadowCascadeRejectedChunks[cascadeIndex]);
                ImGui::TableSetColumnIndex(9);
                ImGui::Text("%u", stats.shadowCascadeInvalidResourceChunks[cascadeIndex]);
                ImGui::TableSetColumnIndex(10);
                ImGui::Text("%u", rendererStats.shadowPassDrawsByCascade[cascadeIndex]);
                ImGui::TableSetColumnIndex(11);
                ImGui::Text("%u", stats.shadowCascadeCasterChunksByLod[cascadeIndex][0]);
                ImGui::TableSetColumnIndex(12);
                ImGui::Text("%u", lowerLods);
            }
            ImGui::EndTable();
        }
        if (ImGui::BeginTable("Shadow Caster LOD Counts", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("LOD");
            ImGui::TableSetupColumn("Caster chunks");
            ImGui::TableSetupColumn("Caster batches");
            ImGui::TableHeadersRow();
            for (std::uint32_t lodIndex = 0; lodIndex < full_renderer::kMaxTerrainLodLevels; ++lodIndex)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", lodIndex);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", stats.shadowCasterChunksByLod[lodIndex]);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%u", stats.shadowCasterBatchesByLod[lodIndex]);
            }
            ImGui::EndTable();
        }
        full_renderer::scene::DirectionalShadowCascadeRange ranges[full_renderer::kMaxDirectionalShadowCascades] = {};
        std::uint32_t computedCascadeCount = 0;
        if (full_renderer::scene::computeDirectionalShadowCascadeRanges(shadow, ranges, computedCascadeCount) &&
            ImGui::BeginTable("Cascade Ranges", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Cascade");
            ImGui::TableSetupColumn("Near");
            ImGui::TableSetupColumn("Far");
            ImGui::TableSetupColumn("Normalized");
            ImGui::TableHeadersRow();
            for (std::uint32_t cascadeIndex = 0; cascadeIndex < computedCascadeCount; ++cascadeIndex)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", cascadeIndex);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2f", ranges[cascadeIndex].nearDistanceMeters);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.2f", ranges[cascadeIndex].farDistanceMeters);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.3f", ranges[cascadeIndex].normalizedSplitDepth);
            }
            ImGui::EndTable();
        }
        full_renderer::scene::DirectionalShadowCascadeSet cascadeSet;
        if (full_renderer::scene::buildDirectionalShadowCascadeSet(submittedLight, submittedShadow, submittedView, cascadeSet) &&
            ImGui::BeginTable("Cascade Stability", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Cascade");
            ImGui::TableSetupColumn("Texel X");
            ImGui::TableSetupColumn("Texel Y");
            ImGui::TableSetupColumn("Snap X");
            ImGui::TableSetupColumn("Snap Y");
            ImGui::TableSetupColumn("Center XY");
            ImGui::TableHeadersRow();
            for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeSet.cascadeCount; ++cascadeIndex)
            {
                const full_renderer::scene::DirectionalShadowSplit& split = cascadeSet.splits[cascadeIndex];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", cascadeIndex);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.4f", split.texelSize[0]);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.4f", split.texelSize[1]);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.4f", split.snapOffset[0]);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.4f", split.snapOffset[1]);
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%.2f, %.2f", split.snappedCenter[0], split.snappedCenter[1]);
            }
            ImGui::EndTable();
        }
        ImGui::TextUnformatted("Caster selection: resident submitted chunks inside the light frustum.");
        ImGui::TextUnformatted("Shadow LOD policy: distance from shadow center to chunk bounds.");
        if (shadowPreview.enabled)
        {
            const std::uint32_t activeCascadeCount = rendererStats.shadowCascadeCount;
            bool selectedCascadeValid = false;
            if (activeCascadeCount > 0)
            {
                shadowPreview.cascadeIndex = std::min(shadowPreview.cascadeIndex, activeCascadeCount - 1U);
                selectedCascadeValid =
                    shadowPreview.cascadeIndex < full_renderer::kMaxDirectionalShadowCascades &&
                    rendererStats.shadowCascadeRenderTargetValid[shadowPreview.cascadeIndex] != 0U;
            }
            full_renderer::debug::ShadowMapPreviewRequest previewRequest =
                full_renderer::debug::makeShadowMapPreviewRequest(
                    shadowPreview,
                    activeCascadeCount,
                    selectedCascadeValid);
            shadowPreview.cascadeIndex = previewRequest.cascadeIndex;
            shadowPreview.previewSize = previewRequest.previewSize;
            shadowPreview.blackDepth = previewRequest.blackDepth;
            shadowPreview.whiteDepth = previewRequest.whiteDepth;

            int previewCascade = static_cast<int>(shadowPreview.cascadeIndex);
            const int previewCascadeMax = activeCascadeCount > 0 ?
                static_cast<int>(activeCascadeCount - 1U) :
                0;
            if (ImGui::SliderInt("Preview cascade", &previewCascade, 0, previewCascadeMax))
            {
                shadowPreview.cascadeIndex = static_cast<std::uint32_t>(std::max(previewCascade, 0));
            }

            selectedCascadeValid =
                activeCascadeCount > 0 &&
                shadowPreview.cascadeIndex < full_renderer::kMaxDirectionalShadowCascades &&
                rendererStats.shadowCascadeRenderTargetValid[shadowPreview.cascadeIndex] != 0U;
            previewRequest = full_renderer::debug::makeShadowMapPreviewRequest(
                shadowPreview,
                activeCascadeCount,
                selectedCascadeValid);
            if (concreteRenderer == nullptr || !previewRequest.available)
            {
                ImGui::TextUnformatted("Shadow-map preview unavailable: no valid active cascade target.");
            }
            else
            {
                const full_renderer::bgfx_backend::ShadowPreviewTextureInfo previewTexture =
                    concreteRenderer->getShadowPreviewTexture(previewRequest.cascadeIndex);
                const bool renderedPreview = previewTexture.valid &&
                    imguiRenderer.renderShadowPreview(
                        previewTexture.texture,
                        previewRequest.previewSize,
                        previewRequest.blackDepth,
                        previewRequest.whiteDepth,
                        previewRequest.invert);
                if (renderedPreview)
                {
                    ImGui::Text(
                        "Cascade %u, source %u px",
                        previewRequest.cascadeIndex,
                        previewTexture.resolution);
                    ImGui::Image(
                        imguiRenderer.shadowPreviewTextureId(),
                        ImVec2(
                            static_cast<float>(previewRequest.previewSize),
                            static_cast<float>(previewRequest.previewSize)));
                }
                else
                {
                    ImGui::TextUnformatted("Shadow-map preview unavailable: backend preview pass failed.");
                }
            }
        }
    }
    ImGui::TextUnformatted(
        "F1 UI, F2 camera bookmark, F3 validation preset, F5 bounds, F6 LOD, F7 splat, F8 combined, F9 frusta, F10 casters, F11 reset.");
    ImGui::End();
}
#endif
} // namespace

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "Failed to initialize SDL video: " << SDL_GetError() << '\n';
        return 1;
    }

    struct SdlQuitGuard
    {
        ~SdlQuitGuard()
        {
            SDL_Quit();
        }
    } sdlQuitGuard;

    SdlWindowPtr window(SDL_CreateWindow(
        "FullRenderer CSM Validation",
        1280,
        720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY));

    if (window == nullptr)
    {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << '\n';
        return 1;
    }

    auto renderer = full_renderer::createRenderer();

    full_renderer::RendererInitDesc initDesc;
    initDesc.enableDebug = true;
    initDesc.shaderBinaryDirectory = FULL_RENDERER_SAMPLE_SHADER_DIR;

    if (!queryWindowSize(window.get(), initDesc.backbufferWidth, initDesc.backbufferHeight) ||
        !fillPlatformWindowDesc(window.get(), initDesc.window))
    {
        return 1;
    }

    full_renderer::RendererResult result = renderer->initialize(initDesc);
    if (result != full_renderer::RendererResult::Success)
    {
        std::cerr << "Renderer initialization failed: " << resultName(result) << '\n';
        return 1;
    }

#if FULL_RENDERER_ENABLE_DEBUG_UI
    full_renderer::bgfx_backend::ImguiBgfxRenderer imguiRenderer;
    bool imguiInitialized = false;
    full_renderer::debug::ShadowMapPreviewSettings shadowPreview =
        full_renderer::debug::makeDefaultShadowMapPreviewSettings();
#endif
    bool showDebugUi = false;
    full_renderer::TerrainDebugOptions terrainDebugOptions;
    full_renderer::AnimationDebugOptions animationDebugOptions;
    full_renderer::EnvironmentDesc environment =
        full_renderer::scene::makeDefaultOpenWorldEnvironmentDesc();
    full_renderer::WeatherDesc weather = makeSampleWeatherDesc();
    full_renderer::SsaoDesc ssao = full_renderer::scene::makeDefaultSsaoDesc();
    full_renderer::ColorGradingDesc colorGrading = full_renderer::scene::makeDefaultColorGradingDesc();
    full_renderer::DecalSubmitDesc decalSubmit;
    decalSubmit.enabled = true;
    decalSubmit.debugDrawVolumes = true;
    full_renderer::ParticleSubmitDesc particleSubmit;
    particleSubmit.enabled = true;
    particleSubmit.cullAgainstViewFrustum = true;
    particleSubmit.softParticlesEnabled = true;
    particleSubmit.softParticleFadeDistanceMeters = 0.75f;
    bool sampleParticleEmissionEnabled = true;
    int sampleParticleCount = 96;
    bool sampleWeatherPrecipitationEnabled = true;
    int samplePrecipitationParticleBudget = 160;
    std::uint32_t samplePrecipitationParticleCount = 0;
    full_renderer::DirectionalShadowDesc directionalShadow =
        full_renderer::debug::makeDefaultCsmValidationShadowDesc();
    full_renderer::SelectionOutlineDesc selectionOutline;
    selectionOutline.enabled = true;
    selectionOutline.thicknessPixels = 3.0f;
    bool selectStaticMeshes = true;
    bool selectInstancedBatch = true;
    bool selectSkinnedMesh = true;
    bool sampleStructureFadeEnabled = true;
    float sampleStructureFadeVisibility = 0.45f;
    full_renderer::FadeMode sampleStructureFadeMode = full_renderer::FadeMode::Dithered;
    bool fadeStaticStructure = true;
    bool fadeInstancedStructure = true;
    bool fadeSkinnedStructure = false;
    int sampleStaticDrawCount = 4;
    int sampleInstancedInstanceCount = 8;
    int sampleSkinnedDrawCount = 1;
    int sampleDecalCount = 4;
    SampleValidationState validationState;
    SampleOpenWorldChurnState openWorldChurnState;
    const full_renderer::debug::CsmValidationSceneConfig validationScene =
        full_renderer::debug::makeDefaultCsmValidationSceneConfig();

    full_renderer::MeshDesc meshDesc;
    meshDesc.vertices = kCubeVertices.data();
    meshDesc.vertexCount = static_cast<std::uint32_t>(kCubeVertices.size());
    meshDesc.indices = kCubeIndices.data();
    meshDesc.indexCount = static_cast<std::uint32_t>(kCubeIndices.size());
    const full_renderer::MeshHandle cubeMesh = renderer->createMesh(meshDesc);
    if (!full_renderer::isValid(cubeMesh))
    {
        std::cerr << "Failed to create cube mesh.\n";
        renderer->shutdown();
        return 1;
    }

    full_renderer::MaterialDesc materialDesc;
    materialDesc.baseColorLinear[0] = 1.0f;
    materialDesc.baseColorLinear[1] = 1.0f;
    materialDesc.baseColorLinear[2] = 1.0f;
    materialDesc.baseColorLinear[3] = 1.0f;
    materialDesc.lit = true;
    const full_renderer::MaterialHandle cubeMaterial = renderer->createMaterial(materialDesc);
    if (!full_renderer::isValid(cubeMaterial))
    {
        std::cerr << "Failed to create cube material.\n";
        renderer->destroyMesh(cubeMesh);
        renderer->shutdown();
        return 1;
    }

    full_renderer::SkeletonJointDesc skeletonJoints[2] = {};
    setIdentity(skeletonJoints[0].inverseBindPose);
    skeletonJoints[0].parentIndex = -1;
    makeTranslation(skeletonJoints[1].inverseBindPose, {0.0f, -1.0f, 0.0f});
    skeletonJoints[1].parentIndex = 0;
    full_renderer::SkeletonDesc skeletonDesc;
    skeletonDesc.joints = skeletonJoints;
    skeletonDesc.jointCount = 2;
    const full_renderer::SkeletonHandle sampleSkeleton = renderer->createSkeleton(skeletonDesc);
    if (!full_renderer::isValid(sampleSkeleton))
    {
        std::cerr << "Failed to create sample skeleton.\n";
        renderer->destroyMaterial(cubeMaterial);
        renderer->destroyMesh(cubeMesh);
        renderer->shutdown();
        return 1;
    }

    std::array<full_renderer::SkinnedMeshVertex, 6> skinnedVertices = {};
    const std::array<Vec3, 6> skinnedPositions = {{
        {-0.35f, 0.0f, 0.0f},
        {0.35f, 0.0f, 0.0f},
        {-0.35f, 1.0f, 0.0f},
        {0.35f, 1.0f, 0.0f},
        {-0.35f, 2.0f, 0.0f},
        {0.35f, 2.0f, 0.0f},
    }};
    for (std::uint32_t vertexIndex = 0; vertexIndex < skinnedVertices.size(); ++vertexIndex)
    {
        full_renderer::SkinnedMeshVertex& vertex = skinnedVertices[vertexIndex];
        vertex.position[0] = skinnedPositions[vertexIndex].x;
        vertex.position[1] = skinnedPositions[vertexIndex].y;
        vertex.position[2] = skinnedPositions[vertexIndex].z;
        vertex.normal[0] = 0.0f;
        vertex.normal[1] = 0.0f;
        vertex.normal[2] = 1.0f;
        vertex.colorLinear[0] = 0.95f;
        vertex.colorLinear[1] = 0.65f;
        vertex.colorLinear[2] = 0.28f;
        vertex.colorLinear[3] = 1.0f;
        vertex.jointIndices[0] = 0.0f;
        vertex.jointIndices[1] = 1.0f;
        vertex.jointIndices[2] = 0.0f;
        vertex.jointIndices[3] = 0.0f;
        const float childWeight = vertex.position[1] <= 0.0f ? 0.0f : vertex.position[1] >= 2.0f ? 1.0f : 0.5f;
        vertex.jointWeights[0] = 1.0f - childWeight;
        vertex.jointWeights[1] = childWeight;
        vertex.jointWeights[2] = 0.0f;
        vertex.jointWeights[3] = 0.0f;
    }
    constexpr std::array<std::uint16_t, 12> kSkinnedIndices = {{
        0, 1, 2,
        1, 3, 2,
        2, 3, 4,
        3, 5, 4,
    }};
    full_renderer::SkinnedMeshDesc skinnedMeshDesc;
    skinnedMeshDesc.skeleton = sampleSkeleton;
    skinnedMeshDesc.vertices = skinnedVertices.data();
    skinnedMeshDesc.vertexCount = static_cast<std::uint32_t>(skinnedVertices.size());
    skinnedMeshDesc.indices = kSkinnedIndices.data();
    skinnedMeshDesc.indexCount = static_cast<std::uint32_t>(kSkinnedIndices.size());
    const full_renderer::SkinnedMeshHandle sampleSkinnedMesh = renderer->createSkinnedMesh(skinnedMeshDesc);
    if (!full_renderer::isValid(sampleSkinnedMesh))
    {
        std::cerr << "Failed to create sample skinned mesh.\n";
        renderer->destroySkeleton(sampleSkeleton);
        renderer->destroyMaterial(cubeMaterial);
        renderer->destroyMesh(cubeMesh);
        renderer->shutdown();
        return 1;
    }

    full_engine::WorldChunkRegistry engineTerrainRegistry;
    full_engine::WorldChunkCatalog engineTerrainCatalog;
    full_engine::WorldChunkResidencyRequestQueue terrainResidencyRequests;
    full_engine::TerrainChunkRequestQueue terrainSetupRequests;
    full_engine::TerrainResourceCatalog engineTerrainResources;
    full_engine::ChunkTerrainHandleMap engineTerrainHandles;
    SampleEngineTerrainPipelineState engineTerrainPipeline;
    full_engine::TerrainIntegrationDiagnostics terrainDiagnostics;
    std::vector<SampleTerrainChunkState> sampleTerrainChunks;
    std::vector<full_renderer::TerrainChunkHandle> terrainChunks;
    SampleTerrainResidencyControls terrainResidencyControls;
    bool engineTerrainPipelineDirty = false;
    const int kGridRadius = static_cast<int>(validationScene.terrainGridRadius);
    terrainResidencyControls.batchRingRadius = kGridRadius;
    const float kChunkSize = validationScene.chunkSizeMeters;
    const std::size_t terrainGridWidth = static_cast<std::size_t>((kGridRadius * 2) + 1);
    const std::size_t terrainGridChunkCount = terrainGridWidth * terrainGridWidth;
    sampleTerrainChunks.reserve(terrainGridChunkCount);
    terrainChunks.reserve(terrainGridChunkCount);
    constexpr float kTerrainY = -1.25f;
    constexpr bool kTerrainSkirtsEnabled = true;
    constexpr float kTerrainSkirtDepth = 0.60f;
    constexpr std::array<std::uint32_t, full_renderer::kMaxTerrainLodLevels> kTerrainSubdivisions = {
        16U,
        8U,
        4U,
        2U,
    };
    constexpr std::array<float, full_renderer::kMaxTerrainLodLevels> kTerrainLodDistances = {
        10.0f,
        22.0f,
        42.0f,
        1000.0f,
    };
    std::array<full_renderer::TextureHandle, full_renderer::kMaxTerrainMaterialLayers> layerTextures = {
        createCheckerTexture(*renderer, {42, 138, 54, 255}, {74, 176, 82, 255}),
        createCheckerTexture(*renderer, {124, 86, 48, 255}, {166, 116, 66, 255}),
        createCheckerTexture(*renderer, {110, 112, 118, 255}, {156, 158, 166, 255}),
        createCheckerTexture(*renderer, {194, 166, 92, 255}, {230, 206, 130, 255}),
    };
    std::array<full_renderer::TextureHandle, full_renderer::kMaxTerrainMaterialLayers> normalTextures = {
        createTerrainNormalTexture(*renderer, 0U),
        createTerrainNormalTexture(*renderer, 1U),
        createTerrainNormalTexture(*renderer, 2U),
        createTerrainNormalTexture(*renderer, 3U),
    };
    const full_renderer::TextureHandle terrainSplatMap = createSplatTexture(*renderer);
    std::array<full_renderer::TextureHandle, 2> decalTextures = {
        createSampleDecalTexture(*renderer, {255, 96, 64, 255}, {94, 16, 12, 255}),
        createSampleDecalTexture(*renderer, {80, 172, 255, 255}, {12, 36, 96, 255}),
    };
    const full_renderer::TextureHandle particleTexture = createSampleParticleTexture(*renderer);

    full_renderer::MaterialDesc terrainMaterialDesc;
    terrainMaterialDesc.kind = full_renderer::MaterialKind::TerrainSplat;
    terrainMaterialDesc.lit = true;
    terrainMaterialDesc.terrain.uvScale = 0.55f;
    terrainMaterialDesc.terrain.normalMapStrength = 0.45f;
    for (std::uint32_t layerIndex = 0; layerIndex < full_renderer::kMaxTerrainMaterialLayers; ++layerIndex)
    {
        terrainMaterialDesc.terrain.layers[layerIndex].albedoTexture = layerTextures[layerIndex];
        terrainMaterialDesc.terrain.layers[layerIndex].normalTexture = normalTextures[layerIndex];
    }
    terrainMaterialDesc.terrain.layers[0].fallbackColorLinear[0] = 0.35f;
    terrainMaterialDesc.terrain.layers[0].fallbackColorLinear[1] = 0.75f;
    terrainMaterialDesc.terrain.layers[0].fallbackColorLinear[2] = 0.35f;
    terrainMaterialDesc.terrain.layers[1].fallbackColorLinear[0] = 0.65f;
    terrainMaterialDesc.terrain.layers[1].fallbackColorLinear[1] = 0.42f;
    terrainMaterialDesc.terrain.layers[1].fallbackColorLinear[2] = 0.24f;
    terrainMaterialDesc.terrain.layers[2].fallbackColorLinear[0] = 0.55f;
    terrainMaterialDesc.terrain.layers[2].fallbackColorLinear[1] = 0.55f;
    terrainMaterialDesc.terrain.layers[2].fallbackColorLinear[2] = 0.58f;
    terrainMaterialDesc.terrain.layers[3].fallbackColorLinear[0] = 0.90f;
    terrainMaterialDesc.terrain.layers[3].fallbackColorLinear[1] = 0.78f;
    terrainMaterialDesc.terrain.layers[3].fallbackColorLinear[2] = 0.46f;
    const full_renderer::MaterialHandle terrainMaterial = renderer->createMaterial(terrainMaterialDesc);

    std::array<full_renderer::MeshHandle, full_renderer::kMaxTerrainLodLevels> terrainMeshes = {};
    for (std::uint32_t lodIndex = 0; lodIndex < full_renderer::kMaxTerrainLodLevels; ++lodIndex)
    {
        terrainMeshes[lodIndex] = createTerrainMesh(
            *renderer,
            kChunkSize,
            kTerrainSubdivisions[lodIndex],
            kTerrainSkirtsEnabled ? kTerrainSkirtDepth : 0.0f);
    }

    bool terrainResourcesValid = true;
    for (std::uint32_t lodIndex = 0; lodIndex < full_renderer::kMaxTerrainLodLevels; ++lodIndex)
    {
        terrainResourcesValid = terrainResourcesValid &&
            full_renderer::isValid(terrainMeshes[lodIndex]);
    }
    for (const full_renderer::TextureHandle texture : layerTextures)
    {
        terrainResourcesValid = terrainResourcesValid && full_renderer::isValid(texture);
    }
    for (const full_renderer::TextureHandle texture : normalTextures)
    {
        terrainResourcesValid = terrainResourcesValid && full_renderer::isValid(texture);
    }
    terrainResourcesValid = terrainResourcesValid &&
        full_renderer::isValid(terrainSplatMap) &&
        full_renderer::isValid(terrainMaterial);
    if (!terrainResourcesValid)
    {
        std::cerr << "Failed to create terrain LOD resources.\n";
        for (const full_renderer::MeshHandle mesh : terrainMeshes)
        {
            renderer->destroyMesh(mesh);
        }
        renderer->destroyMaterial(terrainMaterial);
        renderer->destroyTexture(terrainSplatMap);
        for (const full_renderer::TextureHandle texture : decalTextures)
        {
            if (full_renderer::isValid(texture))
            {
                renderer->destroyTexture(texture);
            }
        }
        if (full_renderer::isValid(particleTexture))
        {
            renderer->destroyTexture(particleTexture);
        }
        for (const full_renderer::TextureHandle texture : layerTextures)
        {
            renderer->destroyTexture(texture);
        }
        for (const full_renderer::TextureHandle texture : normalTextures)
        {
            renderer->destroyTexture(texture);
        }
        renderer->destroySkinnedMesh(sampleSkinnedMesh);
        renderer->destroySkeleton(sampleSkeleton);
        renderer->destroyMaterial(cubeMaterial);
        renderer->destroyMesh(cubeMesh);
        renderer->shutdown();
        return 1;
    }

    for (int z = -kGridRadius; z <= kGridRadius; ++z)
    {
        for (int x = -kGridRadius; x <= kGridRadius; ++x)
        {
            const full_engine::ChunkId chunkId = {
                static_cast<std::int32_t>(x),
                0,
                static_cast<std::int32_t>(z)};
            const float minX = static_cast<float>(x) * kChunkSize;
            const float minZ = static_cast<float>(z) * kChunkSize;
            const full_renderer::Aabb chunkBounds = makeBounds(
                minX,
                kTerrainY - (kTerrainSkirtsEnabled ? kTerrainSkirtDepth : 0.0f) - 0.10f,
                minZ,
                minX + kChunkSize,
                kTerrainY + 0.80f,
                minZ + kChunkSize);

            full_engine::TerrainChunkResourceDesc resourceDesc;
            resourceDesc.id = chunkId;
            resourceDesc.lodCount = full_renderer::kMaxTerrainLodLevels;
            resourceDesc.splatMap = ((x == 0 && z == 0) || (x == -kGridRadius && z == kGridRadius))
                ? full_renderer::TextureHandle{}
                : terrainSplatMap;
            for (std::uint32_t lodIndex = 0; lodIndex < full_renderer::kMaxTerrainLodLevels; ++lodIndex)
            {
                resourceDesc.lods[lodIndex].mesh = terrainMeshes[lodIndex];
                resourceDesc.lods[lodIndex].material = terrainMaterial;
                resourceDesc.lods[lodIndex].maxDistanceMeters = kTerrainLodDistances[lodIndex];
            }

            full_engine::WorldChunkDesc chunkDesc;
            chunkDesc.id = chunkId;
            chunkDesc.bounds = toEngineWorldBounds(chunkBounds);
            full_engine::TerrainChunkRequestQueue setupRequests;
            setupRequests.pushAdd(chunkDesc, resourceDesc);
            const full_engine::TerrainChunkRequestApplyResult setupResult = setupRequests.applyTo(
                engineTerrainRegistry,
                engineTerrainCatalog,
                engineTerrainResources);
            terrainDiagnostics.setupRequests = full_engine::makeTerrainSetupRequestDiagnostics(setupResult);
            const bool setupApplied =
                setupResult.records.size() == 1 &&
                setupResult.records[0].status == full_engine::TerrainChunkRequestStatus::Applied;
            const bool resident =
                setupApplied &&
                engineTerrainRegistry.setResidencyState(chunkId, full_engine::ChunkResidencyState::Loading) ==
                    full_engine::WorldResult::Success &&
                engineTerrainRegistry.setResidencyState(chunkId, full_engine::ChunkResidencyState::Resident) ==
                    full_engine::WorldResult::Success;

            if (!resident)
            {
                std::cerr << "Failed to register sample terrain chunk setup.\n";
                removeSampleTerrainSetup(
                    engineTerrainRegistry,
                    engineTerrainCatalog,
                    engineTerrainResources,
                    sampleTerrainChunks);
                renderer->shutdown();
                return 1;
            }

            SampleTerrainChunkState chunkState;
            chunkState.id = chunkId;
            chunkState.worldDesc = chunkDesc;
            chunkState.resourceDesc = resourceDesc;
            chunkState.setupRegistered = true;
            chunkState.resident = true;
            sampleTerrainChunks.push_back(chunkState);
        }
    }

    full_engine::WorldRenderSnapshotOptions engineTerrainSnapshotOptions;
    engineTerrainSnapshotOptions.origin = full_engine::makeAbsoluteOrigin();
    if (!runSampleTerrainPipeline(
            *renderer,
            engineTerrainRegistry,
            engineTerrainCatalog,
            engineTerrainResources,
            engineTerrainHandles,
            engineTerrainPipeline,
            engineTerrainSnapshotOptions,
            terrainDiagnostics.pipeline) ||
        engineTerrainHandles.mappedCount() != sampleTerrainChunks.size())
    {
        std::cerr << "Failed to submit sample terrain through engine integration.\n";
        destroyMappedTerrainChunks(*renderer, engineTerrainHandles);
        removeSampleTerrainSetup(
            engineTerrainRegistry,
            engineTerrainCatalog,
            engineTerrainResources,
            sampleTerrainChunks);
        renderer->shutdown();
        return 1;
    }
    refreshTerrainSubmitHandles(engineTerrainHandles, terrainChunks);

#if FULL_RENDERER_ENABLE_DEBUG_UI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    imguiInitialized = ImGui_ImplSDL3_InitForOther(window.get());
    imguiInitialized = imguiInitialized && imguiRenderer.initialize(FULL_RENDERER_SAMPLE_SHADER_DIR);
    if (!imguiInitialized)
    {
        std::cerr << "Dear ImGui debug UI failed to initialize; continuing without the panel.\n";
        imguiRenderer.shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
    showDebugUi = imguiInitialized;
#endif

    Vec3 cameraPosition = {
        validationScene.cameraStartX,
        validationScene.cameraStartY,
        validationScene.cameraStartZ};
    Vec3 cameraTarget = {
        validationScene.cameraTargetX,
        validationScene.cameraTargetY,
        validationScene.cameraTargetZ};
    float cameraFovYRadians = 60.0f * 3.1415926535f / 180.0f;
    float cameraNearMeters = 0.1f;
    float cameraFarMeters = 100.0f;
    if (const full_renderer::debug::ValidationCameraBookmark* initialBookmark =
            full_renderer::debug::validationCameraBookmarkByIndex(validationState.activeBookmarkIndex))
    {
        applyCameraBookmark(
            *initialBookmark,
            cameraPosition,
            cameraTarget,
            cameraFovYRadians,
            cameraNearMeters,
            cameraFarMeters);
    }
    auto applyBookmarkIndex = [&](const std::uint32_t index) {
        const std::uint32_t count = full_renderer::debug::validationCameraBookmarkCount();
        if (count == 0)
        {
            return;
        }
        validationState.activeBookmarkIndex = index % count;
        if (const full_renderer::debug::ValidationCameraBookmark* bookmark =
                full_renderer::debug::validationCameraBookmarkByIndex(validationState.activeBookmarkIndex))
        {
            applyCameraBookmark(
                *bookmark,
                cameraPosition,
                cameraTarget,
                cameraFovYRadians,
                cameraNearMeters,
                cameraFarMeters);
        }
    };
    auto applyPresetIndex = [&](const std::uint32_t index) {
        const std::uint32_t count = full_renderer::debug::validationPresetCount();
        if (count == 0)
        {
            return;
        }
        validationState.activePresetIndex = index % count;
        if (const full_renderer::debug::ValidationPreset* preset =
                full_renderer::debug::validationPresetByIndex(validationState.activePresetIndex))
        {
            applyValidationPreset(
                *preset,
                terrainDebugOptions,
                animationDebugOptions,
                environment,
                weather,
                ssao,
                colorGrading,
                decalSubmit,
                particleSubmit,
                selectionOutline,
                sampleParticleEmissionEnabled,
                sampleParticleCount,
                sampleWeatherPrecipitationEnabled,
                samplePrecipitationParticleBudget,
                selectStaticMeshes,
                selectInstancedBatch,
                selectSkinnedMesh,
                sampleStructureFadeEnabled,
                sampleStructureFadeVisibility,
                sampleStructureFadeMode,
                fadeStaticStructure,
                fadeInstancedStructure,
                fadeSkinnedStructure,
                sampleStaticDrawCount,
                sampleInstancedInstanceCount,
                sampleSkinnedDrawCount,
                sampleDecalCount,
                openWorldChurnState);
            applyBookmarkIndex(static_cast<std::uint32_t>(preset->cameraBookmark));
        }
    };
    auto previousTime = std::chrono::steady_clock::now();
    float animationTimeSeconds = 0.0f;

    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
#if FULL_RENDERER_ENABLE_DEBUG_UI
            if (imguiInitialized)
            {
                ImGui_ImplSDL3_ProcessEvent(&event);
            }
#endif
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                running = false;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
            {
                if (event.key.scancode == SDL_SCANCODE_F1)
                {
                    showDebugUi = !showDebugUi;
                }
                else if (event.key.scancode == SDL_SCANCODE_F2)
                {
                    applyBookmarkIndex(validationState.activeBookmarkIndex + 1U);
                }
                else if (event.key.scancode == SDL_SCANCODE_F3)
                {
                    applyPresetIndex(validationState.activePresetIndex + 1U);
                }
                else if (event.key.scancode == SDL_SCANCODE_F5)
                {
                    terrainDebugOptions.drawChunkBounds = !terrainDebugOptions.drawChunkBounds;
                }
                else if (event.key.scancode == SDL_SCANCODE_F6)
                {
                    terrainDebugOptions.drawLodOverlay = !terrainDebugOptions.drawLodOverlay;
                }
                else if (event.key.scancode == SDL_SCANCODE_F7)
                {
                    terrainDebugOptions.drawSplatFallbackOverlay = !terrainDebugOptions.drawSplatFallbackOverlay;
                }
                else if (event.key.scancode == SDL_SCANCODE_F8)
                {
                    terrainDebugOptions.drawCombinedOverlay = !terrainDebugOptions.drawCombinedOverlay;
                }
                else if (event.key.scancode == SDL_SCANCODE_F9)
                {
                    directionalShadow.debugDrawCascadeFrusta = !directionalShadow.debugDrawCascadeFrusta;
                }
                else if (event.key.scancode == SDL_SCANCODE_F10)
                {
                    directionalShadow.debugDrawCascadeCasters = !directionalShadow.debugDrawCascadeCasters;
                }
                else if (event.key.scancode == SDL_SCANCODE_F11)
                {
                    environment = full_renderer::scene::makeDefaultOpenWorldEnvironmentDesc();
                    weather = makeSampleWeatherDesc();
                    ssao = full_renderer::scene::makeDefaultSsaoDesc();
                    colorGrading = full_renderer::scene::makeDefaultColorGradingDesc();
                    decalSubmit = {};
                    decalSubmit.enabled = true;
                    decalSubmit.debugDrawVolumes = true;
                    particleSubmit = {};
                    particleSubmit.enabled = true;
                    particleSubmit.cullAgainstViewFrustum = true;
                    particleSubmit.softParticlesEnabled = true;
                    particleSubmit.softParticleFadeDistanceMeters = 0.75f;
                    sampleParticleEmissionEnabled = true;
                    sampleParticleCount = 96;
                    sampleWeatherPrecipitationEnabled = true;
                    samplePrecipitationParticleBudget = 160;
                    samplePrecipitationParticleCount = 0;
                    directionalShadow = full_renderer::debug::makeDefaultCsmValidationShadowDesc();
                    selectionOutline = {};
                    selectionOutline.enabled = true;
                    selectionOutline.thicknessPixels = 3.0f;
                    selectStaticMeshes = true;
                    selectInstancedBatch = true;
                    selectSkinnedMesh = true;
                    sampleStructureFadeEnabled = true;
                    sampleStructureFadeVisibility = 0.45f;
                    sampleStructureFadeMode = full_renderer::FadeMode::Dithered;
                    fadeStaticStructure = true;
                    fadeInstancedStructure = true;
                    fadeSkinnedStructure = false;
                    sampleStaticDrawCount = 4;
                    sampleInstancedInstanceCount = 8;
                    sampleSkinnedDrawCount = 1;
                    sampleDecalCount = 4;
                    terrainDebugOptions = {};
                    animationDebugOptions = {};
                    validationState = {};
                    applyBookmarkIndex(validationState.activeBookmarkIndex);
#if FULL_RENDERER_ENABLE_DEBUG_UI
                    shadowPreview = full_renderer::debug::makeDefaultShadowMapPreviewSettings();
#endif
                }
            }
            else if (event.type == SDL_EVENT_WINDOW_RESIZED ||
                event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                full_renderer::RendererResizeDesc resizeDesc;
                if (queryWindowSize(window.get(), resizeDesc.backbufferWidth, resizeDesc.backbufferHeight))
                {
                    result = renderer->resize(resizeDesc);
                    if (result != full_renderer::RendererResult::Success)
                    {
                        std::cerr << "Renderer resize failed: " << resultName(result) << '\n';
                        running = false;
                    }
                }
            }
        }

        if (!running)
        {
            break;
        }

#if FULL_RENDERER_ENABLE_DEBUG_UI
        if (imguiInitialized)
        {
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
        }
#endif

        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = now - previousTime;
        previousTime = now;
        const float deltaSeconds = static_cast<float>(elapsed.count());
        animationTimeSeconds += deltaSeconds;

        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        const Vec3 forward = normalize(cameraTarget - cameraPosition);
        const Vec3 right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
        const float moveSpeed = 4.0f * deltaSeconds;
        Vec3 movement = {};
        bool cameraKeyboardEnabled = true;
#if FULL_RENDERER_ENABLE_DEBUG_UI
        if (imguiInitialized)
        {
            cameraKeyboardEnabled = !ImGui::GetIO().WantCaptureKeyboard;
        }
#endif
        if (cameraKeyboardEnabled && keyboard[SDL_SCANCODE_W])
        {
            movement = movement + forward * moveSpeed;
        }
        if (cameraKeyboardEnabled && keyboard[SDL_SCANCODE_S])
        {
            movement = movement - forward * moveSpeed;
        }
        if (cameraKeyboardEnabled && keyboard[SDL_SCANCODE_D])
        {
            movement = movement + right * moveSpeed;
        }
        if (cameraKeyboardEnabled && keyboard[SDL_SCANCODE_A])
        {
            movement = movement - right * moveSpeed;
        }
        if (cameraKeyboardEnabled && keyboard[SDL_SCANCODE_E])
        {
            movement.y += moveSpeed;
        }
        if (cameraKeyboardEnabled && keyboard[SDL_SCANCODE_Q])
        {
            movement.y -= moveSpeed;
        }
        cameraPosition = cameraPosition + movement;
        cameraTarget = cameraTarget + movement;

        full_renderer::FrameDesc frameDesc;
        if (!queryWindowSize(window.get(), frameDesc.backbufferWidth, frameDesc.backbufferHeight))
        {
            break;
        }
        frameDesc.deltaSeconds = elapsed.count();

        if (engineTerrainPipelineDirty)
        {
            if (!applySampleTerrainSetupRequests(
                    terrainSetupRequests,
                    engineTerrainRegistry,
                    engineTerrainCatalog,
                    engineTerrainResources,
                    sampleTerrainChunks,
                    terrainResidencyRequests,
                    terrainDiagnostics.setupRequests))
            {
                std::cerr << "Sample terrain setup request failed.\n";
                break;
            }

            if (terrainResidencyRequests.requestCount() > 0)
            {
                full_engine::WorldChunkResidencyRequestQueue registeredResidencyRequests;
                for (const full_engine::WorldChunkResidencyRequest& request : terrainResidencyRequests.requests())
                {
                    const SampleTerrainChunkState* chunk = findSampleTerrainChunk(sampleTerrainChunks, request.id);
                    if (chunk != nullptr && chunk->setupRegistered)
                    {
                        registeredResidencyRequests.push(request);
                    }
                }
                const full_engine::WorldChunkResidencyApplyResult applyResult =
                    registeredResidencyRequests.applyTo(engineTerrainRegistry);
                terrainDiagnostics.residencyRequests =
                    full_engine::makeTerrainResidencyRequestDiagnostics(applyResult);
                mirrorSampleTerrainResidencyApplyResult(applyResult, sampleTerrainChunks);
                terrainResidencyRequests.clear();
                if (applyResult.summary.invalidTransitionCount > 0)
                {
                    std::cerr << "Sample terrain residency request failed with an invalid transition.\n";
                    break;
                }
            }

            if (!runSampleTerrainPipeline(
                    *renderer,
                    engineTerrainRegistry,
                    engineTerrainCatalog,
                    engineTerrainResources,
                    engineTerrainHandles,
                    engineTerrainPipeline,
                    engineTerrainSnapshotOptions,
                    terrainDiagnostics.pipeline))
            {
                std::cerr << "Failed to update sample terrain residency through engine integration.\n";
                break;
            }
            engineTerrainPipelineDirty = false;
            if (terrainResidencyControls.reloadCenterAfterUnload)
            {
                const full_engine::ChunkId centerChunkId = {};
                if (SampleTerrainChunkState* centerChunk = findSampleTerrainChunk(sampleTerrainChunks, centerChunkId))
                {
                    if (centerChunk->setupRegistered)
                    {
                        centerChunk->resident = true;
                        terrainResidencyRequests.push(
                            centerChunkId,
                            full_engine::WorldChunkResidencyRequestType::MakeResident);
                        engineTerrainPipelineDirty = true;
                    }
                }
                terrainResidencyControls.reloadCenterAfterUnload = false;
            }
        }

        result = renderer->beginFrame(frameDesc);
        if (result != full_renderer::RendererResult::Success)
        {
            std::cerr << "Renderer beginFrame failed: " << resultName(result) << '\n';
            break;
        }

        constexpr std::array<Vec3, 4> kStaticCasterCenters = {{
            {0.0f, 0.0f, 0.0f},
            {-7.0f, 0.0f, 14.0f},
            {6.0f, 0.0f, 28.0f},
            {-3.0f, 0.0f, 46.0f},
        }};
        constexpr std::array<Vec3, 4> kStaticCasterScales = {{
            {1.0f, 1.0f, 1.0f},
            {0.9f, 1.8f, 0.9f},
            {1.4f, 0.8f, 1.4f},
            {1.1f, 2.3f, 1.1f},
        }};
        full_renderer::FadeDesc sampleFade;
        sampleFade.enabled = sampleStructureFadeEnabled;
        sampleFade.visibility = std::clamp(sampleStructureFadeVisibility, 0.0f, 1.0f);
        sampleFade.mode = sampleStructureFadeMode;
        const std::uint32_t activeStaticDrawCount =
            static_cast<std::uint32_t>(std::clamp(sampleStaticDrawCount, 0, 128));
        std::vector<full_renderer::DrawItem> cubeDraws(activeStaticDrawCount);
        for (std::uint32_t drawIndex = 0; drawIndex < activeStaticDrawCount; ++drawIndex)
        {
            Vec3 center = {};
            Vec3 scale = {};
            if (drawIndex < kStaticCasterCenters.size())
            {
                center = kStaticCasterCenters[drawIndex];
                scale = kStaticCasterScales[drawIndex];
            }
            else
            {
                const std::uint32_t extraIndex =
                    drawIndex - static_cast<std::uint32_t>(kStaticCasterCenters.size());
                center = {
                    -22.0f + static_cast<float>(extraIndex % 12U) * 4.0f,
                    0.0f,
                    6.0f + static_cast<float>(extraIndex / 12U) * 5.0f};
                scale = {
                    0.65f + static_cast<float>(extraIndex % 3U) * 0.15f,
                    0.85f + static_cast<float>((extraIndex / 3U) % 4U) * 0.25f,
                    0.65f};
            }
            cubeDraws[drawIndex].mesh = cubeMesh;
            cubeDraws[drawIndex].material = cubeMaterial;
            makeScaleTranslation(
                cubeDraws[drawIndex].model,
                scale,
                center);
            cubeDraws[drawIndex].bounds = makeCubeBounds(center, scale);
            cubeDraws[drawIndex].castsShadow = true;
            cubeDraws[drawIndex].selected = selectStaticMeshes && drawIndex == 1U;
            if (fadeStaticStructure && drawIndex == 2U)
            {
                cubeDraws[drawIndex].fade = sampleFade;
            }
        }

        constexpr std::array<Vec3, 8> kInstancedCasterCenters = {{
            {-10.0f, 0.0f, 6.0f},
            {-5.0f, 0.0f, 10.0f},
            {5.0f, 0.0f, 10.0f},
            {10.0f, 0.0f, 6.0f},
            {-12.0f, 0.0f, 24.0f},
            {-4.0f, 0.0f, 34.0f},
            {4.0f, 0.0f, 38.0f},
            {12.0f, 0.0f, 30.0f},
        }};
        constexpr Vec3 kInstancedCasterScale = {0.55f, 1.25f, 0.55f};
        const std::uint32_t activeInstancedInstanceCount =
            static_cast<std::uint32_t>(std::clamp(sampleInstancedInstanceCount, 0, 512));
        std::vector<float> instancedCubeMatrices(
            static_cast<std::size_t>(activeInstancedInstanceCount) * 16U,
            0.0f);
        full_renderer::Aabb instancedBounds = {};
        bool hasInstancedBounds = false;
        for (std::uint32_t instanceIndex = 0; instanceIndex < activeInstancedInstanceCount; ++instanceIndex)
        {
            Vec3 center = {};
            if (instanceIndex < kInstancedCasterCenters.size())
            {
                center = kInstancedCasterCenters[instanceIndex];
            }
            else
            {
                const std::uint32_t extraIndex =
                    instanceIndex - static_cast<std::uint32_t>(kInstancedCasterCenters.size());
                center = {
                    -28.0f + static_cast<float>(extraIndex % 16U) * 3.5f,
                    0.0f,
                    18.0f + static_cast<float>(extraIndex / 16U) * 4.0f};
            }
            makeScaleTranslation(
                instancedCubeMatrices.data() + static_cast<std::size_t>(instanceIndex) * 16U,
                kInstancedCasterScale,
                center);
            const full_renderer::Aabb instanceBounds =
                makeCubeBounds(center, kInstancedCasterScale);
            if (!hasInstancedBounds)
            {
                instancedBounds = instanceBounds;
                hasInstancedBounds = true;
            }
            else
            {
                for (int axis = 0; axis < 3; ++axis)
                {
                    instancedBounds.min[axis] = std::min(instancedBounds.min[axis], instanceBounds.min[axis]);
                    instancedBounds.max[axis] = std::max(instancedBounds.max[axis], instanceBounds.max[axis]);
                }
            }
        }
        full_renderer::InstancedDrawDesc instancedCubes;
        instancedCubes.mesh = cubeMesh;
        instancedCubes.material = cubeMaterial;
        instancedCubes.modelMatrices = instancedCubeMatrices.empty() ? nullptr : instancedCubeMatrices.data();
        instancedCubes.instanceCount = activeInstancedInstanceCount;
        instancedCubes.bounds = instancedBounds;
        instancedCubes.castsShadow = true;
        instancedCubes.selected = selectInstancedBatch;
        if (fadeInstancedStructure)
        {
            instancedCubes.fade = sampleFade;
        }

        float sampleJointModelMatrices[2][16] = {};
        setIdentity(sampleJointModelMatrices[0]);
        const float bendRadians = std::sin(animationTimeSeconds * 1.4f) * 0.55f;
        makeRotationZTranslation(sampleJointModelMatrices[1], bendRadians, {0.0f, 1.0f, 0.0f});
        float samplePaletteMatrices[2][16] = {};
        setIdentity(samplePaletteMatrices[0]);
        multiplyMatrices(sampleJointModelMatrices[1], skeletonJoints[1].inverseBindPose, samplePaletteMatrices[1]);

        const std::uint32_t activeSkinnedDrawCount =
            static_cast<std::uint32_t>(std::clamp(sampleSkinnedDrawCount, 0, 32));
        std::vector<full_renderer::AnimatedDrawItem> animatedDraws(activeSkinnedDrawCount);
        for (std::uint32_t drawIndex = 0; drawIndex < activeSkinnedDrawCount; ++drawIndex)
        {
            const Vec3 center = {
                3.5f + static_cast<float>(drawIndex % 4U) * 2.5f,
                -0.95f,
                8.0f + static_cast<float>(drawIndex / 4U) * 4.0f};
            full_renderer::AnimatedDrawItem& animatedDraw = animatedDraws[drawIndex];
            animatedDraw.mesh = sampleSkinnedMesh;
            animatedDraw.material = cubeMaterial;
            makeTranslation(animatedDraw.model, center);
            animatedDraw.bounds = makeBounds(
                center.x - 0.7f,
                -1.0f,
                center.z - 0.2f,
                center.x + 0.7f,
                1.4f,
                center.z + 0.2f);
            animatedDraw.palette.skinningMatrices = &samplePaletteMatrices[0][0];
            animatedDraw.palette.matrixCount = 2;
            animatedDraw.palette.debugJointModelMatrices = &sampleJointModelMatrices[0][0];
            animatedDraw.palette.debugJointModelMatrixCount = 2;
            animatedDraw.receivesShadow = true;
            animatedDraw.castsShadow = true;
            animatedDraw.selected = selectSkinnedMesh && drawIndex == 0U;
            if (fadeSkinnedStructure && drawIndex == 0U)
            {
                animatedDraw.fade = sampleFade;
            }
        }

        const std::uint32_t activeDecalCount = static_cast<std::uint32_t>(
            std::clamp(sampleDecalCount, 0, static_cast<int>(full_renderer::kMaxFrameDecals)));
        std::vector<full_renderer::DecalDesc> sampleDecals(activeDecalCount);
        for (std::uint32_t decalIndex = 0; decalIndex < activeDecalCount; ++decalIndex)
        {
            full_renderer::DecalDesc& decal = sampleDecals[decalIndex];
            if (decalIndex == 0U)
            {
                makeTranslation(decal.transform, {-6.0f, -0.55f, 12.0f});
                decal.halfExtentsMeters[0] = 3.0f;
                decal.halfExtentsMeters[1] = 0.85f;
                decal.halfExtentsMeters[2] = 3.0f;
                decal.tintColorLinear[0] = 0.9f;
                decal.tintColorLinear[1] = 0.25f;
                decal.tintColorLinear[2] = 0.18f;
                decal.tintColorLinear[3] = 1.0f;
                decal.opacity = 0.65f;
                decal.sortKey = 10;
                decal.albedoTexture = decalTextures[0];
            }
            else if (decalIndex == 1U)
            {
                makeTranslation(decal.transform, {2.0f, -0.55f, 20.0f});
                decal.halfExtentsMeters[0] = 2.0f;
                decal.halfExtentsMeters[1] = 0.9f;
                decal.halfExtentsMeters[2] = 4.0f;
                decal.tintColorLinear[0] = 0.15f;
                decal.tintColorLinear[1] = 0.55f;
                decal.tintColorLinear[2] = 1.0f;
                decal.tintColorLinear[3] = 1.0f;
                decal.opacity = 0.5f;
                decal.sortKey = 20;
                decal.albedoTexture = decalTextures[1];
            }
            else if (decalIndex == 2U)
            {
                makeTranslation(decal.transform, {9.0f, -0.55f, 31.0f});
                decal.halfExtentsMeters[0] = 2.5f;
                decal.halfExtentsMeters[1] = 0.85f;
                decal.halfExtentsMeters[2] = 2.5f;
                decal.tintColorLinear[0] = 1.0f;
                decal.tintColorLinear[1] = 0.85f;
                decal.tintColorLinear[2] = 0.22f;
                decal.tintColorLinear[3] = 1.0f;
                decal.opacity = 0.55f;
                decal.sortKey = 30;
            }
            else if (decalIndex == 3U)
            {
                makeTranslation(decal.transform, {160.0f, -0.55f, -120.0f});
                decal.halfExtentsMeters[0] = 3.5f;
                decal.halfExtentsMeters[1] = 0.9f;
                decal.halfExtentsMeters[2] = 3.5f;
                decal.tintColorLinear[0] = 0.8f;
                decal.tintColorLinear[1] = 0.2f;
                decal.tintColorLinear[2] = 1.0f;
                decal.tintColorLinear[3] = 1.0f;
                decal.opacity = 0.5f;
                decal.sortKey = 40;
                decal.albedoTexture = decalTextures[0];
            }
            else
            {
                const std::uint32_t extraIndex = decalIndex - 4U;
                makeTranslation(
                    decal.transform,
                    {
                        -18.0f + static_cast<float>(extraIndex % 8U) * 5.0f,
                        -0.55f,
                        10.0f + static_cast<float>(extraIndex / 8U) * 5.0f});
                decal.halfExtentsMeters[0] = 1.2f + static_cast<float>(extraIndex % 3U) * 0.35f;
                decal.halfExtentsMeters[1] = 0.85f;
                decal.halfExtentsMeters[2] = 1.5f + static_cast<float>((extraIndex / 3U) % 3U) * 0.35f;
                decal.tintColorLinear[0] = 0.35f + static_cast<float>(extraIndex % 5U) * 0.10f;
                decal.tintColorLinear[1] = 0.25f + static_cast<float>((extraIndex / 2U) % 5U) * 0.11f;
                decal.tintColorLinear[2] = 0.95f - static_cast<float>(extraIndex % 4U) * 0.10f;
                decal.tintColorLinear[3] = 1.0f;
                decal.opacity = 0.38f + static_cast<float>(extraIndex % 4U) * 0.05f;
                decal.sortKey = 50U + extraIndex;
                if ((extraIndex % 5U) != 0U)
                {
                    decal.albedoTexture = decalTextures[extraIndex % decalTextures.size()];
                }
            }
        }
        decalSubmit.decals = sampleDecals.empty() ? nullptr : sampleDecals.data();
        decalSubmit.decalCount = activeDecalCount;

        std::array<full_renderer::Particle, 256> sampleParticles = {};
        std::array<full_renderer::Particle, 16> offCameraParticles = {};
        const std::uint32_t activeParticleCount = static_cast<std::uint32_t>(
            std::clamp(sampleParticleCount, 0, static_cast<int>(sampleParticles.size())));
        const float particleDenominator = activeParticleCount > 1U ?
            static_cast<float>(activeParticleCount - 1U) :
            1.0f;
        for (std::uint32_t particleIndex = 0; particleIndex < activeParticleCount; ++particleIndex)
        {
            const float t = static_cast<float>(particleIndex) / particleDenominator;
            const float ring = t * 6.283185307f * 3.0f + animationTimeSeconds * 0.7f;
            const float plume = std::fmod(animationTimeSeconds * 0.18f + t, 1.0f);
            full_renderer::Particle& particle = sampleParticles[particleIndex];
            particle.positionWorld[0] = -7.0f + std::cos(ring) * (0.35f + plume * 1.15f);
            particle.positionWorld[1] = -0.45f + plume * 3.5f;
            particle.positionWorld[2] = 14.0f + std::sin(ring) * (0.35f + plume * 1.15f);
            particle.sizeMeters = 0.25f + plume * 0.85f;
            particle.colorLinear[0] = 0.60f;
            particle.colorLinear[1] = 0.64f;
            particle.colorLinear[2] = 0.66f;
            particle.colorLinear[3] = (1.0f - plume) * 0.32f;
            particle.rotationRadians = ring * 0.37f;
        }
        full_renderer::ParticleBatchDesc particleBatch;
        particleBatch.particles = sampleParticles.data();
        particleBatch.particleCount =
            particleSubmit.enabled && sampleParticleEmissionEnabled ? activeParticleCount : 0U;
        particleBatch.texture = particleTexture;
        particleBatch.blendMode = full_renderer::ParticleBlendMode::Alpha;
        particleBatch.sortMode = particleSubmit.sortMode == full_renderer::ParticleSortMode::ParticleDistanceBackToFront ?
            full_renderer::ParticleSortMode::ParticleDistanceBackToFront :
            full_renderer::ParticleSortMode::SubmissionOrder;

        for (std::uint32_t particleIndex = 0; particleIndex < offCameraParticles.size(); ++particleIndex)
        {
            full_renderer::Particle& particle = offCameraParticles[particleIndex];
            const float t = static_cast<float>(particleIndex) / static_cast<float>(offCameraParticles.size());
            particle.positionWorld[0] = 380.0f + t * 6.0f;
            particle.positionWorld[1] = 2.0f + t;
            particle.positionWorld[2] = -260.0f;
            particle.sizeMeters = 1.0f;
            particle.colorLinear[0] = 0.65f;
            particle.colorLinear[1] = 0.70f;
            particle.colorLinear[2] = 0.75f;
            particle.colorLinear[3] = 0.24f;
            particle.rotationRadians = animationTimeSeconds * 0.2f + t;
        }
        full_renderer::ParticleBatchDesc offCameraParticleBatch;
        offCameraParticleBatch.particles = offCameraParticles.data();
        offCameraParticleBatch.particleCount =
            particleSubmit.enabled && sampleParticleEmissionEnabled ?
                static_cast<std::uint32_t>(offCameraParticles.size()) :
                0U;
        offCameraParticleBatch.texture = particleTexture;
        offCameraParticleBatch.blendMode = full_renderer::ParticleBlendMode::Alpha;
        offCameraParticleBatch.sortMode = particleBatch.sortMode;

        std::array<full_renderer::Particle, 256> precipitationParticles = {};
        const bool precipitationRequested =
            particleSubmit.enabled &&
            sampleWeatherPrecipitationEnabled &&
            weather.enabled &&
            weather.precipitation.enabled &&
            weather.precipitation.type != full_renderer::PrecipitationType::None &&
            weather.precipitation.intensity > 0.0f;
        const float precipitationIntensity = std::clamp(weather.precipitation.intensity, 0.0f, 1.0f);
        samplePrecipitationParticleCount = precipitationRequested ?
            static_cast<std::uint32_t>(std::clamp(
                static_cast<int>(static_cast<float>(samplePrecipitationParticleBudget) * precipitationIntensity),
                0,
                static_cast<int>(precipitationParticles.size()))) :
            0U;
        Vec3 precipitationDirection = normalize({
            weather.precipitation.directionWorld[0],
            weather.precipitation.directionWorld[1],
            weather.precipitation.directionWorld[2]});
        if (dot(precipitationDirection, precipitationDirection) <= 0.000001f)
        {
            precipitationDirection = {0.0f, -1.0f, 0.0f};
        }
        Vec3 windDirection = normalize({
            weather.wind.directionWorld[0],
            weather.wind.directionWorld[1],
            weather.wind.directionWorld[2]});
        const float windSpeed = weather.wind.enabled ?
            std::clamp(weather.wind.speedMetersPerSecond, 0.0f, 24.0f) :
            0.0f;
        for (std::uint32_t particleIndex = 0; particleIndex < samplePrecipitationParticleCount; ++particleIndex)
        {
            const float t = static_cast<float>(particleIndex) /
                static_cast<float>(std::max<std::uint32_t>(samplePrecipitationParticleCount, 1U));
            const float phase = std::fmod(animationTimeSeconds * 0.52f + t * 3.71f, 1.0f);
            const float angle = t * 6.283185307f * 17.0f;
            const float radius = 8.0f + std::fmod(t * 97.0f, 1.0f) * 34.0f;
            Vec3 center = {
                cameraPosition.x + std::cos(angle) * radius,
                cameraPosition.y + 10.0f - phase * 16.0f,
                cameraPosition.z + std::sin(angle) * radius};
            center = center + precipitationDirection * (phase * 6.0f) + windDirection * (windSpeed * 0.18f * phase);
            if (weather.precipitation.type == full_renderer::PrecipitationType::Dust)
            {
                center.y = kTerrainY + 0.55f + std::fmod(t * 11.0f + animationTimeSeconds * 0.18f, 1.0f) * 2.4f;
                center = center + windDirection * (2.5f + phase * 3.5f);
            }

            full_renderer::Particle& particle = precipitationParticles[particleIndex];
            particle.positionWorld[0] = center.x;
            particle.positionWorld[1] = center.y;
            particle.positionWorld[2] = center.z;
            const float sizeScale = std::clamp(weather.precipitation.particleSizeScale, 0.0f, 4.0f);
            const float alphaScale = std::clamp(weather.precipitation.particleAlphaScale, 0.0f, 1.0f);
            if (weather.precipitation.type == full_renderer::PrecipitationType::Rain)
            {
                particle.sizeMeters = 0.05f * std::max(sizeScale, 0.25f);
            }
            else if (weather.precipitation.type == full_renderer::PrecipitationType::Snow)
            {
                particle.sizeMeters = 0.16f * std::max(sizeScale, 0.25f);
            }
            else
            {
                particle.sizeMeters = 0.35f * std::max(sizeScale, 0.25f);
            }
            particle.colorLinear[0] = weather.precipitation.particleTintLinear[0];
            particle.colorLinear[1] = weather.precipitation.particleTintLinear[1];
            particle.colorLinear[2] = weather.precipitation.particleTintLinear[2];
            particle.colorLinear[3] =
                weather.precipitation.particleTintLinear[3] * alphaScale * std::max(precipitationIntensity, 0.05f);
            particle.rotationRadians = angle + animationTimeSeconds * 0.25f;
        }

        full_renderer::ParticleBatchDesc precipitationParticleBatch;
        precipitationParticleBatch.particles = precipitationParticles.data();
        precipitationParticleBatch.particleCount = samplePrecipitationParticleCount;
        precipitationParticleBatch.texture = particleTexture;
        precipitationParticleBatch.blendMode = full_renderer::ParticleBlendMode::Alpha;
        precipitationParticleBatch.sortMode = particleBatch.sortMode;

        std::array<full_renderer::ParticleBatchDesc, 3> particleBatches = {
            particleBatch,
            offCameraParticleBatch,
            precipitationParticleBatch};
        particleSubmit.batches = particleBatches.data();
        particleSubmit.batchCount = static_cast<std::uint32_t>(particleBatches.size());

        full_renderer::RenderPacket packet;
        makeLookAt(packet.view.view, cameraPosition, cameraTarget, {0.0f, 1.0f, 0.0f});
        const float aspectRatio = static_cast<float>(frameDesc.backbufferWidth) /
            static_cast<float>(frameDesc.backbufferHeight);
        makePerspective(packet.view.projection, cameraFovYRadians, aspectRatio, cameraNearMeters, cameraFarMeters);
        packet.directionalLight.directionWorld[0] = -0.4f;
        packet.directionalLight.directionWorld[1] = 0.8f;
        packet.directionalLight.directionWorld[2] = -0.5f;
        packet.directionalLight.colorLinear[0] = 1.0f;
        packet.directionalLight.colorLinear[1] = 0.96f;
        packet.directionalLight.colorLinear[2] = 0.88f;
        packet.directionalLight.intensity = 1.2f;
        packet.environment = environment;
        packet.weather = weather;
        packet.ssao = ssao;
        packet.colorGrading = colorGrading;
        packet.decals = &decalSubmit;
        packet.particles = &particleSubmit;
        packet.directionalShadow = directionalShadow;
        packet.selectionOutline = selectionOutline;
        packet.directionalShadow.centerWorld[0] = cameraPosition.x;
        packet.directionalShadow.centerWorld[1] = kTerrainY;
        packet.directionalShadow.centerWorld[2] = cameraPosition.z;
        packet.drawItems = cubeDraws.empty() ? nullptr : cubeDraws.data();
        packet.drawItemCount = static_cast<std::uint32_t>(cubeDraws.size());
        packet.instancedDraws = activeInstancedInstanceCount > 0U ? &instancedCubes : nullptr;
        packet.instancedDrawCount = activeInstancedInstanceCount > 0U ? 1U : 0U;
        packet.animatedDraws = animatedDraws.empty() ? nullptr : animatedDraws.data();
        packet.animatedDrawCount = static_cast<std::uint32_t>(animatedDraws.size());
        packet.animationDebug = animationDebugOptions;
        refreshTerrainSubmitHandles(engineTerrainHandles, terrainChunks);
        full_renderer::TerrainSubmitDesc terrainSubmit;
        terrainSubmit.chunks = terrainChunks.data();
        terrainSubmit.chunkCount = static_cast<std::uint32_t>(terrainChunks.size());
        terrainSubmit.cameraPositionWorld[0] = cameraPosition.x;
        terrainSubmit.cameraPositionWorld[1] = cameraPosition.y;
        terrainSubmit.cameraPositionWorld[2] = cameraPosition.z;
        terrainSubmit.debug = terrainDebugOptions;
        terrainSubmit.debug.captureChunkInfo = true;
        terrainSubmit.debug.captureBatchInfo = true;
        packet.terrain = &terrainSubmit;

        result = renderer->submit(packet);
        if (result != full_renderer::RendererResult::Success)
        {
            std::cerr << "Renderer submit failed: " << resultName(result) << '\n';
            (void)renderer->endFrame();
            break;
        }

#if FULL_RENDERER_ENABLE_DEBUG_UI
        if (imguiInitialized)
        {
            auto* concreteRenderer = dynamic_cast<full_renderer::core::Renderer*>(renderer.get());
            drawTerrainDiagnosticsPanel(
                *renderer,
                concreteRenderer,
                imguiRenderer,
                terrainDebugOptions,
                animationDebugOptions,
                environment,
                weather,
                ssao,
                colorGrading,
                decalSubmit,
                particleSubmit,
                directionalShadow,
                selectionOutline,
                sampleParticleEmissionEnabled,
                sampleParticleCount,
                sampleWeatherPrecipitationEnabled,
                samplePrecipitationParticleBudget,
                samplePrecipitationParticleCount,
                selectStaticMeshes,
                selectInstancedBatch,
                selectSkinnedMesh,
                sampleStructureFadeEnabled,
                sampleStructureFadeVisibility,
                sampleStructureFadeMode,
                fadeStaticStructure,
                fadeInstancedStructure,
                fadeSkinnedStructure,
                sampleStaticDrawCount,
                sampleInstancedInstanceCount,
                sampleSkinnedDrawCount,
                sampleDecalCount,
                validationState,
                openWorldChurnState,
                terrainDiagnostics,
                sampleTerrainChunks,
                terrainResidencyRequests,
                terrainSetupRequests,
                terrainResidencyControls,
                kGridRadius,
                engineTerrainPipelineDirty,
                cameraPosition,
                cameraTarget,
                cameraFovYRadians,
                cameraNearMeters,
                cameraFarMeters,
                packet.directionalShadow,
                terrainMaterialDesc.terrain,
                kTerrainSkirtsEnabled,
                kTerrainSkirtDepth,
                packet.directionalLight,
                packet.view,
                frameDesc.backbufferWidth,
                frameDesc.backbufferHeight,
                shadowPreview,
                showDebugUi);
            ImGui::Render();
            if (!imguiRenderer.render(ImGui::GetDrawData(), frameDesc.backbufferWidth, frameDesc.backbufferHeight))
            {
                std::cerr << "Dear ImGui rendering failed.\n";
                (void)renderer->endFrame();
                break;
            }
        }
#endif

        result = renderer->endFrame();
        if (result != full_renderer::RendererResult::Success)
        {
            std::cerr << "Renderer endFrame failed: " << resultName(result) << '\n';
            break;
        }

        const full_renderer::TerrainStats terrainStats = renderer->getTerrainStats();
        const std::string title =
            "FullRenderer Phase 2 - terrain visible " +
            std::to_string(terrainStats.visibleChunks) +
            "/" +
            std::to_string(terrainStats.submittedChunks) +
            " culled " +
            std::to_string(terrainStats.culledChunks) +
            " draws " +
            std::to_string(terrainStats.terrainDraws) +
            " shadow casters " +
            std::to_string(terrainStats.shadowCasterChunks) +
            " offcam " +
            std::to_string(terrainStats.offCameraShadowCasterChunks) +
            " lod " +
            std::to_string(terrainStats.visibleChunksByLod[0]) +
            "/" +
            std::to_string(terrainStats.visibleChunksByLod[1]) +
            "/" +
            std::to_string(terrainStats.visibleChunksByLod[2]) +
            "/" +
            std::to_string(terrainStats.visibleChunksByLod[3]);
        SDL_SetWindowTitle(window.get(), title.c_str());
    }

    destroyMappedTerrainChunks(*renderer, engineTerrainHandles);
    removeSampleTerrainSetup(
        engineTerrainRegistry,
        engineTerrainCatalog,
        engineTerrainResources,
        sampleTerrainChunks);
    for (const full_renderer::MeshHandle mesh : terrainMeshes)
    {
        renderer->destroyMesh(mesh);
    }
    renderer->destroyMaterial(terrainMaterial);
    renderer->destroyTexture(terrainSplatMap);
    for (const full_renderer::TextureHandle texture : decalTextures)
    {
        if (full_renderer::isValid(texture))
        {
            renderer->destroyTexture(texture);
        }
    }
    if (full_renderer::isValid(particleTexture))
    {
        renderer->destroyTexture(particleTexture);
    }
    for (const full_renderer::TextureHandle texture : layerTextures)
    {
        renderer->destroyTexture(texture);
    }
    for (const full_renderer::TextureHandle texture : normalTextures)
    {
        renderer->destroyTexture(texture);
    }
    renderer->destroyMaterial(cubeMaterial);
    renderer->destroySkinnedMesh(sampleSkinnedMesh);
    renderer->destroySkeleton(sampleSkeleton);
    renderer->destroyMesh(cubeMesh);
#if FULL_RENDERER_ENABLE_DEBUG_UI
    if (imguiInitialized)
    {
        imguiRenderer.shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
#endif
    renderer->shutdown();
    return 0;
}
