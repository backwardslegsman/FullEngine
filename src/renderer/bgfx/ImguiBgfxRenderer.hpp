#pragma once

#if defined(FULL_RENDERER_ENABLE_DEBUG_UI) && FULL_RENDERER_ENABLE_DEBUG_UI

#include <bgfx/bgfx.h>
#include <imgui.h>

#include <cstdint>
#include <string>

struct ImDrawData;

namespace full_renderer::bgfx_backend
{
class ImguiBgfxRenderer
{
public:
    ImguiBgfxRenderer() = default;
    ~ImguiBgfxRenderer();

    ImguiBgfxRenderer(const ImguiBgfxRenderer&) = delete;
    ImguiBgfxRenderer& operator=(const ImguiBgfxRenderer&) = delete;
    ImguiBgfxRenderer(ImguiBgfxRenderer&&) = delete;
    ImguiBgfxRenderer& operator=(ImguiBgfxRenderer&&) = delete;

    bool initialize(const char* shaderBinaryDirectory);
    void shutdown() noexcept;
    bool renderShadowPreview(
        bgfx::TextureHandle sourceTexture,
        std::uint32_t previewSize,
        float blackDepth,
        float whiteDepth,
        bool invert);
    ImTextureID shadowPreviewTextureId() const noexcept;
    bool render(ImDrawData* drawData, std::uint32_t backbufferWidth, std::uint32_t backbufferHeight);

private:
    bool createFontsTexture();
    void destroyFontsTexture() noexcept;
    bool ensureShadowPreviewResources(std::uint32_t previewSize);
    void destroyShadowPreviewResources() noexcept;

    bgfx::VertexLayout vertexLayout_;
    bgfx::ProgramHandle program_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle shadowPreviewProgram_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle textureSampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle shadowPreviewSampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle shadowPreviewParamsUniform_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle fontTexture_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle shadowPreviewTexture_ = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle shadowPreviewFrameBuffer_ = BGFX_INVALID_HANDLE;
    std::uint32_t shadowPreviewSize_ = 0;
    std::string shaderBinaryDirectory_;
    bool initialized_ = false;
};
} // namespace full_renderer::bgfx_backend

#endif
