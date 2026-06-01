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
        break;
    case LoadedAssetUploadExecuteStatus::AlreadyMapped:
        ++summary.alreadyMappedCount;
        break;
    case LoadedAssetUploadExecuteStatus::SkippedMaterial:
        ++summary.skippedMaterialCount;
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
        record.status = LoadedAssetUploadExecuteStatus::SkippedMaterial;
        return record;
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
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
    case LoadedAssetUploadExecuteStatus::SkippedMaterial:
        return "SkippedMaterial";
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
