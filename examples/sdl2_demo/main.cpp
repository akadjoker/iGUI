#include <SDL.h>

#include <igui/Gui.hpp>
#include <igui_sdl2/SdlBackend.hpp>

int main(int, char **)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;

    SDL_Window *window = SDL_CreateWindow("iGUI SDL2 demo", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 960, 640,
                                          SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        SDL_Quit();
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    ig::sdl2::Backend backend(renderer);
    if (!backend.prepareFontAtlas())
    {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    ig::Context ui(backend, &backend.fontAtlas());
    bool running = true;
    bool enabled = false;
    uint64_t previousCounter = SDL_GetPerformanceCounter();
    const uint64_t frequency = SDL_GetPerformanceFrequency();

    while (running)
    {
        const uint64_t currentCounter = SDL_GetPerformanceCounter();
        const float deltaSeconds = static_cast<float>(currentCounter - previousCounter) /
                                   static_cast<float>(frequency);
        previousCounter = currentCounter;

        SDL_Event nativeEvent;
        while (SDL_PollEvent(&nativeEvent))
        {
            if (nativeEvent.type == SDL_QUIT)
                running = false;
            ig::Event event;
            if (ig::sdl2::translateEvent(nativeEvent, event))
                ui.pushEvent(event);
        }

        ui.beginFrame(ig::sdl2::frameInfo(window, deltaSeconds));
        if (ui.beginWindow("iGUI SDL2", ig::Rect(80.0f, 70.0f, 360.0f, 220.0f)))
        {
            ui.label(u8"Olá, fonte UTF-8", ig::Vec2(12.0f, 12.0f));
            if (ui.button("click me", ig::Rect(12.0f, 48.0f, 130.0f, 30.0f)))
                enabled = !enabled;
            ui.checkbox("enabled", enabled, ig::Rect(12.0f, 94.0f, 18.0f, 18.0f));
            ui.endWindow();
        }
        const ig::DrawData &drawData = ui.endFrame();

        SDL_SetRenderDrawColor(renderer, 32u, 34u, 40u, 255u);
        SDL_RenderClear(renderer);
        if (!backend.render(drawData))
            running = false;
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
