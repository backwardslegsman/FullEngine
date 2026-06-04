#include "engine/assets/LoadedAssetPayload.hpp"

#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* const message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

full_engine::AssetId asset(const std::uint64_t value) noexcept
{
    return {value};
}

full_engine::LoadedMeshVertex vertex(const float x, const float y, const float z) noexcept
{
    full_engine::LoadedMeshVertex result;
    result.position[0] = x;
    result.position[1] = y;
    result.position[2] = z;
    result.normal[0] = 0.0f;
    result.normal[1] = 1.0f;
    result.normal[2] = 0.0f;
    result.colorLinear[0] = 1.0f;
    result.colorLinear[1] = 0.75f;
    result.colorLinear[2] = 0.5f;
    result.colorLinear[3] = 1.0f;
    return result;
}

full_engine::LoadedMeshAsset meshAsset()
{
    full_engine::LoadedMeshAsset mesh;
    mesh.id = asset(10);
    mesh.vertices = {
        vertex(0.0f, 0.0f, 0.0f),
        vertex(1.0f, 0.0f, 0.0f),
        vertex(0.0f, 1.0f, 0.0f)};
    mesh.indices = {0, 1, 2};
    mesh.localBounds.min[0] = 0.0f;
    mesh.localBounds.min[1] = 0.0f;
    mesh.localBounds.min[2] = 0.0f;
    mesh.localBounds.max[0] = 1.0f;
    mesh.localBounds.max[1] = 1.0f;
    mesh.localBounds.max[2] = 0.0f;
    return mesh;
}

full_engine::LoadedTextureAsset textureAsset()
{
    full_engine::LoadedTextureAsset texture;
    texture.id = asset(20);
    texture.width = 4;
    texture.height = 2;
    texture.mipCount = 1;
    texture.format = full_engine::AssetSourceTextureFormat::Rgba8;
    texture.semantic = full_engine::AssetSourceTextureSemantic::Color;
    texture.colorSpace = full_engine::AssetSourceTextureColorSpace::Srgb;
    texture.bytes.assign(texture.width * texture.height * 4, 255);
    return texture;
}

full_engine::LoadedMaterialAsset materialAsset()
{
    full_engine::LoadedMaterialAsset material;
    material.id = asset(30);
    material.model = full_engine::AssetSourceMaterialModel::TerrainSplat;
    material.alphaMode = full_engine::AssetSourceMaterialAlphaMode::Opaque;
    material.textureRefs[0] = {full_engine::AssetSourceMaterialTextureSlot::BaseColor, asset(20)};
    material.textureRefs[1] = {full_engine::AssetSourceMaterialTextureSlot::Normal, asset(21)};
    material.textureRefCount = 2;
    return material;
}

void setIdentity(float values[16]) noexcept
{
    for (int index = 0; index < 16; ++index)
    {
        values[index] = 0.0f;
    }
    values[0] = 1.0f;
    values[5] = 1.0f;
    values[10] = 1.0f;
    values[15] = 1.0f;
}

full_engine::LoadedSkeletonJoint joint(const std::int32_t parentIndex, const char* const name)
{
    full_engine::LoadedSkeletonJoint result;
    result.name = name;
    result.parentIndex = parentIndex;
    setIdentity(result.inverseBindPose);
    setIdentity(result.referenceTransform);
    return result;
}

full_engine::LoadedSkeletonAsset skeletonAsset()
{
    full_engine::LoadedSkeletonAsset skeleton;
    skeleton.id = asset(40);
    skeleton.joints = {
        joint(-1, "root"),
        joint(0, "spine")};
    return skeleton;
}

