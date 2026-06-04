#include "engine/assets/LoadedAssetPayload.hpp"

#include <cmath>
#include <limits>

namespace full_engine
{
namespace
{
bool isFinite3(const float values[3]) noexcept
{
    return std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]);
}

bool isFinite2(const float values[2]) noexcept
{
    return std::isfinite(values[0]) && std::isfinite(values[1]);
}

bool isFinite4(const float values[4]) noexcept
{
    return std::isfinite(values[0]) &&
        std::isfinite(values[1]) &&
        std::isfinite(values[2]) &&
        std::isfinite(values[3]);
}

bool isFinite16(const float values[16]) noexcept
{
    for (int index = 0; index < 16; ++index)
    {
        if (!std::isfinite(values[index]))
        {
            return false;
        }
    }
    return true;
}

bool isFiniteBounds(const AssetSourceBounds& bounds) noexcept
{
    return isFinite3(bounds.min) && isFinite3(bounds.max);
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

bool isNonZeroNormal(const float normal[3]) noexcept
{
    constexpr float kEpsilon = 1.0e-8f;
    const float lengthSquared =
        normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2];
    return lengthSquared > kEpsilon;
}

bool isColorInRange(const float color[4]) noexcept
{
    for (int channel = 0; channel < 4; ++channel)
    {
        if (color[channel] < 0.0f || color[channel] > 1.0f)
        {
            return false;
        }
    }
    return true;
}

bool isValidWeightSum(const float weights[kMaxLoadedSkinningInfluences]) noexcept
{
    constexpr float kWeightSumTolerance = 0.001f;
    float sum = 0.0f;
    for (std::uint32_t index = 0; index < kMaxLoadedSkinningInfluences; ++index)
    {
        if (!std::isfinite(weights[index]) || weights[index] < 0.0f)
        {
            return false;
        }
        sum += weights[index];
    }
    return std::isfinite(sum) && std::fabs(sum - 1.0f) <= kWeightSumTolerance;
}

bool hasValidJointIndices(
    const std::uint16_t indices[kMaxLoadedSkinningInfluences]) noexcept
{
    for (std::uint32_t index = 0; index < kMaxLoadedSkinningInfluences; ++index)
    {
        if (indices[index] >= kMaxLoadedSkeletonJoints)
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
    const LoadedMaterialAsset& material,
    const std::uint32_t candidateIndex) noexcept
{
    const AssetSourceMaterialTextureSlot slot = material.textureRefs[candidateIndex].slot;
    for (std::uint32_t index = 0; index < candidateIndex; ++index)
    {
        if (material.textureRefs[index].slot == slot)
        {
            return true;
        }
    }
    return false;
}

bool textureByteCountOverflows(
    const std::uint32_t width,
    const std::uint32_t height) noexcept
{
    constexpr std::uint64_t kBytesPerRgba8Pixel = 4ULL;
    const std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
    if (width != 0 && height > max / width)
    {
        return true;
    }
    const std::uint64_t texelCount = static_cast<std::uint64_t>(width) * height;
    return texelCount > max / kBytesPerRgba8Pixel;
}

std::uint64_t expectedTextureByteCount(
    const std::uint32_t width,
    const std::uint32_t height) noexcept
{
    return static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4ULL;
}
} // namespace

