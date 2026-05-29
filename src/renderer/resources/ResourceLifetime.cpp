#include "renderer/resources/ResourceLifetime.hpp"

#include <limits>

namespace full_renderer::resources
{
namespace
{
std::uint32_t bytesPerPixel(const ResourceMemoryFormat format) noexcept
{
    switch (format)
    {
    case ResourceMemoryFormat::Rgba8:
    case ResourceMemoryFormat::R32F:
    case ResourceMemoryFormat::D24:
        return 4;
    }

    return 0;
}
} // namespace

std::uint64_t addSaturating(const std::uint64_t lhs, const std::uint64_t rhs) noexcept
{
    const std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
    if (max - lhs < rhs)
    {
        return max;
    }

    return lhs + rhs;
}

std::uint64_t estimateBufferBytes(
    const std::uint32_t elementCount,
    const std::uint32_t strideBytes) noexcept
{
    return static_cast<std::uint64_t>(elementCount) * static_cast<std::uint64_t>(strideBytes);
}

std::uint64_t estimateTexture2DBytes(
    const std::uint32_t width,
    const std::uint32_t height,
    const ResourceMemoryFormat format) noexcept
{
    return static_cast<std::uint64_t>(width) *
        static_cast<std::uint64_t>(height) *
        static_cast<std::uint64_t>(bytesPerPixel(format));
}

std::uint64_t estimateTexture2DBytes(
    const std::uint32_t width,
    const std::uint32_t height,
    const TextureFormat format) noexcept
{
    switch (format)
    {
    case TextureFormat::Rgba8:
        return estimateTexture2DBytes(width, height, ResourceMemoryFormat::Rgba8);
    }

    return 0;
}
} // namespace full_renderer::resources
