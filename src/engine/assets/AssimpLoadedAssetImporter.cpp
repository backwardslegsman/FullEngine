#include "engine/assets/AssimpLoadedAssetImporter.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <vector>

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
    if (options.generateMissingTangents)
    {
        flags |= aiProcess_CalcTangentSpace;
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

AssetSourceBounds computeBounds(const std::vector<LoadedSkinnedMeshVertex>& vertices) noexcept
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

bool descriptorMatches(
    const LoadedSkeletonAsset& skeleton,
    const AssetSourceSkeletonDescriptor& descriptor) noexcept
{
    return skeleton.joints.size() == descriptor.jointCount;
}

bool descriptorMatches(
    const LoadedSkinnedMeshAsset& mesh,
    const AssetSourceSkinnedMeshDescriptor& descriptor) noexcept
{
    if (mesh.vertices.size() != descriptor.vertexCount ||
        mesh.indices.size() != descriptor.indexCount ||
        !(mesh.skeletonAssetId == descriptor.skeletonAssetId) ||
        !sameBounds(mesh.localBounds, descriptor.localBounds) ||
        mesh.sections.size() != descriptor.sectionCount)
    {
        return false;
    }

    for (std::uint32_t index = 0; index < descriptor.sectionCount; ++index)
    {
        if (!(mesh.sections[index].materialAssetId == descriptor.sections[index].materialAssetId) ||
            mesh.sections[index].firstIndex != descriptor.sections[index].firstIndex ||
            mesh.sections[index].indexCount != descriptor.sections[index].indexCount)
        {
            return false;
        }
    }

    return true;
}

bool descriptorMatches(
    const LoadedAnimationClipAsset& clip,
    const AssetSourceAnimationClipDescriptor& descriptor) noexcept
{
    return clip.skeletonAssetId == descriptor.skeletonAssetId &&
        clip.tracks.size() == descriptor.trackCount &&
        std::fabs(clip.durationSeconds - descriptor.durationSeconds) <= descriptor.metadataTolerance &&
        std::fabs(clip.ticksPerSecond - descriptor.ticksPerSecond) <= descriptor.metadataTolerance;
}

void copyMatrixColumnMajor(const aiMatrix4x4& source, float (&target)[16]) noexcept
{
    target[0] = source.a1;
    target[1] = source.b1;
    target[2] = source.c1;
    target[3] = source.d1;
    target[4] = source.a2;
    target[5] = source.b2;
    target[6] = source.c2;
    target[7] = source.d2;
    target[8] = source.a3;
    target[9] = source.b3;
    target[10] = source.c3;
    target[11] = source.d3;
    target[12] = source.a4;
    target[13] = source.b4;
    target[14] = source.c4;
    target[15] = source.d4;
}

bool sameMatrix(const aiMatrix4x4& lhs, const aiMatrix4x4& rhs) noexcept
{
    return lhs.a1 == rhs.a1 && lhs.a2 == rhs.a2 && lhs.a3 == rhs.a3 && lhs.a4 == rhs.a4 &&
        lhs.b1 == rhs.b1 && lhs.b2 == rhs.b2 && lhs.b3 == rhs.b3 && lhs.b4 == rhs.b4 &&
        lhs.c1 == rhs.c1 && lhs.c2 == rhs.c2 && lhs.c3 == rhs.c3 && lhs.c4 == rhs.c4 &&
        lhs.d1 == rhs.d1 && lhs.d2 == rhs.d2 && lhs.d3 == rhs.d3 && lhs.d4 == rhs.d4;
}

bool collectBones(const aiScene& scene, std::map<std::string, aiMatrix4x4>& bones)
{
    if (scene.mNumMeshes == 0 || scene.mMeshes == nullptr)
    {
        return false;
    }

    for (unsigned int meshIndex = 0; meshIndex < scene.mNumMeshes; ++meshIndex)
    {
        const aiMesh* const mesh = scene.mMeshes[meshIndex];
        if (mesh == nullptr)
        {
            return false;
        }
        if (mesh->mNumBones == 0 || mesh->mBones == nullptr)
        {
            continue;
        }

        for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
        {
            const aiBone* const bone = mesh->mBones[boneIndex];
            if (bone == nullptr || bone->mName.length == 0)
            {
                return false;
            }

            const std::string name = bone->mName.C_Str();
            const auto inserted = bones.emplace(name, bone->mOffsetMatrix);
            if (!inserted.second && !sameMatrix(inserted.first->second, bone->mOffsetMatrix))
            {
                return false;
            }
        }
    }

    return !bones.empty() && bones.size() <= kMaxLoadedSkeletonJoints;
}

void collectSkeletonNodes(
    const aiNode& node,
    const std::map<std::string, aiMatrix4x4>& bones,
    std::vector<const aiNode*>& nodes)
{
    if (bones.find(node.mName.C_Str()) != bones.end())
    {
        nodes.push_back(&node);
    }

    for (unsigned int childIndex = 0; childIndex < node.mNumChildren; ++childIndex)
    {
        if (node.mChildren[childIndex] != nullptr)
        {
            collectSkeletonNodes(*node.mChildren[childIndex], bones, nodes);
        }
    }
}

bool buildSkeletonJointIndex(
    const aiScene& scene,
    std::map<std::string, std::uint16_t>& jointIndices,
    LoadedSkeletonAsset* skeleton)
{
    if (scene.mRootNode == nullptr)
    {
        return false;
    }

    std::map<std::string, aiMatrix4x4> bones;
    if (!collectBones(scene, bones))
    {
        return false;
    }

    std::vector<const aiNode*> nodes;
    collectSkeletonNodes(*scene.mRootNode, bones, nodes);
    if (nodes.size() != bones.size() || nodes.empty())
    {
        return false;
    }

    std::uint32_t rootCount = 0;
    for (const aiNode* const node : nodes)
    {
        const std::string name = node->mName.C_Str();
        LoadedSkeletonJoint joint;
        joint.name = name;
        const auto bone = bones.find(name);
        if (bone == bones.end())
        {
            return false;
        }
        copyMatrixColumnMajor(bone->second, joint.inverseBindPose);
        copyMatrixColumnMajor(node->mTransformation, joint.referenceTransform);

        joint.parentIndex = -1;
        if (node->mParent != nullptr)
        {
            const auto parent = jointIndices.find(node->mParent->mName.C_Str());
            if (parent != jointIndices.end())
            {
                joint.parentIndex = parent->second;
            }
        }
        if (joint.parentIndex < 0)
        {
            ++rootCount;
        }

        const std::uint16_t index = static_cast<std::uint16_t>(jointIndices.size());
        jointIndices.emplace(name, index);
        if (skeleton != nullptr)
        {
            skeleton->joints.push_back(joint);
        }
    }

    return rootCount == 1;
}

bool canConvertMesh(const aiMesh& mesh, const AssimpLoadedAssetImportOptions& options) noexcept
{
    if (!mesh.HasPositions() ||
        !mesh.HasNormals() ||
        !mesh.HasTangentsAndBitangents() ||
        (!options.defaultMissingUv0ToZero && !mesh.HasTextureCoords(0)) ||
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

float tangentHandedness(
    const aiVector3D& normal,
    const aiVector3D& tangent,
    const aiVector3D& bitangent) noexcept
{
    const aiVector3D cross{
        normal.y * tangent.z - normal.z * tangent.y,
        normal.z * tangent.x - normal.x * tangent.z,
        normal.x * tangent.y - normal.y * tangent.x};
    const float dot =
        cross.x * bitangent.x +
        cross.y * bitangent.y +
        cross.z * bitangent.z;
    return dot < 0.0f ? -1.0f : 1.0f;
}

bool canConvertSkinnedMesh(const aiMesh& mesh, const AssimpLoadedAssetImportOptions& options) noexcept
{
    return canConvertMesh(mesh, options) && mesh.mNumBones > 0 && mesh.mBones != nullptr;
}

bool canAppendMesh(const LoadedMeshAsset& aggregate, const aiMesh& mesh) noexcept
{
    return aggregate.vertices.size() <= kMaxIndexedVertexCount -
        static_cast<std::size_t>(mesh.mNumVertices);
}

bool canAppendMesh(const LoadedSkinnedMeshAsset& aggregate, const aiMesh& mesh) noexcept
{
    return aggregate.vertices.size() <= kMaxIndexedVertexCount -
        static_cast<std::size_t>(mesh.mNumVertices);
}

void appendMesh(
    LoadedMeshAsset& aggregate,
    const aiMesh& mesh,
    const AssimpLoadedAssetImportOptions& options)
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
        vertex.tangent[0] = mesh.mTangents[index].x;
        vertex.tangent[1] = mesh.mTangents[index].y;
        vertex.tangent[2] = mesh.mTangents[index].z;
        vertex.tangent[3] = tangentHandedness(
            mesh.mNormals[index],
            mesh.mTangents[index],
            mesh.mBitangents[index]);
        if (mesh.HasTextureCoords(0))
        {
            vertex.uv0[0] = mesh.mTextureCoords[0][index].x;
            vertex.uv0[1] = mesh.mTextureCoords[0][index].y;
        }
        else if (options.defaultMissingUv0ToZero)
        {
            vertex.uv0[0] = 0.0f;
            vertex.uv0[1] = 0.0f;
        }
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

bool appendSkinnedMesh(
    LoadedSkinnedMeshAsset& aggregate,
    const aiMesh& mesh,
    const std::map<std::string, std::uint16_t>& jointIndices,
    const AssimpLoadedAssetImportOptions& options,
    const AssetSourceSkinnedMeshSectionDescriptor* const sectionDescriptor)
{
    std::vector<std::vector<std::pair<std::uint16_t, float>>> weights(mesh.mNumVertices);
    for (unsigned int boneIndex = 0; boneIndex < mesh.mNumBones; ++boneIndex)
    {
        const aiBone* const bone = mesh.mBones[boneIndex];
        if (bone == nullptr)
        {
            return false;
        }

        const auto joint = jointIndices.find(bone->mName.C_Str());
        if (joint == jointIndices.end())
        {
            return false;
        }

        for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
        {
            const aiVertexWeight& weight = bone->mWeights[weightIndex];
            if (weight.mVertexId >= mesh.mNumVertices || !std::isfinite(weight.mWeight))
            {
                return false;
            }
            if (weight.mWeight > 0.0f)
            {
                weights[weight.mVertexId].push_back({joint->second, weight.mWeight});
            }
        }
    }

    const std::uint16_t baseVertex = static_cast<std::uint16_t>(aggregate.vertices.size());
    aggregate.vertices.reserve(aggregate.vertices.size() + mesh.mNumVertices);
    for (unsigned int index = 0; index < mesh.mNumVertices; ++index)
    {
        if (weights[index].empty() || weights[index].size() > kMaxLoadedSkinningInfluences)
        {
            return false;
        }

        LoadedSkinnedMeshVertex vertex;
        vertex.position[0] = mesh.mVertices[index].x;
        vertex.position[1] = mesh.mVertices[index].y;
        vertex.position[2] = mesh.mVertices[index].z;
        vertex.normal[0] = mesh.mNormals[index].x;
        vertex.normal[1] = mesh.mNormals[index].y;
        vertex.normal[2] = mesh.mNormals[index].z;
        vertex.tangent[0] = mesh.mTangents[index].x;
        vertex.tangent[1] = mesh.mTangents[index].y;
        vertex.tangent[2] = mesh.mTangents[index].z;
        vertex.tangent[3] = tangentHandedness(
            mesh.mNormals[index],
            mesh.mTangents[index],
            mesh.mBitangents[index]);
        if (mesh.HasTextureCoords(0))
        {
            vertex.uv0[0] = mesh.mTextureCoords[0][index].x;
            vertex.uv0[1] = mesh.mTextureCoords[0][index].y;
        }
        else if (options.defaultMissingUv0ToZero)
        {
            vertex.uv0[0] = 0.0f;
            vertex.uv0[1] = 0.0f;
        }
        if (mesh.HasVertexColors(0))
        {
            vertex.colorLinear[0] = mesh.mColors[0][index].r;
            vertex.colorLinear[1] = mesh.mColors[0][index].g;
            vertex.colorLinear[2] = mesh.mColors[0][index].b;
            vertex.colorLinear[3] = mesh.mColors[0][index].a;
        }
        for (std::size_t weightIndex = 0; weightIndex < weights[index].size(); ++weightIndex)
        {
            vertex.jointIndices[weightIndex] = weights[index][weightIndex].first;
            vertex.jointWeights[weightIndex] = weights[index][weightIndex].second;
        }
        aggregate.vertices.push_back(vertex);
    }

    const std::uint32_t firstIndex = static_cast<std::uint32_t>(aggregate.indices.size());
    const std::uint32_t indexCount = static_cast<std::uint32_t>(mesh.mNumFaces * 3U);
    aggregate.indices.reserve(aggregate.indices.size() + indexCount);
    for (unsigned int faceIndex = 0; faceIndex < mesh.mNumFaces; ++faceIndex)
    {
        const aiFace& face = mesh.mFaces[faceIndex];
        aggregate.indices.push_back(static_cast<std::uint16_t>(baseVertex + face.mIndices[0]));
        aggregate.indices.push_back(static_cast<std::uint16_t>(baseVertex + face.mIndices[1]));
        aggregate.indices.push_back(static_cast<std::uint16_t>(baseVertex + face.mIndices[2]));
    }

    if (sectionDescriptor != nullptr)
    {
        LoadedSkinnedMeshSection section;
        section.materialAssetId = sectionDescriptor->materialAssetId;
        section.firstIndex = firstIndex;
        section.indexCount = indexCount;
        aggregate.sections.push_back(section);
    }

    return true;
}

bool convertSceneMeshes(
    const aiScene& scene,
    const AssetId id,
    const AssimpLoadedAssetImportOptions& options,
    LoadedMeshAsset& mesh)
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
        if (sourceMesh == nullptr ||
            !canConvertMesh(*sourceMesh, options) ||
            !canAppendMesh(mesh, *sourceMesh))
        {
            return false;
        }
        appendMesh(mesh, *sourceMesh, options);
    }

    if (mesh.vertices.empty() || mesh.indices.empty())
    {
        return false;
    }
    mesh.localBounds = computeBounds(mesh.vertices);
    return true;
}

bool convertSceneSkeleton(const aiScene& scene, const AssetId id, LoadedSkeletonAsset& skeleton)
{
    skeleton = {};
    skeleton.id = id;
    std::map<std::string, std::uint16_t> jointIndices;
    return buildSkeletonJointIndex(scene, jointIndices, &skeleton);
}

bool convertSceneSkinnedMeshes(
    const aiScene& scene,
    const AssetId id,
    const AssetSourceSkinnedMeshDescriptor& descriptor,
    const AssimpLoadedAssetImportOptions& options,
    LoadedSkinnedMeshAsset& mesh)
{
    if (scene.mNumMeshes == 0 || scene.mMeshes == nullptr)
    {
        return false;
    }

    std::map<std::string, std::uint16_t> jointIndices;
    if (!buildSkeletonJointIndex(scene, jointIndices, nullptr))
    {
        return false;
    }

    mesh = {};
    mesh.id = id;
    mesh.skeletonAssetId = descriptor.skeletonAssetId;
    bool appendedAnySkinnedMesh = false;
    std::uint32_t importedSectionIndex = 0;
    for (unsigned int meshIndex = 0; meshIndex < scene.mNumMeshes; ++meshIndex)
    {
        const aiMesh* const sourceMesh = scene.mMeshes[meshIndex];
        if (sourceMesh == nullptr)
        {
            return false;
        }

        if (sourceMesh->mNumBones == 0 || sourceMesh->mBones == nullptr)
        {
            continue;
        }

        if (descriptor.sectionCount > 0 && importedSectionIndex >= descriptor.sectionCount)
        {
            return false;
        }
        const AssetSourceSkinnedMeshSectionDescriptor* const sectionDescriptor =
            descriptor.sectionCount > 0 ? &descriptor.sections[importedSectionIndex] : nullptr;

        if (!canConvertSkinnedMesh(*sourceMesh, options) ||
            !canAppendMesh(mesh, *sourceMesh) ||
            !appendSkinnedMesh(mesh, *sourceMesh, jointIndices, options, sectionDescriptor))
        {
            return false;
        }
        appendedAnySkinnedMesh = true;
        ++importedSectionIndex;
    }

    if (!appendedAnySkinnedMesh ||
        mesh.vertices.empty() ||
        mesh.indices.empty() ||
        (descriptor.sectionCount > 0 && importedSectionIndex != descriptor.sectionCount))
    {
        return false;
    }
    mesh.localBounds = computeBounds(mesh.vertices);
    return true;
}

double ticksPerSecondFor(
    const aiAnimation& animation,
    const AssimpLoadedAssetImportOptions& options) noexcept
{
    return animation.mTicksPerSecond > 0.0 ?
        animation.mTicksPerSecond :
        options.defaultTicksPerSecond;
}

float keyTimeSeconds(const double timeTicks, const double ticksPerSecond) noexcept
{
    return static_cast<float>(timeTicks / ticksPerSecond);
}

void addDefaultTranslationKey(LoadedAnimationJointTrack& track) noexcept
{
    LoadedAnimationTranslationKey key;
    key.timeSeconds = 0.0f;
    key.value[0] = 0.0f;
    key.value[1] = 0.0f;
    key.value[2] = 0.0f;
    track.translations.push_back(key);
}

void addDefaultRotationKey(LoadedAnimationJointTrack& track) noexcept
{
    LoadedAnimationRotationKey key;
    key.timeSeconds = 0.0f;
    key.value[0] = 0.0f;
    key.value[1] = 0.0f;
    key.value[2] = 0.0f;
    key.value[3] = 1.0f;
    track.rotations.push_back(key);
}

void addDefaultScaleKey(LoadedAnimationJointTrack& track) noexcept
{
    LoadedAnimationScaleKey key;
    key.timeSeconds = 0.0f;
    key.value[0] = 1.0f;
    key.value[1] = 1.0f;
    key.value[2] = 1.0f;
    track.scales.push_back(key);
}

bool appendTranslationKeys(
    const aiNodeAnim& channel,
    const double ticksPerSecond,
    const AssimpLoadedAssetImportOptions& options,
    LoadedAnimationJointTrack& track)
{
    if (channel.mNumPositionKeys == 0 || channel.mPositionKeys == nullptr)
    {
        if (!options.allowMissingTranslationKeys)
        {
            return false;
        }
        addDefaultTranslationKey(track);
        return true;
    }

    track.translations.reserve(channel.mNumPositionKeys);
    for (unsigned int keyIndex = 0; keyIndex < channel.mNumPositionKeys; ++keyIndex)
    {
        const aiVectorKey& source = channel.mPositionKeys[keyIndex];
        LoadedAnimationTranslationKey key;
        key.timeSeconds = keyTimeSeconds(source.mTime, ticksPerSecond);
        key.value[0] = source.mValue.x;
        key.value[1] = source.mValue.y;
        key.value[2] = source.mValue.z;
        track.translations.push_back(key);
    }
    return true;
}

bool appendRotationKeys(
    const aiNodeAnim& channel,
    const double ticksPerSecond,
    const AssimpLoadedAssetImportOptions& options,
    LoadedAnimationJointTrack& track)
{
    if (channel.mNumRotationKeys == 0 || channel.mRotationKeys == nullptr)
    {
        if (!options.allowMissingRotationKeys)
        {
            return false;
        }
        addDefaultRotationKey(track);
        return true;
    }

    track.rotations.reserve(channel.mNumRotationKeys);
    for (unsigned int keyIndex = 0; keyIndex < channel.mNumRotationKeys; ++keyIndex)
    {
        const aiQuatKey& source = channel.mRotationKeys[keyIndex];
        const float x = source.mValue.x;
        const float y = source.mValue.y;
        const float z = source.mValue.z;
        const float w = source.mValue.w;
        const float lengthSquared = x * x + y * y + z * z + w * w;
        if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0f)
        {
            return false;
        }
        const float invLength = 1.0f / std::sqrt(lengthSquared);

        LoadedAnimationRotationKey key;
        key.timeSeconds = keyTimeSeconds(source.mTime, ticksPerSecond);
        key.value[0] = x * invLength;
        key.value[1] = y * invLength;
        key.value[2] = z * invLength;
        key.value[3] = w * invLength;
        track.rotations.push_back(key);
    }
    return true;
}

