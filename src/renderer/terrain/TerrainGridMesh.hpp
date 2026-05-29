#pragma once

#include "full_renderer/Renderer.hpp"

#include <cstdint>
#include <vector>

namespace full_renderer::terrain
{
struct TerrainGridMeshOptions
{
    float skirtDepthMeters = 0.0f;
};

struct TerrainGridMeshData
{
    std::vector<MeshVertex> vertices;
    std::vector<std::uint16_t> indices;

    MeshDesc meshDesc() const noexcept;
};

bool canGenerateTerrainGridMesh(std::uint32_t subdivisions, float sizeMeters) noexcept;
bool canGenerateTerrainGridMesh(
    std::uint32_t subdivisions,
    float sizeMeters,
    const TerrainGridMeshOptions& options) noexcept;
TerrainGridMeshData generateTerrainGridMesh(std::uint32_t subdivisions, float sizeMeters);
TerrainGridMeshData generateTerrainGridMeshWithHeights(
    std::uint32_t subdivisions,
    float sizeMeters,
    const float* heights,
    std::uint32_t heightCount);
TerrainGridMeshData generateTerrainGridMeshWithHeights(
    std::uint32_t subdivisions,
    float sizeMeters,
    const float* heights,
    std::uint32_t heightCount,
    const TerrainGridMeshOptions& options);
bool generateTerrainMeshNormals(
    MeshVertex* vertices,
    std::uint32_t vertexCount,
    const std::uint16_t* indices,
    std::uint32_t indexCount) noexcept;
} // namespace full_renderer::terrain
