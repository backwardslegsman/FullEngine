#include "full_renderer/Animation.hpp"
#include "full_renderer/ColorGrading.hpp"
#include "full_renderer/Fade.hpp"
#include "full_renderer/Handles.hpp"
#include "full_renderer/Renderer.hpp"
#include "full_renderer/Terrain.hpp"

#include <memory>
#include <type_traits>

int main()
{
    static_assert(std::is_default_constructible<full_renderer::RendererInitDesc>::value,
                  "RendererInitDesc must remain externally constructible");
    static_assert(std::is_default_constructible<full_renderer::RenderPacket>::value,
                  "RenderPacket must remain externally constructible");
    static_assert(std::is_default_constructible<full_renderer::MeshDesc>::value,
                  "MeshDesc must remain externally constructible");
    static_assert(std::is_default_constructible<full_renderer::TextureDesc>::value,
                  "TextureDesc must remain externally constructible");
    static_assert(std::is_default_constructible<full_renderer::MaterialDesc>::value,
                  "MaterialDesc must remain externally constructible");
    static_assert(std::is_default_constructible<full_renderer::TerrainChunkDesc>::value,
                  "TerrainChunkDesc must remain externally constructible");
    static_assert(std::is_default_constructible<full_renderer::SkeletonDesc>::value,
                  "SkeletonDesc must remain externally constructible");

    full_renderer::RendererInitDesc init = {};
    init.backbufferWidth = 1;
    init.backbufferHeight = 1;

    full_renderer::TextureDesc texture = {};
    texture.width = 1;
    texture.height = 1;
    texture.semantic = full_renderer::TextureSemantic::Color;
    texture.colorSpace = full_renderer::TextureColorSpace::Srgb;

    full_renderer::MaterialDesc material = {};
    material.alphaMode = full_renderer::MaterialAlphaMode::Opaque;

    full_renderer::RenderPacket packet = {};
    packet.colorGrading.enabled = false;
    packet.selectionOutline.enabled = false;

    full_renderer::TerrainChunkDesc terrain = {};
    terrain.lodCount = 0;
    full_renderer::TerrainChunkHandle terrainHandle = {};

    full_renderer::SkinningPaletteDesc palette = {};
    palette.matrixCount = 0;

    const std::unique_ptr<full_renderer::IRenderer> renderer = full_renderer::createRenderer();
    if (renderer)
    {
        const full_renderer::RendererResult updateResult =
            renderer->updateTerrainChunk(terrainHandle, terrain);
        if (updateResult != full_renderer::RendererResult::NotInitialized)
        {
            return 2;
        }

        full_renderer::TerrainChunkDebugInfo shadowCasterDebug = {};
        (void)renderer->copyTerrainShadowCasterDebugInfo(&shadowCasterDebug, 1);
    }

    return renderer ? 0 : 1;
}
