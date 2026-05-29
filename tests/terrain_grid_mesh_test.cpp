#include "renderer/terrain/TerrainGridMesh.hpp"

#include <cmath>
#include <cstddef>
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

float normalLength(const full_renderer::MeshVertex& vertex)
{
    return std::sqrt(
        vertex.normal[0] * vertex.normal[0] +
        vertex.normal[1] * vertex.normal[1] +
        vertex.normal[2] * vertex.normal[2]);
}

bool near(const float a, const float b, const float epsilon = 0.0001f)
{
    return std::fabs(a - b) <= epsilon;
}

void expectedCounts(int& failures)
{
    constexpr float kSize = 4.0f;
    for (const std::uint32_t subdivisions : {1U, 2U, 4U, 8U, 16U})
    {
        const full_renderer::terrain::TerrainGridMeshData mesh =
            full_renderer::terrain::generateTerrainGridMesh(subdivisions, kSize);

        const std::uint32_t verticesPerSide = subdivisions + 1U;
        const std::uint32_t expectedVertexCount = verticesPerSide * verticesPerSide;
        const std::uint32_t expectedIndexCount = subdivisions * subdivisions * 6U;
        expect(mesh.vertices.size() == expectedVertexCount, "grid vertex count matches subdivision", failures);
        expect(mesh.indices.size() == expectedIndexCount, "grid index count matches subdivision", failures);

        const full_renderer::MeshDesc desc = mesh.meshDesc();
        expect(desc.vertexCount == expectedVertexCount, "mesh descriptor exposes generated vertices", failures);
        expect(desc.indexCount == expectedIndexCount, "mesh descriptor exposes generated indices", failures);
        expect(desc.vertices == mesh.vertices.data(), "mesh descriptor points at generated vertices", failures);
        expect(desc.indices == mesh.indices.data(), "mesh descriptor points at generated indices", failures);
    }
}

void positionsNormalsAndIndicesAreValid(int& failures)
{
    constexpr float kSize = 6.0f;
    const full_renderer::terrain::TerrainGridMeshData mesh =
        full_renderer::terrain::generateTerrainGridMesh(4, kSize);

    expect(!mesh.vertices.empty(), "valid grid has vertices", failures);
    expect(!mesh.indices.empty(), "valid grid has indices", failures);
    for (const full_renderer::MeshVertex& vertex : mesh.vertices)
    {
        expect(std::isfinite(vertex.position[0]), "vertex x is finite", failures);
        expect(vertex.position[0] >= 0.0f && vertex.position[0] <= kSize, "vertex x is chunk-local", failures);
        expect(vertex.position[1] == 0.0f, "terrain grid is flat on local Y", failures);
        expect(std::isfinite(vertex.position[2]), "vertex z is finite", failures);
        expect(vertex.position[2] >= 0.0f && vertex.position[2] <= kSize, "vertex z is chunk-local", failures);
        expect(vertex.normal[0] == 0.0f && vertex.normal[1] == 1.0f && vertex.normal[2] == 0.0f,
            "terrain grid normal is Y-up",
            failures);
        expect(near(normalLength(vertex), 1.0f), "terrain grid normal is normalized", failures);
    }

    for (const std::uint16_t index : mesh.indices)
    {
        expect(index < mesh.vertices.size(), "grid index references a generated vertex", failures);
    }
}

void heightfieldNormalsAreGenerated(int& failures)
{
    constexpr std::uint32_t kSubdivisions = 2;
    constexpr float kSize = 2.0f;
    const std::vector<float> heights = {
        0.0f, 0.5f, 1.0f,
        0.0f, 0.5f, 1.0f,
        0.0f, 0.5f, 1.0f,
    };
    const full_renderer::terrain::TerrainGridMeshData mesh =
        full_renderer::terrain::generateTerrainGridMeshWithHeights(
            kSubdivisions,
            kSize,
            heights.data(),
            static_cast<std::uint32_t>(heights.size()));

    expect(mesh.vertices.size() == 9U, "heightfield grid creates vertices", failures);
    expect(mesh.indices.size() == 24U, "heightfield grid creates indices", failures);
    bool foundTiltedNormal = false;
    for (const full_renderer::MeshVertex& vertex : mesh.vertices)
    {
        expect(std::isfinite(vertex.normal[0]) &&
                std::isfinite(vertex.normal[1]) &&
                std::isfinite(vertex.normal[2]),
            "heightfield normal is finite",
            failures);
        expect(near(normalLength(vertex), 1.0f, 0.001f), "heightfield normal is normalized", failures);
        foundTiltedNormal = foundTiltedNormal || std::fabs(vertex.normal[0]) > 0.01f || std::fabs(vertex.normal[2]) > 0.01f;
    }
    expect(foundTiltedNormal, "heightfield slope produces tilted normals", failures);
}

