#include "engine/assets/GltfMaterialAssetImporter.hpp"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <stb_image.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <vector>

namespace full_engine
{
namespace
{
constexpr std::uint32_t kSingleMip = 1;

AssetId offsetAssetId(const AssetId first, const std::uint32_t offset) noexcept
{
    return {first.value + offset};
}

bool isValidOptions(const GltfMaterialAssetImportOptions& options) noexcept
{
    return isValid(options.firstMaterialId) &&
        isValid(options.firstTextureId) &&
        options.materialModel != AssetSourceMaterialModel::Unknown &&
        options.baseColorTextureSemantic != AssetSourceTextureSemantic::Unknown &&
        options.baseColorTextureColorSpace != AssetSourceTextureColorSpace::Unknown;
}

AssetSourceMaterialAlphaMode alphaModeForMaterial(const aiMaterial& material)
{
    aiString alphaMode;
    if (material.Get("$mat.gltf.alphaMode", 0, 0, alphaMode) != AI_SUCCESS)
    {
        return AssetSourceMaterialAlphaMode::Opaque;
    }

    const std::string value = alphaMode.C_Str();
    if (value == "MASK")
    {
        return AssetSourceMaterialAlphaMode::AlphaTest;
    }
    if (value == "BLEND")
    {
        return AssetSourceMaterialAlphaMode::AlphaBlend;
    }
    return AssetSourceMaterialAlphaMode::Opaque;
}

bool getBaseColorTexturePath(const aiMaterial& material, aiString& texturePath)
{
    if (material.GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) == AI_SUCCESS)
    {
        return true;
    }
    return material.GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS;
}

std::string resolveTextureUri(const std::filesystem::path& gltfPath, const aiString& texturePath)
{
    std::filesystem::path path(texturePath.C_Str());
    if (path.is_relative())
    {
        path = gltfPath.parent_path() / path;
    }
    return path.lexically_normal().string();
}

bool readTextureInfo(const std::string& uri, std::uint32_t& width, std::uint32_t& height)
{
    int decodedWidth = 0;
    int decodedHeight = 0;
    int channels = 0;
    if (stbi_info(uri.c_str(), &decodedWidth, &decodedHeight, &channels) == 0 ||
        decodedWidth <= 0 ||
        decodedHeight <= 0)
    {
        return false;
    }

    width = static_cast<std::uint32_t>(decodedWidth);
    height = static_cast<std::uint32_t>(decodedHeight);
    return true;
}

AssetSourceRecord makeTextureSource(
    const AssetId id,
    const std::string& uri,
    const std::uint32_t width,
    const std::uint32_t height,
    const GltfMaterialAssetImportOptions& options)
{
    AssetSourceRecord source;
    source.id = id;
    source.kind = AssetKind::Texture;
    source.uri = uri;
    source.descriptor.texture.width = width;
    source.descriptor.texture.height = height;
    source.descriptor.texture.mipCount = kSingleMip;
    source.descriptor.texture.format = AssetSourceTextureFormat::Rgba8;
    source.descriptor.texture.semantic = options.baseColorTextureSemantic;
    source.descriptor.texture.colorSpace = options.baseColorTextureColorSpace;
    return source;
}

LoadedAssetPayload makeMaterialPayload(
    const AssetId id,
    const AssetSourceMaterialModel model,
    const AssetSourceMaterialAlphaMode alphaMode,
    const AssetId textureId)
{
    LoadedAssetPayload payload;
    payload.kind = AssetKind::Material;
    payload.material.id = id;
    payload.material.model = model;
    payload.material.alphaMode = alphaMode;
    if (isValid(textureId))
    {
        payload.material.textureRefs[0] = textureId;
        payload.material.textureRefCount = 1;
    }
    return payload;
}

AssetSourceRecord makeMaterialSource(
    const AssetId id,
    const std::string& uri,
    const LoadedMaterialAsset& material)
{
    AssetSourceRecord source;
    source.id = id;
    source.kind = AssetKind::Material;
    source.uri = uri;
    source.descriptor.material.model = material.model;
    source.descriptor.material.alphaMode = material.alphaMode;
    source.descriptor.material.textureRefCount = material.textureRefCount;
    for (std::uint32_t index = 0; index < material.textureRefCount; ++index)
    {
        source.descriptor.material.textureRefs[index] = material.textureRefs[index];
    }
    return source;
}

void incrementSummary(
    GltfMaterialAssetImportSummary& summary,
    const GltfMaterialAssetImportRecordStatus status) noexcept
{
    switch (status)
    {
    case GltfMaterialAssetImportRecordStatus::Planned:
        ++summary.plannedMaterialCount;
        break;
    case GltfMaterialAssetImportRecordStatus::NoBaseColorTexture:
        ++summary.noBaseColorTextureCount;
        break;
    case GltfMaterialAssetImportRecordStatus::TextureInfoFailed:
        ++summary.textureInfoFailedCount;
        break;
    case GltfMaterialAssetImportRecordStatus::SourceValidationFailed:
        ++summary.sourceValidationFailedCount;
        break;
    case GltfMaterialAssetImportRecordStatus::PayloadValidationFailed:
        ++summary.payloadValidationFailedCount;
        break;
    }
}

std::vector<unsigned int> referencedMaterialIndices(const aiScene& scene)
{
    std::set<unsigned int> indices;
    if (scene.mMeshes == nullptr)
    {
        return {};
    }

    for (unsigned int meshIndex = 0; meshIndex < scene.mNumMeshes; ++meshIndex)
    {
        const aiMesh* const mesh = scene.mMeshes[meshIndex];
        if (mesh != nullptr && mesh->mMaterialIndex < scene.mNumMaterials)
        {
            indices.insert(mesh->mMaterialIndex);
        }
    }

    return {indices.begin(), indices.end()};
}
} // namespace

