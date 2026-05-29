#include "full_renderer/Renderer.hpp"
#include "full_renderer/Terrain.hpp"
#include "renderer/resources/ResourceLifetime.hpp"
#include "renderer/terrain/TerrainSystem.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

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

full_renderer::Aabb bounds()
{
    full_renderer::Aabb value;
    value.min[0] = -1.0f;
    value.min[1] = -1.0f;
    value.min[2] = -1.0f;
    value.max[0] = 1.0f;
    value.max[1] = 1.0f;
    value.max[2] = 1.0f;
    return value;
}

void memoryEstimatesAreDeterministic(int& failures)
{
    using full_renderer::resources::ResourceMemoryFormat;

    expect(
        full_renderer::resources::estimateBufferBytes(3, 16) == 48,
        "buffer byte estimate multiplies element count by stride",
        failures);
    expect(
        full_renderer::resources::estimateTexture2DBytes(4, 8, ResourceMemoryFormat::Rgba8) == 128,
        "RGBA8 estimate uses four bytes per pixel",
        failures);
    expect(
        full_renderer::resources::estimateTexture2DBytes(4, 8, ResourceMemoryFormat::R32F) == 128,
        "R32F estimate uses four bytes per pixel",
        failures);
    expect(
        full_renderer::resources::estimateTexture2DBytes(4, 8, ResourceMemoryFormat::D24) == 128,
        "D24 estimate uses conservative four-byte depth pixels",
        failures);
    expect(
        full_renderer::resources::estimateTexture2DBytes(
            2,
            2,
            full_renderer::TextureFormat::Rgba8) == 16,
        "public RGBA8 texture estimate maps to four bytes per pixel",
        failures);
}

void saturatingAddDoesNotWrap(int& failures)
{
    const std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
    expect(
        full_renderer::resources::addSaturating(10, 20) == 30,
        "normal add preserves exact sum",
        failures);
    expect(
        full_renderer::resources::addSaturating(max - 2, 8) == max,
        "overflowing add saturates instead of wrapping",
        failures);
}

void terrainChunkHandlesRejectStaleGenerations(int& failures)
{
    full_renderer::terrain::TerrainSystem system;

    full_renderer::TerrainLodDesc lod;
    lod.mesh = full_renderer::MeshHandle{1};
    lod.material = full_renderer::MaterialHandle{1};

    full_renderer::TerrainChunkDesc desc;
    desc.bounds = bounds();
    desc.lods = &lod;
    desc.lodCount = 1;

    const full_renderer::TerrainChunkHandle handle = system.createChunk(desc);
    expect(full_renderer::isValid(handle), "terrain chunk creation returns a live generation handle", failures);
    system.destroyChunk(handle);
    expect(
        system.getStats().liveChunks == 0,
        "destroying a live terrain chunk clears live count",
        failures);
    const full_renderer::TerrainChunkHandle reused = system.createChunk(desc);
    expect(full_renderer::isValid(reused), "terrain chunk slot can be reused", failures);
    expect(
        reused.id == handle.id && reused.generation != handle.generation,
        "terrain chunk slot reuse advances generation to distinguish stale handles",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    memoryEstimatesAreDeterministic(failures);
    saturatingAddDoesNotWrap(failures);
    terrainChunkHandlesRejectStaleGenerations(failures);

    if (failures != 0)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
