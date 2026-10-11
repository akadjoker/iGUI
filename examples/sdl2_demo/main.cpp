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
    bool firstChoice = true;
    int selectedEntry = 0;
    float volume = 0.5f;
    ig::String name("player");
    int quality = 1;
    const ig::StringView qualityItems[] = {
        ig::StringView("low"), ig::StringView("medium"), ig::StringView("high")
    };
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
        // A barra da aplicação, no topo do ecrã e fora da janela: o clique dela é do
        // menu, não do que estiver por baixo (a janela do demo começa em y=60).
        if (ui.beginMainMenuBar())
        {
            if (ui.beginMenu("File"))
            {
                if (ui.menuItem("New"))
                    name = ig::String("novo");
                if (ui.menuItem("Open", ig::StringView("Ctrl+O")))
                    name = ig::String("aberto");
                ui.menuSeparator();
                if (ui.menuItem("Quit", ig::StringView("Alt+F4")))
                    running = false;
                ui.endMenu();
            }
            if (ui.beginMenu("Edit"))
            {
                ui.menuItem("Undo", ig::StringView("Ctrl+Z"), false);
                ui.menuItem("Redo", ig::StringView("Ctrl+Y"), false);
                ui.menuSeparator();
                if (ui.menuItem("Enabled", ig::StringView(""), true, enabled))
                    enabled = !enabled;
                ui.endMenu();
            }
            if (ui.beginMenu("View"))
            {
                if (ui.menuItem("Toggle window"))
                    enabled = !enabled;
                ui.endMenu();
            }
            ui.endMainMenuBar();
        }
        if (ui.beginWindow("iGUI Widget Gallery",
                           ig::Rect(80.0f, 60.0f + ui.mainMenuBarHeight(), 520.0f, 520.0f)))
        {
            ui.label(u8"Olá, fonte UTF-8");
            if (ui.button("click me"))
                enabled = !enabled;
            ui.sameLine();
            ui.checkbox("enabled", enabled);
            ui.separator();
            ui.label("button + checkbox");
            ui.spacing(4.0f);
            ui.label("slider + input text");
            ui.sliderFloat("volume", volume, 0.0f, 1.0f);
            ui.inputText("name", name);
            ui.progressBar(volume, 1.0f);
            ui.separator();
            ui.label("selection");
            if (ui.selectable("entry one", selectedEntry == 0, 180.0f))
                selectedEntry = 0;
            if (ui.selectable("entry two", selectedEntry == 1, 180.0f))
                selectedEntry = 1;
            if (ui.selectable("entry three", selectedEntry == 2, 180.0f))
                selectedEntry = 2;
            if (ui.radioButton("choice one", firstChoice))
                firstChoice = true;
            if (ui.radioButton("choice two", !firstChoice))
                firstChoice = false;
            ui.comboBox("quality", quality, ig::Span<const ig::StringView>(qualityItems));
            ui.endWindow();
        }
        const ig::DrawData &drawData = ui.endFrame();

        if (ui.wantsTextInput())
            SDL_StartTextInput();
        else
            SDL_StopTextInput();

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
