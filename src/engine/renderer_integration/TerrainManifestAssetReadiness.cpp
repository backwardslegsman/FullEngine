#include "engine/renderer_integration/TerrainManifestAssetReadiness.hpp"

#include <map>

namespace full_engine
{
namespace
{
using RequirementKey = std::pair<int, AssetId>;

int sortKey(const TerrainManifestAssetHandleKind kind) noexcept
{
    switch (kind)
    {
    case TerrainManifestAssetHandleKind::Mesh:
        return 0;
    case TerrainManifestAssetHandleKind::Material:
        return 1;
    case TerrainManifestAssetHandleKind::Texture:
        return 2;
    }

    return 3;
}

void addRequirement(
    std::map<RequirementKey, TerrainManifestAssetHandleKind>& requirements,
    const TerrainManifestAssetHandleKind kind,
    const AssetId id)
{
    requirements.emplace(RequirementKey{sortKey(kind), id}, kind);
}

void addDependencyRequirement(
    std::map<RequirementKey, TerrainManifestAssetHandleKind>& requirements,
    const AssetDependencyRef& dependency)
{
    switch (dependency.kind)
    {
    case AssetKind::Mesh:
        addRequirement(requirements, TerrainManifestAssetHandleKind::Mesh, dependency.id);
        break;
    case AssetKind::Material:
        addRequirement(requirements, TerrainManifestAssetHandleKind::Material, dependency.id);
        break;
    case AssetKind::Texture:
        addRequirement(requirements, TerrainManifestAssetHandleKind::Texture, dependency.id);
        break;
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Skeleton:
    case AssetKind::SkinnedMesh:
    case AssetKind::Shader:
        break;
    }
}

bool hasHandle(
    const RendererAssetHandleCatalog& handles,
    const TerrainManifestAssetHandleKind kind,
    const AssetId id) noexcept
{
    switch (kind)
    {
    case TerrainManifestAssetHandleKind::Mesh:
        return handles.findMeshHandle(id) != nullptr;
    case TerrainManifestAssetHandleKind::Material:
        return handles.findMaterialHandle(id) != nullptr;
    case TerrainManifestAssetHandleKind::Texture:
        return handles.findTextureHandle(id) != nullptr;
    }

    return false;
}

void incrementSummary(
    TerrainManifestAssetReadinessSummary& summary,
    const TerrainManifestAssetHandleKind kind,
    const TerrainManifestAssetReadinessStatus status) noexcept
{
    ++summary.requestedCount;
    if (status == TerrainManifestAssetReadinessStatus::Ready)
    {
        ++summary.readyCount;
    }
    else
    {
        ++summary.missingHandleCount;
    }

    switch (kind)
    {
    case TerrainManifestAssetHandleKind::Mesh:
        ++summary.meshRequestedCount;
        if (status == TerrainManifestAssetReadinessStatus::Ready)
        {
            ++summary.meshReadyCount;
        }
        else
        {
            ++summary.meshMissingHandleCount;
        }
        break;
    case TerrainManifestAssetHandleKind::Material:
        ++summary.materialRequestedCount;
        if (status == TerrainManifestAssetReadinessStatus::Ready)
        {
            ++summary.materialReadyCount;
        }
        else
        {
            ++summary.materialMissingHandleCount;
        }
        break;
    case TerrainManifestAssetHandleKind::Texture:
        ++summary.textureRequestedCount;
        if (status == TerrainManifestAssetReadinessStatus::Ready)
        {
            ++summary.textureReadyCount;
        }
        else
        {
            ++summary.textureMissingHandleCount;
        }
        break;
    }
}
} // namespace

TerrainManifestAssetReadinessPlan planTerrainManifestAssetReadiness(
    const CookedAssetManifest& manifest,
    const RendererAssetHandleCatalog& handles)
{
    std::map<RequirementKey, TerrainManifestAssetHandleKind> requirements;

    for (const AssetRecord& record : manifest.assets)
    {
        const std::uint32_t dependencyCount =
            record.dependencyCount <= kMaxAssetDependencies ? record.dependencyCount : kMaxAssetDependencies;
        for (std::uint32_t index = 0; index < dependencyCount; ++index)
        {
            addDependencyRequirement(requirements, record.dependencies[index]);
        }
    }

    for (const TerrainChunkAssetDesc& desc : manifest.terrainChunks)
    {
        const std::uint32_t lodCount =
            desc.lodCount <= kMaxTerrainAssetLodLevels ? desc.lodCount : kMaxTerrainAssetLodLevels;
        for (std::uint32_t index = 0; index < lodCount; ++index)
        {
            addRequirement(requirements, TerrainManifestAssetHandleKind::Mesh, desc.lods[index].mesh);
            addRequirement(requirements, TerrainManifestAssetHandleKind::Material, desc.lods[index].material);
        }

        if (isValid(desc.splatMap))
        {
            addRequirement(requirements, TerrainManifestAssetHandleKind::Texture, desc.splatMap);
        }
    }

    TerrainManifestAssetReadinessPlan plan;
    plan.records.reserve(requirements.size());
    for (const auto& requirement : requirements)
    {
        TerrainManifestAssetReadinessRecord record;
        record.id = requirement.first.second;
        record.kind = requirement.second;
        record.status = hasHandle(handles, record.kind, record.id)
            ? TerrainManifestAssetReadinessStatus::Ready
            : TerrainManifestAssetReadinessStatus::MissingHandle;
        incrementSummary(plan.summary, record.kind, record.status);
        plan.records.push_back(record);
    }

    return plan;
}
} // namespace full_engine
