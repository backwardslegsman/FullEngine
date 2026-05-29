#include "renderer/bgfx/ImguiBgfxRenderer.hpp"

#if defined(FULL_RENDERER_ENABLE_DEBUG_UI) && FULL_RENDERER_ENABLE_DEBUG_UI

#include <bx/math.h>
#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace full_renderer::bgfx_backend
{
namespace
{
struct PreviewVertex
{
    float position[2] = {};
    float texcoord[2] = {};
    std::uint32_t color = 0xffffffffU;
};

constexpr bgfx::ViewId kShadowPreviewViewId = 254;
constexpr bgfx::ViewId kImguiViewId = 255;

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

    return bgfx::createShader(bgfx::copy(bytes.data(), static_cast<std::uint32_t>(bytes.size())));
}

ImTextureID packTexture(const bgfx::TextureHandle handle) noexcept
{
    return static_cast<ImTextureID>(handle.idx);
}

bgfx::TextureHandle unpackTexture(const ImTextureID textureId) noexcept
{
    return bgfx::TextureHandle{static_cast<std::uint16_t>(textureId)};
}
} // namespace

ImguiBgfxRenderer::~ImguiBgfxRenderer()
{
    shutdown();
}

bool ImguiBgfxRenderer::initialize(const char* shaderBinaryDirectory)
{
    if (initialized_)
    {
        return true;
    }

    if (shaderBinaryDirectory == nullptr || shaderBinaryDirectory[0] == '\0')
    {
        return false;
    }

    shaderBinaryDirectory_ = shaderBinaryDirectory;
    vertexLayout_
        .begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    textureSampler_ = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
    const bgfx::ShaderHandle vertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_imgui.bin"));
    const bgfx::ShaderHandle fragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_imgui.bin"));
    const bgfx::ShaderHandle previewVertexShader = loadShader(joinPath(shaderBinaryDirectory_, "vs_shadow_preview.bin"));
    const bgfx::ShaderHandle previewFragmentShader = loadShader(joinPath(shaderBinaryDirectory_, "fs_shadow_preview.bin"));
    if (!bgfx::isValid(textureSampler_) ||
        !bgfx::isValid(previewVertexShader) ||
        !bgfx::isValid(previewFragmentShader) ||
        !bgfx::isValid(vertexShader) ||
        !bgfx::isValid(fragmentShader))
    {
        if (bgfx::isValid(vertexShader))
        {
            bgfx::destroy(vertexShader);
        }
        if (bgfx::isValid(fragmentShader))
        {
            bgfx::destroy(fragmentShader);
        }
        if (bgfx::isValid(previewVertexShader))
        {
            bgfx::destroy(previewVertexShader);
        }
        if (bgfx::isValid(previewFragmentShader))
        {
            bgfx::destroy(previewFragmentShader);
        }
        shutdown();
        return false;
    }

    shadowPreviewSampler_ = bgfx::createUniform("s_shadowPreview", bgfx::UniformType::Sampler);
    shadowPreviewParamsUniform_ = bgfx::createUniform("u_shadowPreviewParams", bgfx::UniformType::Vec4);
    program_ = bgfx::createProgram(vertexShader, fragmentShader, true);
    shadowPreviewProgram_ = bgfx::createProgram(previewVertexShader, previewFragmentShader, true);
    if (!bgfx::isValid(program_) ||
        !bgfx::isValid(shadowPreviewProgram_) ||
        !bgfx::isValid(shadowPreviewSampler_) ||
        !bgfx::isValid(shadowPreviewParamsUniform_) ||
        !createFontsTexture())
    {
        shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

void ImguiBgfxRenderer::shutdown() noexcept
{
    destroyShadowPreviewResources();
    destroyFontsTexture();

    if (bgfx::isValid(shadowPreviewProgram_))
    {
        bgfx::destroy(shadowPreviewProgram_);
        shadowPreviewProgram_ = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(program_))
    {
        bgfx::destroy(program_);
        program_ = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(textureSampler_))
    {
        bgfx::destroy(textureSampler_);
        textureSampler_ = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(shadowPreviewSampler_))
    {
        bgfx::destroy(shadowPreviewSampler_);
        shadowPreviewSampler_ = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(shadowPreviewParamsUniform_))
    {
        bgfx::destroy(shadowPreviewParamsUniform_);
        shadowPreviewParamsUniform_ = BGFX_INVALID_HANDLE;
    }

    shaderBinaryDirectory_.clear();
    initialized_ = false;
}

bool ImguiBgfxRenderer::createFontsTexture()
{
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (pixels == nullptr || width <= 0 || height <= 0)
    {
        return false;
    }

    const std::uint32_t byteCount = static_cast<std::uint32_t>(width) *
        static_cast<std::uint32_t>(height) *
        4U;
    fontTexture_ = bgfx::createTexture2D(
        static_cast<std::uint16_t>(width),
        static_cast<std::uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        bgfx::copy(pixels, byteCount));
    if (!bgfx::isValid(fontTexture_))
    {
        return false;
    }

    io.Fonts->SetTexID(packTexture(fontTexture_));
    return true;
}

void ImguiBgfxRenderer::destroyFontsTexture() noexcept
{
    if (bgfx::isValid(fontTexture_))
    {
        ImGui::GetIO().Fonts->SetTexID(0);
        bgfx::destroy(fontTexture_);
        fontTexture_ = BGFX_INVALID_HANDLE;
    }
}

bool ImguiBgfxRenderer::ensureShadowPreviewResources(const std::uint32_t previewSize)
{
    if (previewSize == 0)
    {
        return false;
    }

    if (shadowPreviewSize_ == previewSize &&
        bgfx::isValid(shadowPreviewFrameBuffer_) &&
        bgfx::isValid(shadowPreviewTexture_))
    {
        return true;
    }

    destroyShadowPreviewResources();
    shadowPreviewTexture_ = bgfx::createTexture2D(
        static_cast<std::uint16_t>(previewSize),
        static_cast<std::uint16_t>(previewSize),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);
    if (!bgfx::isValid(shadowPreviewTexture_))
    {
        destroyShadowPreviewResources();
        return false;
    }

    shadowPreviewFrameBuffer_ = bgfx::createFrameBuffer(1, &shadowPreviewTexture_, true);
    if (!bgfx::isValid(shadowPreviewFrameBuffer_))
    {
        destroyShadowPreviewResources();
        return false;
    }

    shadowPreviewSize_ = previewSize;
    return true;
}

void ImguiBgfxRenderer::destroyShadowPreviewResources() noexcept
{
    if (bgfx::isValid(shadowPreviewFrameBuffer_))
    {
        bgfx::destroy(shadowPreviewFrameBuffer_);
        shadowPreviewFrameBuffer_ = BGFX_INVALID_HANDLE;
        shadowPreviewTexture_ = BGFX_INVALID_HANDLE;
    }
    else if (bgfx::isValid(shadowPreviewTexture_))
    {
        bgfx::destroy(shadowPreviewTexture_);
        shadowPreviewTexture_ = BGFX_INVALID_HANDLE;
    }

    shadowPreviewSize_ = 0;
}

bool ImguiBgfxRenderer::renderShadowPreview(
    const bgfx::TextureHandle sourceTexture,
    const std::uint32_t previewSize,
    const float blackDepth,
    const float whiteDepth,
    const bool invert)
{
    if (!initialized_ ||
        !bgfx::isValid(sourceTexture) ||
        !bgfx::isValid(shadowPreviewProgram_) ||
        !bgfx::isValid(shadowPreviewSampler_) ||
        !bgfx::isValid(shadowPreviewParamsUniform_) ||
        !ensureShadowPreviewResources(previewSize))
    {
        return false;
    }

    bgfx::setViewName(kShadowPreviewViewId, "Shadow Map Preview");
    bgfx::setViewMode(kShadowPreviewViewId, bgfx::ViewMode::Sequential);
    bgfx::setViewFrameBuffer(kShadowPreviewViewId, shadowPreviewFrameBuffer_);
    bgfx::setViewRect(
        kShadowPreviewViewId,
        0,
        0,
        static_cast<std::uint16_t>(previewSize),
        static_cast<std::uint16_t>(previewSize));
    bgfx::setViewClear(kShadowPreviewViewId, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);

    float ortho[16] = {};
    bx::mtxOrtho(
        ortho,
        0.0f,
        static_cast<float>(previewSize),
        static_cast<float>(previewSize),
        0.0f,
        0.0f,
        1000.0f,
        0.0f,
        bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(kShadowPreviewViewId, nullptr, ortho);

    constexpr std::uint32_t kVertexCount = 4;
    constexpr std::uint32_t kIndexCount = 6;
    if (bgfx::getAvailTransientVertexBuffer(kVertexCount, vertexLayout_) < kVertexCount ||
        bgfx::getAvailTransientIndexBuffer(kIndexCount) < kIndexCount)
    {
        return false;
    }

    bgfx::TransientVertexBuffer vertexBuffer;
    bgfx::TransientIndexBuffer indexBuffer;
    bgfx::allocTransientVertexBuffer(&vertexBuffer, kVertexCount, vertexLayout_);
    bgfx::allocTransientIndexBuffer(&indexBuffer, kIndexCount);

    const float size = static_cast<float>(previewSize);
    const PreviewVertex vertices[kVertexCount] = {
        {{0.0f, 0.0f}, {0.0f, 0.0f}, 0xffffffffU},
        {{size, 0.0f}, {1.0f, 0.0f}, 0xffffffffU},
        {{size, size}, {1.0f, 1.0f}, 0xffffffffU},
        {{0.0f, size}, {0.0f, 1.0f}, 0xffffffffU},
    };
    const std::uint16_t indices[kIndexCount] = {0, 1, 2, 0, 2, 3};
    std::memcpy(vertexBuffer.data, vertices, sizeof(vertices));
    std::memcpy(indexBuffer.data, indices, sizeof(indices));

    const float range = std::max(whiteDepth - blackDepth, 0.0001f);
    const float params[4] = {
        blackDepth,
        1.0f / range,
        invert ? 1.0f : 0.0f,
        0.0f};
    bgfx::setUniform(shadowPreviewParamsUniform_, params);
    bgfx::setTexture(0, shadowPreviewSampler_, sourceTexture);
    bgfx::setVertexBuffer(0, &vertexBuffer);
    bgfx::setIndexBuffer(&indexBuffer);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(kShadowPreviewViewId, shadowPreviewProgram_);
    return true;
}

ImTextureID ImguiBgfxRenderer::shadowPreviewTextureId() const noexcept
{
    return bgfx::isValid(shadowPreviewTexture_) ? packTexture(shadowPreviewTexture_) : ImTextureID{};
}

bool ImguiBgfxRenderer::render(
    ImDrawData* drawData,
    const std::uint32_t backbufferWidth,
    const std::uint32_t backbufferHeight)
{
    if (!initialized_ || drawData == nullptr || backbufferWidth == 0 || backbufferHeight == 0)
    {
        return false;
    }

    const int framebufferWidth = static_cast<int>(drawData->DisplaySize.x * drawData->FramebufferScale.x);
    const int framebufferHeight = static_cast<int>(drawData->DisplaySize.y * drawData->FramebufferScale.y);
    if (framebufferWidth <= 0 || framebufferHeight <= 0)
    {
        return true;
    }

    bgfx::setViewName(kImguiViewId, "Dear ImGui");
    bgfx::setViewMode(kImguiViewId, bgfx::ViewMode::Sequential);
    bgfx::setViewRect(
        kImguiViewId,
        0,
        0,
        static_cast<std::uint16_t>(backbufferWidth),
        static_cast<std::uint16_t>(backbufferHeight));

    float ortho[16] = {};
    const float left = drawData->DisplayPos.x;
    const float right = drawData->DisplayPos.x + drawData->DisplaySize.x;
    const float top = drawData->DisplayPos.y;
    const float bottom = drawData->DisplayPos.y + drawData->DisplaySize.y;
    bx::mtxOrtho(
        ortho,
        left,
        right,
        bottom,
        top,
        0.0f,
        1000.0f,
        0.0f,
        bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(kImguiViewId, nullptr, ortho);

    const ImVec2 clipOffset = drawData->DisplayPos;
    const ImVec2 clipScale = drawData->FramebufferScale;
    for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex)
    {
        const ImDrawList* drawList = drawData->CmdLists[listIndex];
        const std::uint32_t vertexCount = static_cast<std::uint32_t>(drawList->VtxBuffer.Size);
        const std::uint32_t indexCount = static_cast<std::uint32_t>(drawList->IdxBuffer.Size);
        if (vertexCount == 0 || indexCount == 0)
        {
            continue;
        }

        const bool index32 = sizeof(ImDrawIdx) == 4;
        if (bgfx::getAvailTransientVertexBuffer(vertexCount, vertexLayout_) < vertexCount ||
            bgfx::getAvailTransientIndexBuffer(indexCount, index32) < indexCount)
        {
            return false;
        }

        bgfx::TransientVertexBuffer vertexBuffer;
        bgfx::TransientIndexBuffer indexBuffer;
        bgfx::allocTransientVertexBuffer(&vertexBuffer, vertexCount, vertexLayout_);
        bgfx::allocTransientIndexBuffer(&indexBuffer, indexCount, index32);
        std::memcpy(vertexBuffer.data, drawList->VtxBuffer.Data, vertexCount * sizeof(ImDrawVert));
        std::memcpy(indexBuffer.data, drawList->IdxBuffer.Data, indexCount * sizeof(ImDrawIdx));

        std::uint32_t indexOffset = 0;
        for (int commandIndex = 0; commandIndex < drawList->CmdBuffer.Size; ++commandIndex)
        {
            const ImDrawCmd& command = drawList->CmdBuffer[commandIndex];
            if (command.UserCallback != nullptr)
            {
                command.UserCallback(drawList, &command);
                indexOffset += command.ElemCount;
                continue;
            }

            const float clipMinX = (command.ClipRect.x - clipOffset.x) * clipScale.x;
            const float clipMinY = (command.ClipRect.y - clipOffset.y) * clipScale.y;
            const float clipMaxX = (command.ClipRect.z - clipOffset.x) * clipScale.x;
            const float clipMaxY = (command.ClipRect.w - clipOffset.y) * clipScale.y;
            if (clipMaxX <= clipMinX || clipMaxY <= clipMinY)
            {
                indexOffset += command.ElemCount;
                continue;
            }

            const std::uint16_t scissorX = static_cast<std::uint16_t>(std::max(clipMinX, 0.0f));
            const std::uint16_t scissorY = static_cast<std::uint16_t>(std::max(clipMinY, 0.0f));
            const std::uint16_t scissorWidth = static_cast<std::uint16_t>(
                std::min(clipMaxX, static_cast<float>(backbufferWidth)) - static_cast<float>(scissorX));
            const std::uint16_t scissorHeight = static_cast<std::uint16_t>(
                std::min(clipMaxY, static_cast<float>(backbufferHeight)) - static_cast<float>(scissorY));

            bgfx::setScissor(scissorX, scissorY, scissorWidth, scissorHeight);
            bgfx::setTexture(0, textureSampler_, unpackTexture(command.GetTexID()));
            bgfx::setVertexBuffer(0, &vertexBuffer, command.VtxOffset, vertexCount);
            bgfx::setIndexBuffer(&indexBuffer, indexOffset + command.IdxOffset, command.ElemCount);
            bgfx::setState(
                BGFX_STATE_WRITE_RGB |
                BGFX_STATE_WRITE_A |
                BGFX_STATE_MSAA |
                BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));
            bgfx::submit(kImguiViewId, program_);
            indexOffset += command.ElemCount;
        }
    }

    return true;
}
} // namespace full_renderer::bgfx_backend

#endif
