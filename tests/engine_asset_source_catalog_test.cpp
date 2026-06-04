#include "engine/assets/AssetSourceCatalog.hpp"

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
        descriptor.mesh.localBounds.min[0] = -1.0f;
        descriptor.mesh.localBounds.min[1] = 0.0f;
        descriptor.mesh.localBounds.min[2] = -1.0f;
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
    case full_engine::AssetKind::Skeleton:
        descriptor.skeleton.jointCount = 2;
        break;
    case full_engine::AssetKind::SkinnedMesh:
        descriptor.skinnedMesh.vertexCount = 4;
        descriptor.skinnedMesh.indexCount = 6;
        descriptor.skinnedMesh.skeletonAssetId = asset(40);
        descriptor.skinnedMesh.localBounds.min[0] = -1.0f;
        descriptor.skinnedMesh.localBounds.min[1] = 0.0f;
        descriptor.skinnedMesh.localBounds.min[2] = -1.0f;
        descriptor.skinnedMesh.localBounds.max[0] = 1.0f;
        descriptor.skinnedMesh.localBounds.max[1] = 1.0f;
        descriptor.skinnedMesh.localBounds.max[2] = 1.0f;
        break;
    case full_engine::AssetKind::Unknown:
    case full_engine::AssetKind::TerrainChunk:
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

void testDefaultCatalog(std::vector<std::string>& failures)
{
    const full_engine::AssetSourceCatalog catalog;
    expect(catalog.sourceCount() == 0, "default source catalog is empty", failures);
    expect(catalog.sources().empty(), "default source snapshot is empty", failures);
    expect(catalog.findSource(asset(1)) == nullptr, "default source catalog finds no source", failures);
    expect(!catalog.contains(asset(1)), "default source catalog contains no source", failures);
}

void testValidation(std::vector<std::string>& failures)
{
    expect(
        full_engine::validateAssetSourceRecord(source(1, full_engine::AssetKind::Mesh, "mesh.bin")) ==
            full_engine::AssetSourceRecordValidationResult::Success,
        "valid mesh source validates",
        failures);
    expect(
        full_engine::validateAssetSourceRecord(source(5, full_engine::AssetKind::Skeleton, "skeleton.bin")) ==
            full_engine::AssetSourceRecordValidationResult::Success,
        "valid skeleton source validates",
        failures);
    expect(
        full_engine::validateAssetSourceRecord(source(6, full_engine::AssetKind::SkinnedMesh, "skinned.mesh")) ==
            full_engine::AssetSourceRecordValidationResult::Success,
        "valid skinned mesh source validates",
        failures);
    expect(
        full_engine::validateAssetSourceRecord(source(0, full_engine::AssetKind::Mesh, "mesh.bin")) ==
            full_engine::AssetSourceRecordValidationResult::InvalidAssetId,
        "source validation rejects default asset id",
        failures);
    expect(
        full_engine::validateAssetSourceRecord(source(2, full_engine::AssetKind::TerrainChunk, "terrain.bin")) ==
            full_engine::AssetSourceRecordValidationResult::InvalidKind,
        "source validation rejects non-loadable kind",
        failures);
    expect(
        full_engine::validateAssetSourceRecord(source(3, full_engine::AssetKind::Texture, "")) ==
            full_engine::AssetSourceRecordValidationResult::EmptyUri,
        "source validation rejects empty uri",
        failures);

    full_engine::AssetSourceRecord invalidDescriptor =
        source(4, full_engine::AssetKind::Mesh, "mesh.bin");
    invalidDescriptor.descriptor.mesh.vertexCount = 0;
    expect(
        full_engine::validateAssetSourceRecord(invalidDescriptor) ==
            full_engine::AssetSourceRecordValidationResult::InvalidDescriptor,
        "source validation rejects invalid active descriptor",
        failures);
}

