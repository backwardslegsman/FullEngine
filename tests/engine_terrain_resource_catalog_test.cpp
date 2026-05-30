#include "engine/renderer_integration/TerrainResourceCatalog.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
void expect(const bool condition, const char* message, std::vector<std::string>& failures)
{
    if (!condition)
    {
        failures.emplace_back(message);
    }
}

full_renderer::MeshHandle mesh(const std::uint32_t id)
{
    return full_renderer::MeshHandle{id};
}

full_renderer::MaterialHandle material(const std::uint32_t id)
{
    return full_renderer::MaterialHandle{id};
}

full_renderer::TextureHandle texture(const std::uint32_t id)
{
    return full_renderer::TextureHandle{id};
}

full_engine::TerrainChunkResourceDesc makeDesc(
    const full_engine::ChunkId& id,
    const std::uint32_t lodCount = 1)
{
    full_engine::TerrainChunkResourceDesc desc;
    desc.id = id;
    desc.lodCount = lodCount;
    for (std::uint32_t index = 0; index < lodCount && index < full_renderer::kMaxTerrainLodLevels; ++index)
    {
        desc.lods[index].mesh = mesh(10 + index);
        desc.lods[index].material = material(20 + index);
        desc.lods[index].maxDistanceMeters = static_cast<float>((index + 1) * 100);
    }
    return desc;
}

bool sameDescCore(const full_engine::TerrainChunkResourceDesc& lhs, const full_engine::TerrainChunkResourceDesc& rhs)
{
    if (!(lhs.id == rhs.id) || lhs.lodCount != rhs.lodCount || lhs.splatMap.id != rhs.splatMap.id)
    {
        return false;
    }

    for (std::uint32_t index = 0; index < lhs.lodCount; ++index)
    {
        if (lhs.lods[index].mesh.id != rhs.lods[index].mesh.id ||
            lhs.lods[index].material.id != rhs.lods[index].material.id ||
            lhs.lods[index].maxDistanceMeters != rhs.lods[index].maxDistanceMeters)
        {
            return false;
        }
    }

    return true;
}

void testValidDescriptors(std::vector<std::string>& failures)
{
    const full_engine::TerrainChunkResourceDesc oneLod = makeDesc({1, 0, 0}, 1);
    full_engine::TerrainChunkResourceDesc multiLod = makeDesc({2, 0, 0}, full_renderer::kMaxTerrainLodLevels);
    multiLod.splatMap = {};

    expect(
        full_engine::validateTerrainChunkResources(oneLod) == full_engine::TerrainResourceValidationResult::Success,
        "valid one lod descriptor succeeds",
        failures);
    expect(
        full_engine::validateTerrainChunkResources(multiLod) == full_engine::TerrainResourceValidationResult::Success,
        "valid multi lod descriptor succeeds",
        failures);
}

void testInvalidLodCounts(std::vector<std::string>& failures)
{
    full_engine::TerrainChunkResourceDesc zero = makeDesc({3, 0, 0}, 1);
    zero.lodCount = 0;

    full_engine::TerrainChunkResourceDesc tooMany = makeDesc({4, 0, 0}, full_renderer::kMaxTerrainLodLevels);
    tooMany.lodCount = full_renderer::kMaxTerrainLodLevels + 1;

    expect(
        full_engine::validateTerrainChunkResources(zero) == full_engine::TerrainResourceValidationResult::InvalidLodCount,
        "zero lod count is rejected",
        failures);
    expect(
        full_engine::validateTerrainChunkResources(tooMany) == full_engine::TerrainResourceValidationResult::InvalidLodCount,
        "too large lod count is rejected",
        failures);
}

void testInvalidHandlesAndDistances(std::vector<std::string>& failures)
{
    full_engine::TerrainChunkResourceDesc invalidMesh = makeDesc({5, 0, 0}, 1);
    invalidMesh.lods[0].mesh = {};

    full_engine::TerrainChunkResourceDesc invalidMaterial = makeDesc({6, 0, 0}, 1);
    invalidMaterial.lods[0].material = {};

    full_engine::TerrainChunkResourceDesc negativeDistance = makeDesc({7, 0, 0}, 1);
    negativeDistance.lods[0].maxDistanceMeters = -1.0f;

    full_engine::TerrainChunkResourceDesc infinityDistance = makeDesc({8, 0, 0}, 1);
    infinityDistance.lods[0].maxDistanceMeters = std::numeric_limits<float>::infinity();

    full_engine::TerrainChunkResourceDesc nanDistance = makeDesc({9, 0, 0}, 1);
    nanDistance.lods[0].maxDistanceMeters = std::numeric_limits<float>::quiet_NaN();

    full_engine::TerrainChunkResourceDesc unsorted = makeDesc({10, 0, 0}, 2);
    unsorted.lods[0].maxDistanceMeters = 200.0f;
    unsorted.lods[1].maxDistanceMeters = 100.0f;

    expect(
        full_engine::validateTerrainChunkResources(invalidMesh) == full_engine::TerrainResourceValidationResult::InvalidMesh,
        "invalid mesh handle is rejected",
        failures);
    expect(
        full_engine::validateTerrainChunkResources(invalidMaterial) == full_engine::TerrainResourceValidationResult::InvalidMaterial,
        "invalid material handle is rejected",
        failures);
    expect(
        full_engine::validateTerrainChunkResources(negativeDistance) == full_engine::TerrainResourceValidationResult::InvalidDistance,
        "negative distance is rejected",
        failures);
    expect(
        full_engine::validateTerrainChunkResources(infinityDistance) == full_engine::TerrainResourceValidationResult::InvalidDistance,
        "infinite distance is rejected",
        failures);
    expect(
        full_engine::validateTerrainChunkResources(nanDistance) == full_engine::TerrainResourceValidationResult::InvalidDistance,
        "nan distance is rejected",
        failures);
    expect(
        full_engine::validateTerrainChunkResources(unsorted) == full_engine::TerrainResourceValidationResult::UnsortedDistance,
        "unsorted distances are rejected",
        failures);
}