full_engine::LoadedSkinnedMeshVertex skinnedVertex(const float x, const float y, const float z) noexcept
{
    full_engine::LoadedSkinnedMeshVertex result;
    result.position[0] = x;
    result.position[1] = y;
    result.position[2] = z;
    result.normal[0] = 0.0f;
    result.normal[1] = 1.0f;
    result.normal[2] = 0.0f;
    result.uv0[0] = x;
    result.uv0[1] = y;
    result.colorLinear[0] = 1.0f;
    result.colorLinear[1] = 0.75f;
    result.colorLinear[2] = 0.5f;
    result.colorLinear[3] = 1.0f;
    result.jointIndices[0] = 0;
    result.jointIndices[1] = 1;
    result.jointIndices[2] = 0;
    result.jointIndices[3] = 0;
    result.jointWeights[0] = 0.5f;
    result.jointWeights[1] = 0.5f;
    result.jointWeights[2] = 0.0f;
    result.jointWeights[3] = 0.0f;
    return result;
}

full_engine::LoadedSkinnedMeshAsset skinnedMeshAsset()
{
    full_engine::LoadedSkinnedMeshAsset mesh;
    mesh.id = asset(50);
    mesh.skeletonAssetId = asset(40);
    mesh.vertices = {
        skinnedVertex(0.0f, 0.0f, 0.0f),
        skinnedVertex(1.0f, 0.0f, 0.0f),
        skinnedVertex(0.0f, 1.0f, 0.0f)};
    mesh.indices = {0, 1, 2};
    mesh.localBounds.min[0] = 0.0f;
    mesh.localBounds.min[1] = 0.0f;
    mesh.localBounds.min[2] = 0.0f;
    mesh.localBounds.max[0] = 1.0f;
    mesh.localBounds.max[1] = 1.0f;
    mesh.localBounds.max[2] = 0.0f;
    return mesh;
}

full_engine::LoadedAnimationJointTrack animationTrack(const std::uint16_t jointIndex)
{
    full_engine::LoadedAnimationJointTrack track;
    track.jointIndex = jointIndex;

    full_engine::LoadedAnimationTranslationKey translation0;
    translation0.timeSeconds = 0.0f;
    translation0.value[0] = 0.0f;
    translation0.value[1] = 0.0f;
    translation0.value[2] = 0.0f;
    full_engine::LoadedAnimationTranslationKey translation1;
    translation1.timeSeconds = 1.0f;
    translation1.value[0] = 0.0f;
    translation1.value[1] = 1.0f;
    translation1.value[2] = 0.0f;
    track.translations = {translation0, translation1};

    full_engine::LoadedAnimationRotationKey rotation0;
    rotation0.timeSeconds = 0.0f;
    rotation0.value[0] = 0.0f;
    rotation0.value[1] = 0.0f;
    rotation0.value[2] = 0.0f;
    rotation0.value[3] = 1.0f;
    full_engine::LoadedAnimationRotationKey rotation1 = rotation0;
    rotation1.timeSeconds = 1.0f;
    rotation1.value[2] = 0.70710677f;
    rotation1.value[3] = 0.70710677f;
    track.rotations = {rotation0, rotation1};

    full_engine::LoadedAnimationScaleKey scale0;
    scale0.timeSeconds = 0.0f;
    scale0.value[0] = 1.0f;
    scale0.value[1] = 1.0f;
    scale0.value[2] = 1.0f;
    full_engine::LoadedAnimationScaleKey scale1 = scale0;
    scale1.timeSeconds = 1.0f;
    track.scales = {scale0, scale1};
    return track;
}

full_engine::LoadedAnimationClipAsset animationClipAsset()
{
    full_engine::LoadedAnimationClipAsset clip;
    clip.id = asset(60);
    clip.skeletonAssetId = asset(40);
    clip.durationSeconds = 1.0f;
    clip.ticksPerSecond = 30.0f;
    clip.tracks = {
        animationTrack(0),
        animationTrack(1)};
    return clip;
}

