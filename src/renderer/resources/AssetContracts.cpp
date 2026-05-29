#include "renderer/resources/AssetContracts.hpp"

#include <cmath>
#include <cstddef>
#include <limits>

namespace full_renderer::resources
{
namespace
{
constexpr float kMinimumNormalLengthSquared = 0.000001f;
constexpr float kDegenerateAreaSquared = 0.000000000001f;

bool hasFiniteValues(const float* values, const std::size_t count) noexcept
{
    for (std::size_t index = 0; index < count; ++index)
    {
        if (!std::isfinite(values[index]))
        {
            return false;
        }
    }
    return true;
}

bool isUnitRangeColor(const float* values, const std::size_t count) noexcept
{
    for (std::size_t index = 0; index < count; ++index)
    {
        if (!std::isfinite(values[index]) || values[index] < 0.0f || values[index] > 1.0f)
        {
            return false;
        }
    }
    return true;
}

bool hasUsableNormal(const float normal[3]) noexcept
{
    if (!hasFiniteValues(normal, 3))
    {
        return false;
    }

    const float lengthSquared =
        normal[0] * normal[0] +
        normal[1] * normal[1] +
        normal[2] * normal[2];
    return std::isfinite(lengthSquared) && lengthSquared >= kMinimumNormalLengthSquared;
}

bool hasValidMeshStructure(const MeshDesc& desc) noexcept
{
    return desc.vertices != nullptr &&
        desc.indices != nullptr &&
        desc.vertexCount > 0U &&
        desc.indexCount > 0U &&
        (desc.indexCount % 3U) == 0U;
}

bool triangleIsDegenerate(
    const MeshVertex& a,
    const MeshVertex& b,
    const MeshVertex& c) noexcept
{
    const float ab[3] = {
        b.position[0] - a.position[0],
        b.position[1] - a.position[1],
        b.position[2] - a.position[2]};
    const float ac[3] = {
        c.position[0] - a.position[0],
        c.position[1] - a.position[1],
        c.position[2] - a.position[2]};
    const float cross[3] = {
        ab[1] * ac[2] - ab[2] * ac[1],
        ab[2] * ac[0] - ab[0] * ac[2],
        ab[0] * ac[1] - ab[1] * ac[0]};
    const float areaSquared =
        cross[0] * cross[0] +
        cross[1] * cross[1] +
        cross[2] * cross[2];
    return !std::isfinite(areaSquared) || areaSquared <= kDegenerateAreaSquared;
}
} // namespace

bool isValidTextureSemantic(const TextureSemantic semantic) noexcept
{
    switch (semantic)
    {
    case TextureSemantic::Color:
    case TextureSemantic::LinearData:
    case TextureSemantic::NormalMap:
    case TextureSemantic::TerrainSplat:
    case TextureSemantic::ColorGradingLut:
    case TextureSemantic::Debug:
        return true;
    }
    return false;
}

bool isValidTextureColorSpace(const TextureColorSpace colorSpace) noexcept
{
    switch (colorSpace)
    {
    case TextureColorSpace::Srgb:
    case TextureColorSpace::Linear:
    case TextureColorSpace::EncodedNormal:
        return true;
    }
    return false;
}

bool isTextureColorSpaceCompatible(
    const TextureSemantic semantic,
    const TextureColorSpace colorSpace) noexcept
{
    if (!isValidTextureSemantic(semantic) || !isValidTextureColorSpace(colorSpace))
    {
        return false;
    }

    switch (semantic)
    {
    case TextureSemantic::Color:
        return colorSpace == TextureColorSpace::Srgb;
    case TextureSemantic::LinearData:
    case TextureSemantic::TerrainSplat:
    case TextureSemantic::ColorGradingLut:
        return colorSpace == TextureColorSpace::Linear;
    case TextureSemantic::NormalMap:
        return colorSpace == TextureColorSpace::EncodedNormal;
    case TextureSemantic::Debug:
        return colorSpace == TextureColorSpace::Srgb || colorSpace == TextureColorSpace::Linear;
    }
    return false;
}

RendererResult validateMeshAssetContract(const MeshDesc& desc) noexcept
{
    if (!hasValidMeshStructure(desc))
    {
        return RendererResult::InvalidDescriptor;
    }

    for (std::uint32_t vertexIndex = 0; vertexIndex < desc.vertexCount; ++vertexIndex)
    {
        const MeshVertex& vertex = desc.vertices[vertexIndex];
        if (!hasFiniteValues(vertex.position, 3) ||
            !hasUsableNormal(vertex.normal) ||
            !isUnitRangeColor(vertex.colorLinear, 4))
        {
            return RendererResult::InvalidDescriptor;
        }
    }

    for (std::uint32_t index = 0; index < desc.indexCount; ++index)
    {
        if (desc.indices[index] >= desc.vertexCount)
        {
            return RendererResult::InvalidDescriptor;
        }
    }

    if (meshHasDegenerateTriangles(desc))
    {
        return RendererResult::InvalidDescriptor;
    }

    return RendererResult::Success;
}

bool computeMeshLocalBounds(const MeshDesc& desc, Aabb& outBounds) noexcept
{
    if (desc.vertices == nullptr || desc.vertexCount == 0U)
    {
        return false;
    }

    if (!hasFiniteValues(desc.vertices[0].position, 3))
    {
        return false;
    }

    for (std::uint32_t axis = 0; axis < 3U; ++axis)
    {
        outBounds.min[axis] = desc.vertices[0].position[axis];
        outBounds.max[axis] = desc.vertices[0].position[axis];
    }

    for (std::uint32_t vertexIndex = 1; vertexIndex < desc.vertexCount; ++vertexIndex)
    {
        const MeshVertex& vertex = desc.vertices[vertexIndex];
        if (!hasFiniteValues(vertex.position, 3))
        {
            return false;
        }
        for (std::uint32_t axis = 0; axis < 3U; ++axis)
        {
            if (vertex.position[axis] < outBounds.min[axis])
            {
                outBounds.min[axis] = vertex.position[axis];
            }
            if (vertex.position[axis] > outBounds.max[axis])
            {
                outBounds.max[axis] = vertex.position[axis];
            }
        }
    }
    return true;
}

bool meshHasDegenerateTriangles(const MeshDesc& desc) noexcept
{
    if (!hasValidMeshStructure(desc))
    {
        return true;
    }

    for (std::uint32_t index = 0; index < desc.indexCount; index += 3U)
    {
        const std::uint16_t a = desc.indices[index + 0U];
        const std::uint16_t b = desc.indices[index + 1U];
        const std::uint16_t c = desc.indices[index + 2U];
        if (a >= desc.vertexCount || b >= desc.vertexCount || c >= desc.vertexCount)
        {
            return true;
        }

        if (triangleIsDegenerate(desc.vertices[a], desc.vertices[b], desc.vertices[c]))
        {
            return true;
        }
    }
    return false;
}

RendererResult validateTextureAssetContract(const TextureDesc& desc) noexcept
{
    if (desc.width == 0U ||
        desc.height == 0U ||
        desc.width > 65535U ||
        desc.height > 65535U ||
        desc.format != TextureFormat::Rgba8 ||
        desc.data == nullptr ||
        desc.mipCount != 1U ||
        desc.compressed ||
        !isTextureColorSpaceCompatible(desc.semantic, desc.colorSpace))
    {
        return RendererResult::InvalidDescriptor;
    }

    constexpr std::uint32_t kBytesPerPixel = 4U;
    if (desc.width > (std::numeric_limits<std::uint32_t>::max() / kBytesPerPixel) ||
        desc.height > (std::numeric_limits<std::uint32_t>::max() / (desc.width * kBytesPerPixel)))
    {
        return RendererResult::InvalidDescriptor;
    }

    const std::uint32_t requiredBytes = desc.width * desc.height * kBytesPerPixel;
    if (desc.dataSizeBytes < requiredBytes)
    {
        return RendererResult::InvalidDescriptor;
    }

    return RendererResult::Success;
}
} // namespace full_renderer::resources
