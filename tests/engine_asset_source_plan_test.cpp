#include "engine/renderer_integration/TerrainManifestAssetSourcePlan.hpp"

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

full_engine::AssetSourceDescriptor descriptorForKind(const full_engine::AssetKind kind)
{
    full_engine::AssetSourceDescriptor descriptor;
    switch (kind)
    {
    case full_engine::AssetKind::Mesh:
        descriptor.mesh.vertexCount = 4;
        descriptor.mesh.indexCount = 6;
        descriptor.mesh.localBounds.max[0] = 1.0f;
        descriptor.mesh.localBounds.max[1] = 1.0f;
        descriptor.mesh.localBounds.max[2] = 1.0f;
        break;
    case full_engine::AssetKind::Material:
        descriptor.material.model = full_engine::AssetSourceMaterialModel::Basic;
        descriptor.material.alphaMode = full_engine::AssetSourceMaterialAlphaMode::Opaque;
        break;
    case full_engine::AssetKind::Texture:
        descriptor.texture.width = 64;
        descriptor.texture.height = 64;
        descriptor.texture.mipCount = 1;
        descriptor.texture.format = full_engine::AssetSourceTextureFormat::Rgba8;
        descriptor.texture.semantic = full_engine::AssetSourceTextureSemantic::Color;
        descriptor.texture.colorSpace = full_engine::AssetSourceTextureColorSpace::Srgb;
        break;
    case full_engine::AssetKind::Unknown:
    case full_engine::AssetKind::TerrainChunk:
    case full_engine::AssetKind::Skeleton:
    case full_engine::AssetKind::SkinnedMesh:
    case full_engine::AssetKind::Shader:
        break;
    }
    return descriptor;
}

full_engine::AssetSourceRecord source(
    const std::uint64_t id,
    const full_engine::AssetKind kind,
    const char* const uri)
{
    full_engine::AssetSourceRecord record;
    record.id = asset(id);
    record.kind = kind;
    record.uri = uri;
    record.descriptor = descriptorForKind(kind);
    return record;
}

full_engine::TerrainManifestAssetLoadRequest request(
    const std::uint64_t id,
    const full_engine::AssetKind kind) noexcept
{
    full_engine::TerrainManifestAssetLoadRequest result;
    result.id = asset(id);
    result.kind = kind;
    return result;
}

full_engine::AssetSourceCatalog makeSources()
{
    full_engine::AssetSourceCatalog sources;
    (void)sources.addSource(source(1, full_engine::AssetKind::Mesh, "mesh.bin"));
    (void)sources.addSource(source(2, full_engine::AssetKind::Material, "material.bin"));
    (void)sources.addSource(source(3, full_engine::AssetKind::Texture, "texture.bin"));
    (void)sources.addSource(source(4, full_engine::AssetKind::Texture, "other_texture.bin"));
    return sources;
}

void testRequestsMapInSourceOrder(std::vector<std::string>& failures)
{
    const full_engine::AssetSourceCatalog sources = makeSources();
    const std::vector<full_engine::TerrainManifestAssetLoadRequest> requests = {
        request(3, full_engine::AssetKind::Texture),
        request(1, full_engine::AssetKind::Mesh),
        request(2, full_engine::AssetKind::Material),
    };

    const full_engine::TerrainManifestAssetSourceRequestPlan plan =
        full_engine::buildTerrainManifestAssetSourceRequestPlan(
            requests.data(),
            requests.size(),
            sources);

    expect(plan.records.size() == 3, "source plan returns one record per request", failures);
    expect(plan.summary.mappedCount == 3, "source plan counts mapped requests", failures);
    expect(plan.records[0].request.id == asset(3), "source plan preserves first request order", failures);
    expect(plan.records[1].request.id == asset(1), "source plan preserves second request order", failures);
    expect(plan.records[2].request.id == asset(2), "source plan preserves third request order", failures);
    expect(plan.records[0].source.uri == "texture.bin", "source plan copies texture uri", failures);
    expect(plan.records[1].source.uri == "mesh.bin", "source plan copies mesh uri", failures);
    expect(plan.records[2].source.uri == "material.bin", "source plan copies material uri", failures);
    expect(
        plan.records[0].source.descriptor.texture.width == 64,
        "source plan copies texture descriptor",
        failures);
    expect(
        plan.records[1].source.descriptor.mesh.vertexCount == 4,
        "source plan copies mesh descriptor",
        failures);
    expect(
        plan.records[2].source.descriptor.material.model == full_engine::AssetSourceMaterialModel::Basic,
        "source plan copies material descriptor",
        failures);
}

