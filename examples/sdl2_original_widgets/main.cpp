#include "SdlWidgetBridge.hpp"

#include <WidgetApp.hpp>
#include <TreePropertyColorWidgets.hpp>

#include <cstdio>

void registerDemoStage(BuGUI::WidgetApp &app);
void registerMenuStage(BuGUI::WidgetApp &app);
void registerBasicStage(BuGUI::WidgetApp &app);
void registerControlsStage(BuGUI::WidgetApp &app);
void registerScrollStage(BuGUI::WidgetApp &app);
void registerInputsStage(BuGUI::WidgetApp &app);
void registerMenusStage(BuGUI::WidgetApp &app);
void registerDialogsStage(BuGUI::WidgetApp &app);
void registerDockStage(BuGUI::WidgetApp &app);
void registerPropertiesStage(BuGUI::WidgetApp &app);
void registerEditorStage(BuGUI::WidgetApp &app);
void registerNodeStage(BuGUI::WidgetApp &app);
void registerTimelineStage(BuGUI::WidgetApp &app);
void registerGizmosStage(BuGUI::WidgetApp &app);
void registerToolsStage(BuGUI::WidgetApp &app);
void registerGalleryStage(BuGUI::WidgetApp &app);
void registerSpecialtyStage(BuGUI::WidgetApp &app);
void showFileDialogDemo(BuGUI::WidgetApp &app);

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
}

int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    SDL_Window *window = SDL_CreateWindow("iGUI SDL2 - 10 original widget stages",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : nullptr;
    if (!renderer) return 2;

    SdlWidgetBridge bridge(renderer);
    BuGUI::SetCurrentContext(BuGUI::CreateContext());
    auto &io = BuGUI::GetIO();
    io.setClipboardText = [](const char *text) { SDL_SetClipboardText(text); };
    io.getClipboardText = [] { char *value = SDL_GetClipboardText(); std::string result = value ? value : ""; SDL_free(value); return result; };

    BuGUI::FontAtlas atlas;
    const BuGUI::Font *font = nullptr;
    if (atlas.buildDefault())
    {
        atlas.setTexture(bridge.createTexture(atlas.width(), atlas.height(), atlas.pixels()));
        font = &atlas.defaultFont();
        BuGUI::SetWhitePixel(atlas.texture(), atlas.whitePixelUV());
    }

    auto &app = BuGUI::WidgetApp::instance();
    app.setTextureUpload([&bridge](const unsigned char *p, int w, int h) { return bridge.createTexture(w, h, p); });
    app.setTextureDestroy([&bridge](BuGUI::TextureHandle t) { bridge.destroyTexture(t); });
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
    const std::string initialStage = argc > 1 ? argv[1] : "menu";
    const bool testDockColor = initialStage == "--test-dock-color";
    const bool showFileDialog = initialStage == "filedialog";
    app.setStage(showFileDialog ? "tools" : testDockColor ? "dock" : initialStage);
    if (showFileDialog) showFileDialogDemo(app);
    SDL_StartTextInput();

    bool running = true;
    int exitCode = 0;
    int dockColorTestPhase = 0;
    float dockColorTestX = 0.0f;
    float dockColorTestY = 0.0f;
    uint64_t previous = SDL_GetPerformanceCounter();
    while (running)
    {
        uint64_t now = SDL_GetPerformanceCounter();
        io.deltaTime = static_cast<float>(now - previous) / SDL_GetPerformanceFrequency();
        previous = now;
        int width, height; SDL_GetWindowSize(window, &width, &height);
        io.displayWidth = static_cast<float>(width); io.displayHeight = static_cast<float>(height);
        int mx, my; uint32_t buttons = SDL_GetMouseState(&mx, &my);
        io.mouseX = static_cast<float>(mx); io.mouseY = static_cast<float>(my);
        io.mouseDown[0] = (buttons & SDL_BUTTON_LMASK) != 0;
        io.mouseDown[1] = (buttons & SDL_BUTTON_RMASK) != 0;
        io.mouseDown[2] = (buttons & SDL_BUTTON_MMASK) != 0;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) running = false;
            else if (event.type == SDL_MOUSEWHEEL) { io.mouseWheelX += event.wheel.x; io.mouseWheelY += event.wheel.y; }
            else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
            {
                SDL_Keymod mod = static_cast<SDL_Keymod>(event.key.keysym.mod);
                io.addKeyEvent(event.key.keysym.sym, event.key.keysym.scancode,
                    (mod & KMOD_SHIFT) != 0, (mod & KMOD_CTRL) != 0,
                    (mod & KMOD_ALT) != 0, event.type == SDL_KEYDOWN);
            }
            else if (event.type == SDL_TEXTINPUT)
            {
                const char *p = event.text.text;
                while (*p) io.addInputCharacter(nextUtf8(p));
            }
        }
        SDL_Keymod mod = SDL_GetModState();
        io.keyShift = (mod & KMOD_SHIFT) != 0; io.keyCtrl = (mod & KMOD_CTRL) != 0; io.keyAlt = (mod & KMOD_ALT) != 0;

        if (testDockColor && dockColorTestPhase == 1) {
            io.mouseX = dockColorTestX;
            io.mouseY = dockColorTestY;
            io.mouseDown[0] = true;
            dockColorTestPhase = 2;
        }

        BuGUI::NewFrame();
        app.update(io);
        app.paint(*BuGUI::GetDrawData(), font, nullptr);

        // Headless-friendly regression check for DockPanel -> PropertyGrid -> ColorPickerPopup.
        // Run after the first paint so the complete dock tree has final rectangles.
        if (testDockColor && dockColorTestPhase == 0)
        {
            BuGUI::PropertyGrid *grid = nullptr;
            auto findGrid = [&](auto &&self, BuGUI::Widget *widget) -> void {
                if (!widget || grid) return;
                if (auto *candidate = dynamic_cast<BuGUI::PropertyGrid *>(widget);
                    candidate && candidate->isVisible()) {
                    grid = candidate;
                    return;
                }
                for (auto *child : widget->children()) self(self, child);
            };
            findGrid(findGrid, app.stage("dock"));
            if (!grid) {
                std::fprintf(stderr, "dock color test: visible PropertyGrid not found\n");
                exitCode = 3;
                running = false;
            } else {
                const BuGUI::Rect rect = grid->absoluteRect();
                dockColorTestX = rect.x + rect.w * 0.70f;
                dockColorTestY = rect.y + 5.0f * 24.0f + 12.0f;
                dockColorTestPhase = 1;
            }
        }
        else if (testDockColor && dockColorTestPhase == 2)
        {
            if (!app.popup()) {
                std::fprintf(stderr, "dock color test: ColorPicker popup did not open\n");
                exitCode = 4;
            } else {
                std::fprintf(stderr, "dock color test: popup opened through SDL input path\n");
            }
            running = false;
        }
        BuGUI::Render();
        SDL_SetRenderDrawColor(renderer, 24, 26, 30, 255); SDL_RenderClear(renderer);
        if (!bridge.render(*BuGUI::GetDrawData())) running = false;
        SDL_RenderPresent(renderer);
    }
    bridge.destroyTexture(atlas.texture());
    BuGUI::DestroyContext(BuGUI::GetCurrentContext());
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    return exitCode;
}