const char* loadedAssetPayloadValidationResultName(
    const LoadedAssetPayloadValidationResult result) noexcept
{
    switch (result)
    {
    case LoadedAssetPayloadValidationResult::Success:
        return "Success";
    case LoadedAssetPayloadValidationResult::InvalidKind:
        return "InvalidKind";
    case LoadedAssetPayloadValidationResult::InvalidAssetId:
        return "InvalidAssetId";
    case LoadedAssetPayloadValidationResult::InvalidMeshVertices:
        return "InvalidMeshVertices";
    case LoadedAssetPayloadValidationResult::InvalidMeshIndices:
        return "InvalidMeshIndices";
    case LoadedAssetPayloadValidationResult::InvalidMeshVertexData:
        return "InvalidMeshVertexData";
    case LoadedAssetPayloadValidationResult::InvalidMeshBounds:
        return "InvalidMeshBounds";
    case LoadedAssetPayloadValidationResult::InvalidTextureDimensions:
        return "InvalidTextureDimensions";
    case LoadedAssetPayloadValidationResult::InvalidTextureMipCount:
        return "InvalidTextureMipCount";
    case LoadedAssetPayloadValidationResult::InvalidTextureFormat:
        return "InvalidTextureFormat";
    case LoadedAssetPayloadValidationResult::InvalidTextureSemantic:
        return "InvalidTextureSemantic";
    case LoadedAssetPayloadValidationResult::InvalidTextureColorSpace:
        return "InvalidTextureColorSpace";
    case LoadedAssetPayloadValidationResult::InvalidTextureByteCount:
        return "InvalidTextureByteCount";
    case LoadedAssetPayloadValidationResult::InvalidMaterialModel:
        return "InvalidMaterialModel";
    case LoadedAssetPayloadValidationResult::InvalidMaterialAlphaMode:
        return "InvalidMaterialAlphaMode";
    case LoadedAssetPayloadValidationResult::InvalidMaterialTextureCount:
        return "InvalidMaterialTextureCount";
    case LoadedAssetPayloadValidationResult::InvalidMaterialTextureSlot:
        return "InvalidMaterialTextureSlot";
    case LoadedAssetPayloadValidationResult::InvalidMaterialTextureRef:
        return "InvalidMaterialTextureRef";
    case LoadedAssetPayloadValidationResult::DuplicateMaterialTextureSlot:
        return "DuplicateMaterialTextureSlot";
    case LoadedAssetPayloadValidationResult::InvalidSkeletonJointCount:
        return "InvalidSkeletonJointCount";
    case LoadedAssetPayloadValidationResult::InvalidSkeletonHierarchy:
        return "InvalidSkeletonHierarchy";
    case LoadedAssetPayloadValidationResult::InvalidSkeletonJointData:
        return "InvalidSkeletonJointData";
    case LoadedAssetPayloadValidationResult::InvalidSkinnedMeshSkeletonRef:
        return "InvalidSkinnedMeshSkeletonRef";
    case LoadedAssetPayloadValidationResult::InvalidSkinnedMeshVertices:
        return "InvalidSkinnedMeshVertices";
    case LoadedAssetPayloadValidationResult::InvalidSkinnedMeshIndices:
        return "InvalidSkinnedMeshIndices";
    case LoadedAssetPayloadValidationResult::InvalidSkinnedMeshVertexData:
        return "InvalidSkinnedMeshVertexData";
    case LoadedAssetPayloadValidationResult::InvalidSkinnedMeshWeights:
        return "InvalidSkinnedMeshWeights";
    case LoadedAssetPayloadValidationResult::InvalidSkinnedMeshBounds:
        return "InvalidSkinnedMeshBounds";
    }

    return "Unknown";
}

LoadedAssetPayloadValidationResult validateLoadedMeshAsset(
    const LoadedMeshAsset& mesh) noexcept
{
    if (!isValid(mesh.id))
    {
        return LoadedAssetPayloadValidationResult::InvalidAssetId;
    }

    if (mesh.vertices.empty())
    {
        return LoadedAssetPayloadValidationResult::InvalidMeshVertices;
    }

    if (mesh.indices.empty() || mesh.indices.size() % 3 != 0)
    {
        return LoadedAssetPayloadValidationResult::InvalidMeshIndices;
    }

    if (!isFiniteBounds(mesh.localBounds) || !isOrderedBounds(mesh.localBounds))
    {
        return LoadedAssetPayloadValidationResult::InvalidMeshBounds;
    }

    for (const LoadedMeshVertex& vertex : mesh.vertices)
    {
        if (!isFinite3(vertex.position) ||
            !isFinite3(vertex.normal) ||
            !isNonZeroNormal(vertex.normal) ||
            !isFinite2(vertex.uv0) ||
            !isFinite4(vertex.colorLinear) ||
            !isColorInRange(vertex.colorLinear))
        {
            return LoadedAssetPayloadValidationResult::InvalidMeshVertexData;
        }
    }

    const std::size_t vertexCount = mesh.vertices.size();
    for (const std::uint16_t index : mesh.indices)
    {
        if (static_cast<std::size_t>(index) >= vertexCount)
        {
            return LoadedAssetPayloadValidationResult::InvalidMeshIndices;
        }
    }

    return LoadedAssetPayloadValidationResult::Success;
}