const char* gltfMaterialAssetImportStatusName(
    const GltfMaterialAssetImportStatus status) noexcept
{
    switch (status)
    {
    case GltfMaterialAssetImportStatus::Success:
        return "Success";
    case GltfMaterialAssetImportStatus::InvalidArgument:
        return "InvalidArgument";
    case GltfMaterialAssetImportStatus::IoError:
        return "IoError";
    case GltfMaterialAssetImportStatus::ParseError:
        return "ParseError";
    case GltfMaterialAssetImportStatus::UnsupportedScene:
        return "UnsupportedScene";
    }

    return "Unknown";
}

const char* gltfMaterialAssetImportRecordStatusName(
    const GltfMaterialAssetImportRecordStatus status) noexcept
{
    switch (status)
    {
    case GltfMaterialAssetImportRecordStatus::Planned:
        return "Planned";
    case GltfMaterialAssetImportRecordStatus::NoBaseColorTexture:
        return "NoBaseColorTexture";
    case GltfMaterialAssetImportRecordStatus::TextureInfoFailed:
        return "TextureInfoFailed";
    case GltfMaterialAssetImportRecordStatus::SourceValidationFailed:
        return "SourceValidationFailed";
    case GltfMaterialAssetImportRecordStatus::PayloadValidationFailed:
        return "PayloadValidationFailed";
    }

    return "Unknown";
}