bool appendScaleKeys(
    const aiNodeAnim& channel,
    const double ticksPerSecond,
    const AssimpLoadedAssetImportOptions& options,
    LoadedAnimationJointTrack& track)
{
    if (channel.mNumScalingKeys == 0 || channel.mScalingKeys == nullptr)
    {
        if (!options.allowMissingScaleKeys)
        {
            return false;
        }
        addDefaultScaleKey(track);
        return true;
    }

    track.scales.reserve(channel.mNumScalingKeys);
    for (unsigned int keyIndex = 0; keyIndex < channel.mNumScalingKeys; ++keyIndex)
    {
        const aiVectorKey& source = channel.mScalingKeys[keyIndex];
        LoadedAnimationScaleKey key;
        key.timeSeconds = keyTimeSeconds(source.mTime, ticksPerSecond);
        key.value[0] = source.mValue.x;
        key.value[1] = source.mValue.y;
        key.value[2] = source.mValue.z;
        track.scales.push_back(key);
    }
    return true;
}

bool convertSceneAnimationClip(
    const aiScene& scene,
    const AssetId id,
    const AssetId skeletonAssetId,
    const AssimpLoadedAssetImportOptions& options,
    LoadedAnimationClipAsset& clip)
{
    if (scene.mNumAnimations == 0 ||
        scene.mAnimations == nullptr ||
        options.animationIndex >= scene.mNumAnimations ||
        !isValid(skeletonAssetId))
    {
        return false;
    }

    std::map<std::string, std::uint16_t> jointIndices;
    if (!buildSkeletonJointIndex(scene, jointIndices, nullptr))
    {
        return false;
    }

    const aiAnimation* const animation = scene.mAnimations[options.animationIndex];
    if (animation == nullptr || animation->mNumChannels == 0 || animation->mChannels == nullptr)
    {
        return false;
    }

    const double ticksPerSecond = ticksPerSecondFor(*animation, options);
    if (!std::isfinite(ticksPerSecond) || ticksPerSecond <= 0.0 ||
        !std::isfinite(animation->mDuration) || animation->mDuration <= 0.0)
    {
        return false;
    }

    clip = {};
    clip.id = id;
    clip.skeletonAssetId = skeletonAssetId;
    clip.ticksPerSecond = static_cast<float>(ticksPerSecond);
    clip.durationSeconds = static_cast<float>(animation->mDuration / ticksPerSecond);
    clip.tracks.reserve(animation->mNumChannels);

    for (unsigned int channelIndex = 0; channelIndex < animation->mNumChannels; ++channelIndex)
    {
        const aiNodeAnim* const channel = animation->mChannels[channelIndex];
        if (channel == nullptr || channel->mNodeName.length == 0)
        {
            return false;
        }

        const auto joint = jointIndices.find(channel->mNodeName.C_Str());
        if (joint == jointIndices.end())
        {
            continue;
        }

        LoadedAnimationJointTrack track;
        track.jointIndex = joint->second;
        if (!appendTranslationKeys(*channel, ticksPerSecond, options, track) ||
            !appendRotationKeys(*channel, ticksPerSecond, options, track) ||
            !appendScaleKeys(*channel, ticksPerSecond, options, track))
        {
            return false;
        }
        clip.tracks.push_back(track);
    }

    std::sort(
        clip.tracks.begin(),
        clip.tracks.end(),
        [](const LoadedAnimationJointTrack& lhs, const LoadedAnimationJointTrack& rhs)
        {
            return lhs.jointIndex < rhs.jointIndex;
        });

    return !clip.tracks.empty();
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
    if (source.kind != AssetKind::Mesh &&
        source.kind != AssetKind::Skeleton &&
        source.kind != AssetKind::SkinnedMesh &&
        source.kind != AssetKind::AnimationClip)
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
    result.payload.kind = source.kind;
    bool converted = false;
    switch (source.kind)
    {
    case AssetKind::Mesh:
        converted = convertSceneMeshes(*scene, source.id, options, result.payload.mesh);
        break;
    case AssetKind::Skeleton:
        converted = convertSceneSkeleton(*scene, source.id, result.payload.skeleton);
        break;
    case AssetKind::SkinnedMesh:
        converted = convertSceneSkinnedMeshes(
            *scene,
            source.id,
            source.descriptor.skinnedMesh,
            options,
            result.payload.skinnedMesh);
        break;
    case AssetKind::AnimationClip:
        converted = convertSceneAnimationClip(
            *scene,
            source.id,
            source.descriptor.animationClip.skeletonAssetId,
            options,
            result.payload.animationClip);
        break;
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Texture:
    case AssetKind::Material:
    case AssetKind::Shader:
        break;
    }
    if (!converted)
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

    bool descriptorMatch = false;
    switch (source.kind)
    {
    case AssetKind::Mesh:
        descriptorMatch = descriptorMatches(result.payload.mesh, source.descriptor.mesh);
        break;
    case AssetKind::Skeleton:
        descriptorMatch = descriptorMatches(result.payload.skeleton, source.descriptor.skeleton);
        break;
    case AssetKind::SkinnedMesh:
        descriptorMatch = descriptorMatches(result.payload.skinnedMesh, source.descriptor.skinnedMesh);
        break;
    case AssetKind::AnimationClip:
        descriptorMatch = descriptorMatches(result.payload.animationClip, source.descriptor.animationClip);
        break;
    case AssetKind::Unknown:
    case AssetKind::TerrainChunk:
    case AssetKind::Texture:
    case AssetKind::Material:
    case AssetKind::Shader:
        break;
    }
    if (!descriptorMatch)
    {
        result.status = AssimpLoadedAssetImportStatus::DescriptorMismatch;
        return result;
    }

    result.status = AssimpLoadedAssetImportStatus::Success;
    return result;
}
} // namespace full_engine