void testValidPayloads(std::vector<std::string>& failures)
{
    expect(
        full_engine::validateLoadedMeshAsset(meshAsset()) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "valid mesh payload passes",
        failures);
    expect(
        full_engine::validateLoadedTextureAsset(textureAsset()) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "valid texture payload passes",
        failures);
    expect(
        full_engine::validateLoadedMaterialAsset(materialAsset()) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "valid material payload passes",
        failures);
    expect(
        full_engine::validateLoadedSkeletonAsset(skeletonAsset()) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "valid skeleton payload passes",
        failures);
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(skinnedMeshAsset()) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "valid skinned mesh payload passes",
        failures);
    expect(
        full_engine::validateLoadedAnimationClipAsset(animationClipAsset()) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "valid animation clip payload passes",
        failures);
}

void testDefaultIdsFail(std::vector<std::string>& failures)
{
    full_engine::LoadedMeshAsset mesh = meshAsset();
    mesh.id = {};
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAssetId,
        "mesh payload rejects default id",
        failures);

    full_engine::LoadedTextureAsset texture = textureAsset();
    texture.id = {};
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAssetId,
        "texture payload rejects default id",
        failures);

    full_engine::LoadedMaterialAsset material = materialAsset();
    material.id = {};
    expect(
        full_engine::validateLoadedMaterialAsset(material) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAssetId,
        "material payload rejects default id",
        failures);

    full_engine::LoadedSkeletonAsset skeleton = skeletonAsset();
    skeleton.id = {};
    expect(
        full_engine::validateLoadedSkeletonAsset(skeleton) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAssetId,
        "skeleton payload rejects default id",
        failures);

    full_engine::LoadedSkinnedMeshAsset skinnedMesh = skinnedMeshAsset();
    skinnedMesh.id = {};
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(skinnedMesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAssetId,
        "skinned mesh payload rejects default id",
        failures);

    full_engine::LoadedAnimationClipAsset clip = animationClipAsset();
    clip.id = {};
    expect(
        full_engine::validateLoadedAnimationClipAsset(clip) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAssetId,
        "animation clip payload rejects default id",
        failures);
}

void testInvalidMeshPayloads(std::vector<std::string>& failures)
{
    full_engine::LoadedMeshAsset mesh = meshAsset();
    mesh.vertices.clear();
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshVertices,
        "mesh payload rejects empty vertices",
        failures);

    mesh = meshAsset();
    mesh.indices.clear();
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshIndices,
        "mesh payload rejects empty indices",
        failures);

    mesh = meshAsset();
    mesh.indices.push_back(0);
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshIndices,
        "mesh payload rejects non-triangle indices",
        failures);

    mesh = meshAsset();
    mesh.indices[2] = 9;
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshIndices,
        "mesh payload rejects out-of-range index",
        failures);

    mesh = meshAsset();
    mesh.vertices[0].position[1] = std::nanf("");
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshVertexData,
        "mesh payload rejects non-finite position",
        failures);

    mesh = meshAsset();
    mesh.vertices[0].normal[1] = 0.0f;
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshVertexData,
        "mesh payload rejects zero normal",
        failures);

    mesh = meshAsset();
    mesh.vertices[0].colorLinear[0] = 2.0f;
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshVertexData,
        "mesh payload rejects out-of-range color",
        failures);

    mesh = meshAsset();
    mesh.localBounds.min[0] = 2.0f;
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshBounds,
        "mesh payload rejects inverted bounds",
        failures);

    mesh = meshAsset();
    mesh.localBounds.max[2] = std::nanf("");
    expect(
        full_engine::validateLoadedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMeshBounds,
        "mesh payload rejects non-finite bounds",
        failures);
}

