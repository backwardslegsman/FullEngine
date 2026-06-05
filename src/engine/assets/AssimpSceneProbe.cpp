#include "engine/assets/AssimpSceneProbe.hpp"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <set>
#include <vector>

namespace full_engine
{
namespace
{
constexpr std::uint64_t kMax16BitIndexedVertexCount =
    static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max()) + 1ULL;

unsigned int probePostProcessFlags(const AssimpLoadedAssetImportOptions& options) noexcept
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

void copyMatrixColumnMajor(const aiMatrix4x4& source, std::array<float, 16>& target) noexcept
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

bool isIdentity(const std::array<float, 16>& matrix) noexcept
{
    constexpr float kIdentity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
    for (std::size_t index = 0; index < matrix.size(); ++index)
    {
        if (matrix[index] != kIdentity[index])
        {
            return false;
        }
    }
    return true;
}

bool isIdentity(const aiMatrix4x4& matrix) noexcept
{
    std::array<float, 16> copied = {};
    copyMatrixColumnMajor(matrix, copied);
    return isIdentity(copied);
}

bool isFiniteMatrix(const aiMatrix4x4& matrix) noexcept
{
    const float values[16] = {
        matrix.a1, matrix.b1, matrix.c1, matrix.d1,
        matrix.a2, matrix.b2, matrix.c2, matrix.d2,
        matrix.a3, matrix.b3, matrix.c3, matrix.d3,
        matrix.a4, matrix.b4, matrix.c4, matrix.d4};
    for (const float value : values)
    {
        if (!std::isfinite(value))
        {
            return false;
        }
    }
    return true;
}

void includePosition(AssetSourceBounds& bounds, const aiVector3D& position, const bool first) noexcept
{
    if (first)
    {
        bounds.min[0] = position.x;
        bounds.min[1] = position.y;
        bounds.min[2] = position.z;
        bounds.max[0] = position.x;
        bounds.max[1] = position.y;
        bounds.max[2] = position.z;
        return;
    }

    bounds.min[0] = (std::min)(bounds.min[0], position.x);
    bounds.min[1] = (std::min)(bounds.min[1], position.y);
    bounds.min[2] = (std::min)(bounds.min[2], position.z);
    bounds.max[0] = (std::max)(bounds.max[0], position.x);
    bounds.max[1] = (std::max)(bounds.max[1], position.y);
    bounds.max[2] = (std::max)(bounds.max[2], position.z);
}

bool hasAnyWeights(const aiMesh& mesh) noexcept
{
    if (mesh.mBones == nullptr)
    {
        return false;
    }
    for (unsigned int boneIndex = 0; boneIndex < mesh.mNumBones; ++boneIndex)
    {
        const aiBone* const bone = mesh.mBones[boneIndex];
        if (bone != nullptr && bone->mNumWeights > 0 && bone->mWeights != nullptr)
        {
            return true;
        }
    }
    return false;
}

std::uint32_t materialTextureReferenceCount(const aiMaterial& material) noexcept
{
    std::uint32_t count = 0;
    for (int type = aiTextureType_NONE; type <= aiTextureType_TRANSMISSION; ++type)
    {
        count += material.GetTextureCount(static_cast<aiTextureType>(type));
    }
    return count;
}

double effectiveTicksPerSecond(
    const aiAnimation& animation,
    const AssimpLoadedAssetImportOptions& options) noexcept
{
    return animation.mTicksPerSecond > 0.0 ? animation.mTicksPerSecond : options.defaultTicksPerSecond;
}

bool hasSuspiciousBounds(const AssimpSceneProbeResult& result) noexcept
{
    if (!result.hasBounds)
    {
        return false;
    }

    for (int axis = 0; axis < 3; ++axis)
    {
        if (!std::isfinite(result.aggregateBounds.min[axis]) ||
            !std::isfinite(result.aggregateBounds.max[axis]) ||
            result.aggregateBounds.min[axis] > result.aggregateBounds.max[axis])
        {
            return true;
        }
    }

    const float extents[3] = {
        result.aggregateBounds.max[0] - result.aggregateBounds.min[0],
        result.aggregateBounds.max[1] - result.aggregateBounds.min[1],
        result.aggregateBounds.max[2] - result.aggregateBounds.min[2]};
    const float maxExtent = (std::max)(extents[0], (std::max)(extents[1], extents[2]));
    return maxExtent <= 0.0f || maxExtent > 100000.0f;
}

AssimpSceneProbeBlocker classifyBlocker(const AssimpSceneProbeResult& result) noexcept
{
    if (result.status != AssimpSceneProbeStatus::Success)
    {
        return AssimpSceneProbeBlocker::FileOrParseFailed;
    }
    if (result.animationCount > 0 &&
        result.uniqueBoneNameCount == 0 &&
        result.animationChannelNameCount > 0 &&
        result.candidateSkeletonNodeCount > 0)
    {
        return AssimpSceneProbeBlocker::AnimationChannelsWithoutSkinBones;
    }
    if (result.meshCount > 0 && result.meshesWithUv0 < result.meshCount)
    {
        return AssimpSceneProbeBlocker::MissingUv0;
    }
    if (result.meshCount > 0 && result.meshesWithTangents < result.meshCount)
    {
        return AssimpSceneProbeBlocker::MissingTangents;
    }
    if (result.aggregateVertexCount > kMax16BitIndexedVertexCount)
    {
        return AssimpSceneProbeBlocker::Oversized16BitIndexContract;
    }
    if (result.animationCount > 0 && (result.uniqueBoneNameCount == 0 || result.meshesWithWeights == 0))
    {
        return AssimpSceneProbeBlocker::MissingSkeletonOrWeights;
    }
    if (result.animationCount > 0)
    {
        bool hasChannels = false;
        for (const AssimpSceneProbeAnimation& animation : result.animations)
        {
            hasChannels = hasChannels || animation.channelCount > 0;
        }
        if (!hasChannels)
        {
            return AssimpSceneProbeBlocker::MissingAnimationTracks;
        }
    }
    if (hasSuspiciousBounds(result))
    {
        return AssimpSceneProbeBlocker::SuspiciousBoundsOrAxis;
    }
    if (result.materialCount > 0 && result.materialTextureReferenceCount == 0)
    {
        return AssimpSceneProbeBlocker::MissingMaterialTextureRefs;
    }
    return AssimpSceneProbeBlocker::None;
}

void collectAnimationDiagnostics(
    const aiScene& scene,
    const AssimpLoadedAssetImportOptions& options,
    AssimpSceneProbeResult& result,
    std::set<std::string>& animationChannelNames)
{
    if (scene.mAnimations == nullptr)
    {
        return;
    }

    result.animations.reserve(scene.mNumAnimations);
    for (unsigned int animationIndex = 0; animationIndex < scene.mNumAnimations; ++animationIndex)
    {
        const aiAnimation* const animation = scene.mAnimations[animationIndex];
        if (animation == nullptr)
        {
            continue;
        }

        AssimpSceneProbeAnimation record;
        record.index = animationIndex;
        record.name = animation->mName.C_Str();
        record.durationTicks = animation->mDuration;
        record.ticksPerSecond = effectiveTicksPerSecond(*animation, options);
        record.durationSeconds =
            record.ticksPerSecond > 0.0 ? record.durationTicks / record.ticksPerSecond : 0.0;
        record.channelCount = animation->mNumChannels;

        if (animation->mChannels != nullptr)
        {
            for (unsigned int channelIndex = 0; channelIndex < animation->mNumChannels; ++channelIndex)
            {
                const aiNodeAnim* const channel = animation->mChannels[channelIndex];
                if (channel == nullptr || channel->mNodeName.length == 0)
                {
                    continue;
                }

                const std::string name = channel->mNodeName.C_Str();
                animationChannelNames.emplace(name);
                if (record.channelNames.size() < kMaxAssimpSceneProbeAnimationChannelNames)
                {
                    record.channelNames.push_back(name);
                }
            }
        }

        result.animations.push_back(record);
    }
    result.animationChannelNameCount = static_cast<std::uint32_t>(animationChannelNames.size());
}

void collectAnimationChannelNames(
    const aiScene& scene,
    std::set<std::string>& animationChannelNames)
{
    if (scene.mAnimations == nullptr)
    {
        return;
    }

    for (unsigned int animationIndex = 0; animationIndex < scene.mNumAnimations; ++animationIndex)
    {
        const aiAnimation* const animation = scene.mAnimations[animationIndex];
        if (animation == nullptr || animation->mChannels == nullptr)
        {
            continue;
        }

        for (unsigned int channelIndex = 0; channelIndex < animation->mNumChannels; ++channelIndex)
        {
            const aiNodeAnim* const channel = animation->mChannels[channelIndex];
            if (channel != nullptr && channel->mNodeName.length > 0)
            {
                animationChannelNames.emplace(channel->mNodeName.C_Str());
            }
        }
    }
}

void collectNodeDiagnostics(
    const aiNode& node,
    const aiNode* const parent,
    const std::uint32_t depth,
    const std::set<std::string>& boneNames,
    const std::set<std::string>& animationChannelNames,
    std::set<std::string>& nodeNames,
    AssimpSceneProbeResult& result)
{
    const std::string name = node.mName.C_Str();
    if (!name.empty())
    {
        nodeNames.emplace(name);
    }

    ++result.nodeCount;
    result.maxNodeDepth = (std::max)(result.maxNodeDepth, depth);
    if (node.mNumMeshes > 0)
    {
        ++result.meshReferencingNodeCount;
    }

    const bool animated = animationChannelNames.find(name) != animationChannelNames.end();
    const bool bone = boneNames.find(name) != boneNames.end();
    if (animated || bone)
    {
        ++result.candidateSkeletonNodeCount;
    }

    if (result.nodes.size() < kMaxAssimpSceneProbeNodeRecords)
    {
        AssimpSceneProbeNode record;
        record.name = name;
        record.parentName = parent != nullptr ? parent->mName.C_Str() : "";
        record.depth = depth;
        record.childCount = node.mNumChildren;
        record.meshIndexCount = node.mNumMeshes;
        record.referencedByAnimationChannel = animated;
        record.referencedByMeshBone = bone;
        copyMatrixColumnMajor(node.mTransformation, record.localTransform);
        result.nodes.push_back(record);
    }

    for (unsigned int childIndex = 0; childIndex < node.mNumChildren; ++childIndex)
    {
        const aiNode* const child = node.mChildren[childIndex];
        if (child != nullptr)
        {
            collectNodeDiagnostics(
                *child,
                &node,
                depth + 1,
                boneNames,
                animationChannelNames,
                nodeNames,
                result);
        }
    }
}

bool nodeOrAncestorMatches(
    const aiNode* node,
    const std::set<std::string>& names) noexcept
{
    while (node != nullptr)
    {
        if (names.find(node->mName.C_Str()) != names.end())
        {
            return true;
        }
        node = node->mParent;
    }
    return false;
}

bool allNodeMeshesAreStatic(const aiNode& node, const aiScene& scene) noexcept
{
    if (node.mNumMeshes == 0 || scene.mMeshes == nullptr)
    {
        return false;
    }

    for (unsigned int index = 0; index < node.mNumMeshes; ++index)
    {
        const unsigned int meshIndex = node.mMeshes[index];
        if (meshIndex >= scene.mNumMeshes ||
            scene.mMeshes[meshIndex] == nullptr ||
            scene.mMeshes[meshIndex]->mNumBones > 0)
        {
            return false;
        }
    }
    return true;
}

void collectMeshNodeRelationshipCounts(
    const aiNode& node,
    const aiScene& scene,
    const std::set<std::string>& animationChannelNames,
    AssimpSkinDataAuditConfigRecord& record)
{
    if (node.mNumMeshes > 0)
    {
        const bool underAnimated = nodeOrAncestorMatches(&node, animationChannelNames);
        const bool matchesAnimation = animationChannelNames.find(node.mName.C_Str()) != animationChannelNames.end();
        if (underAnimated)
        {
            ++record.meshNodesUnderAnimatedCandidate;
        }
        if (matchesAnimation && allNodeMeshesAreStatic(node, scene))
        {
            ++record.staticMeshNodesMatchingAnimationChannels;
        }
        if (!underAnimated && !matchesAnimation)
        {
            ++record.orphanMeshNodes;
        }
    }

    for (unsigned int childIndex = 0; childIndex < node.mNumChildren; ++childIndex)
    {
        if (node.mChildren[childIndex] != nullptr)
        {
            collectMeshNodeRelationshipCounts(
                *node.mChildren[childIndex],
                scene,
                animationChannelNames,
                record);
        }
    }
}

void analyzeSceneSkinData(
    const aiScene& scene,
    AssimpSkinDataAuditConfigRecord& record)
{
    record.meshCount = scene.mNumMeshes;

    std::set<std::string> boneNames;
    if (scene.mMeshes != nullptr)
    {
        for (unsigned int meshIndex = 0; meshIndex < scene.mNumMeshes; ++meshIndex)
        {
            const aiMesh* const mesh = scene.mMeshes[meshIndex];
            if (mesh == nullptr || mesh->mBones == nullptr || mesh->mNumBones == 0)
            {
                continue;
            }

            ++record.meshBoneCount;
            bool meshHasWeights = false;
            std::vector<std::uint32_t> weightsPerVertex(mesh->mNumVertices);
            for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
            {
                const aiBone* const bone = mesh->mBones[boneIndex];
                if (bone == nullptr)
                {
                    continue;
                }

                if (bone->mName.length > 0)
                {
                    boneNames.emplace(bone->mName.C_Str());
                }

                if (!isFiniteMatrix(bone->mOffsetMatrix))
                {
                    ++record.nonFiniteBindPoseCount;
                }
                else if (isIdentity(bone->mOffsetMatrix))
                {
                    ++record.identityBindPoseCount;
                }
                else
                {
                    ++record.nonIdentityBindPoseCount;
                }

                if (bone->mWeights == nullptr)
                {
                    continue;
                }

                for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
                {
                    const aiVertexWeight& weight = bone->mWeights[weightIndex];
                    if (weight.mVertexId < mesh->mNumVertices && std::isfinite(weight.mWeight) && weight.mWeight > 0.0f)
                    {
                        ++record.totalVertexWeights;
                        ++weightsPerVertex[weight.mVertexId];
                        record.maxWeightsPerVertex =
                            (std::max)(record.maxWeightsPerVertex, weightsPerVertex[weight.mVertexId]);
                        meshHasWeights = true;
                    }
                }
            }
            if (meshHasWeights)
            {
                ++record.weightedMeshCount;
            }
        }
    }
    record.uniqueBoneNameCount = static_cast<std::uint32_t>(boneNames.size());

    std::set<std::string> animationChannelNames;
    collectAnimationChannelNames(scene, animationChannelNames);
    record.animationChannelNameCount = static_cast<std::uint32_t>(animationChannelNames.size());

    if (scene.mRootNode != nullptr)
    {
        std::set<std::string> nodeNames;
        AssimpSceneProbeResult nodeProbe;
        collectNodeDiagnostics(
            *scene.mRootNode,
            nullptr,
            0,
            boneNames,
            animationChannelNames,
            nodeNames,
            nodeProbe);
        record.candidateSkeletonNodeCount = nodeProbe.candidateSkeletonNodeCount;
        collectMeshNodeRelationshipCounts(*scene.mRootNode, scene, animationChannelNames, record);
    }
}

bool auditRecordHasSkinData(const AssimpSkinDataAuditConfigRecord& record) noexcept
{
    return record.status == AssimpSceneProbeStatus::Success &&
        record.meshBoneCount > 0 &&
        record.weightedMeshCount > 0 &&
        record.uniqueBoneNameCount > 0 &&
        record.totalVertexWeights > 0 &&
        record.nonFiniteBindPoseCount == 0;
}

AssimpSkinDataAuditBlocker classifySkinDataAudit(
    const std::vector<AssimpSkinDataAuditConfigRecord>& configs) noexcept
{
    bool anyParsed = false;
    bool anyAnimation = false;
    bool anyRigidCandidate = false;
    bool anyUnsafeSkin = false;

    for (const AssimpSkinDataAuditConfigRecord& config : configs)
    {
        if (config.status != AssimpSceneProbeStatus::Success)
        {
            continue;
        }

        anyParsed = true;
        anyAnimation = anyAnimation || config.animationChannelNameCount > 0;
        anyRigidCandidate = anyRigidCandidate || config.meshNodesUnderAnimatedCandidate > 0;
        if (auditRecordHasSkinData(config))
        {
            if (config.safeForImportPolicy)
            {
                return AssimpSkinDataAuditBlocker::SkinDataFound;
            }
            anyUnsafeSkin = true;
        }
    }

    if (anyUnsafeSkin)
    {
        return AssimpSkinDataAuditBlocker::SkinDataOnlyWithUnsafeConfig;
    }
    if (!anyParsed)
    {
        return AssimpSkinDataAuditBlocker::ParseFailed;
    }
    if (anyAnimation && anyRigidCandidate)
    {
        return AssimpSkinDataAuditBlocker::RigidNodeAnimationOnly;
    }
    if (anyAnimation)
    {
        return AssimpSkinDataAuditBlocker::AnimationOnly;
    }
    return AssimpSkinDataAuditBlocker::NoSkinDataAnyConfig;
}

std::string recommendationFor(const AssimpSkinDataAuditBlocker blocker)
{
    switch (blocker)
    {
    case AssimpSkinDataAuditBlocker::SkinDataFound:
        return "Implement FBX skinned import using the first safe configuration that exposes mesh bones, weights, and finite bind poses.";
    case AssimpSkinDataAuditBlocker::SkinDataOnlyWithUnsafeConfig:
        return "Skin data appears only under an unsafe diagnostic configuration; inspect that configuration before adopting it as import policy.";
    case AssimpSkinDataAuditBlocker::RigidNodeAnimationOnly:
        return "No recoverable skin weights were exposed; consider a rigid node-attached mesh prototype or source conversion.";
    case AssimpSkinDataAuditBlocker::AnimationOnly:
        return "Animation channels are present without recoverable skin or mesh-node relationships; use source conversion or evaluate a dedicated FBX parser.";
    case AssimpSkinDataAuditBlocker::NoSkinDataAnyConfig:
        return "No tested Assimp configuration exposes recoverable skin data; use source conversion or evaluate a dedicated FBX parser.";
    case AssimpSkinDataAuditBlocker::ParseFailed:
        return "All tested Assimp configurations failed to parse; fix file access/import flags before skin recovery work.";
    }
    return "Unknown audit result.";
}

struct SkinAuditConfig
{
    const char* name = "";
    AssimpLoadedAssetImportOptions options = {};
    unsigned int extraFlags = 0;
    bool safeForImportPolicy = true;
};

bool hasFbxExtension(const std::string& uri)
{
    return uri.size() >= 4 &&
        (uri.rfind(".fbx") == uri.size() - 4 || uri.rfind(".FBX") == uri.size() - 4);
}

std::vector<SkinAuditConfig> makeSkinAuditConfigs(
    const AssimpLoadedAssetImportOptions& base,
    const bool includeArmaturePopulation)
{
    std::vector<SkinAuditConfig> configs;
    configs.push_back({"BaselineCurrent", base, 0, true});

    AssimpLoadedAssetImportOptions noJoin = base;
    noJoin.joinIdenticalVertices = false;
    configs.push_back({"NoJoinIdenticalVertices", noJoin, 0, true});

    AssimpLoadedAssetImportOptions noValidate = base;
    noValidate.validateDataStructure = false;
    configs.push_back({"NoValidateDataStructure", noValidate, 0, false});

    AssimpLoadedAssetImportOptions generated = base;
    generated.generateMissingNormals = true;
    generated.generateMissingTangents = true;
    configs.push_back({"GeneratedNormalsTangents", generated, 0, true});

    AssimpLoadedAssetImportOptions noGenerated = base;
    noGenerated.generateMissingNormals = false;
    noGenerated.generateMissingTangents = false;
    configs.push_back({"NoGeneratedNormalsTangents", noGenerated, 0, true});

    if (includeArmaturePopulation)
    {
        configs.push_back({"PopulateArmatureData", base, aiProcess_PopulateArmatureData, true});
    }
    return configs;
}

void appendJsonEscaped(std::ostringstream& output, const std::string& value)
{
    for (const char ch : value)
    {
        switch (ch)
        {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
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
            if (static_cast<unsigned char>(ch) < 0x20)
            {
                output << "\\u"
                       << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(static_cast<unsigned char>(ch))
                       << std::dec << std::setfill(' ');
            }
            else
            {
                output << ch;
            }
            break;
        }
    }
}

void appendJsonStringField(
    std::ostringstream& output,
    const char* const name,
    const std::string& value,
    const bool trailingComma,
    const char* const indent = "  ")
{
    output << indent << "\"" << name << "\": \"";
    appendJsonEscaped(output, value);
    output << '"';
    if (trailingComma)
    {
        output << ',';
    }
    output << '\n';
}

void appendBoundsArray(std::ostringstream& output, const char* const name, const float values[3])
{
    output << "    \"" << name << "\": ["
           << values[0] << ", "
           << values[1] << ", "
           << values[2] << "]";
}
} // namespace

