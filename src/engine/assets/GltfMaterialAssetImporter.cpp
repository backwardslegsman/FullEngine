#include "engine/assets/GltfMaterialAssetImporter.hpp"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <stb_image.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <vector>

namespace full_engine
{
namespace
{
constexpr std::uint32_t kSingleMip = 1;

struct GltfTextureSlotPolicy
{
    AssetSourceMaterialTextureSlot slot = AssetSourceMaterialTextureSlot::Unknown;
    aiTextureType primaryType = aiTextureType_NONE;
    aiTextureType fallbackType = aiTextureType_NONE;
    AssetSourceTextureSemantic semantic = AssetSourceTextureSemantic::Unknown;
    AssetSourceTextureColorSpace colorSpace = AssetSourceTextureColorSpace::Unknown;
};

constexpr GltfTextureSlotPolicy kTextureSlotPolicies[] = {
    {
        AssetSourceMaterialTextureSlot::BaseColor,
        aiTextureType_BASE_COLOR,
        aiTextureType_DIFFUSE,
        AssetSourceTextureSemantic::Color,
        AssetSourceTextureColorSpace::Srgb,
    },
    {
        AssetSourceMaterialTextureSlot::Normal,
        aiTextureType_NORMALS,
        aiTextureType_NONE,
        AssetSourceTextureSemantic::NormalMap,
        AssetSourceTextureColorSpace::EncodedNormal,
    },
    {
        AssetSourceMaterialTextureSlot::MetallicRoughness,
        aiTextureType_DIFFUSE_ROUGHNESS,
        aiTextureType_METALNESS,
        AssetSourceTextureSemantic::LinearData,
        AssetSourceTextureColorSpace::Linear,
    },
    {
        AssetSourceMaterialTextureSlot::Occlusion,
        aiTextureType_AMBIENT_OCCLUSION,
        aiTextureType_LIGHTMAP,
        AssetSourceTextureSemantic::LinearData,
        AssetSourceTextureColorSpace::Linear,
    },
    {
        AssetSourceMaterialTextureSlot::Emissive,
        aiTextureType_EMISSIVE,
        aiTextureType_NONE,
        AssetSourceTextureSemantic::Color,
        AssetSourceTextureColorSpace::Srgb,
    },
};

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

bool getTexturePath(
    const aiMaterial& material,
    const GltfTextureSlotPolicy& policy,
    aiString& texturePath)
{
    if (policy.primaryType != aiTextureType_NONE &&
        material.GetTexture(policy.primaryType, 0, &texturePath) == AI_SUCCESS)
    {
        return true;
    }
    return policy.fallbackType != aiTextureType_NONE &&
        material.GetTexture(policy.fallbackType, 0, &texturePath) == AI_SUCCESS;
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
    const AssetSourceTextureSemantic semantic,
    const AssetSourceTextureColorSpace colorSpace)
{
    AssetSourceRecord source;
    source.id = id;
    source.kind = AssetKind::Texture;
    source.uri = uri;
    source.descriptor.texture.width = width;
    source.descriptor.texture.height = height;
    source.descriptor.texture.mipCount = kSingleMip;
    source.descriptor.texture.format = AssetSourceTextureFormat::Rgba8;
    source.descriptor.texture.semantic = semantic;
    source.descriptor.texture.colorSpace = colorSpace;
    return source;
}

LoadedAssetPayload makeMaterialPayload(
    const AssetId id,
    const AssetSourceMaterialModel model,
    const AssetSourceMaterialAlphaMode alphaMode,
    const AssetSourceMaterialTextureRef* refs,
    const std::uint32_t refCount)
{
    LoadedAssetPayload payload;
    payload.kind = AssetKind::Material;
    payload.material.id = id;
    payload.material.model = model;
    payload.material.alphaMode = alphaMode;
    for (std::uint32_t index = 0; index < refCount; ++index)
    {
        payload.material.textureRefs[index] = refs[index];
    }
    payload.material.textureRefCount = refCount;
    return payload;
}

std::string textureKey(
    const std::string& uri,
    const AssetSourceTextureSemantic semantic,
    const AssetSourceTextureColorSpace colorSpace)
{
    return uri + "|" +
        std::to_string(static_cast<int>(semantic)) + "|" +
        std::to_string(static_cast<int>(colorSpace));
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

bool materialHasAnyTexture(const aiMaterial& material)
{
    for (const GltfTextureSlotPolicy& policy : kTextureSlotPolicies)
    {
        aiString path;
        if (getTexturePath(material, policy, path))
        {
            return true;
        }
    }
    return false;
}

bool sourceDeclaresMaterials(const std::string& uri)
{
    std::ifstream input(uri);
    if (!input)
    {
        return false;
    }

    std::string contents(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    return contents.find("\"materials\"") != std::string::npos;
}

std::vector<unsigned int> allMaterialIndices(const aiScene& scene)
{
    std::vector<unsigned int> indices;
    if (scene.mMaterials == nullptr)
    {
        return indices;
    }

    indices.reserve(scene.mNumMaterials);
    for (unsigned int materialIndex = 0; materialIndex < scene.mNumMaterials; ++materialIndex)
    {
        const aiMaterial* const material = scene.mMaterials[materialIndex];
        if (material == nullptr)
        {
            continue;
        }

        if (materialHasAnyTexture(*material))
        {
            indices.push_back(materialIndex);
        }
    }
    return indices;
}

void removeImplicitNoTextureMaterials(
    const aiScene& scene,
    std::vector<unsigned int>& materialIndices)
{
    if (materialIndices.size() <= 1 || scene.mMaterials == nullptr)
    {
        return;
    }

    materialIndices.erase(
        std::remove_if(
            materialIndices.begin(),
            materialIndices.end(),
            [&scene](const unsigned int materialIndex)
            {
                return materialIndex < scene.mNumMaterials &&
                    scene.mMaterials[materialIndex] != nullptr &&
                    !materialHasAnyTexture(*scene.mMaterials[materialIndex]);
            }),
        materialIndices.end());
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
    std::vector<unsigned int> materialIndices = referencedMaterialIndices(*scene);
    const bool declaresMaterials = sourceDeclaresMaterials(uri);
    bool hasExplicitTexturedMaterial = false;
    if (declaresMaterials)
    {
        const std::vector<unsigned int> texturedMaterialIndices = allMaterialIndices(*scene);
        hasExplicitTexturedMaterial = !texturedMaterialIndices.empty();
        if (!texturedMaterialIndices.empty())
        {
            materialIndices = texturedMaterialIndices;
        }
    }
    if (declaresMaterials && scene->mNumMaterials > 1U && materialIndices.size() > 1U)
    {
        materialIndices.erase(
            std::remove(materialIndices.begin(), materialIndices.end(), 0U),
            materialIndices.end());
    }
    if (materialIndices.empty() && declaresMaterials)
    {
        materialIndices = allMaterialIndices(*scene);
    }
    removeImplicitNoTextureMaterials(*scene, materialIndices);
    if (scene->mNumMaterials == 0 ||
        scene->mMaterials == nullptr ||
        materialIndices.empty())
    {
        result.status = GltfMaterialAssetImportStatus::UnsupportedScene;
        return result;
    }

    const std::filesystem::path gltfPath(uri);
    std::map<std::string, AssetId> textureIdsByKey;
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

        std::array<AssetSourceMaterialTextureRef, kMaxAssetSourceMaterialTextureRefs> textureRefs = {};
        std::uint32_t textureRefCount = 0;
        for (const GltfTextureSlotPolicy& policy : kTextureSlotPolicies)
        {
            aiString texturePath;
            if (!getTexturePath(*material, policy, texturePath))
            {
                continue;
            }

            const std::string textureUri = resolveTextureUri(gltfPath, texturePath);
            if (policy.slot == AssetSourceMaterialTextureSlot::BaseColor)
            {
                record.baseColorTextureUri = textureUri;
            }

            const AssetSourceTextureSemantic semantic =
                policy.slot == AssetSourceMaterialTextureSlot::BaseColor ?
                options.baseColorTextureSemantic :
                policy.semantic;
            const AssetSourceTextureColorSpace colorSpace =
                policy.slot == AssetSourceMaterialTextureSlot::BaseColor ?
                options.baseColorTextureColorSpace :
                policy.colorSpace;
            const std::string key = textureKey(textureUri, semantic, colorSpace);

            AssetId textureId = {};
            const auto existing = textureIdsByKey.find(key);
            if (existing != textureIdsByKey.end())
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
                    makeTextureSource(textureId, textureUri, width, height, semantic, colorSpace);
                record.textureSourceValidation = validateAssetSourceRecord(textureSource);
                if (record.textureSourceValidation != AssetSourceRecordValidationResult::Success)
                {
                    record.status = GltfMaterialAssetImportRecordStatus::SourceValidationFailed;
                    incrementSummary(result.summary, record.status);
                    result.records.push_back(record);
                    continue;
                }

                textureIdsByKey.emplace(key, textureId);
                ++nextTextureOffset;
                result.sourceRecords.push_back(textureSource);
                ++result.summary.emittedTextureSourceCount;
            }

            textureRefs[textureRefCount] = {policy.slot, textureId};
            ++textureRefCount;
            if (policy.slot == AssetSourceMaterialTextureSlot::BaseColor)
            {
                record.baseColorTextureId = textureId;
            }
        }

        if (textureRefCount == 0)
        {
            record.status = GltfMaterialAssetImportRecordStatus::NoBaseColorTexture;
            if (hasExplicitTexturedMaterial && scene->mNumMaterials > 1U)
            {
                continue;
            }
        }
        record.textureRefs = textureRefs;
        record.textureRefCount = textureRefCount;
        LoadedAssetPayload payload = makeMaterialPayload(
            record.materialId,
            options.materialModel,
            alphaModeForMaterial(*material),
            textureRefs.data(),
            textureRefCount);
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
