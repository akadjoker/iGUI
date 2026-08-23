#pragma once

#include <stdint.h>

#include <SDL.h>

#include <ct/vector.hpp>

#include <igui/Backend.hpp>
#include <igui/Events.hpp>
#include <igui/FontAtlas.hpp>

namespace ig
{
namespace sdl2
{

// Converte eventos SDL para o formato independente da plataforma do core.
// Eventos sem equivalente em ig::Event devolvem false e continuam a pertencer à aplicação.
bool translateEvent(const SDL_Event &nativeEvent, Event &event, float dpiScale = 1.0f);

FrameInfo frameInfo(SDL_Window *window, float deltaSeconds, float dpiScale = 1.0f);

// O TextureId do backend SDL2 é o ponteiro SDL_Texture codificado de forma opaca.
TextureId textureId(SDL_Texture *texture);

// Backend SDL2 para geometria e texto UTF-8 através do atlas de fonte embebido.
// O chamador continua responsável por limpar o render target e por SDL_RenderPresent().
class Backend : public ig::Backend
{
public:
    explicit Backend(SDL_Renderer *renderer);
    ~Backend() override;

    void setRenderer(SDL_Renderer *renderer);
    SDL_Renderer *renderer() const;
    FontAtlas &fontAtlas();
    bool prepareFontAtlas();

    TextMetrics measureText(FontId font, StringView text,
                            float logicalSize, float dpiScale) override;
    bool render(const DrawData &data) override;

private:
    SDL_Renderer *renderer_;
    FontAtlas fontAtlas_;
    SDL_Texture *fontTexture_;
    ct::Vector<SDL_Vertex> vertices_;
    ct::Vector<int> indices_;
    ct::Vector<uint8_t> fontPixels_;

    bool setClip(const Rect &clip);
    bool ensureFontTexture();
    bool renderGeometry(const DrawData &data, const GeometryCommand &command);
    bool renderText(const DrawData &data, const TextCommand &command);
};

} // namespace sdl2
} // namespace ig
