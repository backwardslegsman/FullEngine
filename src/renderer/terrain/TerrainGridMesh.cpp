#include "renderer/terrain/TerrainGridMesh.hpp"

#include <cmath>
#include <limits>

namespace full_renderer::terrain
{
namespace
{
constexpr std::uint32_t kMaxIndexValue = std::numeric_limits<std::uint16_t>::max();

bool isValidSkirtDepth(const float skirtDepthMeters) noexcept
{
    return std::isfinite(skirtDepthMeters) && skirtDepthMeters >= 0.0f;
}

float normalizedCoordinate(const std::uint32_t value, const std::uint32_t subdivisions) noexcept
{
    return static_cast<float>(value) / static_cast<float>(subdivisions);
}

void setVertex(
    MeshVertex& vertex,
    const float x,
    const float y,
    const float z,
    const float u,
    const float v) noexcept
{
    vertex.position[0] = x;
    vertex.position[1] = y;
    vertex.position[2] = z;
    vertex.normal[0] = 0.0f;
    vertex.normal[1] = 1.0f;
    vertex.normal[2] = 0.0f;
    vertex.uv0[0] = u;
    vertex.uv0[1] = v;
    vertex.colorLinear[0] = 0.28f + 0.10f * u;
    vertex.colorLinear[1] = 0.58f + 0.16f * v;
    vertex.colorLinear[2] = 0.30f + 0.08f * (1.0f - u);
    vertex.colorLinear[3] = 1.0f;
}

bool isFiniteHeightData(const float* heights, const std::uint32_t heightCount) noexcept
{
    if (heights == nullptr || heightCount == 0)
    {
        return false;
    }

    for (std::uint32_t index = 0; index < heightCount; ++index)
    {
        if (!std::isfinite(heights[index]))
        {
            return false;
        }
    }

    return true;
}

void addNormal(
    MeshVertex& vertex,
    const float x,
    const float y,
    const float z) noexcept
{
    vertex.normal[0] += x;
    vertex.normal[1] += y;
    vertex.normal[2] += z;
}

void setFallbackNormal(MeshVertex& vertex) noexcept
{
    vertex.normal[0] = 0.0f;
    vertex.normal[1] = 1.0f;
    vertex.normal[2] = 0.0f;
}

std::uint32_t gridVertexIndex(
    const std::uint32_t x,
    const std::uint32_t z,
    const std::uint32_t verticesPerSide) noexcept
{
    return z * verticesPerSide + x;
}

void appendSkirtEdge(
    TerrainGridMeshData& data,
    const std::vector<std::uint32_t>& edgeVertices,
    const float skirtDepthMeters)
{
    const std::uint32_t bottomStart = static_cast<std::uint32_t>(data.vertices.size());
    data.vertices.reserve(data.vertices.size() + edgeVertices.size());
    for (const std::uint32_t edgeIndex : edgeVertices)
    {
        MeshVertex skirtVertex = data.vertices[edgeIndex];
        skirtVertex.position[1] -= skirtDepthMeters;
        data.vertices.push_back(skirtVertex);
    }

    for (std::uint32_t index = 0; index + 1U < edgeVertices.size(); ++index)
    {
        const std::uint16_t top0 = static_cast<std::uint16_t>(edgeVertices[index]);
        const std::uint16_t top1 = static_cast<std::uint16_t>(edgeVertices[index + 1U]);
        const std::uint16_t bottom0 = static_cast<std::uint16_t>(bottomStart + index);
        const std::uint16_t bottom1 = static_cast<std::uint16_t>(bottomStart + index + 1U);

        data.indices.push_back(top0);
        data.indices.push_back(bottom0);
        data.indices.push_back(bottom1);
        data.indices.push_back(top0);
        data.indices.push_back(bottom1);
        data.indices.push_back(top1);
    }
}

void appendTerrainSkirts(
    TerrainGridMeshData& data,
    const std::uint32_t subdivisions,
    const float skirtDepthMeters)
{
    if (skirtDepthMeters <= 0.0f)
    {
        return;
    }

    const std::uint32_t verticesPerSide = subdivisions + 1U;
    std::vector<std::uint32_t> edge;
    edge.reserve(verticesPerSide);

    edge.clear();
    for (std::uint32_t x = 0; x < verticesPerSide; ++x)
    {
        edge.push_back(gridVertexIndex(x, 0U, verticesPerSide));
    }
    appendSkirtEdge(data, edge, skirtDepthMeters);

    edge.clear();
    for (std::uint32_t z = 0; z < verticesPerSide; ++z)
    {
        edge.push_back(gridVertexIndex(verticesPerSide - 1U, z, verticesPerSide));
    }
    appendSkirtEdge(data, edge, skirtDepthMeters);

    edge.clear();
    for (std::uint32_t x = verticesPerSide; x > 0U; --x)
    {
        edge.push_back(gridVertexIndex(x - 1U, verticesPerSide - 1U, verticesPerSide));
    }
    appendSkirtEdge(data, edge, skirtDepthMeters);

    edge.clear();
    for (std::uint32_t z = verticesPerSide; z > 0U; --z)
    {
        edge.push_back(gridVertexIndex(0U, z - 1U, verticesPerSide));
    }
    appendSkirtEdge(data, edge, skirtDepthMeters);
}
} // namespace

MeshDesc TerrainGridMeshData::meshDesc() const noexcept
{
    MeshDesc desc;
    desc.vertices = vertices.empty() ? nullptr : vertices.data();
    desc.vertexCount = static_cast<std::uint32_t>(vertices.size());
    desc.indices = indices.empty() ? nullptr : indices.data();
    desc.indexCount = static_cast<std::uint32_t>(indices.size());
    return desc;
}

bool canGenerateTerrainGridMesh(const std::uint32_t subdivisions, const float sizeMeters) noexcept
{
    return canGenerateTerrainGridMesh(subdivisions, sizeMeters, {});
}

bool canGenerateTerrainGridMesh(
    const std::uint32_t subdivisions,
    const float sizeMeters,
    const TerrainGridMeshOptions& options) noexcept
{
    if (subdivisions == 0 || !std::isfinite(sizeMeters) || sizeMeters <= 0.0f)
    {
        return false;
    }

    if (!isValidSkirtDepth(options.skirtDepthMeters))
    {
        return false;
    }

    const std::uint32_t verticesPerSide = subdivisions + 1U;
    if (verticesPerSide == 0 || verticesPerSide > 256U)
    {
        return false;
    }

    const std::uint64_t vertexCount =
        static_cast<std::uint64_t>(verticesPerSide) * static_cast<std::uint64_t>(verticesPerSide) +
        (options.skirtDepthMeters > 0.0f ? static_cast<std::uint64_t>(verticesPerSide) * 4ULL : 0ULL);
    if (vertexCount == 0 || vertexCount > kMaxIndexValue + 1ULL)
    {
        return false;
    }

    const std::uint64_t indexCount =
        static_cast<std::uint64_t>(subdivisions) * static_cast<std::uint64_t>(subdivisions) * 6ULL +
        (options.skirtDepthMeters > 0.0f ? static_cast<std::uint64_t>(subdivisions) * 4ULL * 6ULL : 0ULL);
    return indexCount <= static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max());
}

