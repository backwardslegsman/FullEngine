#include "engine/renderer_integration/LoadedAssetUploadExecutor.hpp"

namespace full_engine
{
namespace
{
void incrementSummary(
    LoadedAssetUploadExecuteSummary& summary,
    const LoadedAssetUploadExecuteRecord& record) noexcept
{
    switch (record.status)
    {
    case LoadedAssetUploadExecuteStatus::Uploaded:
        if (record.kind == AssetKind::Mesh)
        {
            ++summary.uploadedMeshCount;
        }
        else if (record.kind == AssetKind::Texture)
        {
            ++summary.uploadedTextureCount;
        }
        else if (record.kind == AssetKind::Material)
        {
            ++summary.uploadedMaterialCount;
        }
        else if (record.kind == AssetKind::Skeleton)
        {
            ++summary.uploadedSkeletonCount;
        }
        else if (record.kind == AssetKind::SkinnedMesh)
        {
            ++summary.uploadedSkinnedMeshCount;
        }
        break;
    case LoadedAssetUploadExecuteStatus::AlreadyMapped:
        ++summary.alreadyMappedCount;
        break;
    case LoadedAssetUploadExecuteStatus::MissingTextureHandle:
        ++summary.missingTextureHandleCount;
        break;
    case LoadedAssetUploadExecuteStatus::MissingSkeletonHandle:
        ++summary.missingSkeletonHandleCount;
        break;
    case LoadedAssetUploadExecuteStatus::SkippedUnplanned:
        ++summary.skippedUnplannedCount;
        break;
    case LoadedAssetUploadExecuteStatus::RendererFailed:
        ++summary.rendererFailedCount;
        break;
    case LoadedAssetUploadExecuteStatus::CatalogRejected:
        ++summary.catalogRejectedCount;
        break;
    case LoadedAssetUploadExecuteStatus::UnsupportedKind:
        ++summary.unsupportedKindCount;
        break;
    }
}

LoadedAssetUploadExecuteRecord makeBaseRecord(
    const LoadedAssetUploadRecord& source) noexcept
{
    LoadedAssetUploadExecuteRecord record;
    record.id = source.id;
    record.kind = source.kind;
    record.sourceStatus = source.status;
    return record;
}

LoadedAssetUploadExecuteRecord executeMeshUpload(
    full_renderer::IRenderer& renderer,
    const LoadedAssetUploadRecord& source,
    RendererAssetHandleCatalog& handles)
{
    LoadedAssetUploadExecuteRecord record = makeBaseRecord(source);

    const full_renderer::MeshHandle* const existing = handles.findMeshHandle(source.id);
    if (existing != nullptr)
    {
        record.status = LoadedAssetUploadExecuteStatus::AlreadyMapped;
        record.mesh = *existing;
        record.catalogResult = RendererAssetHandleCatalogResult::AlreadyExists;
        return record;
    }

    const full_renderer::MeshHandle created = renderer.createMesh(source.mesh.desc);
    record.mesh = created;
    if (!full_renderer::isValid(created))
    {
        record.status = LoadedAssetUploadExecuteStatus::RendererFailed;
        return record;
    }

    record.catalogResult = handles.addMeshHandle(source.id, created);
    record.status =
        record.catalogResult == RendererAssetHandleCatalogResult::Success ?
        LoadedAssetUploadExecuteStatus::Uploaded :
        LoadedAssetUploadExecuteStatus::CatalogRejected;
    return record;
}

LoadedAssetUploadExecuteRecord executeTextureUpload(
    full_renderer::IRenderer& renderer,
    const LoadedAssetUploadRecord& source,
    RendererAssetHandleCatalog& handles)
{
    LoadedAssetUploadExecuteRecord record = makeBaseRecord(source);

    const full_renderer::TextureHandle* const existing = handles.findTextureHandle(source.id);
    if (existing != nullptr)
    {
        record.status = LoadedAssetUploadExecuteStatus::AlreadyMapped;
        record.texture = *existing;
        record.catalogResult = RendererAssetHandleCatalogResult::AlreadyExists;
        return record;
    }

    const full_renderer::TextureHandle created = renderer.createTexture(source.texture.desc);
    record.texture = created;
    if (!full_renderer::isValid(created))
    {
        record.status = LoadedAssetUploadExecuteStatus::RendererFailed;
        return record;
    }

    record.catalogResult = handles.addTextureHandle(source.id, created);
    record.status =
        record.catalogResult == RendererAssetHandleCatalogResult::Success ?
        LoadedAssetUploadExecuteStatus::Uploaded :
        LoadedAssetUploadExecuteStatus::CatalogRejected;
    return record;
}

LoadedAssetUploadExecuteRecord executeSkeletonUpload(
    full_renderer::IRenderer& renderer,
    const LoadedAssetUploadRecord& source,
    RendererAssetHandleCatalog& handles)
{
    LoadedAssetUploadExecuteRecord record = makeBaseRecord(source);

    const full_renderer::SkeletonHandle* const existing = handles.findSkeletonHandle(source.id);
    if (existing != nullptr)
    {
        record.status = LoadedAssetUploadExecuteStatus::AlreadyMapped;
        record.skeleton = *existing;
        record.catalogResult = RendererAssetHandleCatalogResult::AlreadyExists;
        return record;
    }

    const full_renderer::SkeletonHandle created = renderer.createSkeleton(source.skeleton.desc);
    record.skeleton = created;
    if (!full_renderer::isValid(created))
    {
        record.status = LoadedAssetUploadExecuteStatus::RendererFailed;
        return record;
    }

    record.catalogResult = handles.addSkeletonHandle(source.id, created);
    record.status =
        record.catalogResult == RendererAssetHandleCatalogResult::Success ?
        LoadedAssetUploadExecuteStatus::Uploaded :
        LoadedAssetUploadExecuteStatus::CatalogRejected;
    return record;
}

LoadedAssetUploadExecuteRecord executeSkinnedMeshUpload(
    full_renderer::IRenderer& renderer,
    const LoadedAssetUploadRecord& source,
    RendererAssetHandleCatalog& handles)
{
    LoadedAssetUploadExecuteRecord record = makeBaseRecord(source);

    const full_renderer::SkinnedMeshHandle* const existing = handles.findSkinnedMeshHandle(source.id);
    if (existing != nullptr)
    {
        record.status = LoadedAssetUploadExecuteStatus::AlreadyMapped;
        record.skinnedMesh = *existing;
        record.catalogResult = RendererAssetHandleCatalogResult::AlreadyExists;
        return record;
    }

    const full_renderer::SkeletonHandle* const skeleton =
        handles.findSkeletonHandle(source.skinnedMesh.skeletonAssetId);
    if (skeleton == nullptr)
    {
        record.status = LoadedAssetUploadExecuteStatus::MissingSkeletonHandle;
        return record;
    }

    full_renderer::SkinnedMeshDesc desc = source.skinnedMesh.desc;
    desc.skeleton = *skeleton;
    const full_renderer::SkinnedMeshHandle created = renderer.createSkinnedMesh(desc);
    record.skinnedMesh = created;
    if (!full_renderer::isValid(created))
    {
        record.status = LoadedAssetUploadExecuteStatus::RendererFailed;
        return record;
    }

    record.catalogResult = handles.addSkinnedMeshHandle(source.id, created);
    record.status =
        record.catalogResult == RendererAssetHandleCatalogResult::Success ?
        LoadedAssetUploadExecuteStatus::Uploaded :
        LoadedAssetUploadExecuteStatus::CatalogRejected;
    return record;
}

bool buildMaterialDesc(
    const LoadedAssetUploadRecord& source,
    const RendererAssetHandleCatalog& handles,
    full_renderer::MaterialDesc& desc) noexcept
{
    desc.kind = source.material.kind;
    desc.alphaMode = source.material.alphaMode;

    if (desc.kind == full_renderer::MaterialKind::TerrainSplat)
    {
        if (source.material.textureRefs.size() > full_renderer::kMaxTerrainMaterialLayers)
        {
            return false;
        }

        for (std::size_t index = 0; index < source.material.textureRefs.size(); ++index)
        {
            const full_renderer::TextureHandle* const handle =
                handles.findTextureHandle(source.material.textureRefs[index].id);
            if (handle == nullptr)
            {
                return false;
            }
            desc.terrain.layers[index].albedoTexture = *handle;
        }
    }
    else
    {
        for (const AssetSourceMaterialTextureRef textureRef : source.material.textureRefs)
        {
            const full_renderer::TextureHandle* const handle =
                handles.findTextureHandle(textureRef.id);
            if (handle == nullptr)
            {
                return false;
            }

            switch (textureRef.slot)
            {
            case AssetSourceMaterialTextureSlot::BaseColor:
                desc.basicTextures.baseColor = *handle;
                break;
            case AssetSourceMaterialTextureSlot::Normal:
                desc.basicTextures.normal = *handle;
                break;
            case AssetSourceMaterialTextureSlot::MetallicRoughness:
                desc.basicTextures.metallicRoughness = *handle;
                break;
            case AssetSourceMaterialTextureSlot::Occlusion:
                desc.basicTextures.occlusion = *handle;
                break;
            case AssetSourceMaterialTextureSlot::Emissive:
                desc.basicTextures.emissive = *handle;
                break;
            case AssetSourceMaterialTextureSlot::Unknown:
                return false;
            }
        }
    }

    return true;
}

LoadedAssetUploadExecuteRecord executeMaterialUpload(
    full_renderer::IRenderer& renderer,
    const LoadedAssetUploadRecord& source,
    RendererAssetHandleCatalog& handles)
{
    LoadedAssetUploadExecuteRecord record = makeBaseRecord(source);

    const full_renderer::MaterialHandle* const existing = handles.findMaterialHandle(source.id);
    if (existing != nullptr)
    {
        record.status = LoadedAssetUploadExecuteStatus::AlreadyMapped;
        record.material = *existing;
        record.catalogResult = RendererAssetHandleCatalogResult::AlreadyExists;
        return record;
    }

    full_renderer::MaterialDesc desc;
    if (!buildMaterialDesc(source, handles, desc))
    {
        record.status = LoadedAssetUploadExecuteStatus::MissingTextureHandle;
        return record;
    }

    const full_renderer::MaterialHandle created = renderer.createMaterial(desc);
    record.material = created;
    if (!full_renderer::isValid(created))
    {
        record.status = LoadedAssetUploadExecuteStatus::RendererFailed;
        return record;
    }

    record.catalogResult = handles.addMaterialHandle(source.id, created);
    record.status =
        record.catalogResult == RendererAssetHandleCatalogResult::Success ?
        LoadedAssetUploadExecuteStatus::Uploaded :
        LoadedAssetUploadExecuteStatus::CatalogRejected;
    return record;
}

LoadedAssetUploadExecuteRecord executeRecord(
    full_renderer::IRenderer& renderer,
    const LoadedAssetUploadRecord& source,
    RendererAssetHandleCatalog& handles)
{
    LoadedAssetUploadExecuteRecord record = makeBaseRecord(source);
    if (source.status != LoadedAssetUploadStatus::Planned)
    {
        record.status = LoadedAssetUploadExecuteStatus::SkippedUnplanned;
        return record;
    }

    switch (source.kind)
    {
    case AssetKind::Mesh:
        return executeMeshUpload(renderer, source, handles);
    case AssetKind::Texture:
        return executeTextureUpload(renderer, source, handles);
    case AssetKind::Material:
        return executeMaterialUpload(renderer, source, handles);
    case AssetKind::Skeleton:
        return executeSkeletonUpload(renderer, source, handles);
    case AssetKind::SkinnedMesh:
        return executeSkinnedMeshUpload(renderer, source, handles);
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::AnimationClip:
    case AssetKind::Shader:
        record.status = LoadedAssetUploadExecuteStatus::UnsupportedKind;
        return record;
    }

    record.status = LoadedAssetUploadExecuteStatus::UnsupportedKind;
    return record;
}
} // namespace

const char* loadedAssetUploadExecuteStatusName(
    const LoadedAssetUploadExecuteStatus status) noexcept
{
    switch (status)
    {
    case LoadedAssetUploadExecuteStatus::Uploaded:
        return "Uploaded";
    case LoadedAssetUploadExecuteStatus::AlreadyMapped:
        return "AlreadyMapped";
    case LoadedAssetUploadExecuteStatus::MissingTextureHandle:
        return "MissingTextureHandle";
    case LoadedAssetUploadExecuteStatus::MissingSkeletonHandle:
        return "MissingSkeletonHandle";
    case LoadedAssetUploadExecuteStatus::SkippedUnplanned:
        return "SkippedUnplanned";
    case LoadedAssetUploadExecuteStatus::RendererFailed:
        return "RendererFailed";
    case LoadedAssetUploadExecuteStatus::CatalogRejected:
        return "CatalogRejected";
    case LoadedAssetUploadExecuteStatus::UnsupportedKind:
        return "UnsupportedKind";
    }

    return "Unknown";
}

LoadedAssetUploadExecuteResult executeLoadedAssetUploadPlan(
    full_renderer::IRenderer& renderer,
    const LoadedAssetUploadPlan& plan,
    RendererAssetHandleCatalog& handles)
{
    LoadedAssetUploadExecuteResult result;
    result.records.reserve(plan.records.size());

    for (const LoadedAssetUploadRecord& source : plan.records)
    {
        LoadedAssetUploadExecuteRecord record = executeRecord(renderer, source, handles);
        incrementSummary(result.summary, record);
        result.records.push_back(record);
    }

    return result;
}
} // namespace full_engine
