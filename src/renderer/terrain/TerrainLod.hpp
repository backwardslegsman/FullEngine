#pragma once

#include "full_renderer/Terrain.hpp"

namespace full_renderer::terrain
{
bool hasValidLodCount(std::uint32_t lodCount) noexcept;
std::uint32_t selectLod(const TerrainLodDesc* lods, std::uint32_t lodCount, float distanceMeters) noexcept;
} // namespace full_renderer::terrain