void testInvalidTexturePayloads(std::vector<std::string>& failures)
{
    full_engine::LoadedTextureAsset texture = textureAsset();
    texture.width = 0;
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidTextureDimensions,
        "texture payload rejects zero width",
        failures);

    texture = textureAsset();
    texture.height = 0;
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidTextureDimensions,
        "texture payload rejects zero height",
        failures);

    texture = textureAsset();
    texture.mipCount = 2;
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidTextureMipCount,
        "texture payload rejects non-v1 mip count",
        failures);

    texture = textureAsset();
    texture.format = full_engine::AssetSourceTextureFormat::Unknown;
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidTextureFormat,
        "texture payload rejects unknown format",
        failures);

    texture = textureAsset();
    texture.semantic = full_engine::AssetSourceTextureSemantic::Unknown;
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidTextureSemantic,
        "texture payload rejects unknown semantic",
        failures);

    texture = textureAsset();
    texture.colorSpace = full_engine::AssetSourceTextureColorSpace::Unknown;
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidTextureColorSpace,
        "texture payload rejects unknown color space",
        failures);

    texture = textureAsset();
    texture.bytes.resize(texture.bytes.size() - 1);
    expect(
        full_engine::validateLoadedTextureAsset(texture) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidTextureByteCount,
        "texture payload rejects undersized byte buffer",
        failures);
}

void testInvalidMaterialPayloads(std::vector<std::string>& failures)
{
    full_engine::LoadedMaterialAsset material = materialAsset();
    material.model = full_engine::AssetSourceMaterialModel::Unknown;
    expect(
        full_engine::validateLoadedMaterialAsset(material) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialModel,
        "material payload rejects unknown model",
        failures);

    material = materialAsset();
    material.alphaMode = full_engine::AssetSourceMaterialAlphaMode::Unknown;
    expect(
        full_engine::validateLoadedMaterialAsset(material) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialAlphaMode,
        "material payload rejects unknown alpha mode",
        failures);

    material = materialAsset();
    material.textureRefCount = full_engine::kMaxAssetSourceMaterialTextureRefs + 1;
    expect(
        full_engine::validateLoadedMaterialAsset(material) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialTextureCount,
        "material payload rejects too many texture refs",
        failures);

    material = materialAsset();
    material.textureRefs[1].id = {};
    expect(
        full_engine::validateLoadedMaterialAsset(material) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialTextureRef,
        "material payload rejects active default texture ref",
        failures);
}

void testInvalidSkeletonPayloads(std::vector<std::string>& failures)
{
    full_engine::LoadedSkeletonAsset skeleton = skeletonAsset();
    skeleton.joints.clear();
    expect(
        full_engine::validateLoadedSkeletonAsset(skeleton) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkeletonJointCount,
        "skeleton payload rejects zero joints",
        failures);

    skeleton = skeletonAsset();
    skeleton.joints.assign(full_engine::kMaxLoadedSkeletonJoints + 1, joint(-1, "extra"));
    expect(
        full_engine::validateLoadedSkeletonAsset(skeleton) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkeletonJointCount,
        "skeleton payload rejects too many joints",
        failures);

    skeleton = skeletonAsset();
    skeleton.joints[1].parentIndex = -1;
    expect(
        full_engine::validateLoadedSkeletonAsset(skeleton) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkeletonHierarchy,
        "skeleton payload rejects multiple roots",
        failures);

    skeleton = skeletonAsset();
    skeleton.joints[0].parentIndex = 1;
    skeleton.joints[1].parentIndex = 0;
    expect(
        full_engine::validateLoadedSkeletonAsset(skeleton) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkeletonHierarchy,
        "skeleton payload rejects no root and parent after child",
        failures);

    skeleton = skeletonAsset();
    skeleton.joints[1].parentIndex = 9;
    expect(
        full_engine::validateLoadedSkeletonAsset(skeleton) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkeletonHierarchy,
        "skeleton payload rejects parent out of range",
        failures);

    skeleton = skeletonAsset();
    skeleton.joints[1].inverseBindPose[3] = std::nanf("");
    expect(
        full_engine::validateLoadedSkeletonAsset(skeleton) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkeletonJointData,
        "skeleton payload rejects non-finite inverse bind matrix",
        failures);
}

