#include "engine/renderer_integration/LoadedAssetUploadPlan.hpp"

#include "renderer/resources/AssetContracts.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace full_engine
{
namespace
{
void incrementSummary(
    LoadedAssetUploadSummary& summary,
    const LoadedAssetUploadStatus status) noexcept
{
    switch (status)
    {
    case LoadedAssetUploadStatus::Planned:
        ++summary.plannedCount;
        break;
    case LoadedAssetUploadStatus::InvalidPayload:
        ++summary.invalidPayloadCount;
        break;
    case LoadedAssetUploadStatus::UnsupportedKind:
        ++summary.unsupportedKindCount;
        break;
    case LoadedAssetUploadStatus::UnsupportedRendererContract:
        ++summary.unsupportedRendererContractCount;
        break;
    }
}

AssetId payloadId(const LoadedAssetPayload& payload) noexcept
{
    switch (payload.kind)
    {
    case AssetKind::Mesh:
        return payload.mesh.id;
    case AssetKind::Texture:
        return payload.texture.id;
    case AssetKind::Material:
        return payload.material.id;
    case AssetKind::Skeleton:
        return payload.skeleton.id;
    case AssetKind::SkinnedMesh:
        return payload.skinnedMesh.id;
    case AssetKind::AnimationClip:
        return payload.animationClip.id;
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Shader:
        break;
    }
    return {};
}

full_renderer::TextureFormat toRendererTextureFormat(
    const AssetSourceTextureFormat format) noexcept
{
    switch (format)
    {
    case AssetSourceTextureFormat::Rgba8:
        return full_renderer::TextureFormat::Rgba8;
    case AssetSourceTextureFormat::Unknown:
        break;
    }
    return full_renderer::TextureFormat::Rgba8;
}

full_renderer::TextureSemantic toRendererTextureSemantic(
    const AssetSourceTextureSemantic semantic) noexcept
{
    switch (semantic)
    {
    case AssetSourceTextureSemantic::Color:
        return full_renderer::TextureSemantic::Color;
    case AssetSourceTextureSemantic::LinearData:
        return full_renderer::TextureSemantic::LinearData;
    case AssetSourceTextureSemantic::NormalMap:
        return full_renderer::TextureSemantic::NormalMap;
    case AssetSourceTextureSemantic::TerrainSplat:
        return full_renderer::TextureSemantic::TerrainSplat;
    case AssetSourceTextureSemantic::ColorGradingLut:
        return full_renderer::TextureSemantic::ColorGradingLut;
    case AssetSourceTextureSemantic::Debug:
        return full_renderer::TextureSemantic::Debug;
    case AssetSourceTextureSemantic::Unknown:
        break;
    }
    return full_renderer::TextureSemantic::Color;
}

full_renderer::TextureColorSpace toRendererTextureColorSpace(
    const AssetSourceTextureColorSpace colorSpace) noexcept
{
    switch (colorSpace)
    {
    case AssetSourceTextureColorSpace::Srgb:
        return full_renderer::TextureColorSpace::Srgb;
    case AssetSourceTextureColorSpace::Linear:
        return full_renderer::TextureColorSpace::Linear;
    case AssetSourceTextureColorSpace::EncodedNormal:
        return full_renderer::TextureColorSpace::EncodedNormal;
    case AssetSourceTextureColorSpace::Unknown:
        break;
    }
    return full_renderer::TextureColorSpace::Srgb;
}

full_renderer::MaterialKind toRendererMaterialKind(
    const AssetSourceMaterialModel model) noexcept
{
    switch (model)
    {
    case AssetSourceMaterialModel::Basic:
        return full_renderer::MaterialKind::Basic;
    case AssetSourceMaterialModel::TerrainSplat:
        return full_renderer::MaterialKind::TerrainSplat;
    case AssetSourceMaterialModel::Unknown:
        break;
    }
    return full_renderer::MaterialKind::Basic;
}

full_renderer::MaterialAlphaMode toRendererMaterialAlphaMode(
    const AssetSourceMaterialAlphaMode alphaMode) noexcept
{
    switch (alphaMode)
    {
    case AssetSourceMaterialAlphaMode::Opaque:
        return full_renderer::MaterialAlphaMode::Opaque;
    case AssetSourceMaterialAlphaMode::AlphaTest:
        return full_renderer::MaterialAlphaMode::AlphaTest;
    case AssetSourceMaterialAlphaMode::AlphaBlend:
        return full_renderer::MaterialAlphaMode::AlphaBlend;
    case AssetSourceMaterialAlphaMode::Unknown:
        break;
    }
    return full_renderer::MaterialAlphaMode::Opaque;
}

full_renderer::MeshVertex toRendererVertex(const LoadedMeshVertex& vertex) noexcept
{
    full_renderer::MeshVertex result;
    for (int axis = 0; axis < 3; ++axis)
    {
        result.position[axis] = vertex.position[axis];
        result.normal[axis] = vertex.normal[axis];
    }
    for (int axis = 0; axis < 2; ++axis)
    {
        result.uv0[axis] = vertex.uv0[axis];
    }
    for (int channel = 0; channel < 4; ++channel)
    {
        result.colorLinear[channel] = vertex.colorLinear[channel];
    }
    return result;
}

full_renderer::SkinnedMeshVertex toRendererVertex(const LoadedSkinnedMeshVertex& vertex) noexcept
{
    full_renderer::SkinnedMeshVertex result;
    for (int axis = 0; axis < 3; ++axis)
    {
        result.position[axis] = vertex.position[axis];
        result.normal[axis] = vertex.normal[axis];
    }
    for (int channel = 0; channel < 4; ++channel)
    {
        result.colorLinear[channel] = vertex.colorLinear[channel];
    }
    for (std::uint32_t influence = 0; influence < kMaxLoadedSkinningInfluences; ++influence)
    {
        result.jointIndices[influence] = static_cast<float>(vertex.jointIndices[influence]);
        result.jointWeights[influence] = vertex.jointWeights[influence];
    }
    return result;
}

bool countFitsUint32(const std::size_t count) noexcept
{
    return count <= std::numeric_limits<std::uint32_t>::max();
}

void bindMeshDesc(LoadedMeshUploadWork& work) noexcept
{
    work.desc.vertices = work.vertices.data();
    work.desc.vertexCount = static_cast<std::uint32_t>(work.vertices.size());
    work.desc.indices = work.indices.data();
    work.desc.indexCount = static_cast<std::uint32_t>(work.indices.size());
}

void bindTextureDesc(LoadedTextureUploadWork& work) noexcept
{
    work.desc.data = work.bytes.data();
    work.desc.dataSizeBytes = static_cast<std::uint32_t>(work.bytes.size());
}

void bindSkeletonDesc(LoadedSkeletonUploadWork& work) noexcept
{
    work.desc.joints = work.joints.data();
    work.desc.jointCount = static_cast<std::uint32_t>(work.joints.size());
}

void bindSkinnedMeshDesc(LoadedSkinnedMeshUploadWork& work) noexcept
{
    work.desc.vertices = work.vertices.data();
    work.desc.vertexCount = static_cast<std::uint32_t>(work.vertices.size());
    work.desc.indices = work.indices.data();
    work.desc.indexCount = static_cast<std::uint32_t>(work.indices.size());
}

LoadedMeshUploadWork buildMeshWork(const LoadedMeshAsset& mesh)
{
    LoadedMeshUploadWork work;
    work.id = mesh.id;
    work.vertices.reserve(mesh.vertices.size());
    for (const LoadedMeshVertex& vertex : mesh.vertices)
    {
        work.vertices.push_back(toRendererVertex(vertex));
    }
    work.indices = mesh.indices;
    bindMeshDesc(work);
    return work;
}

LoadedSkeletonUploadWork buildSkeletonWork(const LoadedSkeletonAsset& skeleton)
{
    LoadedSkeletonUploadWork work;
    work.id = skeleton.id;
    work.joints.reserve(skeleton.joints.size());
    for (const LoadedSkeletonJoint& joint : skeleton.joints)
    {
        full_renderer::SkeletonJointDesc desc;
        desc.parentIndex = joint.parentIndex;
        for (int element = 0; element < 16; ++element)
        {
            desc.inverseBindPose[element] = joint.inverseBindPose[element];
        }
        work.joints.push_back(desc);
    }
    bindSkeletonDesc(work);
    return work;
}

LoadedSkinnedMeshUploadWork buildSkinnedMeshWork(const LoadedSkinnedMeshAsset& mesh)
{
    LoadedSkinnedMeshUploadWork work;
    work.id = mesh.id;
    work.skeletonAssetId = mesh.skeletonAssetId;
    work.vertices.reserve(mesh.vertices.size());
    for (const LoadedSkinnedMeshVertex& vertex : mesh.vertices)
    {
        work.vertices.push_back(toRendererVertex(vertex));
    }
    work.indices = mesh.indices;
    bindSkinnedMeshDesc(work);
    return work;
}

LoadedTextureUploadWork buildTextureWork(const LoadedTextureAsset& texture)
{
    LoadedTextureUploadWork work;
    work.id = texture.id;
    work.bytes = texture.bytes;
    work.desc.width = texture.width;
    work.desc.height = texture.height;
    work.desc.format = toRendererTextureFormat(texture.format);
    work.desc.semantic = toRendererTextureSemantic(texture.semantic);
    work.desc.colorSpace = toRendererTextureColorSpace(texture.colorSpace);
    work.desc.mipCount = texture.mipCount;
    work.desc.compressed = false;
    bindTextureDesc(work);
    return work;
}

LoadedMaterialUploadWork buildMaterialWork(const LoadedMaterialAsset& material)
{
    LoadedMaterialUploadWork work;
    work.id = material.id;
    work.kind = toRendererMaterialKind(material.model);
    work.alphaMode = toRendererMaterialAlphaMode(material.alphaMode);
    work.textureRefs.reserve(material.textureRefCount);
    for (std::uint32_t index = 0; index < material.textureRefCount; ++index)
    {
        work.textureRefs.push_back(material.textureRefs[index]);
    }
    return work;
}

LoadedAssetUploadRecord buildRecord(const LoadedAssetPayload& payload)
{
    LoadedAssetUploadRecord record;
    record.id = payloadId(payload);
    record.kind = payload.kind;

    const LoadedAssetPayloadValidationResult validation =
        validateLoadedAssetPayload(payload);
    if (validation != LoadedAssetPayloadValidationResult::Success)
    {
        record.status =
            validation == LoadedAssetPayloadValidationResult::InvalidKind ?
            LoadedAssetUploadStatus::UnsupportedKind :
            LoadedAssetUploadStatus::InvalidPayload;
        return record;
    }

    switch (payload.kind)
    {
    case AssetKind::Mesh:
        if (!countFitsUint32(payload.mesh.vertices.size()) ||
            !countFitsUint32(payload.mesh.indices.size()))
        {
            record.status = LoadedAssetUploadStatus::UnsupportedRendererContract;
            return record;
        }
        record.mesh = buildMeshWork(payload.mesh);
        if (full_renderer::resources::validateMeshAssetContract(record.mesh.desc) !=
            full_renderer::RendererResult::Success)
        {
            record.status = LoadedAssetUploadStatus::UnsupportedRendererContract;
            return record;
        }
        record.status = LoadedAssetUploadStatus::Planned;
        break;
    case AssetKind::Texture:
        if (!countFitsUint32(payload.texture.bytes.size()))
        {
            record.status = LoadedAssetUploadStatus::UnsupportedRendererContract;
            return record;
        }
        record.texture = buildTextureWork(payload.texture);
        if (full_renderer::resources::validateTextureAssetContract(record.texture.desc) !=
            full_renderer::RendererResult::Success)
        {
            record.status = LoadedAssetUploadStatus::UnsupportedRendererContract;
            return record;
        }
        record.status = LoadedAssetUploadStatus::Planned;
        break;
    case AssetKind::Material:
        record.material = buildMaterialWork(payload.material);
        record.status = LoadedAssetUploadStatus::Planned;
        break;
    case AssetKind::Skeleton:
        record.skeleton = buildSkeletonWork(payload.skeleton);
        record.status = LoadedAssetUploadStatus::Planned;
        break;
    case AssetKind::SkinnedMesh:
        if (!countFitsUint32(payload.skinnedMesh.vertices.size()) ||
            !countFitsUint32(payload.skinnedMesh.indices.size()))
        {
            record.status = LoadedAssetUploadStatus::UnsupportedRendererContract;
            return record;
        }
        record.skinnedMesh = buildSkinnedMeshWork(payload.skinnedMesh);
        record.status = LoadedAssetUploadStatus::Planned;
        break;
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::AnimationClip:
    case AssetKind::Shader:
        record.status = LoadedAssetUploadStatus::UnsupportedKind;
        break;
    }

    return record;
}

void rebindDescriptorViews(LoadedAssetUploadPlan& plan) noexcept
{
    for (LoadedAssetUploadRecord& record : plan.records)
    {
        if (record.status != LoadedAssetUploadStatus::Planned)
        {
            continue;
        }

        switch (record.kind)
        {
        case AssetKind::Mesh:
            bindMeshDesc(record.mesh);
            break;
        case AssetKind::Texture:
            bindTextureDesc(record.texture);
            break;
        case AssetKind::Skeleton:
            bindSkeletonDesc(record.skeleton);
            break;
        case AssetKind::SkinnedMesh:
            bindSkinnedMeshDesc(record.skinnedMesh);
            break;
        case AssetKind::Material:
        case AssetKind::AnimationClip:
        case AssetKind::Unknown:
        case AssetKind::TerrainChunk:
        case AssetKind::Shader:
            break;
        }
    }
}
} // namespace

const char* loadedAssetUploadStatusName(
    const LoadedAssetUploadStatus status) noexcept
{
    switch (status)
    {
    case LoadedAssetUploadStatus::Planned:
        return "Planned";
    case LoadedAssetUploadStatus::InvalidPayload:
        return "InvalidPayload";
    case LoadedAssetUploadStatus::UnsupportedKind:
        return "UnsupportedKind";
    case LoadedAssetUploadStatus::UnsupportedRendererContract:
        return "UnsupportedRendererContract";
    }
    return "Unknown";
}

LoadedAssetUploadPlan buildLoadedAssetUploadPlan(
    const LoadedAssetPayload* const payloads,
    const std::size_t payloadCount)
{
    LoadedAssetUploadPlan plan;
    if (payloads == nullptr && payloadCount > 0)
    {
        plan.summary.invalidPayloadCount = payloadCount;
        return plan;
    }

    plan.records.reserve(payloadCount);
    for (std::size_t index = 0; index < payloadCount; ++index)
    {
        LoadedAssetUploadRecord record = buildRecord(payloads[index]);
        incrementSummary(plan.summary, record.status);
        plan.records.push_back(std::move(record));
    }

    rebindDescriptorViews(plan);
    return plan;
}
} // namespace full_engine