const char* assimpSceneProbeStatusName(const AssimpSceneProbeStatus status) noexcept
{
    switch (status)
    {
    case AssimpSceneProbeStatus::Success:
        return "Success";
    case AssimpSceneProbeStatus::InvalidArgument:
        return "InvalidArgument";
    case AssimpSceneProbeStatus::IoError:
        return "IoError";
    case AssimpSceneProbeStatus::ParseError:
        return "ParseError";
    }
    return "Unknown";
}

const char* assimpSceneProbeBlockerName(const AssimpSceneProbeBlocker blocker) noexcept
{
    switch (blocker)
    {
    case AssimpSceneProbeBlocker::None:
        return "None";
    case AssimpSceneProbeBlocker::FileOrParseFailed:
        return "FileOrParseFailed";
    case AssimpSceneProbeBlocker::AnimationChannelsWithoutSkinBones:
        return "AnimationChannelsWithoutSkinBones";
    case AssimpSceneProbeBlocker::MissingUv0:
        return "MissingUv0";
    case AssimpSceneProbeBlocker::MissingTangents:
        return "MissingTangents";
    case AssimpSceneProbeBlocker::Oversized16BitIndexContract:
        return "Oversized16BitIndexContract";
    case AssimpSceneProbeBlocker::MissingSkeletonOrWeights:
        return "MissingSkeletonOrWeights";
    case AssimpSceneProbeBlocker::MissingAnimationTracks:
        return "MissingAnimationTracks";
    case AssimpSceneProbeBlocker::SuspiciousBoundsOrAxis:
        return "SuspiciousBoundsOrAxis";
    case AssimpSceneProbeBlocker::MissingMaterialTextureRefs:
        return "MissingMaterialTextureRefs";
    }
    return "Unknown";
}