void testInvalidSkinnedMeshPayloads(std::vector<std::string>& failures)
{
    full_engine::LoadedSkinnedMeshAsset mesh = skinnedMeshAsset();
    mesh.skeletonAssetId = {};
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshSkeletonRef,
        "skinned mesh payload rejects default skeleton reference",
        failures);

    mesh = skinnedMeshAsset();
    mesh.vertices.clear();
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshVertices,
        "skinned mesh payload rejects empty vertices",
        failures);

    mesh = skinnedMeshAsset();
    mesh.indices.clear();
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshIndices,
        "skinned mesh payload rejects empty indices",
        failures);

    mesh = skinnedMeshAsset();
    mesh.indices.push_back(0);
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshIndices,
        "skinned mesh payload rejects non-triangle indices",
        failures);

    mesh = skinnedMeshAsset();
    mesh.indices[2] = 9;
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshIndices,
        "skinned mesh payload rejects out-of-range index",
        failures);

    mesh = skinnedMeshAsset();
    mesh.vertices[0].position[0] = std::nanf("");
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshVertexData,
        "skinned mesh payload rejects non-finite position",
        failures);

    mesh = skinnedMeshAsset();
    mesh.vertices[0].normal[1] = 0.0f;
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshVertexData,
        "skinned mesh payload rejects zero normal",
        failures);

    mesh = skinnedMeshAsset();
    mesh.vertices[0].uv0[0] = std::nanf("");
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshVertexData,
        "skinned mesh payload rejects non-finite uv0",
        failures);

    mesh = skinnedMeshAsset();
    mesh.vertices[0].colorLinear[2] = 2.0f;
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshVertexData,
        "skinned mesh payload rejects out-of-range color",
        failures);

    mesh = skinnedMeshAsset();
    mesh.vertices[0].jointIndices[0] = full_engine::kMaxLoadedSkeletonJoints;
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshVertexData,
        "skinned mesh payload rejects out-of-range joint index",
        failures);

    mesh = skinnedMeshAsset();
    mesh.vertices[0].jointWeights[0] = -0.1f;
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshWeights,
        "skinned mesh payload rejects negative weight",
        failures);

    mesh = skinnedMeshAsset();
    mesh.vertices[0].jointWeights[0] = std::nanf("");
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshWeights,
        "skinned mesh payload rejects non-finite weight",
        failures);

    mesh = skinnedMeshAsset();
    mesh.vertices[0].jointWeights[0] = 1.0f;
    mesh.vertices[0].jointWeights[1] = 1.0f;
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshWeights,
        "skinned mesh payload rejects weights not summing to one",
        failures);

    mesh = skinnedMeshAsset();
    mesh.localBounds.min[0] = 2.0f;
    expect(
        full_engine::validateLoadedSkinnedMeshAsset(mesh) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshBounds,
        "skinned mesh payload rejects inverted bounds",
        failures);
}

