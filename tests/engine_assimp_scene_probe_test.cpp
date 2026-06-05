#include "engine/assets/AssimpSceneProbe.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef FULL_RENDERER_TEST_GLTF_FIXTURE_DIR
#define FULL_RENDERER_TEST_GLTF_FIXTURE_DIR "."
#endif

#ifndef FULL_RENDERER_TEST_OPTIONAL_ASSET_DIR
#define FULL_RENDERER_TEST_OPTIONAL_ASSET_DIR "."
#endif

namespace
{
void expect(const bool condition, const char* const message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

std::string gltfFixturePath(const char* const name)
{
    return std::string(FULL_RENDERER_TEST_GLTF_FIXTURE_DIR) + "/" + name;
}

std::string optionalAssetPath(const char* const relativePath)
{
    return std::string(FULL_RENDERER_TEST_OPTIONAL_ASSET_DIR) + "/" + relativePath;
}

void replaceAll(std::string& value, const std::string& from, const std::string& to)
{
    std::size_t position = 0;
    while ((position = value.find(from, position)) != std::string::npos)
    {
        value.replace(position, from.size(), to);
        position += to.size();
    }
}

std::string writeAnimationWithoutSkinFixture(std::vector<std::string>& failures)
{
    std::ifstream input(gltfFixturePath("skinned_triangle_animation.gltf"), std::ios::binary);
    if (!input)
    {
        failures.emplace_back("failed to open source gltf for animation-without-skin probe fixture");
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    std::string content = buffer.str();
    replaceAll(content, "\"skin\": 0", "\"extras\": { \"skinRemovedForProbe\": true }");
    replaceAll(content, "\"JOINTS_0\": 3,\n            \"WEIGHTS_0\": 4", "\"COLOR_0\": 3");
    replaceAll(content, "  \"skins\": [\n    {\n      \"skeleton\": 0,\n      \"joints\": [0, 1],\n      \"inverseBindMatrices\": 5\n    }\n  ],\n", "");

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "full_renderer_animation_without_skin_probe.gltf";
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        failures.emplace_back("failed to write animation-without-skin probe fixture");
        return {};
    }
    output << content;
    return path.string();
}

bool optionalProbeMatchesFilter(const char* const label)
{
    const char* const filter = std::getenv("FULL_RENDERER_OPTIONAL_FBX_PROBE_FILTER");
    return filter == nullptr || std::string(filter).empty() || std::string(filter) == label;
}

void writeProbeReportIfRequested(
    const char* const label,
    const std::string& path,
    const full_engine::AssimpSceneProbeResult& result,
    std::vector<std::string>& failures)
{
    const char* const reportDirectory = std::getenv("FULL_RENDERER_OPTIONAL_FBX_PROBE_REPORT_DIR");
    if (reportDirectory == nullptr || std::string(reportDirectory).empty())
    {
        return;
    }

    std::filesystem::create_directories(reportDirectory);
    const std::filesystem::path outputPath =
        std::filesystem::path(reportDirectory) / (std::string(label) + ".assimp_scene_probe.json");
    std::ofstream output(outputPath, std::ios::binary);
    if (!output)
    {
        failures.emplace_back(std::string("failed to open optional FBX probe report: ") + outputPath.string());
        return;
    }
    output << full_engine::formatAssimpSceneProbeResultJson(result, path);
    std::cout << label << ": wrote optional probe report " << outputPath.string() << '\n';
}

void writeSkinAuditReportIfRequested(
    const char* const label,
    const std::string& path,
    const full_engine::AssimpSkinDataAuditResult& result,
    std::vector<std::string>& failures)
{
    const char* const reportDirectory = std::getenv("FULL_RENDERER_OPTIONAL_FBX_PROBE_REPORT_DIR");
    if (reportDirectory == nullptr || std::string(reportDirectory).empty())
    {
        return;
    }

    std::filesystem::create_directories(reportDirectory);
    const std::filesystem::path outputPath =
        std::filesystem::path(reportDirectory) / (std::string(label) + ".assimp_skin_data_audit.json");
    std::ofstream output(outputPath, std::ios::binary);
    if (!output)
    {
        failures.emplace_back(std::string("failed to open optional FBX skin audit report: ") + outputPath.string());
        return;
    }
    output << full_engine::formatAssimpSkinDataAuditResultJson(result, path);
    std::cout << label << ": wrote optional skin audit report " << outputPath.string() << '\n';
}

void printProbeReport(const char* const label, const full_engine::AssimpSceneProbeResult& result)
{
    std::cout
        << label
        << ": status=" << full_engine::assimpSceneProbeStatusName(result.status)
        << ", blocker=" << full_engine::assimpSceneProbeBlockerName(result.firstBlocker)
        << ", meshes=" << result.meshCount
        << ", static=" << result.staticMeshCount
        << ", skinned=" << result.skinnedMeshCount
        << ", vertices=" << result.aggregateVertexCount
        << ", indices=" << result.aggregateIndexCount
        << ", materials=" << result.materialCount
        << ", materialTextures=" << result.materialTextureReferenceCount
        << ", animations=" << result.animationCount
        << ", bones=" << result.uniqueBoneNameCount
        << ", nodes=" << result.nodeCount
        << ", animatedNodes=" << result.candidateSkeletonNodeCount
        << ", unmatchedChannels=" << result.unmatchedAnimationChannelNameCount
        << ", uv0=" << result.meshesWithUv0
        << ", tangents=" << result.meshesWithTangents
        << '\n';
    if (!result.animations.empty())
    {
        std::cout
            << label
            << ": firstAnimation durationSeconds=" << result.animations[0].durationSeconds
            << ", ticksPerSecond=" << result.animations[0].ticksPerSecond
            << ", channels=" << result.animations[0].channelCount
            << '\n';
    }
    if (result.hasBounds)
    {
        std::cout
            << label
            << ": bounds min="
            << result.aggregateBounds.min[0] << ','
            << result.aggregateBounds.min[1] << ','
            << result.aggregateBounds.min[2]
            << " max="
            << result.aggregateBounds.max[0] << ','
            << result.aggregateBounds.max[1] << ','
            << result.aggregateBounds.max[2]
            << '\n';
    }
}

void printSkinAuditReport(const char* const label, const full_engine::AssimpSkinDataAuditResult& result)
{
    std::cout
        << label
        << ": skinAudit="
        << full_engine::assimpSkinDataAuditBlockerName(result.firstBlocker)
        << ", configs=" << result.configs.size()
        << ", recommendation=" << result.recommendation
        << '\n';
}

void testStaticGltfProbe(std::vector<std::string>& failures)
{
    full_engine::AssimpLoadedAssetImportOptions options;
    options.generateMissingTangents = true;
    const full_engine::AssimpSceneProbeResult result =
        full_engine::probeAssimpScene(gltfFixturePath("static_triangle.gltf"), options);

    expect(result.status == full_engine::AssimpSceneProbeStatus::Success, "static gltf probe succeeds", failures);
    expect(result.meshCount == 1, "static gltf reports one mesh", failures);
    expect(result.staticMeshCount == 1, "static gltf reports one static mesh", failures);
    expect(result.skinnedMeshCount == 0, "static gltf reports no skinned mesh", failures);
    expect(result.aggregateVertexCount == 3, "static gltf reports vertex count", failures);
    expect(result.aggregateIndexCount == 3, "static gltf reports index count", failures);
    expect(result.meshesWithNormals == 1, "static gltf reports normals", failures);
    expect(result.meshesWithUv0 == 1, "static gltf reports uv0", failures);
    expect(result.hasBounds, "static gltf reports bounds", failures);
}

void testSkinnedAnimationGltfProbe(std::vector<std::string>& failures)
{
    full_engine::AssimpLoadedAssetImportOptions options;
    options.generateMissingTangents = true;
    const full_engine::AssimpSceneProbeResult result =
        full_engine::probeAssimpScene(gltfFixturePath("skinned_triangle_animation.gltf"), options);

    expect(result.status == full_engine::AssimpSceneProbeStatus::Success, "skinned animation gltf probe succeeds", failures);
    expect(result.meshCount == 1, "skinned animation gltf reports one mesh", failures);
    expect(result.skinnedMeshCount == 1, "skinned animation gltf reports skinned mesh", failures);
    expect(result.meshesWithWeights == 1, "skinned animation gltf reports weights", failures);
    expect(result.uniqueBoneNameCount == 2, "skinned animation gltf reports two bones", failures);
    expect(result.animationCount == 1, "skinned animation gltf reports one animation", failures);
    expect(!result.animations.empty() && result.animations[0].channelCount == 2, "skinned animation gltf reports two animation channels", failures);
    expect(result.firstBlocker == full_engine::AssimpSceneProbeBlocker::MissingMaterialTextureRefs, "skinned animation gltf first blocker reports missing material texture refs", failures);
}

void testFailureStatusAndNames(std::vector<std::string>& failures)
{
    const full_engine::AssimpSceneProbeResult invalid =
        full_engine::probeAssimpScene("");
    expect(invalid.status == full_engine::AssimpSceneProbeStatus::InvalidArgument, "empty probe path is invalid", failures);
    expect(invalid.firstBlocker == full_engine::AssimpSceneProbeBlocker::FileOrParseFailed, "invalid path blocker is parse/file", failures);

    const full_engine::AssimpSceneProbeStatus statuses[] = {
        full_engine::AssimpSceneProbeStatus::Success,
        full_engine::AssimpSceneProbeStatus::InvalidArgument,
        full_engine::AssimpSceneProbeStatus::IoError,
        full_engine::AssimpSceneProbeStatus::ParseError,
    };
    for (const full_engine::AssimpSceneProbeStatus status : statuses)
    {
        expect(std::string(full_engine::assimpSceneProbeStatusName(status)) != "Unknown", "probe status has stable name", failures);
    }

    const full_engine::AssimpSceneProbeBlocker blockers[] = {
        full_engine::AssimpSceneProbeBlocker::None,
        full_engine::AssimpSceneProbeBlocker::FileOrParseFailed,
        full_engine::AssimpSceneProbeBlocker::AnimationChannelsWithoutSkinBones,
        full_engine::AssimpSceneProbeBlocker::MissingUv0,
        full_engine::AssimpSceneProbeBlocker::MissingTangents,
        full_engine::AssimpSceneProbeBlocker::Oversized16BitIndexContract,
        full_engine::AssimpSceneProbeBlocker::MissingSkeletonOrWeights,
        full_engine::AssimpSceneProbeBlocker::MissingAnimationTracks,
        full_engine::AssimpSceneProbeBlocker::SuspiciousBoundsOrAxis,
        full_engine::AssimpSceneProbeBlocker::MissingMaterialTextureRefs,
    };
    for (const full_engine::AssimpSceneProbeBlocker blocker : blockers)
    {
        expect(std::string(full_engine::assimpSceneProbeBlockerName(blocker)) != "Unknown", "probe blocker has stable name", failures);
    }

    const full_engine::AssimpSceneProbeResult jsonSource =
        full_engine::probeAssimpScene(gltfFixturePath("static_triangle.gltf"));
    const std::string json = full_engine::formatAssimpSceneProbeResultJson(jsonSource, "static \"triangle\"");
    expect(json.find("\"source\": \"static \\\"triangle\\\"\"") != std::string::npos, "probe json escapes source labels", failures);
    expect(json.find("\"status\": \"Success\"") != std::string::npos, "probe json includes status", failures);
    expect(json.find("\"meshes\": 1") != std::string::npos, "probe json includes mesh count", failures);
}

void testNodeAndAnimationDiagnostics(std::vector<std::string>& failures)
{
    full_engine::AssimpLoadedAssetImportOptions options;
    options.generateMissingTangents = true;
    const full_engine::AssimpSceneProbeResult result =
        full_engine::probeAssimpScene(gltfFixturePath("skinned_triangle_animation.gltf"), options);

    expect(result.nodeCount >= 3, "skinned animation gltf reports node count", failures);
    expect(result.rootChildCount > 0, "skinned animation gltf reports root child count", failures);
    expect(result.meshReferencingNodeCount == 1, "skinned animation gltf reports mesh node count", failures);
    expect(result.animationChannelNameCount == 2, "skinned animation gltf reports unique animation channel names", failures);
    expect(result.unmatchedAnimationChannelNameCount == 0, "skinned animation gltf channels match scene nodes", failures);
    expect(result.candidateSkeletonNodeCount == 2, "skinned animation gltf reports skeleton candidate nodes", failures);
    expect(result.nodes.size() >= 3, "skinned animation gltf copies bounded node records", failures);
    expect(!result.animations.empty() && result.animations[0].channelNames.size() == 2, "skinned animation gltf copies channel names", failures);
    expect(result.nodes.size() > 1 && result.nodes[1].referencedByAnimationChannel, "child node is marked animated", failures);
    expect(result.nodes.size() > 1 && result.nodes[1].referencedByMeshBone, "child node is marked as mesh bone", failures);

    const std::string animationWithoutSkin = writeAnimationWithoutSkinFixture(failures);
    if (!animationWithoutSkin.empty())
    {
        const full_engine::AssimpSceneProbeResult noSkin =
            full_engine::probeAssimpScene(animationWithoutSkin, options);
        expect(noSkin.status == full_engine::AssimpSceneProbeStatus::Success, "animation-without-skin fixture probes", failures);
        expect(noSkin.uniqueBoneNameCount == 0, "animation-without-skin fixture has no mesh bones", failures);
        expect(noSkin.animationChannelNameCount == 2, "animation-without-skin fixture preserves animated channels", failures);
        expect(noSkin.candidateSkeletonNodeCount == 2, "animation-without-skin fixture reports animated node candidates", failures);
        expect(noSkin.firstBlocker == full_engine::AssimpSceneProbeBlocker::AnimationChannelsWithoutSkinBones, "animation-without-skin fixture classifies animated nodes without skin bones", failures);
    }

    const std::string json = full_engine::formatAssimpSceneProbeResultJson(result, "skinned probe");
    expect(json.find("\"nodes\": ") != std::string::npos, "probe json includes node count", failures);
    expect(json.find("\"candidateSkeletonNodes\": 2") != std::string::npos, "probe json includes skeleton candidates", failures);
    expect(json.find("\"copiedChannelNames\"") != std::string::npos, "probe json includes channel names", failures);
    expect(json.find("\"referencedByAnimationChannel\": true") != std::string::npos, "probe json includes animated node flags", failures);
}

void testSkinDataAudit(std::vector<std::string>& failures)
{
    full_engine::AssimpLoadedAssetImportOptions options;
    options.generateMissingTangents = true;
    const full_engine::AssimpSkinDataAuditResult skinned =
        full_engine::auditAssimpSkinDataAvailability(
            gltfFixturePath("skinned_triangle_animation.gltf"),
            options);

    expect(skinned.firstBlocker == full_engine::AssimpSkinDataAuditBlocker::SkinDataFound, "skinned gltf audit reports skin data found", failures);
    expect(!skinned.recommendation.empty(), "skinned gltf audit has recommendation", failures);
    expect(skinned.configs.size() >= 5, "skinned gltf audit runs fixed config matrix", failures);
    expect(!skinned.configs.empty() && skinned.configs[0].meshBoneCount == 1, "skinned gltf audit records mesh bones", failures);
    expect(!skinned.configs.empty() && skinned.configs[0].weightedMeshCount == 1, "skinned gltf audit records weighted meshes", failures);
    expect(!skinned.configs.empty() && skinned.configs[0].totalVertexWeights > 0, "skinned gltf audit records total weights", failures);
    expect(!skinned.configs.empty() && skinned.configs[0].nonFiniteBindPoseCount == 0, "skinned gltf audit reports finite bind poses", failures);

    const std::string animationWithoutSkin = writeAnimationWithoutSkinFixture(failures);
    if (!animationWithoutSkin.empty())
    {
        const full_engine::AssimpSkinDataAuditResult noSkin =
            full_engine::auditAssimpSkinDataAvailability(animationWithoutSkin, options);
        expect(noSkin.firstBlocker != full_engine::AssimpSkinDataAuditBlocker::SkinDataFound, "animation-without-skin audit does not report skin data", failures);
        expect(!noSkin.recommendation.empty(), "animation-without-skin audit has recommendation", failures);
        expect(!noSkin.configs.empty() && noSkin.configs[0].animationChannelNameCount == 2, "animation-without-skin audit records animation channels", failures);
        expect(!noSkin.configs.empty() && noSkin.configs[0].weightedMeshCount == 0, "animation-without-skin audit records missing weights", failures);
    }

    const std::string json = full_engine::formatAssimpSkinDataAuditResultJson(skinned, "skin audit");
    expect(json.find("\"firstBlocker\": \"SkinDataFound\"") != std::string::npos, "skin audit json includes blocker", failures);
    expect(json.find("\"recommendation\"") != std::string::npos, "skin audit json includes recommendation", failures);
    expect(json.find("\"configs\"") != std::string::npos, "skin audit json includes config records", failures);
    expect(json.find("\"totalVertexWeights\"") != std::string::npos, "skin audit json includes total weights", failures);
    expect(json.find("\"identityBindPoseCount\"") != std::string::npos, "skin audit json includes bind pose counters", failures);

    const full_engine::AssimpSkinDataAuditBlocker blockers[] = {
        full_engine::AssimpSkinDataAuditBlocker::SkinDataFound,
        full_engine::AssimpSkinDataAuditBlocker::NoSkinDataAnyConfig,
        full_engine::AssimpSkinDataAuditBlocker::SkinDataOnlyWithUnsafeConfig,
        full_engine::AssimpSkinDataAuditBlocker::ParseFailed,
        full_engine::AssimpSkinDataAuditBlocker::AnimationOnly,
        full_engine::AssimpSkinDataAuditBlocker::RigidNodeAnimationOnly};
    for (const full_engine::AssimpSkinDataAuditBlocker blocker : blockers)
    {
        expect(std::string(full_engine::assimpSkinDataAuditBlockerName(blocker)) != "Unknown", "skin audit blocker has stable name", failures);
    }
}

void probeOptionalFbx(
    const char* const label,
    const char* const relativePath,
    std::vector<std::string>& failures)
{
    if (!optionalProbeMatchesFilter(label))
    {
        std::cout << label << ": skipped by FULL_RENDERER_OPTIONAL_FBX_PROBE_FILTER\n";
        return;
    }

    const std::string path = optionalAssetPath(relativePath);
    if (!std::filesystem::exists(path))
    {
        std::cout << label << ": skipped missing optional asset " << path << '\n';
        return;
    }

    full_engine::AssimpLoadedAssetImportOptions options;
    options.generateMissingNormals = true;
    options.generateMissingTangents = true;
    const full_engine::AssimpSceneProbeResult result =
        full_engine::probeAssimpScene(path, options);
    printProbeReport(label, result);
    writeProbeReportIfRequested(label, path, result, failures);
    const full_engine::AssimpSkinDataAuditResult skinAudit =
        full_engine::auditAssimpSkinDataAvailability(path, options);
    printSkinAuditReport(label, skinAudit);
    writeSkinAuditReportIfRequested(label, path, skinAudit, failures);
    expect(result.status == full_engine::AssimpSceneProbeStatus::Success, "optional fbx parses through Assimp", failures);
    expect(result.meshCount > 0 || result.animationCount > 0, "optional fbx reports mesh or animation data", failures);
    expect(!skinAudit.configs.empty(), "optional fbx skin audit records config results", failures);
    expect(!skinAudit.recommendation.empty(), "optional fbx skin audit emits recommendation", failures);
    expect(skinAudit.configs[0].status == full_engine::AssimpSceneProbeStatus::Success, "optional fbx baseline skin audit parses", failures);
}

void testOptionalFbxSmoke(std::vector<std::string>& failures)
{
    const char* const enabled = std::getenv("FULL_RENDERER_RUN_OPTIONAL_FBX_PROBES");
    if (enabled == nullptr || std::string(enabled) != "1")
    {
        std::cout << "Optional FBX probes skipped; set FULL_RENDERER_RUN_OPTIONAL_FBX_PROBES=1 to run local Sponza/Knight characterization.\n";
        return;
    }

    probeOptionalFbx(
        "Sponza_Yup",
        "main_sponza/main_sponza/NewSponza_Main_Yup_003.fbx",
        failures);
    probeOptionalFbx(
        "Sponza_Zup",
        "main_sponza/main_sponza/NewSponza_Main_Zup_003.fbx",
        failures);
    probeOptionalFbx(
        "Knight_USD",
        "pkg_e_knight_anim/pkg_e_knight_anim/Exports/FBX/Knight_USD_002.fbx",
        failures);
    probeOptionalFbx(
        "Knight_AnimationOnly",
        "pkg_e_knight_anim/pkg_e_knight_anim/Exports/FBX/Knight_Animation_Data_Only_002.fbx",
        failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testStaticGltfProbe(failures);
    testSkinnedAnimationGltfProbe(failures);
    testNodeAndAnimationDiagnostics(failures);
    testSkinDataAudit(failures);
    testFailureStatusAndNames(failures);
    testOptionalFbxSmoke(failures);

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
