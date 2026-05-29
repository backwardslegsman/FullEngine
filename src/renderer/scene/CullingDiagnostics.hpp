#pragma once

#include "full_renderer/Renderer.hpp"
#include "renderer/scene/Shadow.hpp"

namespace full_renderer::scene
{
struct SharedCullingDiagnosticsPlan
{
    CullingCategoryStats staticMeshes;
    CullingCategoryStats instancedMeshes;
    CullingCategoryStats skinnedMeshes;
    std::uint32_t shadowCascadeCount = 0;
};

SharedCullingDiagnosticsPlan buildSharedCullingDiagnosticsPlan(const RenderPacket& packet) noexcept;

void copySharedCullingDiagnosticsToStats(
    const SharedCullingDiagnosticsPlan& plan,
    RendererStats& stats) noexcept;
} // namespace full_renderer::scene
