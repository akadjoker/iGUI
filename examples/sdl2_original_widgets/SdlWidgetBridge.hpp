#pragma once

#include <SDL.h>
#include <BuGUI.hpp>
#include <vector>

class SdlWidgetBridge
{
public:
    explicit SdlWidgetBridge(SDL_Renderer *renderer) : renderer_(renderer) {}
    BuGUI::TextureHandle createTexture(int width, int height, const unsigned char *rgba);
    void destroyTexture(BuGUI::TextureHandle texture);
    bool render(BuGUI::DrawData &data);

private:
    SDL_Renderer *renderer_;
    std::vector<SDL_Vertex> vertices_;
    std::vector<int> indices_;
};
