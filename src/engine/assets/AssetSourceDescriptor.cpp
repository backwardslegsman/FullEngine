#include "engine/assets/AssetSourceDescriptor.hpp"

#include <cmath>

namespace full_engine
{
namespace
{
bool isFiniteBounds(const AssetSourceBounds& bounds) noexcept
{
    for (int axis = 0; axis < 3; ++axis)
    {
        if (!std::isfinite(bounds.min[axis]) || !std::isfinite(bounds.max[axis]))
        {
            return false;
        }
    }
    return true;
}

bool isOrderedBounds(const AssetSourceBounds& bounds) noexcept
{
    for (int axis = 0; axis < 3; ++axis)
    {
        if (bounds.min[axis] > bounds.max[axis])
        {
            return false;
        }
    }
    return true;
}

bool isValidTextureFormat(const AssetSourceTextureFormat format) noexcept
{
    return format == AssetSourceTextureFormat::Rgba8;
}

bool isValidTextureSemantic(const AssetSourceTextureSemantic semantic) noexcept
{
    return semantic == AssetSourceTextureSemantic::Color ||
        semantic == AssetSourceTextureSemantic::LinearData ||
        semantic == AssetSourceTextureSemantic::NormalMap ||
        semantic == AssetSourceTextureSemantic::TerrainSplat ||
        semantic == AssetSourceTextureSemantic::ColorGradingLut ||
        semantic == AssetSourceTextureSemantic::Debug;
}

bool isValidTextureColorSpace(const AssetSourceTextureColorSpace colorSpace) noexcept
{
    return colorSpace == AssetSourceTextureColorSpace::Srgb ||
        colorSpace == AssetSourceTextureColorSpace::Linear ||
        colorSpace == AssetSourceTextureColorSpace::EncodedNormal;
}

bool isValidMaterialModel(const AssetSourceMaterialModel model) noexcept
{
    return model == AssetSourceMaterialModel::Basic ||
        model == AssetSourceMaterialModel::TerrainSplat;
}

bool isValidMaterialAlphaMode(const AssetSourceMaterialAlphaMode alphaMode) noexcept
{
    return alphaMode == AssetSourceMaterialAlphaMode::Opaque ||
        alphaMode == AssetSourceMaterialAlphaMode::AlphaTest ||
        alphaMode == AssetSourceMaterialAlphaMode::AlphaBlend;
}

bool isValidMaterialTextureSlot(const AssetSourceMaterialTextureSlot slot) noexcept
{
    return slot == AssetSourceMaterialTextureSlot::BaseColor ||
        slot == AssetSourceMaterialTextureSlot::Normal ||
        slot == AssetSourceMaterialTextureSlot::MetallicRoughness ||
        slot == AssetSourceMaterialTextureSlot::Occlusion ||
        slot == AssetSourceMaterialTextureSlot::Emissive;
}

bool hasDuplicateMaterialTextureSlot(
    const AssetSourceMaterialDescriptor& descriptor,
    const std::uint32_t candidateIndex) noexcept
{
    const AssetSourceMaterialTextureSlot slot = descriptor.textureRefs[candidateIndex].slot;
    for (std::uint32_t index = 0; index < candidateIndex; ++index)
    {
        if (descriptor.textureRefs[index].slot == slot)
        {
            return true;
        }
    }
    return false;
}

AssetSourceDescriptorValidationResult validateMeshDescriptor(
    const AssetSourceMeshDescriptor& descriptor) noexcept
{
    if (descriptor.vertexCount == 0 || descriptor.indexCount == 0)
    {
        return AssetSourceDescriptorValidationResult::InvalidMeshCounts;
    }

    if (!isFiniteBounds(descriptor.localBounds) || !isOrderedBounds(descriptor.localBounds))
    {
        return AssetSourceDescriptorValidationResult::InvalidMeshBounds;
    }

    return AssetSourceDescriptorValidationResult::Success;
}

AssetSourceDescriptorValidationResult validateTextureDescriptor(
    const AssetSourceTextureDescriptor& descriptor) noexcept
{
    if (descriptor.width == 0 || descriptor.height == 0)
    {
        return AssetSourceDescriptorValidationResult::InvalidTextureDimensions;
    }

    if (descriptor.mipCount == 0)
    {
        return AssetSourceDescriptorValidationResult::InvalidTextureMipCount;
    }

    if (!isValidTextureFormat(descriptor.format))
    {
        return AssetSourceDescriptorValidationResult::InvalidTextureFormat;
    }

    if (!isValidTextureSemantic(descriptor.semantic))
    {
        return AssetSourceDescriptorValidationResult::InvalidTextureSemantic;
    }

    if (!isValidTextureColorSpace(descriptor.colorSpace))
    {
        return AssetSourceDescriptorValidationResult::InvalidTextureColorSpace;
    }

    return AssetSourceDescriptorValidationResult::Success;
}

AssetSourceDescriptorValidationResult validateSkeletonDescriptor(
    const AssetSourceSkeletonDescriptor& descriptor) noexcept
{
    if (descriptor.jointCount == 0)
    {
        return AssetSourceDescriptorValidationResult::InvalidSkeletonJointCount;
    }

    return AssetSourceDescriptorValidationResult::Success;
}

AssetSourceDescriptorValidationResult validateSkinnedMeshDescriptor(
    const AssetSourceSkinnedMeshDescriptor& descriptor) noexcept
{
    if (descriptor.vertexCount == 0 || descriptor.indexCount == 0)
    {
        return AssetSourceDescriptorValidationResult::InvalidSkinnedMeshCounts;
    }

    if (!isValid(descriptor.skeletonAssetId))
    {
        return AssetSourceDescriptorValidationResult::InvalidSkinnedMeshSkeletonRef;
    }

    if (!isFiniteBounds(descriptor.localBounds) || !isOrderedBounds(descriptor.localBounds))
    {
        return AssetSourceDescriptorValidationResult::InvalidSkinnedMeshBounds;
    }

    return AssetSourceDescriptorValidationResult::Success;
}

AssetSourceDescriptorValidationResult validateMaterialDescriptor(
    const AssetSourceMaterialDescriptor& descriptor) noexcept
{
    if (!isValidMaterialModel(descriptor.model))
    {
        return AssetSourceDescriptorValidationResult::InvalidMaterialModel;
    }

    if (!isValidMaterialAlphaMode(descriptor.alphaMode))
    {
        return AssetSourceDescriptorValidationResult::InvalidMaterialAlphaMode;
    }

    if (descriptor.textureRefCount > kMaxAssetSourceMaterialTextureRefs)
    {
        return AssetSourceDescriptorValidationResult::InvalidMaterialTextureCount;
    }

    for (std::uint32_t index = 0; index < descriptor.textureRefCount; ++index)
    {
        if (!isValidMaterialTextureSlot(descriptor.textureRefs[index].slot))
        {
            return AssetSourceDescriptorValidationResult::InvalidMaterialTextureSlot;
        }

        if (!isValid(descriptor.textureRefs[index].id))
        {
            return AssetSourceDescriptorValidationResult::InvalidMaterialTextureRef;
        }

        if (hasDuplicateMaterialTextureSlot(descriptor, index))
        {
            return AssetSourceDescriptorValidationResult::DuplicateMaterialTextureSlot;
        }
    }

    return AssetSourceDescriptorValidationResult::Success;
}
} // namespace

const char* assetSourceDescriptorValidationResultName(
    const AssetSourceDescriptorValidationResult result) noexcept
{
    switch (result)
    {
    case AssetSourceDescriptorValidationResult::Success:
        return "Success";
    case AssetSourceDescriptorValidationResult::InvalidKind:
        return "InvalidKind";
    case AssetSourceDescriptorValidationResult::InvalidMeshCounts:
        return "InvalidMeshCounts";
    case AssetSourceDescriptorValidationResult::InvalidMeshBounds:
        return "InvalidMeshBounds";
    case AssetSourceDescriptorValidationResult::InvalidTextureDimensions:
        return "InvalidTextureDimensions";
    case AssetSourceDescriptorValidationResult::InvalidTextureMipCount:
        return "InvalidTextureMipCount";
    case AssetSourceDescriptorValidationResult::InvalidTextureFormat:
        return "InvalidTextureFormat";
    case AssetSourceDescriptorValidationResult::InvalidTextureSemantic:
        return "InvalidTextureSemantic";
    case AssetSourceDescriptorValidationResult::InvalidTextureColorSpace:
        return "InvalidTextureColorSpace";
    case AssetSourceDescriptorValidationResult::InvalidMaterialModel:
        return "InvalidMaterialModel";
    case AssetSourceDescriptorValidationResult::InvalidMaterialAlphaMode:
        return "InvalidMaterialAlphaMode";
    case AssetSourceDescriptorValidationResult::InvalidMaterialTextureCount:
        return "InvalidMaterialTextureCount";
    case AssetSourceDescriptorValidationResult::InvalidMaterialTextureSlot:
        return "InvalidMaterialTextureSlot";
    case AssetSourceDescriptorValidationResult::InvalidMaterialTextureRef:
        return "InvalidMaterialTextureRef";
    case AssetSourceDescriptorValidationResult::DuplicateMaterialTextureSlot:
        return "DuplicateMaterialTextureSlot";
    case AssetSourceDescriptorValidationResult::InvalidSkeletonJointCount:
        return "InvalidSkeletonJointCount";
    case AssetSourceDescriptorValidationResult::InvalidSkinnedMeshCounts:
        return "InvalidSkinnedMeshCounts";
    case AssetSourceDescriptorValidationResult::InvalidSkinnedMeshSkeletonRef:
        return "InvalidSkinnedMeshSkeletonRef";
    case AssetSourceDescriptorValidationResult::InvalidSkinnedMeshBounds:
        return "InvalidSkinnedMeshBounds";
    }

    return "Unknown";
}

AssetSourceDescriptorValidationResult validateAssetSourceDescriptor(
    const AssetKind kind,
    const AssetSourceDescriptor& descriptor) noexcept
{
    switch (kind)
    {
    case AssetKind::Mesh:
        return validateMeshDescriptor(descriptor.mesh);
    case AssetKind::Material:
        return validateMaterialDescriptor(descriptor.material);
    case AssetKind::Texture:
        return validateTextureDescriptor(descriptor.texture);
    case AssetKind::Skeleton:
        return validateSkeletonDescriptor(descriptor.skeleton);
    case AssetKind::SkinnedMesh:
        return validateSkinnedMeshDescriptor(descriptor.skinnedMesh);
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Shader:
        break;
    }

    return AssetSourceDescriptorValidationResult::InvalidKind;
}
} // namespace full_engine
