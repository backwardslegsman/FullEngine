#pragma once

#include "engine/assets/AssimpLoadedAssetImporter.hpp"
#include "engine/assets/AssetSourceDescriptor.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace full_engine
{
/** @brief Top-level status for probing an Assimp-supported scene file. */
enum class AssimpSceneProbeStatus
{
    Success,
    InvalidArgument,
    IoError,
    ParseError,
};

/** @brief First likely import blocker diagnosed by a probed scene. */
enum class AssimpSceneProbeBlocker
{
    None,
    FileOrParseFailed,
    AnimationChannelsWithoutSkinBones,
    MissingUv0,
    MissingTangents,
    Oversized16BitIndexContract,
    MissingSkeletonOrWeights,
    MissingAnimationTracks,
    SuspiciousBoundsOrAxis,
    MissingMaterialTextureRefs,
};

/** @brief Final recommendation from a skin-data availability audit. */
enum class AssimpSkinDataAuditBlocker
{
    SkinDataFound,
    NoSkinDataAnyConfig,
    SkinDataOnlyWithUnsafeConfig,
    ParseFailed,
    AnimationOnly,
    RigidNodeAnimationOnly,
};

/** @brief Copied diagnostics for one animation reported by Assimp. */
struct AssimpSceneProbeAnimation
{
    std::uint32_t index = 0;
    std::string name;
    double durationTicks = 0.0;
    double ticksPerSecond = 0.0;
    double durationSeconds = 0.0;
    std::uint32_t channelCount = 0;
    std::vector<std::string> channelNames;
};

/** @brief Maximum copied scene-node records retained by one probe result. */
constexpr std::uint32_t kMaxAssimpSceneProbeNodeRecords = 64;

/** @brief Maximum copied channel names retained per animation record. */
constexpr std::uint32_t kMaxAssimpSceneProbeAnimationChannelNames = 64;

/** @brief Bounded copied diagnostics for one Assimp scene node. */
struct AssimpSceneProbeNode
{
    std::string name;
    std::string parentName;
    std::uint32_t depth = 0;
    std::uint32_t childCount = 0;
    std::uint32_t meshIndexCount = 0;
    bool referencedByAnimationChannel = false;
    bool referencedByMeshBone = false;
    std::array<float, 16> localTransform = {};
};

/**
 * @brief CPU-only diagnostic summary of one Assimp scene.
 *
 * The probe copies counts and metadata only. It performs no payload conversion,
 * texture decoding, renderer calls, renderer handle lookup, resource creation,
 * catalog mutation, or async work. Bounds and root transform values are copied
 * exactly as Assimp reports them after the requested post-processing flags.
 */
struct AssimpSceneProbeResult
{
    AssimpSceneProbeStatus status = AssimpSceneProbeStatus::InvalidArgument;
    AssimpSceneProbeBlocker firstBlocker = AssimpSceneProbeBlocker::None;

    std::uint32_t meshCount = 0;
    std::uint32_t staticMeshCount = 0;
    std::uint32_t skinnedMeshCount = 0;
    std::uint32_t materialCount = 0;
    std::uint32_t embeddedTextureCount = 0;
    std::uint32_t materialTextureReferenceCount = 0;
    std::uint32_t animationCount = 0;

    std::uint64_t aggregateVertexCount = 0;
    std::uint64_t aggregateIndexCount = 0;
    std::uint64_t triangleFaceCount = 0;
    std::uint64_t nonTriangleFaceCount = 0;

    std::uint32_t meshesWithPositions = 0;
    std::uint32_t meshesWithNormals = 0;
    std::uint32_t meshesWithUv0 = 0;
    std::uint32_t meshesWithTangents = 0;
    std::uint32_t meshesWithVertexColors = 0;
    std::uint32_t meshesWithBones = 0;
    std::uint32_t meshesWithWeights = 0;
    std::uint32_t uniqueBoneNameCount = 0;

    std::uint32_t nodeCount = 0;
    std::uint32_t maxNodeDepth = 0;
    std::uint32_t rootChildCount = 0;
    std::uint32_t meshReferencingNodeCount = 0;
    std::uint32_t animationChannelNameCount = 0;
    std::uint32_t unmatchedAnimationChannelNameCount = 0;
    std::uint32_t candidateSkeletonNodeCount = 0;

    bool hasBounds = false;
    AssetSourceBounds aggregateBounds = {};
    std::array<float, 16> rootTransform = {};
    bool rootTransformIdentity = false;

    std::vector<AssimpSceneProbeAnimation> animations;
    std::vector<AssimpSceneProbeNode> nodes;
};

/** @brief One Assimp configuration tested by a skin-data availability audit. */
struct AssimpSkinDataAuditConfigRecord
{
    std::string name;
    AssimpSceneProbeStatus status = AssimpSceneProbeStatus::InvalidArgument;

    std::uint32_t meshCount = 0;
    std::uint32_t meshBoneCount = 0;
    std::uint32_t weightedMeshCount = 0;
    std::uint32_t uniqueBoneNameCount = 0;
    std::uint64_t totalVertexWeights = 0;
    std::uint32_t maxWeightsPerVertex = 0;

    std::uint32_t animationChannelNameCount = 0;
    std::uint32_t candidateSkeletonNodeCount = 0;

    std::uint32_t identityBindPoseCount = 0;
    std::uint32_t nonIdentityBindPoseCount = 0;
    std::uint32_t nonFiniteBindPoseCount = 0;

    std::uint32_t meshNodesUnderAnimatedCandidate = 0;
    std::uint32_t staticMeshNodesMatchingAnimationChannels = 0;
    std::uint32_t orphanMeshNodes = 0;

    bool safeForImportPolicy = true;
};

/**
 * @brief CPU-only skin-data availability audit across multiple Assimp configurations.
 *
 * The audit is diagnostic-only. It copies counts and recommendation metadata,
 * performs no payload conversion, does not decode textures, does not create
 * renderer resources, and does not mutate catalogs or importer options.
 */
struct AssimpSkinDataAuditResult
{
    AssimpSkinDataAuditBlocker firstBlocker = AssimpSkinDataAuditBlocker::ParseFailed;
    std::string recommendation;
    std::vector<AssimpSkinDataAuditConfigRecord> configs;
};

/** @brief Returns a stable diagnostic name for a probe status. */
const char* assimpSceneProbeStatusName(AssimpSceneProbeStatus status) noexcept;

/** @brief Returns a stable diagnostic name for a probe blocker. */
const char* assimpSceneProbeBlockerName(AssimpSceneProbeBlocker blocker) noexcept;

/** @brief Returns a stable diagnostic name for a skin-data audit blocker. */
const char* assimpSkinDataAuditBlockerName(AssimpSkinDataAuditBlocker blocker) noexcept;

/**
 * @brief Probes an Assimp-supported scene file without converting it to engine payloads.
 *
 * `uri` is treated as a direct filesystem path. `options` choose the same
 * Assimp post-processing flags used by the payload importer so callers can
 * compare probe diagnostics with import behavior. Returned vectors and strings
 * own their data and do not retain Assimp scene pointers.
 */
AssimpSceneProbeResult probeAssimpScene(
    const std::string& uri,
    const AssimpLoadedAssetImportOptions& options = {});

/**
 * @brief Audits whether an Assimp-supported file exposes recoverable skin data.
 *
 * `uri` is treated as a direct filesystem path. The audit runs a fixed matrix
 * of practical Assimp post-processing configurations and reports whether any
 * configuration exposes real `aiMesh::mBones` with nonzero vertex weights and
 * finite bind-pose offset matrices. It performs no renderer work and does not
 * treat node-derived identity bind poses as recovered skinning data.
 */
AssimpSkinDataAuditResult auditAssimpSkinDataAvailability(
    const std::string& uri,
    const AssimpLoadedAssetImportOptions& options = {});

/**
 * @brief Formats a probe result as deterministic JSON text.
 *
 * The formatter copies no scene data and performs no file IO. It is intended for
 * optional local characterization reports, tooling logs, and regression tests.
 * `sourceLabel` is copied into the `"source"` field when non-empty and may be a
 * path, asset nickname, or caller-owned diagnostic label.
 */
std::string formatAssimpSceneProbeResultJson(
    const AssimpSceneProbeResult& result,
    const std::string& sourceLabel = {});

/**
 * @brief Formats a skin-data audit as deterministic JSON text.
 *
 * The formatter performs no file IO. `sourceLabel` is copied into the
 * `"source"` field when non-empty.
 */
std::string formatAssimpSkinDataAuditResultJson(
    const AssimpSkinDataAuditResult& result,
    const std::string& sourceLabel = {});
} // namespace full_engine