void testMissingAndInvalidRequests(std::vector<std::string>& failures)
{
    const full_engine::AssetSourceCatalog sources = makeSources();
    const std::vector<full_engine::TerrainManifestAssetLoadRequest> requests = {
        request(9, full_engine::AssetKind::Texture),
        request(4, full_engine::AssetKind::Mesh),
        request(0, full_engine::AssetKind::Mesh),
        request(1, full_engine::AssetKind::TerrainChunk),
    };

    const full_engine::TerrainManifestAssetSourceRequestPlan plan =
        full_engine::buildTerrainManifestAssetSourceRequestPlan(
            requests.data(),
            requests.size(),
            sources);

    expect(plan.records.size() == 4, "mixed source plan returns records for valid pointer input", failures);
    expect(plan.summary.missingSourceCount == 2, "source plan counts missing source records", failures);
    expect(plan.summary.invalidRequestCount == 2, "source plan counts invalid requests", failures);
    expect(plan.records[0].status == full_engine::TerrainManifestAssetSourceRequestStatus::MissingSource, "missing id reports missing source", failures);
    expect(plan.records[1].status == full_engine::TerrainManifestAssetSourceRequestStatus::MissingSource, "wrong source kind reports missing source", failures);
    expect(plan.records[2].status == full_engine::TerrainManifestAssetSourceRequestStatus::InvalidRequest, "default id reports invalid request", failures);
    expect(plan.records[3].status == full_engine::TerrainManifestAssetSourceRequestStatus::InvalidRequest, "unsupported request kind reports invalid request", failures);
}

void testDuplicateRequestsProduceDuplicateDiagnostics(std::vector<std::string>& failures)
{
    full_engine::AssetSourceCatalog sources = makeSources();
    full_engine::TerrainManifestAssetLoadRequestPlan requests;
    requests.requests.push_back(request(1, full_engine::AssetKind::Mesh));
    requests.requests.push_back(request(1, full_engine::AssetKind::Mesh));

    const full_engine::TerrainManifestAssetSourceRequestPlan plan =
        full_engine::buildTerrainManifestAssetSourceRequestPlan(requests, sources);

    expect(plan.records.size() == 2, "duplicate source plan returns duplicate records", failures);
    expect(plan.summary.mappedCount == 2, "duplicate source plan counts both mapped requests", failures);
    expect(plan.records[0].source.uri == "mesh.bin", "duplicate source plan maps first request", failures);
    expect(plan.records[1].source.uri == "mesh.bin", "duplicate source plan maps second request", failures);
    expect(sources.sourceCount() == 4, "source plan does not mutate source catalog", failures);
}

void testNullRequestArray(std::vector<std::string>& failures)
{
    const full_engine::AssetSourceCatalog sources = makeSources();
    const full_engine::TerrainManifestAssetSourceRequestPlan empty =
        full_engine::buildTerrainManifestAssetSourceRequestPlan(nullptr, 0, sources);
    const full_engine::TerrainManifestAssetSourceRequestPlan invalid =
        full_engine::buildTerrainManifestAssetSourceRequestPlan(nullptr, 3, sources);

    expect(empty.records.empty(), "null empty source request array has no records", failures);
    expect(empty.summary.invalidRequestCount == 0, "null empty source request array has zero invalid count", failures);
    expect(invalid.records.empty(), "null non-empty source request array has no records", failures);
    expect(invalid.summary.invalidRequestCount == 3, "null non-empty source request array counts invalid requests", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testRequestsMapInSourceOrder(failures);
    testMissingAndInvalidRequests(failures);
    testDuplicateRequestsProduceDuplicateDiagnostics(failures);
    testNullRequestArray(failures);

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
