#pragma once

namespace full_engine
{
/**
 * @brief Narrow placeholder for the engine-to-renderer boundary.
 *
 * The header intentionally exposes no renderer, bgfx, SDL3, ImGui, or sample
 * app types. Renderer public headers are included only by the implementation.
 */
class RendererHost
{
public:
    /** @brief Smoke-checks that the engine target links against the renderer API. */
    bool rendererApiLinked() const;
};
} // namespace full_engine
