#pragma once

#include "full_renderer/Terrain.hpp"

#include <cstdint>
#include <vector>

namespace full_renderer::debug
{
using TerrainDebugSnapshot = std::vector<TerrainChunkDebugInfo>;
using TerrainBatchDebugSnapshot = std::vector<TerrainBatchDebugInfo>;

struct DebugLineVertex
{
    float position[3] = {};
    float colorLinear[4] = {};
};

bool hasTerrainGpuDebugOverlay(const TerrainDebugOptions& options) noexcept;

std::uint32_t buildTerrainDebugLines(
    const TerrainDebugOptions& options,
    const TerrainChunkDebugInfo* chunks,
    std::uint32_t chunkCount,
    std::vector<DebugLineVertex>& outLines);

std::uint32_t appendShadowFrustumDebugLines(
    const float corners[8][3],
    std::vector<DebugLineVertex>& outLines);
std::uint32_t appendShadowFrustumDebugLines(
    const float corners[8][3],
    const float colorLinear[4],
    std::vector<DebugLineVertex>& outLines);

std::uint32_t appendAabbDebugLines(
    const Aabb& bounds,
    const float colorLinear[4],
    std::vector<DebugLineVertex>& outLines);
} // namespace full_renderer::debug