void skirtGeometryIsGenerated(int& failures)
{
    constexpr std::uint32_t kSubdivisions = 4;
    constexpr float kSize = 8.0f;
    constexpr float kSkirtDepth = 0.75f;
    const full_renderer::terrain::TerrainGridMeshData mesh =
        full_renderer::terrain::generateTerrainGridMeshWithHeights(
            kSubdivisions,
            kSize,
            nullptr,
            0,
            full_renderer::terrain::TerrainGridMeshOptions{kSkirtDepth});

    const std::uint32_t verticesPerSide = kSubdivisions + 1U;
    const std::uint32_t expectedBaseVertexCount = verticesPerSide * verticesPerSide;
    const std::uint32_t expectedSkirtVertexCount = verticesPerSide * 4U;
    const std::uint32_t expectedBaseIndexCount = kSubdivisions * kSubdivisions * 6U;
    const std::uint32_t expectedSkirtIndexCount = kSubdivisions * 4U * 6U;
    expect(mesh.vertices.size() == expectedBaseVertexCount + expectedSkirtVertexCount,
        "skirt grid vertex count includes four edge strips",
        failures);
    expect(mesh.indices.size() == expectedBaseIndexCount + expectedSkirtIndexCount,
        "skirt grid index count includes four edge strips",
        failures);

    bool foundSkirtVertex = false;
    for (std::size_t index = expectedBaseVertexCount; index < mesh.vertices.size(); ++index)
    {
        const full_renderer::MeshVertex& vertex = mesh.vertices[index];
        foundSkirtVertex = foundSkirtVertex || near(vertex.position[1], -kSkirtDepth);
        expect(std::isfinite(vertex.position[0]) &&
                std::isfinite(vertex.position[1]) &&
                std::isfinite(vertex.position[2]),
            "skirt vertex position is finite",
            failures);
        expect(near(normalLength(vertex), 1.0f, 0.001f), "skirt vertex normal is normalized", failures);
    }
    expect(foundSkirtVertex, "skirt vertices extend downward by skirt depth", failures);
}

void slopedSkirtGenerationIsDeterministic(int& failures)
{
    constexpr std::uint32_t kSubdivisions = 2;
    const std::vector<float> heights = {
        0.0f, 0.3f, 0.6f,
        0.1f, 0.4f, 0.7f,
        0.2f, 0.5f, 0.8f,
    };
    const full_renderer::terrain::TerrainGridMeshOptions options{0.5f};
    const full_renderer::terrain::TerrainGridMeshData first =
        full_renderer::terrain::generateTerrainGridMeshWithHeights(
            kSubdivisions,
            2.0f,
            heights.data(),
            static_cast<std::uint32_t>(heights.size()),
            options);
    const full_renderer::terrain::TerrainGridMeshData second =
        full_renderer::terrain::generateTerrainGridMeshWithHeights(
            kSubdivisions,
            2.0f,
            heights.data(),
            static_cast<std::uint32_t>(heights.size()),
            options);

    expect(first.indices == second.indices, "sloped skirt indices are deterministic", failures);
    expect(first.vertices.size() == second.vertices.size(), "sloped skirt vertex count is deterministic", failures);
    for (std::size_t index = 0; index < first.vertices.size() && index < second.vertices.size(); ++index)
    {
        expect(first.vertices[index].position[0] == second.vertices[index].position[0] &&
                first.vertices[index].position[1] == second.vertices[index].position[1] &&
                first.vertices[index].position[2] == second.vertices[index].position[2],
            "sloped skirt positions are deterministic",
            failures);
        expect(first.vertices[index].normal[0] == second.vertices[index].normal[0] &&
                first.vertices[index].normal[1] == second.vertices[index].normal[1] &&
                first.vertices[index].normal[2] == second.vertices[index].normal[2],
            "sloped skirt normals are deterministic",
            failures);
    }
}

void normalGenerationHandlesDegenerateTriangles(int& failures)
{
    full_renderer::MeshVertex vertices[3] = {};
    vertices[0].position[0] = 0.0f;
    vertices[0].position[1] = 0.0f;
    vertices[0].position[2] = 0.0f;
    vertices[1] = vertices[0];
    vertices[2] = vertices[0];
    const std::uint16_t indices[3] = {0, 1, 2};

    expect(!full_renderer::terrain::generateTerrainMeshNormals(vertices, 3, indices, 3),
        "degenerate normal generation reports no accumulated faces",
        failures);
    for (const full_renderer::MeshVertex& vertex : vertices)
    {
        expect(vertex.normal[0] == 0.0f && vertex.normal[1] == 1.0f && vertex.normal[2] == 0.0f,
            "degenerate terrain normal falls back to world up",
            failures);
    }
}