LoadedAssetPayloadValidationResult validateLoadedTextureAsset(
    const LoadedTextureAsset& texture) noexcept
{
    if (!isValid(texture.id))
    {
        return LoadedAssetPayloadValidationResult::InvalidAssetId;
    }

    if (texture.width == 0 || texture.height == 0)
    {
        return LoadedAssetPayloadValidationResult::InvalidTextureDimensions;
    }

    if (texture.mipCount != 1)
    {
        return LoadedAssetPayloadValidationResult::InvalidTextureMipCount;
    }

    if (!isValidTextureFormat(texture.format))
    {
        return LoadedAssetPayloadValidationResult::InvalidTextureFormat;
    }

    if (!isValidTextureSemantic(texture.semantic))
    {
        return LoadedAssetPayloadValidationResult::InvalidTextureSemantic;
    }

    if (!isValidTextureColorSpace(texture.colorSpace))
    {
        return LoadedAssetPayloadValidationResult::InvalidTextureColorSpace;
    }

    if (textureByteCountOverflows(texture.width, texture.height) ||
        texture.bytes.size() < expectedTextureByteCount(texture.width, texture.height))
    {
        return LoadedAssetPayloadValidationResult::InvalidTextureByteCount;
    }

    return LoadedAssetPayloadValidationResult::Success;
}

LoadedAssetPayloadValidationResult validateLoadedMaterialAsset(
    const LoadedMaterialAsset& material) noexcept
{
    if (!isValid(material.id))
    {
        return LoadedAssetPayloadValidationResult::InvalidAssetId;
    }

    if (!isValidMaterialModel(material.model))
    {
        return LoadedAssetPayloadValidationResult::InvalidMaterialModel;
    }

    if (!isValidMaterialAlphaMode(material.alphaMode))
    {
        return LoadedAssetPayloadValidationResult::InvalidMaterialAlphaMode;
    }

    if (material.textureRefCount > kMaxAssetSourceMaterialTextureRefs)
    {
        return LoadedAssetPayloadValidationResult::InvalidMaterialTextureCount;
    }

    for (std::uint32_t index = 0; index < material.textureRefCount; ++index)
    {
        if (!isValidMaterialTextureSlot(material.textureRefs[index].slot))
        {
            return LoadedAssetPayloadValidationResult::InvalidMaterialTextureSlot;
        }

        if (!isValid(material.textureRefs[index].id))
        {
            return LoadedAssetPayloadValidationResult::InvalidMaterialTextureRef;
        }

        if (hasDuplicateMaterialTextureSlot(material, index))
        {
            return LoadedAssetPayloadValidationResult::DuplicateMaterialTextureSlot;
        }
    }

    return LoadedAssetPayloadValidationResult::Success;
}

