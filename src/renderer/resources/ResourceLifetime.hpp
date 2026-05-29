#pragma once

#include "full_renderer/Renderer.hpp"

#include <cstdint>

namespace full_renderer::resources
{
enum class ResourceMemoryFormat
{
    Rgba8,
    R32F,
    D24
};

std::uint64_t estimateBufferBytes(std::uint32_t elementCount, std::uint32_t strideBytes) noexcept;
std::uint64_t estimateTexture2DBytes(
    std::uint32_t width,
    std::uint32_t height,
    ResourceMemoryFormat format) noexcept;
std::uint64_t estimateTexture2DBytes(
    std::uint32_t width,
    std::uint32_t height,
    TextureFormat format) noexcept;
std::uint64_t addSaturating(std::uint64_t lhs, std::uint64_t rhs) noexcept;
} // namespace full_renderer::resources