void testAddFindUpdateRemoveAndClear(std::vector<std::string>& failures)
{
    full_engine::AssetSourceCatalog catalog;
    const full_engine::AssetSourceRecord mesh = source(1, full_engine::AssetKind::Mesh, "meshes/tree.mesh");
    const full_engine::AssetSourceRecord material =
        source(2, full_engine::AssetKind::Material, "materials/tree.mat");
    const full_engine::AssetSourceRecord texture =
        source(3, full_engine::AssetKind::Texture, "textures/tree.dds");
    const full_engine::AssetSourceRecord skeleton =
        source(4, full_engine::AssetKind::Skeleton, "skeletons/character.skel");
    const full_engine::AssetSourceRecord skinnedMesh =
        source(5, full_engine::AssetKind::SkinnedMesh, "meshes/character.mesh");

    expect(catalog.addSource(mesh) == full_engine::AssetSourceCatalogResult::Success, "mesh source add succeeds", failures);
    expect(catalog.addSource(material) == full_engine::AssetSourceCatalogResult::Success, "material source add succeeds", failures);
    expect(catalog.addSource(texture) == full_engine::AssetSourceCatalogResult::Success, "texture source add succeeds", failures);
    expect(catalog.addSource(skeleton) == full_engine::AssetSourceCatalogResult::Success, "skeleton source add succeeds", failures);
    expect(catalog.addSource(skinnedMesh) == full_engine::AssetSourceCatalogResult::Success, "skinned mesh source add succeeds", failures);
    expect(catalog.sourceCount() == 5, "source catalog stores five records", failures);
    expect(catalog.contains(asset(2)), "source catalog contains material id", failures);
    expect(catalog.contains(asset(5)), "source catalog contains skinned mesh id", failures);

    const full_engine::AssetSourceRecord* found = catalog.findSource(asset(1));
    expect(found != nullptr, "source catalog finds mesh source", failures);
    expect(found != nullptr && found->uri == "meshes/tree.mesh", "source catalog preserves uri", failures);

    const full_engine::AssetSourceRecord replacement =
        source(2, full_engine::AssetKind::Material, "materials/tree_v2.mat");
    expect(catalog.updateSource(replacement) == full_engine::AssetSourceCatalogResult::Success, "source update succeeds", failures);
    found = catalog.findSource(asset(2));
    expect(found != nullptr && found->uri == "materials/tree_v2.mat", "source update replaces uri", failures);
    expect(
        found != nullptr &&
            found->descriptor.material.model == full_engine::AssetSourceMaterialModel::Basic,
        "source update preserves descriptor",
        failures);

    expect(catalog.removeSource(asset(1)) == full_engine::AssetSourceCatalogResult::Success, "source remove succeeds", failures);
    expect(!catalog.contains(asset(1)), "source remove clears id", failures);
    expect(catalog.sourceCount() == 4, "source remove decrements count", failures);

    catalog.clear();
    expect(catalog.sourceCount() == 0, "source clear empties catalog", failures);
    expect(catalog.sources().empty(), "source clear empties snapshot", failures);
}

void testMutationFailuresDoNotChangeCatalog(std::vector<std::string>& failures)
{
    full_engine::AssetSourceCatalog catalog;
    const full_engine::AssetSourceRecord original = source(10, full_engine::AssetKind::Mesh, "mesh.bin");

    expect(catalog.addSource(original) == full_engine::AssetSourceCatalogResult::Success, "initial source add succeeds", failures);
    expect(catalog.addSource(original) == full_engine::AssetSourceCatalogResult::AlreadyExists, "duplicate source add reports already exists", failures);
    expect(catalog.updateSource(source(11, full_engine::AssetKind::Texture, "texture.dds")) == full_engine::AssetSourceCatalogResult::NotFound, "missing source update reports not found", failures);
    expect(catalog.removeSource(asset(11)) == full_engine::AssetSourceCatalogResult::NotFound, "missing source remove reports not found", failures);
    expect(catalog.removeSource(asset(0)) == full_engine::AssetSourceCatalogResult::InvalidArgument, "invalid source remove reports invalid argument", failures);
    expect(catalog.addSource(source(12, full_engine::AssetKind::Shader, "shader.bin")) == full_engine::AssetSourceCatalogResult::InvalidArgument, "invalid kind add reports invalid argument", failures);
    expect(catalog.updateSource(source(10, full_engine::AssetKind::Mesh, "")) == full_engine::AssetSourceCatalogResult::InvalidArgument, "empty uri update reports invalid argument", failures);
    full_engine::AssetSourceRecord invalidDescriptor = source(10, full_engine::AssetKind::Mesh, "mesh_v2.bin");
    invalidDescriptor.descriptor.mesh.indexCount = 0;
    expect(catalog.updateSource(invalidDescriptor) == full_engine::AssetSourceCatalogResult::InvalidArgument, "invalid descriptor update reports invalid argument", failures);

    const full_engine::AssetSourceRecord* found = catalog.findSource(asset(10));
    expect(catalog.sourceCount() == 1, "failed mutations preserve source count", failures);
    expect(found != nullptr && found->uri == "mesh.bin", "failed mutations preserve existing source", failures);
}

void testSourceSnapshotIsSortedByAssetId(std::vector<std::string>& failures)
{
    full_engine::AssetSourceCatalog catalog;
    (void)catalog.addSource(source(30, full_engine::AssetKind::Texture, "c.dds"));
    (void)catalog.addSource(source(10, full_engine::AssetKind::Mesh, "a.mesh"));
    (void)catalog.addSource(source(20, full_engine::AssetKind::Material, "b.mat"));

    const std::vector<full_engine::AssetSourceRecord> records = catalog.sources();
    expect(records.size() == 3, "source snapshot returns all records", failures);
    expect(records[0].id == asset(10), "source snapshot sorts first id", failures);
    expect(records[1].id == asset(20), "source snapshot sorts second id", failures);
    expect(records[2].id == asset(30), "source snapshot sorts third id", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;
    testDefaultCatalog(failures);
    testValidation(failures);
    testAddFindUpdateRemoveAndClear(failures);
    testMutationFailuresDoNotChangeCatalog(failures);
    testSourceSnapshotIsSortedByAssetId(failures);

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