TerrainGridMeshData generateTerrainGridMesh(const std::uint32_t subdivisions, const float sizeMeters)
{
    return generateTerrainGridMeshWithHeights(subdivisions, sizeMeters, nullptr, 0);
}

TerrainGridMeshData generateTerrainGridMeshWithHeights(
    const std::uint32_t subdivisions,
    const float sizeMeters,
    const float* heights,
    const std::uint32_t heightCount)
{
    return generateTerrainGridMeshWithHeights(subdivisions, sizeMeters, heights, heightCount, {});
}

TerrainGridMeshData generateTerrainGridMeshWithHeights(
    const std::uint32_t subdivisions,
    const float sizeMeters,
    const float* heights,
    const std::uint32_t heightCount,
    const TerrainGridMeshOptions& options)
{
    TerrainGridMeshData data;
    if (!canGenerateTerrainGridMesh(subdivisions, sizeMeters, options))
    {
        return data;
    }

    const std::uint32_t verticesPerSide = subdivisions + 1U;
    const std::uint32_t vertexCount = verticesPerSide * verticesPerSide;
    const bool useHeights = heights != nullptr;
    if (useHeights && (heightCount != vertexCount || !isFiniteHeightData(heights, heightCount)))
    {
        return data;
    }

    data.vertices.resize(static_cast<std::size_t>(verticesPerSide) * static_cast<std::size_t>(verticesPerSide));
    data.indices.reserve(static_cast<std::size_t>(subdivisions) * static_cast<std::size_t>(subdivisions) * 6U);

    for (std::uint32_t z = 0; z < verticesPerSide; ++z)
    {
        const float v = normalizedCoordinate(z, subdivisions);
        for (std::uint32_t x = 0; x < verticesPerSide; ++x)
        {
            const float u = normalizedCoordinate(x, subdivisions);
            MeshVertex& vertex = data.vertices[static_cast<std::size_t>(z) * verticesPerSide + x];
            const std::uint32_t vertexIndex = z * verticesPerSide + x;
            const float y = useHeights ? heights[vertexIndex] : 0.0f;
            setVertex(vertex, u * sizeMeters, y, v * sizeMeters, u, v);
        }
    }

    for (std::uint32_t z = 0; z < subdivisions; ++z)
    {
        for (std::uint32_t x = 0; x < subdivisions; ++x)
        {
            const std::uint32_t topLeft = z * verticesPerSide + x;
            const std::uint32_t topRight = topLeft + 1U;
            const std::uint32_t bottomLeft = topLeft + verticesPerSide;
            const std::uint32_t bottomRight = bottomLeft + 1U;

            data.indices.push_back(static_cast<std::uint16_t>(topLeft));
            data.indices.push_back(static_cast<std::uint16_t>(topRight));
            data.indices.push_back(static_cast<std::uint16_t>(bottomRight));
            data.indices.push_back(static_cast<std::uint16_t>(topLeft));
            data.indices.push_back(static_cast<std::uint16_t>(bottomRight));
            data.indices.push_back(static_cast<std::uint16_t>(bottomLeft));
        }
    }

    (void)generateTerrainMeshNormals(
        data.vertices.data(),
        static_cast<std::uint32_t>(data.vertices.size()),
        data.indices.data(),
        static_cast<std::uint32_t>(data.indices.size()));
    appendTerrainSkirts(data, subdivisions, options.skirtDepthMeters);
    return data;
}