LoadedAssetPayloadValidationResult validateLoadedSkeletonAsset(
    const LoadedSkeletonAsset& skeleton) noexcept
{
    if (!isValid(skeleton.id))
    {
        return LoadedAssetPayloadValidationResult::InvalidAssetId;
    }

    if (skeleton.joints.empty() || skeleton.joints.size() > kMaxLoadedSkeletonJoints)
    {
        return LoadedAssetPayloadValidationResult::InvalidSkeletonJointCount;
    }

    std::uint32_t rootCount = 0;
    for (std::size_t index = 0; index < skeleton.joints.size(); ++index)
    {
        const LoadedSkeletonJoint& joint = skeleton.joints[index];
        if (!isFinite16(joint.inverseBindPose) || !isFinite16(joint.referenceTransform))
        {
            return LoadedAssetPayloadValidationResult::InvalidSkeletonJointData;
        }

        if (joint.parentIndex == -1)
        {
            ++rootCount;
            continue;
        }

        if (joint.parentIndex < 0 ||
            static_cast<std::size_t>(joint.parentIndex) >= index)
        {
            return LoadedAssetPayloadValidationResult::InvalidSkeletonHierarchy;
        }
    }

    return rootCount == 1U ?
        LoadedAssetPayloadValidationResult::Success :
        LoadedAssetPayloadValidationResult::InvalidSkeletonHierarchy;
}

LoadedAssetPayloadValidationResult validateLoadedSkinnedMeshAsset(
    const LoadedSkinnedMeshAsset& mesh) noexcept
{
    if (!isValid(mesh.id))
    {
        return LoadedAssetPayloadValidationResult::InvalidAssetId;
    }

    if (!isValid(mesh.skeletonAssetId))
    {
        return LoadedAssetPayloadValidationResult::InvalidSkinnedMeshSkeletonRef;
    }

    if (mesh.vertices.empty())
    {
        return LoadedAssetPayloadValidationResult::InvalidSkinnedMeshVertices;
    }

    if (mesh.indices.empty() || mesh.indices.size() % 3 != 0)
    {
        return LoadedAssetPayloadValidationResult::InvalidSkinnedMeshIndices;
    }

    if (!isFiniteBounds(mesh.localBounds) || !isOrderedBounds(mesh.localBounds))
    {
        return LoadedAssetPayloadValidationResult::InvalidSkinnedMeshBounds;
    }

    for (const LoadedSkinnedMeshVertex& vertex : mesh.vertices)
    {
        if (!isFinite3(vertex.position) ||
            !isFinite3(vertex.normal) ||
            !isNonZeroNormal(vertex.normal) ||
            !isFinite2(vertex.uv0) ||
            !isFinite4(vertex.colorLinear) ||
            !isColorInRange(vertex.colorLinear) ||
            !hasValidJointIndices(vertex.jointIndices))
        {
            return LoadedAssetPayloadValidationResult::InvalidSkinnedMeshVertexData;
        }

        if (!isValidWeightSum(vertex.jointWeights))
        {
            return LoadedAssetPayloadValidationResult::InvalidSkinnedMeshWeights;
        }
    }

    const std::size_t vertexCount = mesh.vertices.size();
    for (const std::uint16_t index : mesh.indices)
    {
        if (static_cast<std::size_t>(index) >= vertexCount)
        {
            return LoadedAssetPayloadValidationResult::InvalidSkinnedMeshIndices;
        }
    }

    return LoadedAssetPayloadValidationResult::Success;
}

LoadedAssetPayloadValidationResult validateLoadedAssetPayload(
    const LoadedAssetPayload& payload) noexcept
{
    switch (payload.kind)
    {
    case AssetKind::Mesh:
        return validateLoadedMeshAsset(payload.mesh);
    case AssetKind::Texture:
        return validateLoadedTextureAsset(payload.texture);
    case AssetKind::Material:
        return validateLoadedMaterialAsset(payload.material);
    case AssetKind::Skeleton:
        return validateLoadedSkeletonAsset(payload.skeleton);
    case AssetKind::SkinnedMesh:
        return validateLoadedSkinnedMeshAsset(payload.skinnedMesh);
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Shader:
        break;
    }

    return LoadedAssetPayloadValidationResult::InvalidKind;
}
} // namespace full_engine
