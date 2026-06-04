#include "engine/renderer_integration/AssetSourceUploadIntent.hpp"

#include <utility>

namespace full_engine
{
namespace
{
void incrementSummary(
    AssetSourceUploadIntentSummary& summary,
    const AssetSourceUploadIntentStatus status) noexcept
{
    switch (status)
    {
    case AssetSourceUploadIntentStatus::Planned:
        ++summary.plannedCount;
        break;
    case AssetSourceUploadIntentStatus::SourceNotMapped:
        ++summary.sourceNotMappedCount;
        break;
    case AssetSourceUploadIntentStatus::InvalidSource:
        ++summary.invalidSourceCount;
        break;
    case AssetSourceUploadIntentStatus::UnsupportedRendererContract:
        ++summary.unsupportedRendererContractCount;
        break;
    }
}

full_renderer::Aabb toRendererBounds(const AssetSourceBounds& bounds) noexcept
{
    full_renderer::Aabb result;
    for (int axis = 0; axis < 3; ++axis)
    {
        result.min[axis] = bounds.min[axis];
        result.max[axis] = bounds.max[axis];
    }
    return result;
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

std::uint64_t expectedRgba8ByteCount(
    const std::uint32_t width,
    const std::uint32_t height) noexcept
{
    return static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4ULL;
}

AssetSourceUploadIntentRecord buildRecordForSource(const AssetSourceRecord& source)
{
    AssetSourceUploadIntentRecord record;
    record.id = source.id;
    record.kind = source.kind;
    record.uri = source.uri;

    if (validateAssetSourceRecord(source) != AssetSourceRecordValidationResult::Success)
    {
        record.status = AssetSourceUploadIntentStatus::InvalidSource;
        return record;
    }

    switch (source.kind)
    {
    case AssetKind::Mesh:
        record.mesh.vertexCount = source.descriptor.mesh.vertexCount;
        record.mesh.indexCount = source.descriptor.mesh.indexCount;
        record.mesh.localBounds = toRendererBounds(source.descriptor.mesh.localBounds);
        record.status = AssetSourceUploadIntentStatus::Planned;
        break;
    case AssetKind::Texture:
        record.texture.width = source.descriptor.texture.width;
        record.texture.height = source.descriptor.texture.height;
        record.texture.mipCount = source.descriptor.texture.mipCount;
        record.texture.format = toRendererTextureFormat(source.descriptor.texture.format);
        record.texture.semantic = toRendererTextureSemantic(source.descriptor.texture.semantic);
        record.texture.colorSpace = toRendererTextureColorSpace(source.descriptor.texture.colorSpace);
        record.texture.expectedMinimumByteCount =
            expectedRgba8ByteCount(source.descriptor.texture.width, source.descriptor.texture.height);
        record.status = source.descriptor.texture.mipCount == 1 ?
            AssetSourceUploadIntentStatus::Planned :
            AssetSourceUploadIntentStatus::UnsupportedRendererContract;
        break;
    case AssetKind::Material:
        record.material.kind = toRendererMaterialKind(source.descriptor.material.model);
        record.material.alphaMode = toRendererMaterialAlphaMode(source.descriptor.material.alphaMode);
        record.material.textureRefs.reserve(source.descriptor.material.textureRefCount);
        for (std::uint32_t index = 0; index < source.descriptor.material.textureRefCount; ++index)
        {
            record.material.textureRefs.push_back(source.descriptor.material.textureRefs[index]);
        }
        record.status = AssetSourceUploadIntentStatus::Planned;
        break;
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Shader:
        record.status = AssetSourceUploadIntentStatus::InvalidSource;
        break;
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::AnimationClip:
        record.status = AssetSourceUploadIntentStatus::UnsupportedRendererContract;
        break;
    }

    return record;
}

AssetSourceUploadIntentRecord buildUnmappedRecord(
    const TerrainManifestAssetSourceRequestRecord& sourceRecord)
{
    AssetSourceUploadIntentRecord record;
    record.id = sourceRecord.request.id;
    record.kind = sourceRecord.request.kind;
    record.status = AssetSourceUploadIntentStatus::SourceNotMapped;
    return record;
}
} // namespace

const char* assetSourceUploadIntentStatusName(
    const AssetSourceUploadIntentStatus status) noexcept
{
    switch (status)
    {
    case AssetSourceUploadIntentStatus::Planned:
        return "Planned";
    case AssetSourceUploadIntentStatus::SourceNotMapped:
        return "SourceNotMapped";
    case AssetSourceUploadIntentStatus::InvalidSource:
        return "InvalidSource";
    case AssetSourceUploadIntentStatus::UnsupportedRendererContract:
        return "UnsupportedRendererContract";
    }
    return "Unknown";
}

AssetSourceUploadIntentPlan buildAssetSourceUploadIntentPlan(
    const AssetSourceRecord* const sources,
    const std::size_t sourceCount)
{
    AssetSourceUploadIntentPlan plan;
    if (sources == nullptr && sourceCount > 0)
    {
        plan.summary.invalidSourceCount = sourceCount;
        return plan;
    }

    plan.records.reserve(sourceCount);
    for (std::size_t index = 0; index < sourceCount; ++index)
    {
        AssetSourceUploadIntentRecord record = buildRecordForSource(sources[index]);
        incrementSummary(plan.summary, record.status);
        plan.records.push_back(std::move(record));
    }

    return plan;
}

AssetSourceUploadIntentPlan buildAssetSourceUploadIntentPlan(
    const TerrainManifestAssetSourceRequestPlan& sourcePlan)
{
    AssetSourceUploadIntentPlan plan;
    plan.records.reserve(sourcePlan.records.size());

    for (const TerrainManifestAssetSourceRequestRecord& sourceRecord : sourcePlan.records)
    {
        AssetSourceUploadIntentRecord record =
            sourceRecord.status == TerrainManifestAssetSourceRequestStatus::Mapped ?
            buildRecordForSource(sourceRecord.source) :
            buildUnmappedRecord(sourceRecord);
        incrementSummary(plan.summary, record.status);
        plan.records.push_back(std::move(record));
    }

    return plan;
}
} // namespace full_engine
