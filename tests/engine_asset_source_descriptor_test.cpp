#include "engine/assets/AssetSourceDescriptor.hpp"

#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* const message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

full_engine::AssetSourceDescriptor meshDescriptor()
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.mesh.vertexCount = 4;
    descriptor.mesh.indexCount = 6;
    descriptor.mesh.localBounds.min[0] = -1.0f;
    descriptor.mesh.localBounds.min[1] = 0.0f;
    descriptor.mesh.localBounds.min[2] = -1.0f;
    descriptor.mesh.localBounds.max[0] = 1.0f;
    descriptor.mesh.localBounds.max[1] = 2.0f;
    descriptor.mesh.localBounds.max[2] = 1.0f;
    return descriptor;
}

full_engine::AssetSourceDescriptor textureDescriptor()
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.texture.width = 256;
    descriptor.texture.height = 128;
    descriptor.texture.mipCount = 1;
    descriptor.texture.format = full_engine::AssetSourceTextureFormat::Rgba8;
    descriptor.texture.semantic = full_engine::AssetSourceTextureSemantic::Color;
    descriptor.texture.colorSpace = full_engine::AssetSourceTextureColorSpace::Srgb;
    return descriptor;
}

full_engine::AssetSourceDescriptor materialDescriptor()
{
    full_engine::AssetSourceDescriptor descriptor;
    descriptor.material.model = full_engine::AssetSourceMaterialModel::TerrainSplat;
    descriptor.material.alphaMode = full_engine::AssetSourceMaterialAlphaMode::Opaque;
    descriptor.material.textureRefs[0] = asset(100);
    descriptor.material.textureRefs[1] = asset(101);
    descriptor.material.textureRefCount = 2;
    return descriptor;
}

void testValidDescriptors(std::vector<std::string>& failures)
{
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Mesh, meshDescriptor()) ==
            full_engine::AssetSourceDescriptorValidationResult::Success,
        "valid mesh descriptor validates",
        failures);
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Texture, textureDescriptor()) ==
            full_engine::AssetSourceDescriptorValidationResult::Success,
        "valid texture descriptor validates",
        failures);
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Material, materialDescriptor()) ==
            full_engine::AssetSourceDescriptorValidationResult::Success,
        "valid material descriptor validates",
        failures);
}

void testInvalidMeshDescriptors(std::vector<std::string>& failures)
{
    full_engine::AssetSourceDescriptor descriptor = meshDescriptor();
    descriptor.mesh.vertexCount = 0;
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Mesh, descriptor) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidMeshCounts,
        "mesh descriptor rejects zero vertex count",
        failures);

    descriptor = meshDescriptor();
    descriptor.mesh.indexCount = 0;
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Mesh, descriptor) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidMeshCounts,
        "mesh descriptor rejects zero index count",
        failures);

    descriptor = meshDescriptor();
    descriptor.mesh.localBounds.min[0] = 2.0f;
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Mesh, descriptor) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidMeshBounds,
        "mesh descriptor rejects inverted bounds",
        failures);

    descriptor = meshDescriptor();
    descriptor.mesh.localBounds.max[1] = std::nanf("");
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Mesh, descriptor) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidMeshBounds,
        "mesh descriptor rejects non-finite bounds",
        failures);
}

void testInvalidTextureDescriptors(std::vector<std::string>& failures)
{
    full_engine::AssetSourceDescriptor descriptor = textureDescriptor();
    descriptor.texture.width = 0;
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Texture, descriptor) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidTextureDimensions,
        "texture descriptor rejects zero width",
        failures);

    descriptor = textureDescriptor();
    descriptor.texture.height = 0;
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Texture, descriptor) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidTextureDimensions,
        "texture descriptor rejects zero height",
        failures);

    descriptor = textureDescriptor();
    descriptor.texture.mipCount = 0;
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Texture, descriptor) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidTextureMipCount,
        "texture descriptor rejects zero mip count",
        failures);

    descriptor = textureDescriptor();
    descriptor.texture.format = full_engine::AssetSourceTextureFormat::Unknown;
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Texture, descriptor) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidTextureFormat,
        "texture descriptor rejects unknown format",
        failures);

    descriptor = textureDescriptor();
    descriptor.texture.semantic = full_engine::AssetSourceTextureSemantic::Unknown;
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Texture, descriptor) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidTextureSemantic,
        "texture descriptor rejects unknown semantic",
        failures);

    descriptor = textureDescriptor();
    descriptor.texture.colorSpace = full_engine::AssetSourceTextureColorSpace::Unknown;
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Texture, descriptor) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidTextureColorSpace,
        "texture descriptor rejects unknown color space",
        failures);
}

void testInvalidMaterialDescriptors(std::vector<std::string>& failures)
{
    full_engine::AssetSourceDescriptor descriptor = materialDescriptor();
    descriptor.material.model = full_engine::AssetSourceMaterialModel::Unknown;
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Material, descriptor) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidMaterialModel,
        "material descriptor rejects unknown model",
        failures);

    descriptor = materialDescriptor();
    descriptor.material.alphaMode = full_engine::AssetSourceMaterialAlphaMode::Unknown;
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Material, descriptor) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidMaterialAlphaMode,
        "material descriptor rejects unknown alpha mode",
        failures);

    descriptor = materialDescriptor();
    descriptor.material.textureRefCount = full_engine::kMaxAssetSourceMaterialTextureRefs + 1;
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Material, descriptor) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidMaterialTextureCount,
        "material descriptor rejects too many texture refs",
        failures);

    descriptor = materialDescriptor();
    descriptor.material.textureRefs[1] = {};
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Material, descriptor) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidMaterialTextureRef,
        "material descriptor rejects active default texture ref",
        failures);
}

void testInactiveDescriptorSlotsAreIgnored(std::vector<std::string>& failures)
{
    full_engine::AssetSourceDescriptor mesh = meshDescriptor();
    mesh.texture = {};
    mesh.material = {};
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Mesh, mesh) ==
            full_engine::AssetSourceDescriptorValidationResult::Success,
        "mesh validation ignores invalid inactive texture/material slots",
        failures);

    full_engine::AssetSourceDescriptor texture = textureDescriptor();
    texture.mesh = {};
    texture.material = {};
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::Texture, texture) ==
            full_engine::AssetSourceDescriptorValidationResult::Success,
        "texture validation ignores invalid inactive mesh/material slots",
        failures);
}

void testInvalidKind(std::vector<std::string>& failures)
{
    expect(
        full_engine::validateAssetSourceDescriptor(full_engine::AssetKind::TerrainChunk, meshDescriptor()) ==
            full_engine::AssetSourceDescriptorValidationResult::InvalidKind,
        "descriptor validation rejects unsupported kind",
        failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testValidDescriptors(failures);
    testInvalidMeshDescriptors(failures);
    testInvalidTextureDescriptors(failures);
    testInvalidMaterialDescriptors(failures);
    testInactiveDescriptorSlotsAreIgnored(failures);
    testInvalidKind(failures);

    if (!failures.empty())
    {
        for (const std::string& failure : failures)
        {
            std::cerr << failure << '\n';
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
