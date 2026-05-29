#include "renderer/terrain/TerrainSystem.hpp"
#include "renderer/scene/Shadow.hpp"
#include "renderer/terrain/TerrainLod.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
void expect(const bool condition, const char* message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

full_renderer::Aabb makeBounds(
    const float minX,
    const float minY,
    const float minZ,
    const float maxX,
    const float maxY,
    const float maxZ)
{
    full_renderer::Aabb bounds;
    bounds.min[0] = minX;
    bounds.min[1] = minY;
    bounds.min[2] = minZ;
    bounds.max[0] = maxX;
    bounds.max[1] = maxY;
    bounds.max[2] = maxZ;
    return bounds;
}

void identity(float out[16])
{
    for (int index = 0; index < 16; ++index)
    {
        out[index] = 0.0f;
    }
    out[0] = 1.0f;
    out[5] = 1.0f;
    out[10] = 1.0f;
    out[15] = 1.0f;
}

full_renderer::TerrainLodDesc lod(
    const std::uint32_t meshId,
    const std::uint32_t materialId,
    const float maxDistance)
{
    full_renderer::TerrainLodDesc desc;
    desc.mesh.id = meshId;
    desc.material.id = materialId;
    desc.maxDistanceMeters = maxDistance;
    return desc;
}

full_renderer::TerrainChunkDesc chunkDesc(
    const full_renderer::Aabb& bounds,
    const full_renderer::TerrainLodDesc* lods,
    const std::uint32_t lodCount)
{
    full_renderer::TerrainChunkDesc desc;
    desc.bounds = bounds;
    desc.lods = lods;
    desc.lodCount = lodCount;
    return desc;
}

full_renderer::RenderViewDesc identityView()
{
    full_renderer::RenderViewDesc view;
    identity(view.view);
    identity(view.projection);
    return view;
}

full_renderer::RenderViewDesc wideView()
{
    full_renderer::RenderViewDesc view = identityView();
    view.projection[0] = 0.1f;
    view.projection[5] = 0.1f;
    view.projection[10] = 0.1f;
    return view;
}

full_renderer::RenderViewDesc rebasedWideView(const float originX, const float originZ)
{
    full_renderer::RenderViewDesc view = wideView();
    view.view[12] = -originX;
    view.view[14] = -originZ;
    return view;
}

full_renderer::TerrainSubmitDesc submitDesc(
    const full_renderer::TerrainChunkHandle* chunks,
    const std::uint32_t chunkCount,
    const float cameraX,
    const float cameraY,
    const float cameraZ)
{
    full_renderer::TerrainSubmitDesc desc;
    desc.chunks = chunks;
    desc.chunkCount = chunkCount;
    desc.cameraPositionWorld[0] = cameraX;
    desc.cameraPositionWorld[1] = cameraY;
    desc.cameraPositionWorld[2] = cameraZ;
    desc.debug.captureChunkInfo = true;
    desc.debug.captureBatchInfo = true;
    return desc;
}

full_renderer::DirectionalLightDesc topDownLight()
{
    full_renderer::DirectionalLightDesc light;
    light.directionWorld[0] = 0.0f;
    light.directionWorld[1] = 1.0f;
    light.directionWorld[2] = 0.0f;
    light.colorLinear[0] = 1.0f;
    light.colorLinear[1] = 1.0f;
    light.colorLinear[2] = 1.0f;
    light.intensity = 1.0f;
    return light;
}

full_renderer::DirectionalShadowDesc shadowDesc(const float extent)
{
    full_renderer::DirectionalShadowDesc shadow;
    shadow.enabled = true;
    shadow.mapResolution = 1024;
    shadow.extentMeters = extent;
    shadow.centerWorld[0] = 0.0f;
    shadow.centerWorld[1] = 0.0f;
    shadow.centerWorld[2] = 0.0f;
    return shadow;
}

void lifecycleAndValidation(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc lods[] = {
        lod(1, 1, 10.0f),
        lod(2, 2, 100.0f),
    };

    const full_renderer::TerrainChunkHandle chunk = system.createChunk(
        chunkDesc(makeBounds(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f), lods, 2));
    expect(full_renderer::isValid(chunk), "valid chunk creation succeeds", failures);
    expect(system.getStats().liveChunks == 1, "live chunk count updates after create", failures);

    full_renderer::TerrainChunkDesc bad = chunkDesc(makeBounds(1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f), lods, 2);
    expect(!full_renderer::isValid(system.createChunk(bad)), "invalid bounds are rejected", failures);

    bad = chunkDesc(makeBounds(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f), lods, 0);
    expect(!full_renderer::isValid(system.createChunk(bad)), "zero LOD count is rejected", failures);

    const full_renderer::TerrainLodDesc tooManyLods[] = {
        lod(1, 1, 10.0f),
        lod(2, 2, 20.0f),
        lod(3, 3, 30.0f),
        lod(4, 4, 40.0f),
        lod(5, 5, 50.0f),
    };
    bad = chunkDesc(makeBounds(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f), tooManyLods, 5);
    expect(!full_renderer::isValid(system.createChunk(bad)), "more than four LODs are rejected", failures);

    const full_renderer::TerrainLodDesc unsorted[] = {
        lod(1, 1, 100.0f),
        lod(2, 2, 10.0f),
    };
    bad = chunkDesc(makeBounds(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f), unsorted, 2);
    expect(!full_renderer::isValid(system.createChunk(bad)), "unsorted LOD distances are rejected", failures);

    const full_renderer::TerrainLodDesc negativeDistance[] = {
        lod(1, 1, -1.0f),
    };
    bad = chunkDesc(makeBounds(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f), negativeDistance, 1);
    expect(!full_renderer::isValid(system.createChunk(bad)), "negative LOD distances are rejected", failures);

    const full_renderer::TerrainLodDesc nonFiniteDistance[] = {
        lod(1, 1, std::numeric_limits<float>::quiet_NaN()),
    };
    bad = chunkDesc(makeBounds(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f), nonFiniteDistance, 1);
    expect(!full_renderer::isValid(system.createChunk(bad)), "non-finite LOD distances are rejected", failures);

    const full_renderer::TerrainLodDesc invalidHandle[] = {
        lod(0, 1, 10.0f),
    };
    bad = chunkDesc(makeBounds(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f), invalidHandle, 1);
    expect(!full_renderer::isValid(system.createChunk(bad)), "invalid LOD handles are rejected", failures);

    system.destroyChunk(chunk);
    expect(system.getStats().liveChunks == 0, "live chunk count updates after destroy", failures);
    expect(system.getStats().residentChunks == 0, "resident chunk count mirrors live chunks", failures);
    expect(system.getStats().nonResidentChunks == 1, "destroyed chunk slot becomes nonresident", failures);

    const full_renderer::TerrainChunkHandle reused = system.createChunk(
        chunkDesc(makeBounds(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f), lods, 2));
    expect(full_renderer::isValid(reused), "chunk slot can be reused", failures);
    expect(reused.id == chunk.id && reused.generation != chunk.generation, "reused slot gets new generation", failures);
    expect(system.getStats().allocatedChunkSlots == 1, "slot reuse does not grow allocation slots", failures);
    expect(system.getStats().totalChunkSlotReuseCount == 1, "slot reuse is counted", failures);
}

void updateChunkReplacesDescriptorWithoutChangingHandle(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc originalLods[] = {
        lod(11, 21, 10.0f),
    };
    const full_renderer::TerrainLodDesc updatedLods[] = {
        lod(12, 22, 25.0f),
    };

    const full_renderer::TerrainChunkHandle chunk = system.createChunk(
        chunkDesc(makeBounds(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f), originalLods, 1));
    expect(full_renderer::isValid(chunk), "terrain chunk for update is created", failures);

    full_renderer::TerrainChunkDesc updated =
        chunkDesc(makeBounds(2.0f, -0.25f, -0.5f, 3.0f, 0.75f, 0.5f), updatedLods, 1);
    updated.splatMap.id = 77;
    expect(system.updateChunk(chunk, updated) == full_renderer::RendererResult::Success,
        "terrain chunk update succeeds",
        failures);
    expect(system.getStats().liveChunks == 1, "terrain chunk update preserves live count", failures);
    expect(system.getStats().chunksUpdatedSinceLastSubmit == 1,
        "terrain chunk update is counted before submission",
        failures);
    expect(system.getStats().totalChunkUpdateCount == 1,
        "terrain chunk lifetime update count increments",
        failures);

    full_renderer::TerrainChunkDesc invalid = updated;
    invalid.bounds.min[0] = 4.0f;
    invalid.bounds.max[0] = 3.0f;
    expect(system.updateChunk(chunk, invalid) == full_renderer::RendererResult::InvalidDescriptor,
        "invalid terrain chunk update descriptor is rejected",
        failures);

    const full_renderer::TerrainChunkHandle chunks[] = {chunk};
    std::vector<float> modelMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> batches;
    const full_renderer::RendererResult result = system.buildDrawBatches(
        wideView(),
        submitDesc(chunks, 1, 2.5f, 0.0f, 0.0f),
        modelMatrices,
        batches);
    expect(result == full_renderer::RendererResult::Success, "updated terrain chunk submission succeeds", failures);
    expect(batches.size() == 1, "updated terrain chunk produces one batch", failures);
    if (!batches.empty())
    {
        expect(batches[0].mesh.id == 12 && batches[0].material.id == 22,
            "updated terrain chunk uses replacement LOD resources",
            failures);
        expect(batches[0].splatMap.id == 77,
            "updated terrain chunk uses replacement splat map handle",
            failures);
        expect(batches[0].bounds.min[0] == 2.0f && batches[0].bounds.max[0] == 3.0f,
            "updated terrain chunk uses replacement bounds",
            failures);
    }
    expect(system.getStats().chunksUpdatedSinceLastSubmit == 1,
        "terrain submission reports updates since the previous submit",
        failures);

    system.destroyChunk(chunk);
    expect(system.updateChunk(chunk, updated) == full_renderer::RendererResult::InvalidArgument,
        "stale terrain chunk update handle is rejected",
        failures);
}

void lodThresholdSelectionIsDeterministic(int& failures)
{
    const full_renderer::TerrainLodDesc lods[] = {
        lod(11, 21, 10.0f),
        lod(12, 22, 25.0f),
        lod(13, 23, 100.0f),
    };

    expect(full_renderer::terrain::hasValidLodCount(1), "one LOD is a valid count", failures);
    expect(full_renderer::terrain::hasValidLodCount(full_renderer::kMaxTerrainLodLevels),
        "four LODs is a valid count",
        failures);
    expect(!full_renderer::terrain::hasValidLodCount(0), "zero LODs is an invalid count", failures);
    expect(!full_renderer::terrain::hasValidLodCount(full_renderer::kMaxTerrainLodLevels + 1U),
        "five LODs is an invalid count",
        failures);
    expect(full_renderer::terrain::selectLod(lods, 3, 0.0f) == 0, "near distance selects LOD 0", failures);
    expect(full_renderer::terrain::selectLod(lods, 3, 10.0f) == 0,
        "exact first threshold remains LOD 0",
        failures);
    expect(full_renderer::terrain::selectLod(lods, 3, 10.0001f) == 1,
        "distance past first threshold selects LOD 1",
        failures);
    expect(full_renderer::terrain::selectLod(lods, 3, 25.0f) == 1,
        "exact second threshold remains LOD 1",
        failures);
    expect(full_renderer::terrain::selectLod(lods, 3, 200.0f) == 2,
        "distance beyond final threshold selects final LOD",
        failures);
    expect(full_renderer::terrain::selectLod(lods, 0, 1.0f) == full_renderer::kInvalidTerrainLodIndex,
        "invalid LOD count returns invalid sentinel",
        failures);
    expect(full_renderer::terrain::selectLod(lods, 3, std::numeric_limits<float>::quiet_NaN()) ==
            full_renderer::kInvalidTerrainLodIndex,
        "non-finite distance returns invalid sentinel",
        failures);

    const std::uint32_t first = full_renderer::terrain::selectLod(lods, 3, 42.0f);
    const std::uint32_t second = full_renderer::terrain::selectLod(lods, 3, 42.0f);
    expect(first == second, "repeated LOD selection is deterministic", failures);
}

void submissionCullingLodAndDebug(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc lods[] = {
        lod(11, 21, 1.0f),
        lod(12, 22, 100.0f),
    };

    const full_renderer::TerrainChunkHandle visibleChunk = system.createChunk(
        chunkDesc(makeBounds(-0.4f, -0.4f, -0.4f, 0.4f, 0.4f, 0.4f), lods, 2));
    const full_renderer::TerrainChunkHandle culledChunk = system.createChunk(
        chunkDesc(makeBounds(3.0f, 3.0f, 3.0f, 4.0f, 4.0f, 4.0f), lods, 2));
    const full_renderer::TerrainChunkHandle chunks[] = {
        visibleChunk,
        culledChunk,
        {99, 1},
    };

    std::vector<full_renderer::DrawItem> drawItems;
    const full_renderer::RendererResult result = system.buildDrawItems(
        identityView(),
        submitDesc(chunks, 3, 0.0f, 0.0f, -3.0f),
        drawItems);

    expect(result == full_renderer::RendererResult::Success, "terrain submission succeeds", failures);
    expect(drawItems.size() == 1, "only visible chunks produce draw items", failures);
    expect(drawItems[0].mesh.id == 12, "distance-based LOD selects far mesh", failures);
    expect(system.getStats().submittedChunks == 3, "stats count submitted chunks", failures);
    expect(system.getStats().visibleChunks == 1, "stats count visible chunks", failures);
    expect(system.getStats().culledChunks == 2, "stats count culled chunks", failures);
    expect(system.getStats().terrainDraws == 1, "stats count terrain draws", failures);
    expect(system.getStats().visibleChunksByLod[1] == 1, "stats count visible chunks by LOD", failures);
    expect(system.getStats().terrainBatchesByLod[1] == 1, "draw item path counts terrain draws by LOD", failures);
    expect(system.getStats().splatMapFallbackChunks == 1, "missing splat map uses fallback weights", failures);

    const std::uint32_t debugCount = system.copyDebugInfo(nullptr, 0);
    expect(debugCount == 3, "debug query returns available count", failures);
    full_renderer::TerrainChunkDebugInfo debugInfo[3];
    expect(system.copyDebugInfo(debugInfo, 3) == 3, "debug copy returns available count", failures);
    expect(debugInfo[0].cullResult == full_renderer::TerrainCullResult::Visible, "debug marks visible chunk", failures);
    expect(debugInfo[0].selectedLod == 1, "debug records selected LOD", failures);
    expect(debugInfo[0].hasTerrainMaterial, "debug records selected material", failures);
    expect(!debugInfo[0].hasSplatMap, "debug records missing splat map", failures);
    expect(debugInfo[1].cullResult == full_renderer::TerrainCullResult::OutsideFrustum, "debug marks frustum cull", failures);
    expect(debugInfo[2].cullResult == full_renderer::TerrainCullResult::InvalidHandle, "debug marks invalid handle", failures);
}

void instancedBatchingGroupsByMeshAndMaterial(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc sharedLods[] = {
        lod(11, 21, 100.0f),
    };
    const full_renderer::TerrainLodDesc otherMaterialLods[] = {
        lod(11, 22, 100.0f),
    };

    const full_renderer::TerrainChunkHandle chunkA = system.createChunk(
        chunkDesc(makeBounds(-0.8f, -0.4f, -0.8f, -0.5f, 0.4f, -0.5f), sharedLods, 1));
    const full_renderer::TerrainChunkHandle chunkB = system.createChunk(
        chunkDesc(makeBounds(0.2f, -0.4f, 0.2f, 0.5f, 0.4f, 0.5f), sharedLods, 1));
    const full_renderer::TerrainChunkHandle chunkC = system.createChunk(
        chunkDesc(makeBounds(-0.2f, -0.4f, -0.2f, 0.1f, 0.4f, 0.1f), otherMaterialLods, 1));
    const full_renderer::TerrainChunkHandle chunks[] = {
        chunkA,
        chunkB,
        chunkC,
    };

    std::vector<float> modelMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> batches;
    const full_renderer::RendererResult result = system.buildDrawBatches(
        identityView(),
        submitDesc(chunks, 3, 0.0f, 0.0f, -3.0f),
        modelMatrices,
        batches);

    expect(result == full_renderer::RendererResult::Success, "terrain batch submission succeeds", failures);
    expect(batches.size() == 2, "visible chunks are grouped by mesh/material", failures);
    expect(batches[0].mesh.id == 11 && batches[0].material.id == 21, "first batch keeps first visible key", failures);
    expect(batches[0].lodIndex == 0, "first batch records selected LOD", failures);
    expect(batches[0].instanceCount == 2, "shared mesh/material chunks share one instance batch", failures);
    expect(batches[1].mesh.id == 11 && batches[1].material.id == 22, "different material starts new batch", failures);
    expect(batches[1].instanceCount == 1, "different material batch has one instance", failures);
    expect(modelMatrices.size() == 48, "three instance matrices are emitted", failures);
    expect(system.getStats().visibleChunks == 3, "visible chunk count remains chunk-based", failures);
    expect(system.getStats().terrainDraws == 2, "terrain draw count is batch-based", failures);
    expect(system.getStats().splatMapFallbackChunks == 3, "batch stats count splat fallback chunks", failures);
    expect(system.getStats().visibleChunksByLod[0] == 3, "visible chunk LOD stats count all chunks", failures);
    expect(system.getStats().terrainBatchesByLod[0] == 2, "batch LOD stats count batches", failures);

    expect(system.copyBatchDebugInfo(nullptr, 0) == 2, "batch debug query returns available count", failures);
    full_renderer::TerrainBatchDebugInfo debugInfo[2];
    expect(system.copyBatchDebugInfo(debugInfo, 2) == 2, "batch debug copy returns available count", failures);
    expect(debugInfo[0].selectedLod == 0, "batch debug records LOD", failures);
    expect(debugInfo[0].hasTerrainMaterial, "batch debug records selected material", failures);
    expect(!debugInfo[0].hasSplatMap, "batch debug records missing splat map", failures);
    expect(debugInfo[0].instanceCount == 2, "batch debug records instance count", failures);
    expect(debugInfo[1].instanceCount == 1, "second batch debug records instance count", failures);
}

void lodChangesCreateSeparateBatches(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc lods[] = {
        lod(11, 21, 1.0f),
        lod(12, 21, 100.0f),
    };

    const full_renderer::TerrainChunkHandle nearChunk = system.createChunk(
        chunkDesc(makeBounds(-0.1f, -0.1f, -0.1f, 0.1f, 0.1f, 0.1f), lods, 2));
    const full_renderer::TerrainChunkHandle farChunk = system.createChunk(
        chunkDesc(makeBounds(0.7f, -0.1f, 0.7f, 0.8f, 0.1f, 0.8f), lods, 2));
    const full_renderer::TerrainChunkHandle chunks[] = {
        nearChunk,
        farChunk,
    };

    std::vector<float> modelMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> batches;
    const full_renderer::RendererResult result = system.buildDrawBatches(
        identityView(),
        submitDesc(chunks, 2, 0.0f, 0.0f, 0.0f),
        modelMatrices,
        batches);

    expect(result == full_renderer::RendererResult::Success, "LOD batch submission succeeds", failures);
    expect(batches.size() == 2, "different selected LOD meshes create separate batches", failures);
    expect(batches[0].mesh.id == 11, "near chunk selects near mesh", failures);
    expect(batches[0].lodIndex == 0, "near batch records LOD 0", failures);
    expect(batches[1].mesh.id == 12, "far chunk selects far mesh", failures);
    expect(batches[1].lodIndex == 1, "far batch records LOD 1", failures);
    expect(system.getStats().visibleChunksByLod[0] == 1, "one visible chunk is LOD 0", failures);
    expect(system.getStats().visibleChunksByLod[1] == 1, "one visible chunk is LOD 1", failures);
    expect(system.getStats().terrainBatchesByLod[0] == 1, "one terrain batch is LOD 0", failures);
    expect(system.getStats().terrainBatchesByLod[1] == 1, "one terrain batch is LOD 1", failures);
}

void threeLodSelectionMapsToRenderResources(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc lods[] = {
        lod(101, 201, 1.0f),
        lod(102, 202, 3.0f),
        lod(103, 203, 100.0f),
    };

    const full_renderer::TerrainChunkHandle nearChunk = system.createChunk(
        chunkDesc(makeBounds(-0.1f, -0.1f, -0.1f, 0.1f, 0.1f, 0.1f), lods, 3));
    const full_renderer::TerrainChunkHandle midChunk = system.createChunk(
        chunkDesc(makeBounds(1.9f, -0.1f, 0.0f, 2.1f, 0.1f, 0.2f), lods, 3));
    const full_renderer::TerrainChunkHandle farChunk = system.createChunk(
        chunkDesc(makeBounds(4.9f, -0.1f, 0.0f, 5.1f, 0.1f, 0.2f), lods, 3));
    const full_renderer::TerrainChunkHandle chunks[] = {
        nearChunk,
        midChunk,
        farChunk,
    };

    std::vector<float> modelMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> batches;
    const full_renderer::RendererResult result = system.buildDrawBatches(
        wideView(),
        submitDesc(chunks, 3, 0.0f, 0.0f, 0.0f),
        modelMatrices,
        batches);

    expect(result == full_renderer::RendererResult::Success, "three LOD terrain submission succeeds", failures);
    expect(batches.size() == 3, "three selected LOD resources produce three batches", failures);
    if (batches.size() == 3)
    {
        expect(batches[0].mesh.id == 101 && batches[0].material.id == 201 && batches[0].lodIndex == 0,
            "LOD 0 maps to its mesh and material",
            failures);
        expect(batches[1].mesh.id == 102 && batches[1].material.id == 202 && batches[1].lodIndex == 1,
            "LOD 1 maps to its mesh and material",
            failures);
        expect(batches[2].mesh.id == 103 && batches[2].material.id == 203 && batches[2].lodIndex == 2,
            "LOD 2 maps to its mesh and material",
            failures);
    }
    expect(system.getStats().visibleChunksByLod[0] == 1, "LOD 0 visible count updates", failures);
    expect(system.getStats().visibleChunksByLod[1] == 1, "LOD 1 visible count updates", failures);
    expect(system.getStats().visibleChunksByLod[2] == 1, "LOD 2 visible count updates", failures);
    expect(system.getStats().terrainBatchesByLod[0] == 1, "LOD 0 batch count updates", failures);
    expect(system.getStats().terrainBatchesByLod[1] == 1, "LOD 1 batch count updates", failures);
    expect(system.getStats().terrainBatchesByLod[2] == 1, "LOD 2 batch count updates", failures);
}

void splatMapParticipatesInBatchKeyAndDebug(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc lods[] = {
        lod(11, 21, 100.0f),
    };

    full_renderer::TerrainChunkDesc firstDesc =
        chunkDesc(makeBounds(-0.8f, -0.1f, -0.8f, -0.6f, 0.1f, -0.6f), lods, 1);
    firstDesc.splatMap.id = 31;
    full_renderer::TerrainChunkDesc secondDesc =
        chunkDesc(makeBounds(0.1f, -0.1f, 0.1f, 0.3f, 0.1f, 0.3f), lods, 1);
    secondDesc.splatMap.id = 32;

    const full_renderer::TerrainChunkHandle firstChunk = system.createChunk(firstDesc);
    const full_renderer::TerrainChunkHandle secondChunk = system.createChunk(secondDesc);
    const full_renderer::TerrainChunkHandle chunks[] = {
        firstChunk,
        secondChunk,
    };

    std::vector<float> modelMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> batches;
    const full_renderer::RendererResult result = system.buildDrawBatches(
        identityView(),
        submitDesc(chunks, 2, 0.0f, 0.0f, -3.0f),
        modelMatrices,
        batches);

    expect(result == full_renderer::RendererResult::Success, "splat-map batch submission succeeds", failures);
    expect(batches.size() == 2, "different splat maps create separate terrain batches", failures);
    if (batches.size() == 2)
    {
        expect(batches[0].splatMap.id == 31, "first batch keeps first splat map", failures);
        expect(batches[1].splatMap.id == 32, "second batch keeps second splat map", failures);
    }
    expect(system.getStats().splatMapFallbackChunks == 0, "valid splat handles do not count as fallback", failures);
    full_renderer::TerrainChunkDebugInfo chunkDebug[2];
    expect(system.copyDebugInfo(chunkDebug, 2) == 2, "splat chunk debug count is available", failures);
    expect(chunkDebug[0].hasSplatMap && chunkDebug[1].hasSplatMap, "chunk debug records splat maps", failures);
    full_renderer::TerrainBatchDebugInfo batchDebug[2];
    expect(system.copyBatchDebugInfo(batchDebug, 2) == 2, "splat batch debug count is available", failures);
    expect(batchDebug[0].hasSplatMap && batchDebug[1].hasSplatMap, "batch debug records splat maps", failures);
}

void culledChunksDoNotSelectLodOrBatch(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc lods[] = {
        lod(11, 21, 100.0f),
    };

    const full_renderer::TerrainChunkHandle culledChunk = system.createChunk(
        chunkDesc(makeBounds(3.0f, 3.0f, 3.0f, 4.0f, 4.0f, 4.0f), lods, 1));
    const full_renderer::TerrainChunkHandle chunks[] = {
        culledChunk,
    };

    std::vector<float> modelMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> batches;
    const full_renderer::RendererResult result = system.buildDrawBatches(
        identityView(),
        submitDesc(chunks, 1, 0.0f, 0.0f, 0.0f),
        modelMatrices,
        batches);

    expect(result == full_renderer::RendererResult::Success, "culled terrain submission succeeds", failures);
    expect(batches.empty(), "culled chunks do not enter batches", failures);
    expect(system.getStats().visibleChunks == 0, "culled chunks are not visible", failures);
    expect(system.getStats().terrainDraws == 0, "culled chunks do not produce terrain draws", failures);

    full_renderer::TerrainChunkDebugInfo debugInfo;
    expect(system.copyDebugInfo(&debugInfo, 1) == 1, "culled chunk has one debug record", failures);
    expect(debugInfo.cullResult == full_renderer::TerrainCullResult::OutsideFrustum,
        "culled chunk debug records frustum result",
        failures);
    expect(debugInfo.selectedLod == full_renderer::kInvalidTerrainLodIndex,
        "culled chunk does not record a selected LOD",
        failures);
}

void lightFrustumSelectsOffCameraShadowCasters(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc lods[] = {
        lod(11, 21, 2.0f),
        lod(12, 22, 100.0f),
    };

    const full_renderer::TerrainChunkHandle cameraVisibleChunk = system.createChunk(
        chunkDesc(makeBounds(-0.2f, -0.1f, -0.2f, 0.2f, 0.1f, 0.2f), lods, 2));
    const full_renderer::TerrainChunkHandle offCameraCaster = system.createChunk(
        chunkDesc(makeBounds(3.0f, -0.1f, -0.2f, 3.4f, 0.1f, 0.2f), lods, 2));
    const full_renderer::TerrainChunkHandle outsideLight = system.createChunk(
        chunkDesc(makeBounds(8.0f, -0.1f, -0.2f, 8.4f, 0.1f, 0.2f), lods, 2));
    const full_renderer::TerrainChunkHandle chunks[] = {
        cameraVisibleChunk,
        offCameraCaster,
        outsideLight,
    };

    std::vector<float> modelMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> batches;
    full_renderer::TerrainSubmitDesc submit = submitDesc(chunks, 3, 0.0f, 0.0f, 0.0f);
    full_renderer::DirectionalShadowDesc shadow = shadowDesc(5.0f);
    shadow.debugDrawShadowCasters = true;
    const full_renderer::RendererResult result = system.buildShadowCasterBatches(
        identityView(),
        submit,
        topDownLight(),
        shadow,
        modelMatrices,
        batches);

    expect(result == full_renderer::RendererResult::Success, "shadow caster selection succeeds", failures);
    expect(batches.size() == 2, "camera-visible and off-camera chunks create separate LOD batches", failures);
    expect(modelMatrices.size() == 32, "two shadow caster instance matrices are emitted", failures);
    expect(system.getStats().shadowCasterChunks == 2, "shadow stats count selected casters", failures);
    expect(system.getStats().offCameraShadowCasterChunks == 1, "shadow stats count off-camera caster", failures);
    expect(system.getStats().shadowRejectedChunks == 1, "shadow stats count light-frustum reject", failures);
    expect(system.getStats().shadowCasterChunksByLod[0] == 1, "near shadow caster selects LOD 0", failures);
    expect(system.getStats().shadowCasterChunksByLod[1] == 1, "far shadow caster selects LOD 1", failures);
    expect(system.getStats().shadowCasterBatchesByLod[0] == 1, "LOD 0 shadow batch count updates", failures);
    expect(system.getStats().shadowCasterBatchesByLod[1] == 1, "LOD 1 shadow batch count updates", failures);

    full_renderer::TerrainChunkDebugInfo debugInfo[3];
    expect(system.copyShadowCasterDebugInfo(debugInfo, 3) == 3, "shadow caster debug records are available", failures);
    expect(debugInfo[0].selectedAsShadowCaster, "camera-visible chunk is marked as shadow caster", failures);
    expect(debugInfo[0].cameraVisible, "camera-visible caster records camera visibility", failures);
    expect(debugInfo[1].selectedAsShadowCaster, "off-camera chunk is marked as shadow caster", failures);
    expect(!debugInfo[1].cameraVisible, "off-camera caster records camera invisibility", failures);
    expect(debugInfo[2].cullResult == full_renderer::TerrainCullResult::OutsideFrustum,
        "outside-light chunk records light-frustum cull",
        failures);

    std::vector<float> repeatedMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> repeatedBatches;
    const full_renderer::RendererResult repeatedResult = system.buildShadowCasterBatches(
        identityView(),
        submit,
        topDownLight(),
        shadow,
        repeatedMatrices,
        repeatedBatches);
    expect(repeatedResult == full_renderer::RendererResult::Success, "repeated shadow selection succeeds", failures);
    expect(repeatedBatches.size() == batches.size(), "shadow caster selection is deterministic", failures);
    if (repeatedBatches.size() == batches.size() && batches.size() == 2)
    {
        expect(repeatedBatches[0].mesh.id == batches[0].mesh.id &&
                repeatedBatches[1].mesh.id == batches[1].mesh.id,
            "repeated shadow batches preserve deterministic order",
            failures);
    }
}

void perCascadeShadowCasterDiagnosticsAreDeterministic(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc lods[] = {
        lod(31, 41, 2.0f),
        lod(32, 42, 8.0f),
        lod(33, 43, 100.0f),
    };

    full_renderer::DirectionalShadowDesc shadow = shadowDesc(10.0f);
    shadow.cascadeCount = 3;
    shadow.cascadeSplitMode = full_renderer::ShadowCascadeSplitMode::Uniform;
    shadow.cascadeShadowDistanceMeters = 10.0f;
    shadow.cascadeCameraNearMeters = 0.1f;
    shadow.cascadeCameraFarMeters = 10.0f;
    shadow.debugDrawCascadeCasters = true;

    const full_renderer::TerrainChunkHandle nearChunk = system.createChunk(
        chunkDesc(makeBounds(-20.0f, -20.0f, -20.0f, 20.0f, 20.0f, 20.0f), lods, 3));
    const full_renderer::TerrainChunkHandle midChunk = system.createChunk(
        chunkDesc(makeBounds(-18.0f, -18.0f, -18.0f, 18.0f, 18.0f, 18.0f), lods, 3));
    const full_renderer::TerrainChunkHandle farChunk = system.createChunk(
        chunkDesc(makeBounds(-16.0f, -16.0f, -16.0f, 16.0f, 16.0f, 16.0f), lods, 3));
    const full_renderer::TerrainChunkHandle outsideChunk = system.createChunk(
        chunkDesc(makeBounds(30.0f, -0.1f, 0.0f, 31.0f, 0.1f, 1.0f), lods, 3));
    const full_renderer::TerrainChunkHandle chunks[] = {
        nearChunk,
        midChunk,
        farChunk,
        outsideChunk,
    };

    full_renderer::TerrainSubmitDesc submit = submitDesc(chunks, 4, 0.0f, 0.0f, 0.0f);

    std::vector<float> modelMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> batches;
    const full_renderer::RendererResult result = system.buildShadowCasterBatches(
        wideView(),
        submit,
        topDownLight(),
        shadow,
        modelMatrices,
        batches);

    expect(result == full_renderer::RendererResult::Success, "per-cascade shadow selection succeeds", failures);
    const full_renderer::TerrainStats stats = system.getStats();
    expect(stats.shadowCascadeCount == 3, "stats record evaluated cascade count", failures);
    expect(stats.shadowCasterChunks >= stats.shadowCascadeCasterChunks[0],
        "aggregate shadow caster count includes cascade 0",
        failures);
    expect(stats.shadowCascadeCasterChunks[0] > 0, "cascade 0 selects at least one caster", failures);
    expect(stats.shadowCascadeCasterChunks[1] > 0 || stats.shadowCascadeCasterChunks[2] > 0,
        "higher cascades select terrain casters",
        failures);
    bool hasCascade0Batch = false;
    bool hasPreviewCascadeBatch = false;
    for (const full_renderer::core::InstancedDrawBatch& batch : batches)
    {
        hasCascade0Batch = hasCascade0Batch || batch.shadowCascadeIndex == 0;
        hasPreviewCascadeBatch = hasPreviewCascadeBatch ||
            (batch.shadowCascadeIndex > 0 &&
                batch.shadowCascadeIndex != full_renderer::kInvalidShadowCascadeIndex);
    }
    expect(hasCascade0Batch, "shadow batches include rendered cascade 0", failures);
    expect(hasPreviewCascadeBatch, "shadow batches include higher cascade render inputs", failures);
    expect(stats.shadowCascadeRejectedChunks[0] > 0 ||
            stats.shadowCascadeRejectedChunks[1] > 0 ||
            stats.shadowCascadeRejectedChunks[2] > 0,
        "per-cascade selection records rejected terrain",
        failures);
    expect(stats.shadowCascadeCasterChunksByLod[0][0] +
            stats.shadowCascadeCasterChunksByLod[0][1] +
            stats.shadowCascadeCasterChunksByLod[0][2] +
            stats.shadowCascadeCasterChunksByLod[0][3] == stats.shadowCascadeCasterChunks[0],
        "cascade 0 LOD buckets sum to cascade caster count",
        failures);

    const std::uint32_t debugCount = system.copyShadowCasterDebugInfo(nullptr, 0);
    expect(debugCount >= stats.shadowCascadeCasterChunks[0],
        "cascade caster debug records are available",
        failures);
    std::vector<full_renderer::TerrainChunkDebugInfo> debugInfo(debugCount);
    (void)system.copyShadowCasterDebugInfo(debugInfo.data(), debugCount);
    bool hasPreviewCascadeRecord = false;
    for (const full_renderer::TerrainChunkDebugInfo& info : debugInfo)
    {
        if (info.shadowCascadeIndex > 0 &&
            info.shadowCascadeIndex != full_renderer::kInvalidShadowCascadeIndex)
        {
            hasPreviewCascadeRecord = true;
            break;
        }
    }
    expect(hasPreviewCascadeRecord, "debug records include higher cascade indices", failures);

    std::vector<float> repeatedMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> repeatedBatches;
    const full_renderer::RendererResult repeatedResult = system.buildShadowCasterBatches(
        wideView(),
        submit,
        topDownLight(),
        shadow,
        repeatedMatrices,
        repeatedBatches);
    const full_renderer::TerrainStats repeatedStats = system.getStats();
    expect(repeatedResult == full_renderer::RendererResult::Success,
        "repeated per-cascade shadow selection succeeds",
        failures);
    expect(repeatedStats.shadowCascadeCasterChunks[0] == stats.shadowCascadeCasterChunks[0] &&
            repeatedStats.shadowCascadeCasterChunks[1] == stats.shadowCascadeCasterChunks[1] &&
            repeatedStats.shadowCascadeCasterChunks[2] == stats.shadowCascadeCasterChunks[2],
        "per-cascade caster selection is deterministic",
        failures);
}

void chunkResidencyChurnStatsAreDeterministic(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc lods[] = {
        lod(11, 21, 100.0f),
    };

    const full_renderer::TerrainChunkHandle first = system.createChunk(
        chunkDesc(makeBounds(-0.4f, -0.1f, -0.4f, -0.1f, 0.1f, -0.1f), lods, 1));
    const full_renderer::TerrainChunkHandle staleAfterDestroy = system.createChunk(
        chunkDesc(makeBounds(0.1f, -0.1f, 0.1f, 0.4f, 0.1f, 0.4f), lods, 1));
    const full_renderer::TerrainChunkHandle third = system.createChunk(
        chunkDesc(makeBounds(0.5f, -0.1f, 0.5f, 0.8f, 0.1f, 0.8f), lods, 1));
    system.destroyChunk(staleAfterDestroy);
    system.destroyChunk(staleAfterDestroy);
    const full_renderer::TerrainChunkHandle reused = system.createChunk(
        chunkDesc(makeBounds(-0.8f, -0.1f, 0.5f, -0.5f, 0.1f, 0.8f), lods, 1));
    expect(reused.id == staleAfterDestroy.id && reused.generation != staleAfterDestroy.generation,
        "destroyed terrain chunk slot is reused with a new generation",
        failures);

    const full_renderer::TerrainChunkHandle submitted[] = {
        first,
        staleAfterDestroy,
        third,
        reused,
        {},
    };
    std::vector<float> modelMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> batches;
    const full_renderer::RendererResult result = system.buildDrawBatches(
        wideView(),
        submitDesc(submitted, 5, 0.0f, 0.0f, 0.0f),
        modelMatrices,
        batches);

    expect(result == full_renderer::RendererResult::Success, "churned terrain submission succeeds", failures);
    const full_renderer::TerrainStats stats = system.getStats();
    expect(stats.liveChunks == 3, "live chunk count survives churn", failures);
    expect(stats.residentChunks == 3, "resident chunk count survives churn", failures);
    expect(stats.allocatedChunkSlots == 3, "slot reuse keeps allocated chunk slot count stable", failures);
    expect(stats.nonResidentChunks == 0, "all allocated slots are resident after reuse", failures);
    expect(stats.chunksCreatedSinceLastSubmit == 4, "created-since-submit includes reused chunk creation", failures);
    expect(stats.chunksDestroyedSinceLastSubmit == 1, "destroyed-since-submit ignores duplicate stale destroy", failures);
    expect(stats.chunkSlotsReusedSinceLastSubmit == 1, "reused-since-submit counts one slot reuse", failures);
    expect(stats.totalChunkCreateCount == 4, "total chunk creates are counted", failures);
    expect(stats.totalChunkDestroyCount == 1, "total chunk destroys are counted", failures);
    expect(stats.totalChunkSlotReuseCount == 1, "total chunk slot reuses are counted", failures);
    expect(stats.visibleChunks == 3, "only live handles render after churn", failures);
    expect(stats.invalidHandleChunks == 1, "default terrain handle is counted as invalid", failures);
    expect(stats.staleHandleChunks == 1, "destroyed-generation terrain handle is counted as stale", failures);

    std::vector<float> repeatedMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> repeatedBatches;
    const full_renderer::RendererResult repeatedResult = system.buildDrawBatches(
        wideView(),
        submitDesc(submitted, 5, 0.0f, 0.0f, 0.0f),
        repeatedMatrices,
        repeatedBatches);
    const full_renderer::TerrainStats repeatedStats = system.getStats();
    expect(repeatedResult == full_renderer::RendererResult::Success,
        "repeated churned terrain submission succeeds",
        failures);
    expect(repeatedStats.chunksCreatedSinceLastSubmit == 0 &&
            repeatedStats.chunksDestroyedSinceLastSubmit == 0 &&
            repeatedStats.chunkSlotsReusedSinceLastSubmit == 0,
        "churn counters are consumed by the next terrain submission",
        failures);
    expect(repeatedStats.visibleChunks == stats.visibleChunks &&
            repeatedStats.invalidHandleChunks == stats.invalidHandleChunks &&
            repeatedStats.staleHandleChunks == stats.staleHandleChunks,
        "repeated churned planning is deterministic",
        failures);
}

void largeGridCullingLodAndStatsStress(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc lods[] = {
        lod(101, 201, 4.0f),
        lod(102, 202, 12.0f),
        lod(103, 203, 1000.0f),
    };

    constexpr int kGridRadius = 25;
    constexpr float kSpacing = 0.75f;
    std::vector<full_renderer::TerrainChunkHandle> chunks;
    chunks.reserve((kGridRadius * 2 + 1) * (kGridRadius * 2 + 1));
    for (int z = -kGridRadius; z <= kGridRadius; ++z)
    {
        for (int x = -kGridRadius; x <= kGridRadius; ++x)
        {
            const float minX = static_cast<float>(x) * kSpacing;
            const float minZ = static_cast<float>(z) * kSpacing;
            full_renderer::TerrainChunkDesc desc =
                chunkDesc(makeBounds(minX, -0.1f, minZ, minX + 0.45f, 0.1f, minZ + 0.45f), lods, 3);
            if (((x + z) & 1) == 0)
            {
                desc.splatMap.id = 700U + static_cast<std::uint32_t>(chunks.size());
            }
            chunks.push_back(system.createChunk(desc));
        }
    }

    std::vector<float> modelMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> batches;
    const full_renderer::RendererResult result = system.buildDrawBatches(
        wideView(),
        submitDesc(chunks.data(), static_cast<std::uint32_t>(chunks.size()), 0.0f, 0.0f, 0.0f),
        modelMatrices,
        batches);

    expect(result == full_renderer::RendererResult::Success, "large terrain grid planning succeeds", failures);
    const full_renderer::TerrainStats stats = system.getStats();
    expect(stats.liveChunks == chunks.size(), "large grid live chunk count is tracked", failures);
    expect(stats.submittedChunks == chunks.size(), "large grid submitted chunk count is tracked", failures);
    expect(stats.visibleChunks > 0, "large grid has visible chunks", failures);
    expect(stats.culledChunks > 0, "large grid has culled chunks", failures);
    expect(stats.visibleChunks + stats.culledChunks == stats.submittedChunks,
        "large grid visible and culled counts cover submissions",
        failures);
    expect(stats.visibleChunksByLod[0] > 0, "large grid selects near LOD", failures);
    expect(stats.visibleChunksByLod[1] > 0 || stats.visibleChunksByLod[2] > 0,
        "large grid selects at least one farther LOD",
        failures);
    expect(stats.splatMapFallbackChunks + stats.splatMapResidentChunks == stats.visibleChunks,
        "large grid splat residency counts cover visible chunks",
        failures);

    std::vector<float> repeatedMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> repeatedBatches;
    const full_renderer::RendererResult repeatedResult = system.buildDrawBatches(
        wideView(),
        submitDesc(chunks.data(), static_cast<std::uint32_t>(chunks.size()), 0.0f, 0.0f, 0.0f),
        repeatedMatrices,
        repeatedBatches);
    const full_renderer::TerrainStats repeatedStats = system.getStats();
    expect(repeatedResult == full_renderer::RendererResult::Success,
        "repeated large terrain grid planning succeeds",
        failures);
    expect(repeatedStats.visibleChunks == stats.visibleChunks &&
            repeatedStats.culledChunks == stats.culledChunks &&
            repeatedStats.terrainDraws == stats.terrainDraws,
        "large terrain grid planning is deterministic",
        failures);
}

void streamingStyleChurnAcrossFramesIsDeterministic(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc lods[] = {
        lod(11, 21, 100.0f),
    };
    std::vector<full_renderer::TerrainChunkHandle> liveChunks;
    for (int index = 0; index < 8; ++index)
    {
        const float offset = static_cast<float>(index) * 0.2f;
        liveChunks.push_back(system.createChunk(
            chunkDesc(makeBounds(offset, -0.1f, offset, offset + 0.1f, 0.1f, offset + 0.1f), lods, 1)));
    }

    std::vector<float> initialMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> initialBatches;
    expect(system.buildDrawBatches(
               wideView(),
               submitDesc(liveChunks.data(), static_cast<std::uint32_t>(liveChunks.size()), 0.0f, 0.0f, 0.0f),
               initialMatrices,
               initialBatches) == full_renderer::RendererResult::Success,
        "initial streaming-style terrain residency submission succeeds",
        failures);

    for (int frame = 0; frame < 4; ++frame)
    {
        const full_renderer::TerrainChunkHandle staleA = liveChunks[0];
        const full_renderer::TerrainChunkHandle staleB = liveChunks[1];
        system.destroyChunk(staleA);
        system.destroyChunk(staleB);
        liveChunks.erase(liveChunks.begin(), liveChunks.begin() + 2);
        for (int createIndex = 0; createIndex < 2; ++createIndex)
        {
            const float offset = static_cast<float>(frame * 2 + createIndex) * 0.3f;
            liveChunks.push_back(system.createChunk(
                chunkDesc(makeBounds(-offset, -0.1f, offset, -offset + 0.1f, 0.1f, offset + 0.1f), lods, 1)));
        }

        std::vector<full_renderer::TerrainChunkHandle> submitted = liveChunks;
        submitted.push_back(staleA);
        submitted.push_back(staleB);

        std::vector<float> modelMatrices;
        std::vector<full_renderer::core::InstancedDrawBatch> batches;
        const full_renderer::RendererResult result = system.buildDrawBatches(
            wideView(),
            submitDesc(submitted.data(), static_cast<std::uint32_t>(submitted.size()), 0.0f, 0.0f, 0.0f),
            modelMatrices,
            batches);
        const full_renderer::TerrainStats stats = system.getStats();
        expect(result == full_renderer::RendererResult::Success, "streaming-style churn submission succeeds", failures);
        expect(stats.liveChunks == 8, "streaming-style churn keeps resident set size stable", failures);
        expect(stats.chunksCreatedSinceLastSubmit == 2, "streaming-style churn counts two creates", failures);
        expect(stats.chunksDestroyedSinceLastSubmit == 2, "streaming-style churn counts two destroys", failures);
        expect(stats.chunkSlotsReusedSinceLastSubmit == 2, "streaming-style churn reuses two slots", failures);
        expect(stats.staleHandleChunks == 2, "streaming-style churn reports stale submitted handles", failures);
        expect(stats.visibleChunks == 8, "streaming-style churn renders only live chunks", failures);
    }
}

void largeCoordinatePlanningUsesCallerRebasedView(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc lods[] = {
        lod(11, 21, 5.0f),
        lod(12, 22, 100.0f),
    };
    constexpr float kOrigin = 1000000.0f;
    const full_renderer::TerrainChunkHandle chunk = system.createChunk(
        chunkDesc(makeBounds(kOrigin - 0.5f, -0.1f, kOrigin - 0.5f, kOrigin + 0.5f, 0.1f, kOrigin + 0.5f), lods, 2));
    const full_renderer::TerrainChunkHandle chunks[] = {chunk};

    std::vector<float> modelMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> batches;
    const full_renderer::RendererResult result = system.buildDrawBatches(
        rebasedWideView(kOrigin, kOrigin),
        submitDesc(chunks, 1, kOrigin, 0.0f, kOrigin),
        modelMatrices,
        batches);

    expect(result == full_renderer::RendererResult::Success,
        "large-coordinate terrain planning succeeds with caller-rebased view",
        failures);
    expect(batches.size() == 1, "caller-rebased large-coordinate chunk is visible", failures);
    expect(batches[0].lodIndex == 0, "large-coordinate LOD selection uses submitted camera position", failures);
    expect(system.getStats().visibleChunksByLod[0] == 1,
        "large-coordinate stats remain deterministic",
        failures);
}

void skirtAdjustedBoundsSurviveChunkReuse(int& failures)
{
    full_renderer::terrain::TerrainSystem system;
    const full_renderer::TerrainLodDesc lods[] = {
        lod(11, 21, 100.0f),
    };
    constexpr float kSkirtDepth = 0.6f;
    const full_renderer::TerrainChunkHandle first = system.createChunk(
        chunkDesc(makeBounds(-0.5f, -kSkirtDepth, -0.5f, 0.5f, 0.2f, 0.5f), lods, 1));
    system.destroyChunk(first);
    const full_renderer::TerrainChunkHandle reused = system.createChunk(
        chunkDesc(makeBounds(-0.25f, -kSkirtDepth, -0.25f, 0.25f, 0.2f, 0.25f), lods, 1));
    const full_renderer::TerrainChunkHandle chunks[] = {reused};

    std::vector<float> modelMatrices;
    std::vector<full_renderer::core::InstancedDrawBatch> batches;
    const full_renderer::RendererResult result = system.buildDrawBatches(
        wideView(),
        submitDesc(chunks, 1, 0.0f, 0.0f, 0.0f),
        modelMatrices,
        batches);

    expect(result == full_renderer::RendererResult::Success, "skirt-adjusted reused chunk submission succeeds", failures);
    full_renderer::TerrainChunkDebugInfo debugInfo;
    expect(system.copyDebugInfo(&debugInfo, 1) == 1, "skirt-adjusted debug info is captured", failures);
    expect(debugInfo.bounds.min[1] == -kSkirtDepth,
        "skirt-adjusted bounds survive chunk slot reuse",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    lifecycleAndValidation(failures);
    updateChunkReplacesDescriptorWithoutChangingHandle(failures);
    lodThresholdSelectionIsDeterministic(failures);
    submissionCullingLodAndDebug(failures);
    instancedBatchingGroupsByMeshAndMaterial(failures);
    lodChangesCreateSeparateBatches(failures);
    threeLodSelectionMapsToRenderResources(failures);
    splatMapParticipatesInBatchKeyAndDebug(failures);
    culledChunksDoNotSelectLodOrBatch(failures);
    lightFrustumSelectsOffCameraShadowCasters(failures);
    perCascadeShadowCasterDiagnosticsAreDeterministic(failures);
    chunkResidencyChurnStatsAreDeterministic(failures);
    largeGridCullingLodAndStatsStress(failures);
    streamingStyleChurnAcrossFramesIsDeterministic(failures);
    largeCoordinatePlanningUsesCallerRebasedView(failures);
    skirtAdjustedBoundsSurviveChunkReuse(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
