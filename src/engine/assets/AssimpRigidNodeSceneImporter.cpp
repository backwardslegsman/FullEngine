#include "engine/assets/AssimpRigidNodeSceneImporter.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <set>
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

void setIdentity(float target[16]) noexcept
{
    for (int index = 0; index < 16; ++index)
    {
        target[index] = 0.0f;
    }
    target[0] = 1.0f;
    target[5] = 1.0f;
    target[10] = 1.0f;
    target[15] = 1.0f;
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

void collectAnimationChannelNames(
    const aiScene& scene,
    const AssimpLoadedAssetImportOptions& options,
    std::set<std::string>& names)
{
    if (scene.mAnimations == nullptr || options.animationIndex >= scene.mNumAnimations)
    {
        return;
    }

    const aiAnimation* const animation = scene.mAnimations[options.animationIndex];
    if (animation == nullptr || animation->mChannels == nullptr)
    {
        return;
    }

    for (unsigned int channelIndex = 0; channelIndex < animation->mNumChannels; ++channelIndex)
    {
        const aiNodeAnim* const channel = animation->mChannels[channelIndex];
        if (channel != nullptr && channel->mNodeName.length > 0)
        {
            names.emplace(channel->mNodeName.C_Str());
        }
    }
}

void collectAnimatedNodeAncestors(
    const aiNode* node,
    std::set<std::string>& selectedNames)
{
    while (node != nullptr && node->mName.length > 0)
    {
        selectedNames.emplace(node->mName.C_Str());
        node = node->mParent;
    }
}

void collectAnimatedSceneNodeNames(
    const aiNode& node,
    const std::set<std::string>& animationChannelNames,
    std::set<std::string>& selectedNames)
{
    if (animationChannelNames.find(node.mName.C_Str()) != animationChannelNames.end())
    {
        collectAnimatedNodeAncestors(&node, selectedNames);
    }

    for (unsigned int childIndex = 0; childIndex < node.mNumChildren; ++childIndex)
    {
        if (node.mChildren[childIndex] != nullptr)
        {
            collectAnimatedSceneNodeNames(*node.mChildren[childIndex], animationChannelNames, selectedNames);
        }
    }
}

void collectSelectedSceneNodes(
    const aiNode& node,
    const std::set<std::string>& selectedNames,
    std::set<std::string>& visitedNames,
    std::vector<const aiNode*>& nodes)
{
    const std::string name = node.mName.C_Str();
    if (selectedNames.find(name) != selectedNames.end() &&
        visitedNames.emplace(name).second)
    {
        nodes.push_back(&node);
    }

    for (unsigned int childIndex = 0; childIndex < node.mNumChildren; ++childIndex)
    {
        if (node.mChildren[childIndex] != nullptr)
        {
            collectSelectedSceneNodes(*node.mChildren[childIndex], selectedNames, visitedNames, nodes);
        }
    }
}

bool buildAnimatedSceneNodeJointIndex(
    const aiScene& scene,
    const AssimpLoadedAssetImportOptions& options,
    std::map<std::string, std::uint16_t>& jointIndices,
    LoadedSkeletonAsset& skeleton)
{
    if (scene.mRootNode == nullptr || scene.mAnimations == nullptr || options.animationIndex >= scene.mNumAnimations)
    {
        return false;
    }

    std::set<std::string> animationChannelNames;
    collectAnimationChannelNames(scene, options, animationChannelNames);
    if (animationChannelNames.empty())
    {
        return false;
    }

    std::set<std::string> selectedNames;
    collectAnimatedSceneNodeNames(*scene.mRootNode, animationChannelNames, selectedNames);
    if (selectedNames.empty() || selectedNames.size() > kMaxLoadedSkeletonJoints)
    {
        return false;
    }

    std::vector<const aiNode*> nodes;
    std::set<std::string> visitedNames;
    collectSelectedSceneNodes(*scene.mRootNode, selectedNames, visitedNames, nodes);
    if (nodes.empty() || nodes.size() != selectedNames.size())
    {
        return false;
    }

    std::uint32_t rootCount = 0;
    for (const aiNode* const node : nodes)
    {
        const std::string name = node->mName.C_Str();
        LoadedSkeletonJoint joint;
        joint.name = name;
        setIdentity(joint.inverseBindPose);
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
        skeleton.joints.push_back(joint);
    }

    return rootCount == 1;
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

bool convertAnimationClip(
    const aiScene& scene,
    const AssetId id,
    const AssetId skeletonAssetId,
    const std::map<std::string, std::uint16_t>& jointIndices,
    const AssimpLoadedAssetImportOptions& options,
    LoadedAnimationClipAsset& clip)
{
    if (scene.mAnimations == nullptr ||
        options.animationIndex >= scene.mNumAnimations ||
        !isValid(skeletonAssetId))
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

AssimpRigidNodeMeshStatus validateMeshShape(
    const aiMesh& mesh,
    const AssimpLoadedAssetImportOptions& options) noexcept
{
    if (!mesh.HasPositions() || mesh.mNumVertices == 0)
    {
        return AssimpRigidNodeMeshStatus::MissingPositions;
    }
    if (!mesh.HasNormals())
    {
        return AssimpRigidNodeMeshStatus::MissingNormals;
    }
    if (!mesh.HasTangentsAndBitangents())
    {
        return AssimpRigidNodeMeshStatus::MissingTangents;
    }
    if (!options.defaultMissingUv0ToZero && !mesh.HasTextureCoords(0))
    {
        return AssimpRigidNodeMeshStatus::MissingUv0;
    }
    if (static_cast<std::size_t>(mesh.mNumVertices) > kMaxIndexedVertexCount)
    {
        return AssimpRigidNodeMeshStatus::TooManyVertices;
    }
    if (mesh.mNumFaces == 0)
    {
        return AssimpRigidNodeMeshStatus::NonTriangleFaces;
    }
    for (unsigned int faceIndex = 0; faceIndex < mesh.mNumFaces; ++faceIndex)
    {
        const aiFace& face = mesh.mFaces[faceIndex];
        if (face.mNumIndices != 3)
        {
            return AssimpRigidNodeMeshStatus::NonTriangleFaces;
        }
        for (unsigned int index = 0; index < face.mNumIndices; ++index)
        {
            if (face.mIndices[index] >= mesh.mNumVertices)
            {
                return AssimpRigidNodeMeshStatus::NonTriangleFaces;
            }
        }
    }
    return AssimpRigidNodeMeshStatus::Imported;
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

LoadedMeshAsset convertMesh(
    const aiMesh& mesh,
    const AssetId id,
    const AssimpLoadedAssetImportOptions& options)
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
    result.localBounds = computeBounds(result.vertices);
    return result;
}

std::map<std::string, std::uint16_t>::const_iterator nearestJointAncestor(
    const aiNode& node,
    const std::map<std::string, std::uint16_t>& jointIndices) noexcept
{
    const aiNode* current = &node;
    while (current != nullptr)
    {
        const auto found = jointIndices.find(current->mName.C_Str());
        if (found != jointIndices.end())
        {
            return found;
        }
        current = current->mParent;
    }
    return jointIndices.end();
}

void incrementSummary(
    AssimpRigidNodeSceneImportSummary& summary,
    const AssimpRigidNodeMeshStatus status) noexcept
{
    if (status == AssimpRigidNodeMeshStatus::Imported)
    {
        ++summary.importedAttachmentCount;
        return;
    }

    ++summary.skippedMeshCount;
    switch (status)
    {
    case AssimpRigidNodeMeshStatus::NoAnimatedNode:
        ++summary.missingAnimatedNodeCount;
        break;
    case AssimpRigidNodeMeshStatus::MissingPositions:
    case AssimpRigidNodeMeshStatus::MissingNormals:
    case AssimpRigidNodeMeshStatus::MissingTangents:
    case AssimpRigidNodeMeshStatus::MissingUv0:
        ++summary.missingAttributeCount;
        break;
    case AssimpRigidNodeMeshStatus::NonTriangleFaces:
        ++summary.nonTriangleCount;
        break;
    case AssimpRigidNodeMeshStatus::TooManyVertices:
        ++summary.tooManyVerticesCount;
        break;
    case AssimpRigidNodeMeshStatus::InvalidPayload:
        ++summary.invalidPayloadCount;
        break;
    case AssimpRigidNodeMeshStatus::Imported:
    case AssimpRigidNodeMeshStatus::InvalidMeshIndex:
    case AssimpRigidNodeMeshStatus::NullMesh:
        break;
    }
}

bool importNodeMeshes(
    const aiScene& scene,
    const aiNode& node,
    const std::map<std::string, std::uint16_t>& jointIndices,
    const AssimpRigidNodeSceneImportOptions& options,
    AssimpRigidNodeSceneImportResult& result)
{
    bool payloadFailed = false;
    const auto joint = nearestJointAncestor(node, jointIndices);
    for (unsigned int meshRefIndex = 0; meshRefIndex < node.mNumMeshes; ++meshRefIndex)
    {
        AssimpRigidNodeMeshRecord record;
        record.sourceNodeName = node.mName.C_Str();
        record.status = AssimpRigidNodeMeshStatus::InvalidMeshIndex;

        const unsigned int meshIndex = node.mMeshes[meshRefIndex];
        record.sourceMeshIndex = meshIndex;
        if (meshIndex >= scene.mNumMeshes || scene.mMeshes == nullptr)
        {
            incrementSummary(result.summary, record.status);
            result.meshRecords.push_back(record);
            continue;
        }

        const aiMesh* const mesh = scene.mMeshes[meshIndex];
        if (mesh == nullptr)
        {
            record.status = AssimpRigidNodeMeshStatus::NullMesh;
            incrementSummary(result.summary, record.status);
            result.meshRecords.push_back(record);
            continue;
        }

        if (joint == jointIndices.end())
        {
            record.status = AssimpRigidNodeMeshStatus::NoAnimatedNode;
            incrementSummary(result.summary, record.status);
            result.meshRecords.push_back(record);
            continue;
        }

        record.status = validateMeshShape(*mesh, options.assimp);
        if (record.status != AssimpRigidNodeMeshStatus::Imported)
        {
            incrementSummary(result.summary, record.status);
            result.meshRecords.push_back(record);
            continue;
        }

        const AssetId meshAssetId{
            options.firstMeshAssetId.value + static_cast<std::uint64_t>(result.attachments.size())};
        LoadedMeshAsset loaded = convertMesh(*mesh, meshAssetId, options.assimp);
        const LoadedAssetPayloadValidationResult validation = validateLoadedMeshAsset(loaded);
        if (validation != LoadedAssetPayloadValidationResult::Success)
        {
            result.payloadValidation = validation;
            record.status = AssimpRigidNodeMeshStatus::InvalidPayload;
            incrementSummary(result.summary, record.status);
            result.meshRecords.push_back(record);
            payloadFailed = true;
            continue;
        }

        AssimpRigidNodeMeshAttachment attachment;
        attachment.meshAssetId = meshAssetId;
        attachment.jointIndex = joint->second;
        attachment.sourceMeshIndex = meshIndex;
        attachment.sourceMaterialIndex = mesh->mMaterialIndex >= 0 ?
            static_cast<std::int32_t>(mesh->mMaterialIndex) :
            -1;
        attachment.sourceNodeName = node.mName.C_Str();
        attachment.sourceMeshName = mesh->mName.C_Str();
        attachment.mesh = loaded;
        result.attachments.push_back(attachment);

        record.meshAssetId = meshAssetId;
        record.jointIndex = joint->second;
        record.status = AssimpRigidNodeMeshStatus::Imported;
        incrementSummary(result.summary, record.status);
        result.meshRecords.push_back(record);
    }

    for (unsigned int childIndex = 0; childIndex < node.mNumChildren; ++childIndex)
    {
        if (node.mChildren[childIndex] != nullptr)
        {
            payloadFailed = importNodeMeshes(
                scene,
                *node.mChildren[childIndex],
                jointIndices,
                options,
                result) || payloadFailed;
        }
    }

    return payloadFailed;
}

LoadedAssetPayloadValidationResult validateSkeletonPayload(const LoadedSkeletonAsset& skeleton) noexcept
{
    LoadedAssetPayload payload;
    payload.kind = AssetKind::Skeleton;
    payload.skeleton = skeleton;
    return validateLoadedAssetPayload(payload);
}

LoadedAssetPayloadValidationResult validateClipPayload(const LoadedAnimationClipAsset& clip) noexcept
{
    LoadedAssetPayload payload;
    payload.kind = AssetKind::AnimationClip;
    payload.animationClip = clip;
    return validateLoadedAssetPayload(payload);
}

std::string jsonString(const std::string& value)
{
    std::ostringstream output;
    output << '"';
    for (const char character : value)
    {
        switch (character)
        {
        case '\\':
            output << "\\\\";
            break;
        case '"':
            output << "\\\"";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            output << character;
            break;
        }
    }
    output << '"';
    return output.str();
}
} // namespace

const char* assimpRigidNodeSceneImportStatusName(
    const AssimpRigidNodeSceneImportStatus status) noexcept
{
    switch (status)
    {
    case AssimpRigidNodeSceneImportStatus::Success:
        return "Success";
    case AssimpRigidNodeSceneImportStatus::InvalidArgument:
        return "InvalidArgument";
    case AssimpRigidNodeSceneImportStatus::IoError:
        return "IoError";
    case AssimpRigidNodeSceneImportStatus::ParseError:
        return "ParseError";
    case AssimpRigidNodeSceneImportStatus::UnsupportedScene:
        return "UnsupportedScene";
    case AssimpRigidNodeSceneImportStatus::PayloadValidationFailed:
        return "PayloadValidationFailed";
    }
    return "Unknown";
}

const char* assimpRigidNodeMeshStatusName(
    const AssimpRigidNodeMeshStatus status) noexcept
{
    switch (status)
    {
    case AssimpRigidNodeMeshStatus::Imported:
        return "Imported";
    case AssimpRigidNodeMeshStatus::InvalidMeshIndex:
        return "InvalidMeshIndex";
    case AssimpRigidNodeMeshStatus::NullMesh:
        return "NullMesh";
    case AssimpRigidNodeMeshStatus::NoAnimatedNode:
        return "NoAnimatedNode";
    case AssimpRigidNodeMeshStatus::MissingPositions:
        return "MissingPositions";
    case AssimpRigidNodeMeshStatus::MissingNormals:
        return "MissingNormals";
    case AssimpRigidNodeMeshStatus::MissingTangents:
        return "MissingTangents";
    case AssimpRigidNodeMeshStatus::MissingUv0:
        return "MissingUv0";
    case AssimpRigidNodeMeshStatus::NonTriangleFaces:
        return "NonTriangleFaces";
    case AssimpRigidNodeMeshStatus::TooManyVertices:
        return "TooManyVertices";
    case AssimpRigidNodeMeshStatus::InvalidPayload:
        return "InvalidPayload";
    }
    return "Unknown";
}

std::string formatAssimpRigidNodeSceneImportResultJson(
    const AssimpRigidNodeSceneImportResult& result)
{
    std::ostringstream output;
    output << "{\n";
    output << "  \"status\": \"" << assimpRigidNodeSceneImportStatusName(result.status) << "\",\n";
    output << "  \"payloadValidation\": \"" << loadedAssetPayloadValidationResultName(result.payloadValidation) << "\",\n";
    output << "  \"skeletonJointCount\": " << result.skeleton.joints.size() << ",\n";
    output << "  \"animationTrackCount\": " << result.animationClip.tracks.size() << ",\n";
    output << "  \"attachmentCount\": " << result.attachments.size() << ",\n";
    output << "  \"summary\": {\n";
    output << "    \"importedAttachments\": " << result.summary.importedAttachmentCount << ",\n";
    output << "    \"skippedMeshes\": " << result.summary.skippedMeshCount << ",\n";
    output << "    \"missingAnimatedNodes\": " << result.summary.missingAnimatedNodeCount << ",\n";
    output << "    \"missingAttributes\": " << result.summary.missingAttributeCount << ",\n";
    output << "    \"nonTriangles\": " << result.summary.nonTriangleCount << ",\n";
    output << "    \"tooManyVertices\": " << result.summary.tooManyVerticesCount << ",\n";
    output << "    \"invalidPayloads\": " << result.summary.invalidPayloadCount << "\n";
    output << "  },\n";
    output << "  \"meshRecords\": [\n";
    for (std::size_t index = 0; index < result.meshRecords.size(); ++index)
    {
        const AssimpRigidNodeMeshRecord& record = result.meshRecords[index];
        output << "    { \"meshIndex\": " << record.sourceMeshIndex
               << ", \"status\": " << jsonString(assimpRigidNodeMeshStatusName(record.status))
               << ", \"assetId\": " << record.meshAssetId.value
               << ", \"jointIndex\": " << record.jointIndex
               << ", \"node\": " << jsonString(record.sourceNodeName) << " }";
        if (index + 1U < result.meshRecords.size())
        {
            output << ",";
        }
        output << "\n";
    }
    output << "  ]\n";
    output << "}\n";
    return output.str();
}

AssimpRigidNodeSceneImportResult importRigidNodeSceneWithAssimp(
    const AssimpRigidNodeSceneImportOptions& options)
{
    AssimpRigidNodeSceneImportResult result;
    if (options.path.empty() ||
        !isValid(options.skeletonAssetId) ||
        !isValid(options.animationClipAssetId) ||
        !isValid(options.firstMeshAssetId))
    {
        result.status = AssimpRigidNodeSceneImportStatus::InvalidArgument;
        return result;
    }

    std::ifstream input(options.path);
    if (!input)
    {
        result.status = AssimpRigidNodeSceneImportStatus::IoError;
        return result;
    }

    AssimpLoadedAssetImportOptions assimp = options.assimp;
    assimp.skeletonSourceMode = AssimpSkeletonSourceMode::AnimatedSceneNodes;

    Assimp::Importer importer;
    const aiScene* const scene = importer.ReadFile(options.path, postProcessFlags(assimp));
    if (scene == nullptr)
    {
        result.status = AssimpRigidNodeSceneImportStatus::ParseError;
        return result;
    }

    result.skeleton.id = options.skeletonAssetId;
    std::map<std::string, std::uint16_t> jointIndices;
    if (!buildAnimatedSceneNodeJointIndex(*scene, assimp, jointIndices, result.skeleton) ||
        !convertAnimationClip(*scene, options.animationClipAssetId, options.skeletonAssetId, jointIndices, assimp, result.animationClip) ||
        scene->mRootNode == nullptr)
    {
        result.status = AssimpRigidNodeSceneImportStatus::UnsupportedScene;
        return result;
    }

    result.payloadValidation = validateSkeletonPayload(result.skeleton);
    if (result.payloadValidation != LoadedAssetPayloadValidationResult::Success)
    {
        result.status = AssimpRigidNodeSceneImportStatus::PayloadValidationFailed;
        return result;
    }
    result.payloadValidation = validateClipPayload(result.animationClip);
    if (result.payloadValidation != LoadedAssetPayloadValidationResult::Success)
    {
        result.status = AssimpRigidNodeSceneImportStatus::PayloadValidationFailed;
        return result;
    }

    const bool payloadFailed = importNodeMeshes(*scene, *scene->mRootNode, jointIndices, options, result);
    if (payloadFailed)
    {
        result.status = AssimpRigidNodeSceneImportStatus::PayloadValidationFailed;
        return result;
    }
    if (result.attachments.empty())
    {
        result.status = AssimpRigidNodeSceneImportStatus::UnsupportedScene;
        return result;
    }

    result.status = AssimpRigidNodeSceneImportStatus::Success;
    return result;
}
} // namespace full_engine
