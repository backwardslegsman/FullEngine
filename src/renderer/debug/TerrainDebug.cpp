#include "renderer/debug/TerrainDebug.hpp"

#include <algorithm>
#include <cmath>

namespace full_renderer::debug
{
namespace
{
struct Color
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

constexpr Color kVisibleBoundsColor = {0.20f, 0.90f, 0.35f, 1.0f};
constexpr Color kCulledBoundsColor = {0.85f, 0.30f, 0.25f, 1.0f};
constexpr Color kValidMaterialColor = {0.25f, 0.85f, 1.0f, 1.0f};
constexpr Color kMissingMaterialColor = {1.0f, 0.15f, 0.12f, 1.0f};
constexpr Color kValidSplatColor = {0.45f, 0.90f, 0.55f, 1.0f};
constexpr Color kFallbackSplatColor = {1.0f, 0.10f, 0.85f, 1.0f};
constexpr Color kInvalidColor = {0.50f, 0.50f, 0.50f, 1.0f};
constexpr Color kCameraVisibleShadowCasterColor = {0.30f, 0.95f, 1.0f, 1.0f};
constexpr Color kOffCameraShadowCasterColor = {1.0f, 0.55f, 0.10f, 1.0f};
constexpr Color kShadowFrustumColor = {0.95f, 0.95f, 0.25f, 1.0f};

constexpr Color kLodColors[kMaxTerrainLodLevels] = {
    {0.20f, 0.55f, 1.0f, 1.0f},
    {0.20f, 0.95f, 0.35f, 1.0f},
    {1.0f, 0.85f, 0.20f, 1.0f},
    {1.0f, 0.45f, 0.12f, 1.0f},
};

constexpr Color kCascadeColors[kMaxTerrainShadowCascades] = {
    {0.30f, 0.95f, 1.0f, 1.0f},
    {0.20f, 0.95f, 0.35f, 1.0f},
    {1.0f, 0.85f, 0.20f, 1.0f},
    {1.0f, 0.45f, 0.12f, 1.0f},
};

bool hasStableBounds(const TerrainChunkDebugInfo& info) noexcept
{
    return info.cullResult != TerrainCullResult::InvalidHandle;
}

bool isFiniteCorner(const float corner[3]) noexcept
{
    return corner != nullptr &&
        std::isfinite(corner[0]) &&
        std::isfinite(corner[1]) &&
        std::isfinite(corner[2]);
}

bool hasFiniteCorners(const float corners[8][3]) noexcept
{
    if (corners == nullptr)
    {
        return false;
    }

    for (int index = 0; index < 8; ++index)
    {
        if (!isFiniteCorner(corners[index]))
        {
            return false;
        }
    }

    return true;
}

Color lodColor(const std::uint32_t lod) noexcept
{
    if (lod < kMaxTerrainLodLevels)
    {
        return kLodColors[lod];
    }

    return kInvalidColor;
}

Color chooseColor(const TerrainDebugOptions& options, const TerrainChunkDebugInfo& info) noexcept
{
    if (options.drawCombinedOverlay)
    {
        if (info.selectedAsShadowCaster)
        {
            if (info.shadowCascadeIndex != kInvalidShadowCascadeIndex &&
                info.shadowCascadeIndex > 0 &&
                info.shadowCascadeIndex < kMaxTerrainShadowCascades)
            {
                return kCascadeColors[info.shadowCascadeIndex];
            }
            return info.cameraVisible ? kCameraVisibleShadowCasterColor : kOffCameraShadowCasterColor;
        }
        if (info.cullResult != TerrainCullResult::Visible)
        {
            return kCulledBoundsColor;
        }
        if (!info.hasTerrainMaterial)
        {
            return kMissingMaterialColor;
        }
        if (!info.hasSplatMap)
        {
            return kFallbackSplatColor;
        }
        return lodColor(info.selectedLod);
    }

    if (options.drawSplatFallbackOverlay)
    {
        return info.hasSplatMap ? kValidSplatColor : kFallbackSplatColor;
    }

    if (options.drawMaterialOverlay)
    {
        return info.hasTerrainMaterial ? kValidMaterialColor : kMissingMaterialColor;
    }

    if (options.drawLodOverlay)
    {
        if (info.cullResult != TerrainCullResult::Visible)
        {
            return kCulledBoundsColor;
        }
        return lodColor(info.selectedLod);
    }

    return info.cullResult == TerrainCullResult::Visible ? kVisibleBoundsColor : kCulledBoundsColor;
}

void appendLine(
    const float ax,
    const float ay,
    const float az,
    const float bx,
    const float by,
    const float bz,
    const Color color,
    std::vector<DebugLineVertex>& outLines)
{
    DebugLineVertex first;
    first.position[0] = ax;
    first.position[1] = ay;
    first.position[2] = az;
    first.colorLinear[0] = color.r;
    first.colorLinear[1] = color.g;
    first.colorLinear[2] = color.b;
    first.colorLinear[3] = color.a;

    DebugLineVertex second = first;
    second.position[0] = bx;
    second.position[1] = by;
    second.position[2] = bz;

    outLines.push_back(first);
    outLines.push_back(second);
}

void appendAabb(const Aabb& bounds, const Color color, std::vector<DebugLineVertex>& outLines)
{
    const float minX = bounds.min[0];
    const float minY = bounds.min[1];
    const float minZ = bounds.min[2];
    const float maxX = bounds.max[0];
    const float maxY = bounds.max[1];
    const float maxZ = bounds.max[2];

    appendLine(minX, minY, minZ, maxX, minY, minZ, color, outLines);
    appendLine(maxX, minY, minZ, maxX, minY, maxZ, color, outLines);
    appendLine(maxX, minY, maxZ, minX, minY, maxZ, color, outLines);
    appendLine(minX, minY, maxZ, minX, minY, minZ, color, outLines);

    appendLine(minX, maxY, minZ, maxX, maxY, minZ, color, outLines);
    appendLine(maxX, maxY, minZ, maxX, maxY, maxZ, color, outLines);
    appendLine(maxX, maxY, maxZ, minX, maxY, maxZ, color, outLines);
    appendLine(minX, maxY, maxZ, minX, maxY, minZ, color, outLines);

    appendLine(minX, minY, minZ, minX, maxY, minZ, color, outLines);
    appendLine(maxX, minY, minZ, maxX, maxY, minZ, color, outLines);
    appendLine(maxX, minY, maxZ, maxX, maxY, maxZ, color, outLines);
    appendLine(minX, minY, maxZ, minX, maxY, maxZ, color, outLines);
}
} // namespace

bool hasTerrainGpuDebugOverlay(const TerrainDebugOptions& options) noexcept
{
    return options.drawChunkBounds ||
        options.drawLodOverlay ||
        options.drawMaterialOverlay ||
        options.drawSplatFallbackOverlay ||
        options.drawCombinedOverlay;
}

std::uint32_t buildTerrainDebugLines(
    const TerrainDebugOptions& options,
    const TerrainChunkDebugInfo* chunks,
    const std::uint32_t chunkCount,
    std::vector<DebugLineVertex>& outLines)
{
    outLines.clear();
    if (!hasTerrainGpuDebugOverlay(options) || chunks == nullptr || chunkCount == 0)
    {
        return 0;
    }

    outLines.reserve(static_cast<std::size_t>(chunkCount) * 24U);
    for (std::uint32_t index = 0; index < chunkCount; ++index)
    {
        const TerrainChunkDebugInfo& info = chunks[index];
        if (!hasStableBounds(info))
        {
            continue;
        }

        if (options.drawLodOverlay && !options.drawCombinedOverlay && info.cullResult != TerrainCullResult::Visible)
        {
            continue;
        }

        appendAabb(info.bounds, chooseColor(options, info), outLines);
    }

    return static_cast<std::uint32_t>(outLines.size());
}

std::uint32_t appendShadowFrustumDebugLines(
    const float corners[8][3],
    const float colorLinear[4],
    std::vector<DebugLineVertex>& outLines)
{
    if (!hasFiniteCorners(corners))
    {
        return 0;
    }

    Color color = kShadowFrustumColor;
    if (colorLinear != nullptr)
    {
        color = {colorLinear[0], colorLinear[1], colorLinear[2], colorLinear[3]};
    }

    const std::uint32_t previousSize = static_cast<std::uint32_t>(outLines.size());
    constexpr std::uint32_t kEdges[12][2] = {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 0},
        {4, 5},
        {5, 6},
        {6, 7},
        {7, 4},
        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7},
    };

    outLines.reserve(outLines.size() + 24U);
    for (const auto& edge : kEdges)
    {
        const float* a = corners[edge[0]];
        const float* b = corners[edge[1]];
        appendLine(a[0], a[1], a[2], b[0], b[1], b[2], color, outLines);
    }

    return static_cast<std::uint32_t>(outLines.size()) - previousSize;
}

