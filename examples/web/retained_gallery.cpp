#include "SdlWidgetBridge.hpp"

#include <WidgetApp.hpp>
#include <TreePropertyColorWidgets.hpp>

#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

void registerDemoStage(ig::retained::WidgetApp &app);
void registerMenuStage(ig::retained::WidgetApp &app);
void registerBasicStage(ig::retained::WidgetApp &app);
void registerControlsStage(ig::retained::WidgetApp &app);
void registerScrollStage(ig::retained::WidgetApp &app);
void registerInputsStage(ig::retained::WidgetApp &app);
void registerMenusStage(ig::retained::WidgetApp &app);
void registerDialogsStage(ig::retained::WidgetApp &app);
void registerDockStage(ig::retained::WidgetApp &app);
void registerPropertiesStage(ig::retained::WidgetApp &app);
void registerEditorStage(ig::retained::WidgetApp &app);
void registerNodeStage(ig::retained::WidgetApp &app);
void registerTimelineStage(ig::retained::WidgetApp &app);
void registerGizmosStage(ig::retained::WidgetApp &app);
void registerToolsStage(ig::retained::WidgetApp &app);
void registerGalleryStage(ig::retained::WidgetApp &app);
void registerSpecialtyStage(ig::retained::WidgetApp &app);

namespace
{
uint32_t nextUtf8(const char *&text)
{
    const unsigned char *s = reinterpret_cast<const unsigned char *>(text);
    if (s[0] < 0x80u) { ++text; return s[0]; }
    if ((s[0] & 0xe0u) == 0xc0u) { text += 2; return ((s[0] & 0x1fu) << 6) | (s[1] & 0x3fu); }
    if ((s[0] & 0xf0u) == 0xe0u) { text += 3; return ((s[0] & 0x0fu) << 12) | ((s[1] & 0x3fu) << 6) | (s[2] & 0x3fu); }
    if ((s[0] & 0xf8u) == 0xf0u) { text += 4; return ((s[0] & 7u) << 18) | ((s[1] & 0x3fu) << 12) | ((s[2] & 0x3fu) << 6) | (s[3] & 0x3fu); }
    ++text; return 0xfffdu;
}

struct Gallery
{
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SdlWidgetBridge *bridge = nullptr;
    ig::retained::FontAtlas atlas;
    const ig::retained::Font *font = nullptr;
    uint64_t previous = 0;
    bool running = true;
    bool latched[3] = {false, false, false};
};

Gallery g;

void frame(void *)
{
    auto &io = ig::retained::GetIO();
    auto &app = ig::retained::WidgetApp::instance();

#ifdef __EMSCRIPTEN__
    double cssWidth = 0.0;
    double cssHeight = 0.0;
    emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(g.window, &windowWidth, &windowHeight);
    if (cssWidth > 0.0 && cssHeight > 0.0 &&
        (windowWidth != static_cast<int>(cssWidth) || windowHeight != static_cast<int>(cssHeight)))
        SDL_SetWindowSize(g.window, static_cast<int>(cssWidth), static_cast<int>(cssHeight));
#endif

    const uint64_t now = SDL_GetPerformanceCounter();
    io.deltaTime = static_cast<float>(now - g.previous) / SDL_GetPerformanceFrequency();
    g.previous = now;
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(g.window, &width, &height);
    io.displayWidth = static_cast<float>(width);
    io.displayHeight = static_cast<float>(height);
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
            g.running = false;
        else if (event.type == SDL_MOUSEBUTTONDOWN)
        {
            if (event.button.button >= 1 && event.button.button <= 3)
                g.latched[event.button.button - 1] = true;
        }
        else if (event.type == SDL_MOUSEWHEEL)
        {
            io.mouseWheelX += event.wheel.x;
            io.mouseWheelY += event.wheel.y;
        }
        else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
        {
            const SDL_Keymod mod = static_cast<SDL_Keymod>(event.key.keysym.mod);
            io.addKeyEvent(event.key.keysym.sym, event.key.keysym.scancode,
                           (mod & KMOD_SHIFT) != 0, (mod & KMOD_CTRL) != 0,
                           (mod & KMOD_ALT) != 0, event.type == SDL_KEYDOWN);
        }
        else if (event.type == SDL_TEXTINPUT)
        {
            const char *p = event.text.text;
            while (*p)
                io.addInputCharacter(nextUtf8(p));
        }
    }
    int mx = 0;
    int my = 0;
    const uint32_t buttons = SDL_GetMouseState(&mx, &my);
    io.mouseX = static_cast<float>(mx);
    io.mouseY = static_cast<float>(my);
    io.mouseDown[0] = (buttons & SDL_BUTTON_LMASK) != 0 || g.latched[0];
    io.mouseDown[1] = (buttons & SDL_BUTTON_RMASK) != 0 || g.latched[1];
    io.mouseDown[2] = (buttons & SDL_BUTTON_MMASK) != 0 || g.latched[2];
    for (bool &latch : g.latched)
        latch = false;
    const SDL_Keymod mod = SDL_GetModState();
    io.keyShift = (mod & KMOD_SHIFT) != 0;
    io.keyCtrl = (mod & KMOD_CTRL) != 0;
    io.keyAlt = (mod & KMOD_ALT) != 0;