void testInactiveLodsIgnoredAndDefaultSplatAccepted(std::vector<std::string>& failures)
{
    full_engine::TerrainChunkResourceDesc desc = makeDesc({11, 0, 0}, 1);
    desc.splatMap = {};
    desc.lods[1].mesh = {};
    desc.lods[1].material = {};
    desc.lods[1].maxDistanceMeters = -100.0f;

    expect(
        full_engine::validateTerrainChunkResources(desc) == full_engine::TerrainResourceValidationResult::Success,
        "inactive lods are ignored and default splat is accepted",
        failures);
}

void testCatalogAddFindDuplicate(std::vector<std::string>& failures)
{
    full_engine::TerrainResourceCatalog catalog;
    const full_engine::TerrainChunkResourceDesc desc = makeDesc({12, 0, 0}, 2);

    expect(catalog.resourceCount() == 0, "catalog starts empty", failures);
    expect(
        catalog.addChunkResources(desc) == full_engine::TerrainResourceResult::Success,
        "valid descriptor can be added",
        failures);
    expect(catalog.resourceCount() == 1, "add increments catalog count", failures);
    expect(catalog.contains(desc.id), "catalog contains added id", failures);

    const full_engine::TerrainChunkResourceDesc* found = catalog.findChunkResources(desc.id);
    expect(found != nullptr, "added descriptor can be found", failures);
    expect(found != nullptr && sameDescCore(*found, desc), "found descriptor matches added descriptor", failures);
    expect(
        catalog.addChunkResources(desc) == full_engine::TerrainResourceResult::AlreadyExists,
        "duplicate add returns already exists",
        failures);
    expect(catalog.resourceCount() == 1, "duplicate add does not mutate count", failures);
}

void testCatalogUpdateAndMissingBehavior(std::vector<std::string>& failures)
{
    full_engine::TerrainResourceCatalog catalog;
    full_engine::TerrainChunkResourceDesc original = makeDesc({13, 0, 0}, 1);
    full_engine::TerrainChunkResourceDesc replacement = makeDesc({13, 0, 0}, 2);
    replacement.splatMap = texture(55);
    replacement.lods[0].mesh = mesh(101);

    expect(
        catalog.updateChunkResources(original) == full_engine::TerrainResourceResult::NotFound,
        "missing update returns not found",
        failures);
    expect(
        catalog.removeChunkResources(original.id) == full_engine::TerrainResourceResult::NotFound,
        "missing remove returns not found",
        failures);

    catalog.addChunkResources(original);

    expect(
        catalog.updateChunkResources(replacement) == full_engine::TerrainResourceResult::Success,
        "existing descriptor updates",
        failures);

    const full_engine::TerrainChunkResourceDesc* found = catalog.findChunkResources(original.id);
    expect(found != nullptr && sameDescCore(*found, replacement), "update replaces stored descriptor", failures);
    expect(
        catalog.removeChunkResources(original.id) == full_engine::TerrainResourceResult::Success,
        "existing descriptor removes",
        failures);
    expect(!catalog.contains(original.id), "removed descriptor is no longer contained", failures);
    expect(catalog.resourceCount() == 0, "remove decrements catalog count", failures);
}

void testInvalidMutationsDoNotChangeCatalog(std::vector<std::string>& failures)
{
    full_engine::TerrainResourceCatalog catalog;
    full_engine::TerrainChunkResourceDesc valid = makeDesc({14, 0, 0}, 1);
    full_engine::TerrainChunkResourceDesc invalidAdd = makeDesc({15, 0, 0}, 1);
    invalidAdd.lods[0].mesh = {};

    catalog.addChunkResources(valid);

    full_engine::TerrainChunkResourceDesc invalidUpdate = makeDesc({14, 0, 0}, 1);
    invalidUpdate.lods[0].material = {};

    expect(
        catalog.addChunkResources(invalidAdd) == full_engine::TerrainResourceResult::InvalidArgument,
        "invalid add is rejected",
        failures);
    expect(
        catalog.updateChunkResources(invalidUpdate) == full_engine::TerrainResourceResult::InvalidArgument,
        "invalid update is rejected",
        failures);
    expect(catalog.resourceCount() == 1, "invalid mutations do not change count", failures);

    const full_engine::TerrainChunkResourceDesc* found = catalog.findChunkResources(valid.id);
    expect(found != nullptr && sameDescCore(*found, valid), "invalid update preserves existing descriptor", failures);
}

void testClear(std::vector<std::string>& failures)
{
    full_engine::TerrainResourceCatalog catalog;
    catalog.addChunkResources(makeDesc({16, 0, 0}, 1));
    catalog.addChunkResources(makeDesc({17, 0, 0}, 1));

    catalog.clear();

    expect(catalog.resourceCount() == 0, "clear removes all descriptors", failures);
    expect(!catalog.contains({16, 0, 0}), "clear removes first descriptor", failures);
    expect(!catalog.contains({17, 0, 0}), "clear removes second descriptor", failures);
}
} // namespace

int main()
{
    std::vector<std::string> failures;

    testValidDescriptors(failures);
    testInvalidLodCounts(failures);
    testInvalidHandlesAndDistances(failures);
    testInactiveLodsIgnoredAndDefaultSplatAccepted(failures);
    testCatalogAddFindDuplicate(failures);
    testCatalogUpdateAndMissingBehavior(failures);
    testInvalidMutationsDoNotChangeCatalog(failures);
    testClear(failures);

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
