#include "full_renderer/Animation.hpp"
#include "full_renderer/ColorGrading.hpp"
#include "full_renderer/Fade.hpp"
#include "full_renderer/Handles.hpp"
#include "full_renderer/Renderer.hpp"
#include "full_renderer/Terrain.hpp"

#include <memory>
#include <type_traits>

namespace
{
static_assert(std::is_default_constructible<full_renderer::RendererInitDesc>::value, "RendererInitDesc is value-type");
static_assert(std::is_default_constructible<full_renderer::RenderPacket>::value, "RenderPacket is value-type");
static_assert(std::is_default_constructible<full_renderer::MeshDesc>::value, "MeshDesc is value-type");
static_assert(std::is_default_constructible<full_renderer::TextureDesc>::value, "TextureDesc is value-type");
static_assert(std::is_default_constructible<full_renderer::SkeletonDesc>::value, "SkeletonDesc is value-type");
static_assert(std::is_default_constructible<full_renderer::TerrainChunkDesc>::value, "TerrainChunkDesc is value-type");

void touchPublicApiTypes()
{
    full_renderer::RendererInitDesc init;
    init.backbufferWidth = 1;
    init.backbufferHeight = 1;

    full_renderer::TextureDesc texture;
    texture.semantic = full_renderer::TextureSemantic::Color;
    texture.colorSpace = full_renderer::TextureColorSpace::Srgb;

    full_renderer::MaterialDesc material;
    material.alphaMode = full_renderer::MaterialAlphaMode::Opaque;

    full_renderer::RenderPacket packet;
    packet.drawItemCount = 0;

    full_renderer::TerrainChunkDesc terrain;
    terrain.lodCount = 0;
    const full_renderer::TerrainChunkHandle terrainHandle = {};

    full_renderer::SkinningPaletteDesc palette;
    palette.matrixCount = 0;

    const std::unique_ptr<full_renderer::IRenderer> renderer = full_renderer::createRenderer();
    auto updateTerrainChunk = &full_renderer::IRenderer::updateTerrainChunk;
    auto copyTerrainShadowCasterDebugInfo = &full_renderer::IRenderer::copyTerrainShadowCasterDebugInfo;
    (void)init;
    (void)texture;
    (void)material;
    (void)packet;
    (void)terrain;
    (void)terrainHandle;
    (void)palette;
    (void)renderer;
    (void)updateTerrainChunk;
    (void)copyTerrainShadowCasterDebugInfo;
}
} // namespace

int main()
{
    touchPublicApiTypes();
    return 0;
}