void testInvalidAnimationClipPayloads(std::vector<std::string>& failures)
{
    full_engine::LoadedAnimationClipAsset clip = animationClipAsset();
    clip.skeletonAssetId = {};
    expect(
        full_engine::validateLoadedAnimationClipAsset(clip) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipSkeletonRef,
        "animation clip payload rejects default skeleton reference",
        failures);

    clip = animationClipAsset();
    clip.durationSeconds = 0.0f;
    expect(
        full_engine::validateLoadedAnimationClipAsset(clip) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipDuration,
        "animation clip payload rejects zero duration",
        failures);

    clip = animationClipAsset();
    clip.ticksPerSecond = 0.0f;
    expect(
        full_engine::validateLoadedAnimationClipAsset(clip) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipTicksPerSecond,
        "animation clip payload rejects zero ticks per second",
        failures);

    clip = animationClipAsset();
    clip.tracks.clear();
    expect(
        full_engine::validateLoadedAnimationClipAsset(clip) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipTracks,
        "animation clip payload rejects empty tracks",
        failures);

    clip = animationClipAsset();
    clip.tracks[1].jointIndex = clip.tracks[0].jointIndex;
    expect(
        full_engine::validateLoadedAnimationClipAsset(clip) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipJointTrack,
        "animation clip payload rejects duplicate joint tracks",
        failures);

    clip = animationClipAsset();
    clip.tracks[0].jointIndex = full_engine::kMaxLoadedSkeletonJoints;
    expect(
        full_engine::validateLoadedAnimationClipAsset(clip) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipJointTrack,
        "animation clip payload rejects out-of-range joint track",
        failures);

    clip = animationClipAsset();
    clip.tracks[0].translations[1].timeSeconds = -1.0f;
    expect(
        full_engine::validateLoadedAnimationClipAsset(clip) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipKeyData,
        "animation clip payload rejects negative key time",
        failures);

    clip = animationClipAsset();
    clip.tracks[0].translations[1].timeSeconds = clip.durationSeconds + 1.0f;
    expect(
        full_engine::validateLoadedAnimationClipAsset(clip) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipKeyData,
        "animation clip payload rejects key beyond duration",
        failures);

    clip = animationClipAsset();
    clip.tracks[0].translations[0].value[1] = std::nanf("");
    expect(
        full_engine::validateLoadedAnimationClipAsset(clip) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipKeyData,
        "animation clip payload rejects non-finite translation",
        failures);

    clip = animationClipAsset();
    clip.tracks[0].rotations[0].value[3] = 2.0f;
    expect(
        full_engine::validateLoadedAnimationClipAsset(clip) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipKeyData,
        "animation clip payload rejects non-normalized rotation",
        failures);

    clip = animationClipAsset();
    clip.tracks[0].scales.clear();
    expect(
        full_engine::validateLoadedAnimationClipAsset(clip) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipKeyData,
        "animation clip payload rejects missing scale keys",
        failures);
}

void testPayloadDispatchAndInactiveSlots(std::vector<std::string>& failures)
{
    full_engine::LoadedAssetPayload meshPayload;
    meshPayload.kind = full_engine::AssetKind::Mesh;
    meshPayload.mesh = meshAsset();
    meshPayload.texture = {};
    meshPayload.material = {};
    expect(
        full_engine::validateLoadedAssetPayload(meshPayload) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "payload dispatch validates active mesh slot only",
        failures);

    full_engine::LoadedAssetPayload texturePayload;
    texturePayload.kind = full_engine::AssetKind::Texture;
    texturePayload.mesh = {};
    texturePayload.texture = textureAsset();
    texturePayload.material = {};
    expect(
        full_engine::validateLoadedAssetPayload(texturePayload) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "payload dispatch validates active texture slot only",
        failures);

    full_engine::LoadedAssetPayload materialPayload;
    materialPayload.kind = full_engine::AssetKind::Material;
    materialPayload.mesh = {};
    materialPayload.texture = {};
    materialPayload.material = materialAsset();
    expect(
        full_engine::validateLoadedAssetPayload(materialPayload) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "payload dispatch validates active material slot only",
        failures);

    full_engine::LoadedAssetPayload skeletonPayload;
    skeletonPayload.kind = full_engine::AssetKind::Skeleton;
    skeletonPayload.mesh = {};
    skeletonPayload.texture = {};
    skeletonPayload.material = {};
    skeletonPayload.skeleton = skeletonAsset();
    skeletonPayload.skinnedMesh = {};
    expect(
        full_engine::validateLoadedAssetPayload(skeletonPayload) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "payload dispatch validates active skeleton slot only",
        failures);

    full_engine::LoadedAssetPayload skinnedMeshPayload;
    skinnedMeshPayload.kind = full_engine::AssetKind::SkinnedMesh;
    skinnedMeshPayload.mesh = {};
    skinnedMeshPayload.texture = {};
    skinnedMeshPayload.material = {};
    skinnedMeshPayload.skeleton = {};
    skinnedMeshPayload.skinnedMesh = skinnedMeshAsset();
    expect(
        full_engine::validateLoadedAssetPayload(skinnedMeshPayload) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "payload dispatch validates active skinned mesh slot only",
        failures);

    full_engine::LoadedAssetPayload animationPayload;
    animationPayload.kind = full_engine::AssetKind::AnimationClip;
    animationPayload.mesh = {};
    animationPayload.texture = {};
    animationPayload.material = {};
    animationPayload.skeleton = {};
    animationPayload.skinnedMesh = {};
    animationPayload.animationClip = animationClipAsset();
    expect(
        full_engine::validateLoadedAssetPayload(animationPayload) ==
            full_engine::LoadedAssetPayloadValidationResult::Success,
        "payload dispatch validates active animation clip slot only",
        failures);

    full_engine::LoadedAssetPayload invalidPayload;
    invalidPayload.kind = full_engine::AssetKind::TerrainChunk;
    expect(
        full_engine::validateLoadedAssetPayload(invalidPayload) ==
            full_engine::LoadedAssetPayloadValidationResult::InvalidKind,
        "payload dispatch rejects unsupported kind",
        failures);
}