bool generateTerrainMeshNormals(
    MeshVertex* vertices,
    const std::uint32_t vertexCount,
    const std::uint16_t* indices,
    const std::uint32_t indexCount) noexcept
{
    if (vertices == nullptr || vertexCount == 0 || indices == nullptr || indexCount == 0 || (indexCount % 3U) != 0)
    {
        return false;
    }

    for (std::uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        vertices[vertexIndex].normal[0] = 0.0f;
        vertices[vertexIndex].normal[1] = 0.0f;
        vertices[vertexIndex].normal[2] = 0.0f;
    }

    bool accumulatedAnyFace = false;
    for (std::uint32_t index = 0; index < indexCount; index += 3U)
    {
        const std::uint16_t ia = indices[index + 0U];
        const std::uint16_t ib = indices[index + 1U];
        const std::uint16_t ic = indices[index + 2U];
        if (ia >= vertexCount || ib >= vertexCount || ic >= vertexCount)
        {
            return false;
        }

        const MeshVertex& a = vertices[ia];
        const MeshVertex& b = vertices[ib];
        const MeshVertex& c = vertices[ic];
        const float edge1[3] = {
            b.position[0] - a.position[0],
            b.position[1] - a.position[1],
            b.position[2] - a.position[2]};
        const float edge2[3] = {
            c.position[0] - a.position[0],
            c.position[1] - a.position[1],
            c.position[2] - a.position[2]};
        const float faceNormal[3] = {
            edge2[1] * edge1[2] - edge2[2] * edge1[1],
            edge2[2] * edge1[0] - edge2[0] * edge1[2],
            edge2[0] * edge1[1] - edge2[1] * edge1[0]};
        const float lengthSquared =
            faceNormal[0] * faceNormal[0] +
            faceNormal[1] * faceNormal[1] +
            faceNormal[2] * faceNormal[2];
        if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0f)
        {
            continue;
        }

        addNormal(vertices[ia], faceNormal[0], faceNormal[1], faceNormal[2]);
        addNormal(vertices[ib], faceNormal[0], faceNormal[1], faceNormal[2]);
        addNormal(vertices[ic], faceNormal[0], faceNormal[1], faceNormal[2]);
        accumulatedAnyFace = true;
    }

    for (std::uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        MeshVertex& vertex = vertices[vertexIndex];
        const float lengthSquared =
            vertex.normal[0] * vertex.normal[0] +
            vertex.normal[1] * vertex.normal[1] +
            vertex.normal[2] * vertex.normal[2];
        if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0f)
        {
            setFallbackNormal(vertex);
            continue;
        }

        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        vertex.normal[0] *= inverseLength;
        vertex.normal[1] *= inverseLength;
        vertex.normal[2] *= inverseLength;
    }

    return accumulatedAnyFace;
}
} // namespace full_renderer::terrain
