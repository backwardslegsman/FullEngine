#include "engine/assets/AssimpLoadedAssetImporter.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>

namespace full_engine
{
namespace
{
AssimpLoadedAssetImportResult makeStatus(const AssimpLoadedAssetImportStatus status)
{
    AssimpLoadedAssetImportResult result;
    result.status = status;
    return result;
}

unsigned int postProcessFlags(const AssimpLoadedAssetImportOptions& options) noexcept
{
    unsigned int flags = 0;
    if (options.triangulate)
    {
        flags |= aiProcess_Triangulate;
    }
    if (options.joinIdenticalVertices)
    {
        flags |= aiProcess_JoinIdenticalVertices;
    }
    if (options.validateDataStructure)
    {
        flags |= aiProcess_ValidateDataStructure;
    }
    if (options.generateMissingNormals)
    {
        flags |= aiProcess_GenNormals;
    }
    return flags;
}

bool sameBounds(const AssetSourceBounds& lhs, const AssetSourceBounds& rhs) noexcept
{
    for (int axis = 0; axis < 3; ++axis)
    {
        if (lhs.min[axis] != rhs.min[axis] || lhs.max[axis] != rhs.max[axis])
        {
            return false;
        }
    }
    return true;
}

AssetSourceBounds computeBounds(const aiMesh& mesh) noexcept
{
    AssetSourceBounds bounds;
    bounds.min[0] = mesh.mVertices[0].x;
    bounds.min[1] = mesh.mVertices[0].y;
    bounds.min[2] = mesh.mVertices[0].z;
    bounds.max[0] = mesh.mVertices[0].x;
    bounds.max[1] = mesh.mVertices[0].y;
    bounds.max[2] = mesh.mVertices[0].z;

    for (unsigned int index = 1; index < mesh.mNumVertices; ++index)
    {
        bounds.min[0] = (std::min)(bounds.min[0], mesh.mVertices[index].x);
        bounds.min[1] = (std::min)(bounds.min[1], mesh.mVertices[index].y);
        bounds.min[2] = (std::min)(bounds.min[2], mesh.mVertices[index].z);
        bounds.max[0] = (std::max)(bounds.max[0], mesh.mVertices[index].x);
        bounds.max[1] = (std::max)(bounds.max[1], mesh.mVertices[index].y);
        bounds.max[2] = (std::max)(bounds.max[2], mesh.mVertices[index].z);
    }

    return bounds;
}

bool descriptorMatches(
    const LoadedMeshAsset& mesh,
    const AssetSourceMeshDescriptor& descriptor) noexcept
{
    return mesh.vertices.size() == descriptor.vertexCount &&
        mesh.indices.size() == descriptor.indexCount &&
        sameBounds(mesh.localBounds, descriptor.localBounds);
}

bool canConvertMesh(const aiMesh& mesh) noexcept
{
    if (!mesh.HasPositions() ||
        !mesh.HasNormals() ||
        mesh.mNumVertices == 0 ||
        mesh.mNumVertices > std::numeric_limits<std::uint16_t>::max() ||
        mesh.mNumFaces == 0)
    {
        return false;
    }

    if ((mesh.mPrimitiveTypes & ~aiPrimitiveType_TRIANGLE) != 0)
    {
        return false;
    }

    for (unsigned int faceIndex = 0; faceIndex < mesh.mNumFaces; ++faceIndex)
    {
        const aiFace& face = mesh.mFaces[faceIndex];
        if (face.mNumIndices != 3)
        {
            return false;
        }
        for (unsigned int index = 0; index < face.mNumIndices; ++index)
        {
            if (face.mIndices[index] > std::numeric_limits<std::uint16_t>::max())
            {
                return false;
            }
        }
    }

    return true;
}

LoadedMeshAsset convertMesh(const aiMesh& mesh, const AssetId id)
{
    LoadedMeshAsset result;
    result.id = id;
    result.vertices.reserve(mesh.mNumVertices);
    for (unsigned int index = 0; index < mesh.mNumVertices; ++index)
    {
        LoadedMeshVertex vertex;
        vertex.position[0] = mesh.mVertices[index].x;
        vertex.position[1] = mesh.mVertices[index].y;
        vertex.position[2] = mesh.mVertices[index].z;
        vertex.normal[0] = mesh.mNormals[index].x;
        vertex.normal[1] = mesh.mNormals[index].y;
        vertex.normal[2] = mesh.mNormals[index].z;
        if (mesh.HasVertexColors(0))
        {
            vertex.colorLinear[0] = mesh.mColors[0][index].r;
            vertex.colorLinear[1] = mesh.mColors[0][index].g;
            vertex.colorLinear[2] = mesh.mColors[0][index].b;
            vertex.colorLinear[3] = mesh.mColors[0][index].a;
        }
        result.vertices.push_back(vertex);
    }

    result.indices.reserve(static_cast<std::size_t>(mesh.mNumFaces) * 3U);
    for (unsigned int faceIndex = 0; faceIndex < mesh.mNumFaces; ++faceIndex)
    {
        const aiFace& face = mesh.mFaces[faceIndex];
        result.indices.push_back(static_cast<std::uint16_t>(face.mIndices[0]));
        result.indices.push_back(static_cast<std::uint16_t>(face.mIndices[1]));
        result.indices.push_back(static_cast<std::uint16_t>(face.mIndices[2]));
    }

    result.localBounds = computeBounds(mesh);
    return result;
}
} // namespace

const char* assimpLoadedAssetImportStatusName(
    const AssimpLoadedAssetImportStatus status) noexcept
{
    switch (status)
    {
    case AssimpLoadedAssetImportStatus::Success:
        return "Success";
    case AssimpLoadedAssetImportStatus::InvalidArgument:
        return "InvalidArgument";
    case AssimpLoadedAssetImportStatus::SourceValidationFailed:
        return "SourceValidationFailed";
    case AssimpLoadedAssetImportStatus::IoError:
        return "IoError";
    case AssimpLoadedAssetImportStatus::ParseError:
        return "ParseError";
    case AssimpLoadedAssetImportStatus::UnsupportedScene:
        return "UnsupportedScene";
    case AssimpLoadedAssetImportStatus::DescriptorMismatch:
        return "DescriptorMismatch";
    case AssimpLoadedAssetImportStatus::PayloadValidationFailed:
        return "PayloadValidationFailed";
    case AssimpLoadedAssetImportStatus::UnsupportedKind:
        return "UnsupportedKind";
    }

    return "Unknown";
}

AssimpLoadedAssetImportResult importLoadedAssetPayloadWithAssimp(
    const AssetSourceRecord& source,
    const AssimpLoadedAssetImportOptions& options)
{
    AssimpLoadedAssetImportResult result;
    if (source.kind != AssetKind::Mesh)
    {
        result.status = AssimpLoadedAssetImportStatus::UnsupportedKind;
        return result;
    }

    result.sourceValidation = validateAssetSourceRecord(source);
    if (result.sourceValidation != AssetSourceRecordValidationResult::Success)
    {
        result.status = AssimpLoadedAssetImportStatus::SourceValidationFailed;
        return result;
    }

    std::ifstream input(source.uri);
    if (!input)
    {
        result.status = AssimpLoadedAssetImportStatus::IoError;
        return result;
    }

    Assimp::Importer importer;
    const aiScene* const scene = importer.ReadFile(source.uri, postProcessFlags(options));
    if (scene == nullptr)
    {
        result.status = AssimpLoadedAssetImportStatus::ParseError;
        return result;
    }
    if (scene->mNumMeshes != 1 || scene->mMeshes == nullptr || scene->mMeshes[0] == nullptr)
    {
        result.status = AssimpLoadedAssetImportStatus::UnsupportedScene;
        return result;
    }

    const aiMesh& mesh = *scene->mMeshes[0];
    if (!canConvertMesh(mesh))
    {
        result.status = AssimpLoadedAssetImportStatus::UnsupportedScene;
        return result;
    }

    result.payload.kind = AssetKind::Mesh;
    result.payload.mesh = convertMesh(mesh, source.id);
    if (!descriptorMatches(result.payload.mesh, source.descriptor.mesh))
    {
        result.status = AssimpLoadedAssetImportStatus::DescriptorMismatch;
        return result;
    }

    result.payloadValidation = validateLoadedAssetPayload(result.payload);
    result.status =
        result.payloadValidation == LoadedAssetPayloadValidationResult::Success ?
        AssimpLoadedAssetImportStatus::Success :
        AssimpLoadedAssetImportStatus::PayloadValidationFailed;
    return result;
}
} // namespace full_engine