void testResultNames(std::vector<std::string>& failures)
{
    const full_engine::LoadedAssetPayloadValidationResult results[] = {
        full_engine::LoadedAssetPayloadValidationResult::Success,
        full_engine::LoadedAssetPayloadValidationResult::InvalidKind,
        full_engine::LoadedAssetPayloadValidationResult::InvalidAssetId,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMeshVertices,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMeshIndices,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMeshVertexData,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMeshBounds,
        full_engine::LoadedAssetPayloadValidationResult::InvalidTextureDimensions,
        full_engine::LoadedAssetPayloadValidationResult::InvalidTextureMipCount,
        full_engine::LoadedAssetPayloadValidationResult::InvalidTextureFormat,
        full_engine::LoadedAssetPayloadValidationResult::InvalidTextureSemantic,
        full_engine::LoadedAssetPayloadValidationResult::InvalidTextureColorSpace,
        full_engine::LoadedAssetPayloadValidationResult::InvalidTextureByteCount,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialModel,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialAlphaMode,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialTextureCount,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialTextureSlot,
        full_engine::LoadedAssetPayloadValidationResult::InvalidMaterialTextureRef,
        full_engine::LoadedAssetPayloadValidationResult::DuplicateMaterialTextureSlot,
        full_engine::LoadedAssetPayloadValidationResult::InvalidSkeletonJointCount,
        full_engine::LoadedAssetPayloadValidationResult::InvalidSkeletonHierarchy,
        full_engine::LoadedAssetPayloadValidationResult::InvalidSkeletonJointData,
        full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshSkeletonRef,
        full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshVertices,
        full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshIndices,
        full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshVertexData,
        full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshWeights,
        full_engine::LoadedAssetPayloadValidationResult::InvalidSkinnedMeshBounds,
        full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipSkeletonRef,
        full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipDuration,
        full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipTicksPerSecond,
        full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipTracks,
        full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipJointTrack,
        full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipKeyTimes,
        full_engine::LoadedAssetPayloadValidationResult::InvalidAnimationClipKeyData};

    for (const full_engine::LoadedAssetPayloadValidationResult result : results)
    {
        expect(
            std::string(full_engine::loadedAssetPayloadValidationResultName(result)) != "Unknown",
            "loaded payload validation result has stable name",
            failures);
    }
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testValidPayloads(failures);
    testDefaultIdsFail(failures);
    testInvalidMeshPayloads(failures);
    testInvalidTexturePayloads(failures);
    testInvalidMaterialPayloads(failures);
    testInvalidSkeletonPayloads(failures);
    testInvalidSkinnedMeshPayloads(failures);
    testInvalidAnimationClipPayloads(failures);
    testPayloadDispatchAndInactiveSlots(failures);
    testResultNames(failures);

    if (!failures.empty())
    {
        for (const std::string& failure : failures)
        {
            std::cerr << failure << '\n';
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