const char* assimpSkinDataAuditBlockerName(const AssimpSkinDataAuditBlocker blocker) noexcept
{
    switch (blocker)
    {
    case AssimpSkinDataAuditBlocker::SkinDataFound:
        return "SkinDataFound";
    case AssimpSkinDataAuditBlocker::NoSkinDataAnyConfig:
        return "NoSkinDataAnyConfig";
    case AssimpSkinDataAuditBlocker::SkinDataOnlyWithUnsafeConfig:
        return "SkinDataOnlyWithUnsafeConfig";
    case AssimpSkinDataAuditBlocker::ParseFailed:
        return "ParseFailed";
    case AssimpSkinDataAuditBlocker::AnimationOnly:
        return "AnimationOnly";
    case AssimpSkinDataAuditBlocker::RigidNodeAnimationOnly:
        return "RigidNodeAnimationOnly";
    }
    return "Unknown";
}

AssimpSceneProbeResult probeAssimpScene(
    const std::string& uri,
    const AssimpLoadedAssetImportOptions& options)
{
    AssimpSceneProbeResult result;
    if (uri.empty())
    {
        result.status = AssimpSceneProbeStatus::InvalidArgument;
        result.firstBlocker = classifyBlocker(result);
        return result;
    }

    std::ifstream input(uri);
    if (!input)
    {
        result.status = AssimpSceneProbeStatus::IoError;
        result.firstBlocker = classifyBlocker(result);
        return result;
    }

    Assimp::Importer importer;
    const aiScene* const scene = importer.ReadFile(uri, probePostProcessFlags(options));
    if (scene == nullptr)
    {
        result.status = AssimpSceneProbeStatus::ParseError;
        result.firstBlocker = classifyBlocker(result);
        return result;
    }

    result.status = AssimpSceneProbeStatus::Success;
    result.meshCount = scene->mNumMeshes;
    result.materialCount = scene->mNumMaterials;
    result.embeddedTextureCount = scene->mNumTextures;
    result.animationCount = scene->mNumAnimations;

    if (scene->mRootNode != nullptr)
    {
        copyMatrixColumnMajor(scene->mRootNode->mTransformation, result.rootTransform);
        result.rootTransformIdentity = isIdentity(result.rootTransform);
    }

    std::set<std::string> boneNames;
    std::set<std::string> animationChannelNames;
    std::set<std::string> nodeNames;
    if (scene->mMeshes != nullptr)
    {
        bool firstPosition = true;
        for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
        {
            const aiMesh* const mesh = scene->mMeshes[meshIndex];
            if (mesh == nullptr)
            {
                continue;
            }

            result.aggregateVertexCount += mesh->mNumVertices;
            if (mesh->mNumBones > 0)
            {
                ++result.skinnedMeshCount;
                ++result.meshesWithBones;
            }
            else
            {
                ++result.staticMeshCount;
            }
            if (mesh->HasPositions())
            {
                ++result.meshesWithPositions;
                for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
                {
                    includePosition(result.aggregateBounds, mesh->mVertices[vertexIndex], firstPosition);
                    firstPosition = false;
                    result.hasBounds = true;
                }
            }
            if (mesh->HasNormals())
            {
                ++result.meshesWithNormals;
            }
            if (mesh->HasTextureCoords(0))
            {
                ++result.meshesWithUv0;
            }
            if (mesh->HasTangentsAndBitangents())
            {
                ++result.meshesWithTangents;
            }
            if (mesh->HasVertexColors(0))
            {
                ++result.meshesWithVertexColors;
            }
            if (hasAnyWeights(*mesh))
            {
                ++result.meshesWithWeights;
            }

            if (mesh->mBones != nullptr)
            {
                for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
                {
                    const aiBone* const bone = mesh->mBones[boneIndex];
                    if (bone != nullptr && bone->mName.length > 0)
                    {
                        boneNames.emplace(bone->mName.C_Str());
                    }
                }
            }

            if (mesh->mFaces != nullptr)
            {
                for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
                {
                    const aiFace& face = mesh->mFaces[faceIndex];
                    result.aggregateIndexCount += face.mNumIndices;
                    if (face.mNumIndices == 3)
                    {
                        ++result.triangleFaceCount;
                    }
                    else
                    {
                        ++result.nonTriangleFaceCount;
                    }
                }
            }
        }
    }
    result.uniqueBoneNameCount = static_cast<std::uint32_t>(boneNames.size());

    collectAnimationDiagnostics(*scene, options, result, animationChannelNames);

    if (scene->mRootNode != nullptr)
    {
        result.rootChildCount = scene->mRootNode->mNumChildren;
        collectNodeDiagnostics(
            *scene->mRootNode,
            nullptr,
            0,
            boneNames,
            animationChannelNames,
            nodeNames,
            result);
    }

    for (const std::string& channelName : animationChannelNames)
    {
        if (boneNames.find(channelName) == boneNames.end() &&
            nodeNames.find(channelName) == nodeNames.end())
        {
            ++result.unmatchedAnimationChannelNameCount;
        }
    }

    if (scene->mMaterials != nullptr)
    {
        for (unsigned int materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
        {
            const aiMaterial* const material = scene->mMaterials[materialIndex];
            if (material != nullptr)
            {
                result.materialTextureReferenceCount += materialTextureReferenceCount(*material);
            }
        }
    }

    result.firstBlocker = classifyBlocker(result);
    return result;
}

AssimpSkinDataAuditResult auditAssimpSkinDataAvailability(
    const std::string& uri,
    const AssimpLoadedAssetImportOptions& options)
{
    AssimpSkinDataAuditResult result;
    const std::vector<SkinAuditConfig> configs = makeSkinAuditConfigs(options, hasFbxExtension(uri));
    result.configs.reserve(configs.size());

    for (const SkinAuditConfig& config : configs)
    {
        AssimpSkinDataAuditConfigRecord record;
        record.name = config.name;
        record.safeForImportPolicy = config.safeForImportPolicy;

        if (uri.empty())
        {
            record.status = AssimpSceneProbeStatus::InvalidArgument;
            result.configs.push_back(record);
            continue;
        }

        std::ifstream input(uri);
        if (!input)
        {
            record.status = AssimpSceneProbeStatus::IoError;
            result.configs.push_back(record);
            continue;
        }

        Assimp::Importer importer;
        const unsigned int flags = probePostProcessFlags(config.options) | config.extraFlags;
        const aiScene* const scene = importer.ReadFile(uri, flags);
        if (scene == nullptr)
        {
            record.status = AssimpSceneProbeStatus::ParseError;
            result.configs.push_back(record);
            continue;
        }

        record.status = AssimpSceneProbeStatus::Success;
        analyzeSceneSkinData(*scene, record);
        result.configs.push_back(record);
    }

    result.firstBlocker = classifySkinDataAudit(result.configs);
    result.recommendation = recommendationFor(result.firstBlocker);
    return result;
}

std::string formatAssimpSceneProbeResultJson(
    const AssimpSceneProbeResult& result,
    const std::string& sourceLabel)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(6);
    output << "{\n";
    appendJsonStringField(output, "source", sourceLabel, true);
    appendJsonStringField(output, "status", assimpSceneProbeStatusName(result.status), true);
    appendJsonStringField(output, "firstBlocker", assimpSceneProbeBlockerName(result.firstBlocker), true);

    output << "  \"counts\": {\n"
           << "    \"meshes\": " << result.meshCount << ",\n"
           << "    \"staticMeshes\": " << result.staticMeshCount << ",\n"
           << "    \"skinnedMeshes\": " << result.skinnedMeshCount << ",\n"
           << "    \"materials\": " << result.materialCount << ",\n"
           << "    \"embeddedTextures\": " << result.embeddedTextureCount << ",\n"
           << "    \"materialTextureReferences\": " << result.materialTextureReferenceCount << ",\n"
           << "    \"animations\": " << result.animationCount << ",\n"
           << "    \"vertices\": " << result.aggregateVertexCount << ",\n"
           << "    \"indices\": " << result.aggregateIndexCount << ",\n"
           << "    \"triangleFaces\": " << result.triangleFaceCount << ",\n"
           << "    \"nonTriangleFaces\": " << result.nonTriangleFaceCount << ",\n"
           << "    \"meshesWithPositions\": " << result.meshesWithPositions << ",\n"
           << "    \"meshesWithNormals\": " << result.meshesWithNormals << ",\n"
           << "    \"meshesWithUv0\": " << result.meshesWithUv0 << ",\n"
           << "    \"meshesWithTangents\": " << result.meshesWithTangents << ",\n"
           << "    \"meshesWithVertexColors\": " << result.meshesWithVertexColors << ",\n"
           << "    \"meshesWithBones\": " << result.meshesWithBones << ",\n"
           << "    \"meshesWithWeights\": " << result.meshesWithWeights << ",\n"
           << "    \"uniqueBoneNames\": " << result.uniqueBoneNameCount << ",\n"
           << "    \"nodes\": " << result.nodeCount << ",\n"
           << "    \"maxNodeDepth\": " << result.maxNodeDepth << ",\n"
           << "    \"rootChildren\": " << result.rootChildCount << ",\n"
           << "    \"meshReferencingNodes\": " << result.meshReferencingNodeCount << ",\n"
           << "    \"animationChannelNames\": " << result.animationChannelNameCount << ",\n"
           << "    \"unmatchedAnimationChannelNames\": " << result.unmatchedAnimationChannelNameCount << ",\n"
           << "    \"candidateSkeletonNodes\": " << result.candidateSkeletonNodeCount << "\n"
           << "  },\n";

    output << "  \"bounds\": {\n"
           << "    \"present\": " << (result.hasBounds ? "true" : "false");
    if (result.hasBounds)
    {
        output << ",\n";
        appendBoundsArray(output, "min", result.aggregateBounds.min);
        output << ",\n";
        appendBoundsArray(output, "max", result.aggregateBounds.max);
        output << '\n';
    }
    else
    {
        output << '\n';
    }
    output << "  },\n";

    output << "  \"rootTransform\": {\n"
           << "    \"identity\": " << (result.rootTransformIdentity ? "true" : "false") << ",\n"
           << "    \"matrixColumnMajor\": [";
    for (std::size_t index = 0; index < result.rootTransform.size(); ++index)
    {
        if (index > 0)
        {
            output << ", ";
        }
        output << result.rootTransform[index];
    }
    output << "]\n"
           << "  },\n";

    output << "  \"animations\": [\n";
    for (std::size_t index = 0; index < result.animations.size(); ++index)
    {
        const AssimpSceneProbeAnimation& animation = result.animations[index];
        output << "    {\n"
               << "      \"index\": " << animation.index << ",\n";
        appendJsonStringField(output, "name", animation.name, true, "      ");
        output << "      \"durationTicks\": " << animation.durationTicks << ",\n"
               << "      \"ticksPerSecond\": " << animation.ticksPerSecond << ",\n"
               << "      \"durationSeconds\": " << animation.durationSeconds << ",\n"
               << "      \"channels\": " << animation.channelCount << ",\n"
               << "      \"copiedChannelNames\": [";
        for (std::size_t channelIndex = 0; channelIndex < animation.channelNames.size(); ++channelIndex)
        {
            if (channelIndex > 0)
            {
                output << ", ";
            }
            output << '"';
            appendJsonEscaped(output, animation.channelNames[channelIndex]);
            output << '"';
        }
        output << "]\n"
               << "    }";
        if (index + 1 < result.animations.size())
        {
            output << ',';
        }
        output << '\n';
    }
    output << "  ],\n"
           << "  \"nodes\": [\n";
    for (std::size_t index = 0; index < result.nodes.size(); ++index)
    {
        const AssimpSceneProbeNode& node = result.nodes[index];
        output << "    {\n";
        appendJsonStringField(output, "name", node.name, true, "      ");
        appendJsonStringField(output, "parent", node.parentName, true, "      ");
        output << "      \"depth\": " << node.depth << ",\n"
               << "      \"children\": " << node.childCount << ",\n"
               << "      \"meshIndices\": " << node.meshIndexCount << ",\n"
               << "      \"referencedByAnimationChannel\": " << (node.referencedByAnimationChannel ? "true" : "false") << ",\n"
               << "      \"referencedByMeshBone\": " << (node.referencedByMeshBone ? "true" : "false") << ",\n"
               << "      \"localTransformColumnMajor\": [";
        for (std::size_t matrixIndex = 0; matrixIndex < node.localTransform.size(); ++matrixIndex)
        {
            if (matrixIndex > 0)
            {
                output << ", ";
            }
            output << node.localTransform[matrixIndex];
        }
        output << "]\n"
               << "    }";
        if (index + 1 < result.nodes.size())
        {
            output << ',';
        }
        output << '\n';
    }
    output << "  ]\n"
           << "}\n";
    return output.str();
}

