#include "engine/renderer_integration/RendererHost.hpp"

#include "full_renderer/Renderer.hpp"

#include <memory>

namespace full_engine
{
bool RendererHost::rendererApiLinked() const
{
    const std::unique_ptr<full_renderer::IRenderer> renderer = full_renderer::createRenderer();
    return renderer != nullptr;
}
} // namespace full_engine