void invalidHeightfieldInputsAreRejected(int& failures)
{
    const std::vector<float> tooFewHeights = {0.0f, 1.0f};
    const full_renderer::terrain::TerrainGridMeshData tooFew =
        full_renderer::terrain::generateTerrainGridMeshWithHeights(2, 2.0f, tooFewHeights.data(), 2);
    expect(tooFew.vertices.empty() && tooFew.indices.empty(), "height count mismatch is rejected", failures);

    const std::vector<float> nonFiniteHeights = {
        0.0f, 0.0f, 0.0f,
        0.0f, std::numeric_limits<float>::quiet_NaN(), 0.0f,
        0.0f, 0.0f, 0.0f,
    };
    const full_renderer::terrain::TerrainGridMeshData nonFinite =
        full_renderer::terrain::generateTerrainGridMeshWithHeights(
            2,
            2.0f,
            nonFiniteHeights.data(),
            static_cast<std::uint32_t>(nonFiniteHeights.size()));
    expect(nonFinite.vertices.empty() && nonFinite.indices.empty(), "non-finite heights are rejected", failures);
}

void generationIsDeterministic(int& failures)
{
    const full_renderer::terrain::TerrainGridMeshData first =
        full_renderer::terrain::generateTerrainGridMesh(8, 5.0f);
    const full_renderer::terrain::TerrainGridMeshData second =
        full_renderer::terrain::generateTerrainGridMesh(8, 5.0f);

    expect(first.vertices.size() == second.vertices.size(), "deterministic grid vertex count", failures);
    expect(first.indices == second.indices, "deterministic grid indices", failures);
    for (std::size_t index = 0; index < first.vertices.size() && index < second.vertices.size(); ++index)
    {
        const full_renderer::MeshVertex& a = first.vertices[index];
        const full_renderer::MeshVertex& b = second.vertices[index];
        expect(a.position[0] == b.position[0] &&
                a.position[1] == b.position[1] &&
                a.position[2] == b.position[2],
            "deterministic grid positions",
            failures);
        expect(a.colorLinear[0] == b.colorLinear[0] &&
                a.colorLinear[1] == b.colorLinear[1] &&
                a.colorLinear[2] == b.colorLinear[2] &&
                a.colorLinear[3] == b.colorLinear[3],
            "deterministic grid colors",
            failures);
        expect(a.normal[0] == b.normal[0] &&
                a.normal[1] == b.normal[1] &&
                a.normal[2] == b.normal[2],
            "deterministic grid normals",
            failures);
    }
}

void invalidInputsAreRejected(int& failures)
{
    expect(!full_renderer::terrain::canGenerateTerrainGridMesh(0, 1.0f),
        "zero subdivisions are rejected",
        failures);
    expect(!full_renderer::terrain::canGenerateTerrainGridMesh(1, 0.0f),
        "zero size is rejected",
        failures);
    expect(!full_renderer::terrain::canGenerateTerrainGridMesh(1, -1.0f),
        "negative size is rejected",
        failures);
    expect(!full_renderer::terrain::canGenerateTerrainGridMesh(
            1,
            std::numeric_limits<float>::quiet_NaN()),
        "non-finite size is rejected",
        failures);
    expect(!full_renderer::terrain::canGenerateTerrainGridMesh(256, 1.0f),
        "too many vertices for 16-bit indices are rejected",
        failures);
    expect(!full_renderer::terrain::canGenerateTerrainGridMesh(
            1,
            1.0f,
            full_renderer::terrain::TerrainGridMeshOptions{-0.1f}),
        "negative skirt depth is rejected",
        failures);
    expect(!full_renderer::terrain::canGenerateTerrainGridMesh(
            1,
            1.0f,
            full_renderer::terrain::TerrainGridMeshOptions{std::numeric_limits<float>::infinity()}),
        "non-finite skirt depth is rejected",
        failures);

    const full_renderer::terrain::TerrainGridMeshData invalid =
        full_renderer::terrain::generateTerrainGridMesh(0, 1.0f);
    expect(invalid.vertices.empty() && invalid.indices.empty(), "invalid generation returns empty data", failures);
    expect(invalid.meshDesc().vertices == nullptr && invalid.meshDesc().indices == nullptr,
        "invalid mesh descriptor has null buffers",
        failures);
}
} // namespace

int main()
{
    int failures = 0;
    expectedCounts(failures);
    positionsNormalsAndIndicesAreValid(failures);
    heightfieldNormalsAreGenerated(failures);
    skirtGeometryIsGenerated(failures);
    slopedSkirtGenerationIsDeterministic(failures);
    normalGenerationHandlesDegenerateTriangles(failures);
    generationIsDeterministic(failures);
    invalidInputsAreRejected(failures);
    invalidHeightfieldInputsAreRejected(failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