GltfMaterialAssetImportResult importGltfMaterialAssetSources(
    const std::string& uri,
    const GltfMaterialAssetImportOptions& options)
{
    GltfMaterialAssetImportResult result;
    if (uri.empty() || !isValidOptions(options))
    {
        result.status = GltfMaterialAssetImportStatus::InvalidArgument;
        return result;
    }

    std::ifstream input(uri);
    if (!input)
    {
        result.status = GltfMaterialAssetImportStatus::IoError;
        return result;
    }

    Assimp::Importer importer;
    const aiScene* const scene = importer.ReadFile(
        uri,
        aiProcess_ValidateDataStructure);
    if (scene == nullptr)
    {
        result.status = GltfMaterialAssetImportStatus::ParseError;
        return result;
    }
    const std::vector<unsigned int> materialIndices = referencedMaterialIndices(*scene);
    if (scene->mNumMaterials == 0 ||
        scene->mMaterials == nullptr ||
        materialIndices.empty())
    {
        result.status = GltfMaterialAssetImportStatus::UnsupportedScene;
        return result;
    }

    const std::filesystem::path gltfPath(uri);
    std::map<std::string, AssetId> textureIdsByUri;
    std::uint32_t nextTextureOffset = 0;

    for (std::size_t outputIndex = 0; outputIndex < materialIndices.size(); ++outputIndex)
    {
        const unsigned int materialIndex = materialIndices[outputIndex];
        GltfMaterialAssetImportRecord record;
        record.materialIndex = materialIndex;
        record.materialId = offsetAssetId(options.firstMaterialId, static_cast<std::uint32_t>(outputIndex));

        const aiMaterial* const material = scene->mMaterials[materialIndex];
        if (material == nullptr)
        {
            record.status = GltfMaterialAssetImportRecordStatus::PayloadValidationFailed;
            record.payloadValidation = LoadedAssetPayloadValidationResult::InvalidMaterialModel;
            incrementSummary(result.summary, record.status);
            result.records.push_back(record);
            continue;
        }

        AssetId textureId = {};
        aiString texturePath;
        if (getBaseColorTexturePath(*material, texturePath))
        {
            const std::string textureUri = resolveTextureUri(gltfPath, texturePath);
            record.baseColorTextureUri = textureUri;

            const auto existing = textureIdsByUri.find(textureUri);
            if (existing != textureIdsByUri.end())
            {
                textureId = existing->second;
            }
            else
            {
                std::uint32_t width = 0;
                std::uint32_t height = 0;
                if (!readTextureInfo(textureUri, width, height))
                {
                    record.status = GltfMaterialAssetImportRecordStatus::TextureInfoFailed;
                    incrementSummary(result.summary, record.status);
                    result.records.push_back(record);
                    continue;
                }

                textureId = offsetAssetId(options.firstTextureId, nextTextureOffset);
                const AssetSourceRecord textureSource =
                    makeTextureSource(textureId, textureUri, width, height, options);
                record.textureSourceValidation = validateAssetSourceRecord(textureSource);
                if (record.textureSourceValidation != AssetSourceRecordValidationResult::Success)
                {
                    record.status = GltfMaterialAssetImportRecordStatus::SourceValidationFailed;
                    incrementSummary(result.summary, record.status);
                    result.records.push_back(record);
                    continue;
                }

                textureIdsByUri.emplace(textureUri, textureId);
                ++nextTextureOffset;
                result.sourceRecords.push_back(textureSource);
                ++result.summary.emittedTextureSourceCount;
            }
        }
        else
        {
            record.status = GltfMaterialAssetImportRecordStatus::NoBaseColorTexture;
        }

        record.baseColorTextureId = textureId;
        LoadedAssetPayload payload = makeMaterialPayload(
            record.materialId,
            options.materialModel,
            alphaModeForMaterial(*material),
            textureId);
        record.payloadValidation = validateLoadedAssetPayload(payload);
        if (record.payloadValidation != LoadedAssetPayloadValidationResult::Success)
        {
            record.status = GltfMaterialAssetImportRecordStatus::PayloadValidationFailed;
            incrementSummary(result.summary, record.status);
            result.records.push_back(record);
            continue;
        }

        const AssetSourceRecord materialSource =
            makeMaterialSource(record.materialId, uri, payload.material);
        record.materialSourceValidation = validateAssetSourceRecord(materialSource);
        if (record.materialSourceValidation != AssetSourceRecordValidationResult::Success)
        {
            record.status = GltfMaterialAssetImportRecordStatus::SourceValidationFailed;
            incrementSummary(result.summary, record.status);
            result.records.push_back(record);
            continue;
        }

        result.sourceRecords.push_back(materialSource);
        result.materialPayloads.push_back(payload);
        ++result.summary.emittedMaterialSourceCount;
        ++result.summary.emittedMaterialPayloadCount;
        incrementSummary(result.summary, record.status);
        result.records.push_back(record);
    }

    result.status = GltfMaterialAssetImportStatus::Success;
    return result;
}
} // namespace full_engine