std::uint32_t appendShadowFrustumDebugLines(
    const float corners[8][3],
    std::vector<DebugLineVertex>& outLines)
{
    return appendShadowFrustumDebugLines(corners, nullptr, outLines);
}

std::uint32_t appendAabbDebugLines(
    const Aabb& bounds,
    const float colorLinear[4],
    std::vector<DebugLineVertex>& outLines)
{
    if (!std::isfinite(bounds.min[0]) ||
        !std::isfinite(bounds.min[1]) ||
        !std::isfinite(bounds.min[2]) ||
        !std::isfinite(bounds.max[0]) ||
        !std::isfinite(bounds.max[1]) ||
        !std::isfinite(bounds.max[2]) ||
        bounds.min[0] > bounds.max[0] ||
        bounds.min[1] > bounds.max[1] ||
        bounds.min[2] > bounds.max[2])
    {
        return 0;
    }

    Color color = kShadowFrustumColor;
    if (colorLinear != nullptr)
    {
        color = {colorLinear[0], colorLinear[1], colorLinear[2], colorLinear[3]};
    }

    const std::uint32_t previousSize = static_cast<std::uint32_t>(outLines.size());
    appendAabb(bounds, color, outLines);
    return static_cast<std::uint32_t>(outLines.size()) - previousSize;
}
} // namespace full_renderer::debug