std::string formatAssimpSkinDataAuditResultJson(
    const AssimpSkinDataAuditResult& result,
    const std::string& sourceLabel)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(6);
    output << "{\n";
    appendJsonStringField(output, "source", sourceLabel, true);
    appendJsonStringField(output, "firstBlocker", assimpSkinDataAuditBlockerName(result.firstBlocker), true);
    appendJsonStringField(output, "recommendation", result.recommendation, true);
    output << "  \"configs\": [\n";
    for (std::size_t index = 0; index < result.configs.size(); ++index)
    {
        const AssimpSkinDataAuditConfigRecord& record = result.configs[index];
        output << "    {\n";
        appendJsonStringField(output, "name", record.name, true, "      ");
        appendJsonStringField(output, "status", assimpSceneProbeStatusName(record.status), true, "      ");
        output << "      \"safeForImportPolicy\": " << (record.safeForImportPolicy ? "true" : "false") << ",\n"
               << "      \"meshes\": " << record.meshCount << ",\n"
               << "      \"meshBoneCount\": " << record.meshBoneCount << ",\n"
               << "      \"weightedMeshCount\": " << record.weightedMeshCount << ",\n"
               << "      \"uniqueBoneNames\": " << record.uniqueBoneNameCount << ",\n"
               << "      \"totalVertexWeights\": " << record.totalVertexWeights << ",\n"
               << "      \"maxWeightsPerVertex\": " << record.maxWeightsPerVertex << ",\n"
               << "      \"animationChannelNames\": " << record.animationChannelNameCount << ",\n"
               << "      \"candidateSkeletonNodes\": " << record.candidateSkeletonNodeCount << ",\n"
               << "      \"identityBindPoseCount\": " << record.identityBindPoseCount << ",\n"
               << "      \"nonIdentityBindPoseCount\": " << record.nonIdentityBindPoseCount << ",\n"
               << "      \"nonFiniteBindPoseCount\": " << record.nonFiniteBindPoseCount << ",\n"
               << "      \"meshNodesUnderAnimatedCandidate\": " << record.meshNodesUnderAnimatedCandidate << ",\n"
               << "      \"staticMeshNodesMatchingAnimationChannels\": " << record.staticMeshNodesMatchingAnimationChannels << ",\n"
               << "      \"orphanMeshNodes\": " << record.orphanMeshNodes << "\n"
               << "    }";
        if (index + 1 < result.configs.size())
        {
            output << ',';
        }
        output << '\n';
    }
    output << "  ]\n"
           << "}\n";
    return output.str();
}
} // namespace full_engine
