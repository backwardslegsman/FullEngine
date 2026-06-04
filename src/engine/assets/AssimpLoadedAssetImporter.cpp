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
constexpr std::size_t kMaxIndexedVertexCount =
    static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1U;

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

AssetSourceBounds computeBounds(const std::vector<LoadedMeshVertex>& vertices) noexcept
{
    AssetSourceBounds bounds;
    bounds.min[0] = vertices[0].position[0];
    bounds.min[1] = vertices[0].position[1];
    bounds.min[2] = vertices[0].position[2];
    bounds.max[0] = vertices[0].position[0];
    bounds.max[1] = vertices[0].position[1];
    bounds.max[2] = vertices[0].position[2];

    for (std::size_t index = 1; index < vertices.size(); ++index)
    {
        bounds.min[0] = (std::min)(bounds.min[0], vertices[index].position[0]);
        bounds.min[1] = (std::min)(bounds.min[1], vertices[index].position[1]);
        bounds.min[2] = (std::min)(bounds.min[2], vertices[index].position[2]);
        bounds.max[0] = (std::max)(bounds.max[0], vertices[index].position[0]);
        bounds.max[1] = (std::max)(bounds.max[1], vertices[index].position[1]);
        bounds.max[2] = (std::max)(bounds.max[2], vertices[index].position[2]);
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
        static_cast<std::size_t>(mesh.mNumVertices) > kMaxIndexedVertexCount ||
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
            if (face.mIndices[index] >= mesh.mNumVertices)
            {
                return false;
            }
        }
    }

    return true;
}

bool canAppendMesh(const LoadedMeshAsset& aggregate, const aiMesh& mesh) noexcept
{
    return aggregate.vertices.size() <= kMaxIndexedVertexCount -
        static_cast<std::size_t>(mesh.mNumVertices);
}

void appendMesh(LoadedMeshAsset& aggregate, const aiMesh& mesh)
{
    const std::uint16_t baseVertex = static_cast<std::uint16_t>(aggregate.vertices.size());
    aggregate.vertices.reserve(aggregate.vertices.size() + mesh.mNumVertices);
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
        aggregate.vertices.push_back(vertex);
    }

    aggregate.indices.reserve(aggregate.indices.size() + static_cast<std::size_t>(mesh.mNumFaces) * 3U);
    for (unsigned int faceIndex = 0; faceIndex < mesh.mNumFaces; ++faceIndex)
    {
        const aiFace& face = mesh.mFaces[faceIndex];
        aggregate.indices.push_back(static_cast<std::uint16_t>(baseVertex + face.mIndices[0]));
        aggregate.indices.push_back(static_cast<std::uint16_t>(baseVertex + face.mIndices[1]));
        aggregate.indices.push_back(static_cast<std::uint16_t>(baseVertex + face.mIndices[2]));
    }
}

bool convertSceneMeshes(const aiScene& scene, const AssetId id, LoadedMeshAsset& mesh)
{
    if (scene.mNumMeshes == 0 || scene.mMeshes == nullptr)
    {
        return false;
    }

    mesh = {};
    mesh.id = id;
    for (unsigned int meshIndex = 0; meshIndex < scene.mNumMeshes; ++meshIndex)
    {
        const aiMesh* const sourceMesh = scene.mMeshes[meshIndex];
        if (sourceMesh == nullptr || !canConvertMesh(*sourceMesh) || !canAppendMesh(mesh, *sourceMesh))
        {
            return false;
        }
        appendMesh(mesh, *sourceMesh);
    }

    if (mesh.vertices.empty() || mesh.indices.empty())
    {
        return false;
    }
    mesh.localBounds = computeBounds(mesh.vertices);
    return true;
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
    result.payload.kind = AssetKind::Mesh;
    if (!convertSceneMeshes(*scene, source.id, result.payload.mesh))
    {
        result.status = AssimpLoadedAssetImportStatus::UnsupportedScene;
        return result;
    }

    result.payloadValidation = validateLoadedAssetPayload(result.payload);
    if (result.payloadValidation != LoadedAssetPayloadValidationResult::Success)
    {
        result.status = AssimpLoadedAssetImportStatus::PayloadValidationFailed;
        return result;
    }

    if (!descriptorMatches(result.payload.mesh, source.descriptor.mesh))
    {
        result.status = AssimpLoadedAssetImportStatus::DescriptorMismatch;
        return result;
    }

    result.status = AssimpLoadedAssetImportStatus::Success;
    return result;
}
} // namespace full_engine
