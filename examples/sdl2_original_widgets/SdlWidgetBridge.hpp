#pragma once

#include <SDL.h>
#include <igui/widgets/Retained.hpp>
#include <ct/vector.hpp>

class SdlWidgetBridge
{
public:
    explicit SdlWidgetBridge(SDL_Renderer *renderer) : renderer_(renderer) {}
    ig::retained::TextureHandle createTexture(int width, int height, const unsigned char *rgba);
    void destroyTexture(ig::retained::TextureHandle texture);
    bool render(ig::retained::DrawData &data);

private:
    SDL_Renderer *renderer_;
    ct::Vector<SDL_Vertex> vertices_;
    ct::Vector<int> indices_;
};
