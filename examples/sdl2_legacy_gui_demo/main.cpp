#include <igui/iGUI.hpp>
#include <igui_sdl2/SdlBackend.hpp>

#include <SDL.h>
#include <cstdio>
#include <cstring>

int main(int argc, char **argv)
{
    const bool benchmark = argc > 1 && std::strcmp(argv[1], "--benchmark") == 0;
    const bool smoke = benchmark || (argc > 1 && std::strcmp(argv[1], "--smoke") == 0);
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;

    const Uint32 flags = SDL_WINDOW_RESIZABLE | (smoke ? SDL_WINDOW_HIDDEN : 0u);
    SDL_Window *window = SDL_CreateWindow("iGUI GUI.cpp widget tests - Backend SDL2",
                                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                           1220, 760, flags);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : nullptr;
    if (!renderer)
    {
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 2;
    }

    ig::sdl2::Backend backend(renderer);
    if (!backend.prepareFontAtlas())
        return 3;

    GUI gui;
    gui.Init(&backend, backend.fontAtlas().defaultFont());
    bool running = true;
    bool checked = true;
    bool toggle = true;
    float value = 0.5f;
    float dragValue = 2.5f;
    float verticalFloat = 0.65f;
    int integerValue = 42;
    int verticalInt = 65;
    int radio = 0;
    int dropdown = 1;
    int listSelection = 2;
    Color color(70, 150, 220, 255);
    char text[128] = "editable text";
    const char *items[] = {"First", "Second", "Third", "Fourth", "Fifth", "Sixth"};
    uint64_t previous = SDL_GetPerformanceCounter();
    const uint64_t benchmarkStart = previous;
    int frames = 0;

    while (running)
    {
        SDL_Event nativeEvent;
        while (SDL_PollEvent(&nativeEvent))
        {
            if (nativeEvent.type == SDL_QUIT)
                running = false;
            ig::Event event;
            if (ig::sdl2::translateEvent(nativeEvent, event))
                gui.PushEvent(event);
        }

        const uint64_t now = SDL_GetPerformanceCounter();
        const float delta = static_cast<float>(now - previous) /
                            static_cast<float>(SDL_GetPerformanceFrequency());
        previous = now;
        const ig::FrameInfo frame = ig::sdl2::frameInfo(window, delta);

        SDL_SetRenderDrawColor(renderer, 24, 26, 30, 255);
        SDL_RenderClear(renderer);
        gui.BeginFrame(frame);
        if (gui.BeginWindow("GUI.cpp widgets through ig::Backend",
                            20.0f, 20.0f, 1170.0f, 700.0f))
        {
            gui.LabelColored("Original immediate widgets, backend-independent",
                             Color(100, 180, 255, 255), 14.0f, 12.0f);
            gui.SeparatorText("Basic controls", 14.0f, 42.0f, 250.0f);
            gui.Button("Button", 14.0f, 58.0f, 120.0f, 28.0f);
            gui.Checkbox("Checkbox", &checked, 150.0f, 64.0f, 18.0f);
            gui.ToggleSwitch("Toggle", &toggle, 14.0f, 102.0f, 46.0f, 22.0f);
            gui.RadioButton("A", &radio, 0, 14.0f, 142.0f, 18.0f);
            gui.RadioButton("B", &radio, 1, 85.0f, 142.0f, 18.0f);

            gui.SeparatorText("Values", 14.0f, 185.0f, 250.0f);
            gui.SliderFloat("Float", &value, 0.0f, 1.0f,
                            14.0f, 205.0f, 250.0f, 20.0f);
            gui.SliderInt("Int", &integerValue, 0, 100,
                          14.0f, 240.0f, 250.0f, 20.0f);
            gui.DragFloat("Drag", &dragValue, 0.05f, -10.0f, 10.0f,
                          14.0f, 275.0f, 250.0f, 24.0f);
            gui.ProgressBar(value, 14.0f, 315.0f, 250.0f, 20.0f,
                            Orientation::Horizontal, "progress");
            gui.TextInput("Text", text, sizeof(text), 55.0f, 355.0f, 209.0f, 26.0f);

            gui.SeparatorText("Selection", 300.0f, 42.0f, 220.0f);
            gui.Dropdown("Dropdown", &dropdown, items, 6,
                         300.0f, 75.0f, 220.0f, 28.0f);
            gui.ListBox("ListBox", &listSelection, items, 6,
                        300.0f, 130.0f, 220.0f, 210.0f, 5);
            gui.Spinner(410.0f, 390.0f, 22.0f);

            gui.SeparatorText("ColorPicker", 555.0f, 42.0f, 210.0f);
            gui.ColorPicker("Color", &color, 555.0f, 65.0f, 210.0f);

            gui.SeparatorText("Vertical", 815.0f, 42.0f, 300.0f);
            gui.SliderFloatVertical("Float", &verticalFloat, 0.0f, 1.0f,
                                    835.0f, 90.0f, 28.0f, 230.0f);
            gui.SliderIntVertical("Int", &verticalInt, 0, 100,
                                  930.0f, 90.0f, 28.0f, 230.0f);
            gui.ProgressBar(value, 1040.0f, 90.0f, 30.0f, 230.0f,
                            Orientation::Vertical, nullptr);
            gui.Separator(815.0f, 370.0f, 300.0f);
            gui.Text(815.0f, 390.0f, "float %.2f / int %d", value, integerValue);
            gui.EndWindow();
        }
        gui.EndFrame();
        SDL_RenderPresent(renderer);

        if (smoke && ++frames >= (benchmark ? 300 : 3))
            running = false;
    }

    if (benchmark)
    {
        const double seconds = static_cast<double>(SDL_GetPerformanceCounter() - benchmarkStart) /
                               static_cast<double>(SDL_GetPerformanceFrequency());
        std::printf("300 frames in %.3f ms (%.1f FPS)\n",
                    seconds * 1000.0, 300.0 / seconds);
    }

    gui.Release();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