    ig::retained::NewFrame();
    app.update(io);
    app.paint(*ig::retained::GetDrawData(), g.font, nullptr);
    ig::retained::Render();
    SDL_SetRenderDrawColor(g.renderer, 24, 26, 30, 255);
    SDL_RenderClear(g.renderer);
    if (!g.bridge->render(*ig::retained::GetDrawData()))
        g.running = false;
    SDL_RenderPresent(g.renderer);
}
}

int main(int, char **)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;
    g.window = SDL_CreateWindow("iGUI retained widgets", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                1280, 720, SDL_WINDOW_RESIZABLE);
    if (!g.window)
        return 2;
    g.renderer = SDL_CreateRenderer(g.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g.renderer)
        return 3;

    g.bridge = new SdlWidgetBridge(g.renderer);
    ig::retained::SetCurrentContext(ig::retained::CreateContext());
    auto &io = ig::retained::GetIO();
    io.setClipboardText = [](const char *text) { SDL_SetClipboardText(text); };
    io.getClipboardText = []
    {
        char *value = SDL_GetClipboardText();
        ig::retained::String result = value ? value : "";
        SDL_free(value);
        return result;
    };

    if (g.atlas.buildDefault())
    {
        g.atlas.setTexture(g.bridge->createTexture(g.atlas.width(), g.atlas.height(), g.atlas.pixels()));
        g.font = &g.atlas.defaultFont();
        ig::retained::SetWhitePixel(g.atlas.texture(), g.atlas.whitePixelUV());
    }

    auto &app = ig::retained::WidgetApp::instance();
    app.setTextureUpload([](const unsigned char *p, int w, int h) { return g.bridge->createTexture(w, h, p); });
    app.setTextureDestroy([](ig::retained::TextureHandle t) { g.bridge->destroyTexture(t); });
    registerDemoStage(app);
    registerMenuStage(app);
    registerBasicStage(app);
    registerControlsStage(app);
    registerScrollStage(app);
    registerInputsStage(app);
    registerMenusStage(app);
    registerDialogsStage(app);
    registerDockStage(app);
    registerPropertiesStage(app);
    registerEditorStage(app);
    registerNodeStage(app);
    registerTimelineStage(app);
    registerGizmosStage(app);
    registerToolsStage(app);
    registerGalleryStage(app);
    registerSpecialtyStage(app);
    app.setStage("menu");
    SDL_StartTextInput();
    g.previous = SDL_GetPerformanceCounter();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(frame, nullptr, 0, 1);
#else
    while (g.running)
        frame(nullptr);
#endif

    g.bridge->destroyTexture(g.atlas.texture());
    ig::retained::DestroyContext(ig::retained::GetCurrentContext());
    delete g.bridge;
    SDL_DestroyRenderer(g.renderer);
    SDL_DestroyWindow(g.window);
    SDL_Quit();
    return 0;
}
