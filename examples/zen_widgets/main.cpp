// Retained widget gallery on zen_plataform, drawn by the software rasteriser.
// Usage: igui_zen_widgets [stage] [samples-per-axis]
// Keys: F2 cycles the anti-aliasing (1x, 2x, 3x, 4x per axis).

#include "SoftRaster.hpp"

#include <WidgetApp.hpp>
#include <TreePropertyColorWidgets.hpp>

#include <platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
void registerVideoStage(ig::retained::WidgetApp &app);

namespace
{

namespace rt = ig::retained;

struct Demo
{
    SoftRaster raster;
    const rt::Font *font = nullptr;
    double statsTime = 0.0;
    double lastFrame = 0.0;
    double rasterMs = 0.0;
    int statsFrames = 0;
    int rasterFrames = 0;
};

// The retained widgets use SDL keycodes, so keys are translated to those.
int sdlKey(int key)
{
    if (key >= KEY_A && key <= KEY_Z)
        return key - KEY_A + 'a';
    if (key >= KEY_SPACE && key <= KEY_GRAVE)
        return key;
    if (key >= KEY_F1 && key <= KEY_F12)
        return rt::Key::F1 + (key - KEY_F1);
    switch (key)
    {
    case KEY_ESCAPE: return rt::Key::Escape;
    case KEY_ENTER: return rt::Key::Return;
    case KEY_TAB: return rt::Key::Tab;
    case KEY_BACKSPACE: return rt::Key::Backspace;
    case KEY_DELETE: return rt::Key::Delete;
    case KEY_INSERT: return 1073741897;
    case KEY_RIGHT: return rt::Key::Right;
    case KEY_LEFT: return rt::Key::Left;
    case KEY_DOWN: return rt::Key::Down;
    case KEY_UP: return rt::Key::Up;
    case KEY_PAGE_UP: return rt::Key::PageUp;
    case KEY_PAGE_DOWN: return rt::Key::PageDown;
    case KEY_HOME: return rt::Key::Home;
    case KEY_END: return rt::Key::End;
    case KEY_KP_ENTER: return rt::Key::KPEnter;
    default: return 0;
    }
}

int cursorShape(int wanted)
{
    switch (wanted)
    {
    case 1: return CURSOR_HAND;
    case 2: return CURSOR_IBEAM;
    case 3: return CURSOR_CROSSHAIR;
    case 4: return CURSOR_RESIZE_EW;
    case 5: return CURSOR_RESIZE_NS;
    case 6: return CURSOR_RESIZE_ALL;
    case 7: return CURSOR_NOT_ALLOWED;
    default: return CURSOR_ARROW;
    }
}

void feedInput(PlatformWindow *w, Demo *d)
{
    rt::IO &io = rt::GetIO();
    const int mods = key_mods(w);
    io.keyShift = (mods & KEYMOD_SHIFT) != 0;
    io.keyCtrl = (mods & KEYMOD_CTRL) != 0;
    io.keyAlt = (mods & KEYMOD_ALT) != 0;

    Event e;
    while (poll_event(w, &e))
    {
        if (e.type == EVENT_MOUSE_WHEEL)
        {
            io.mouseWheelX += e.data.wheel.x;
            io.mouseWheelY += e.data.wheel.y;
        }
        else if (e.type == EVENT_CHAR)
        {
            const uint32_t c = e.data.codepoint;
            if (c >= 32u && c != 127u)
                io.addInputCharacter(c);
        }
        else if (e.type == EVENT_KEY)
        {
            if (e.data.key.down && e.data.key.key == KEY_F2)
            {
                d->raster.setSamples(d->raster.samples() % 4 + 1);
                printf("anti-aliasing: %dx%d samples\n", d->raster.samples(), d->raster.samples());
                continue;
            }
            const int key = sdlKey(e.data.key.key);
            if (key == 0)
                continue;
            const int scancode = key == rt::Key::F1 ? 58 : e.data.key.scancode;
            io.addKeyEvent(key, scancode, io.keyShift, io.keyCtrl, io.keyAlt, e.data.key.down);
        }
    }
}

void frame(PlatformWindow *w, void *user)
{
    Demo *d = static_cast<Demo *>(user);
    rt::IO &io = rt::GetIO();

    int sw = 0, sh = 0;
    window_get_size(w, &sw, &sh);
    int mx = 0, my = 0;
    mouse_position(w, &mx, &my);

    const double now = time_seconds();
    const double dt = now - d->lastFrame;
    d->lastFrame = now;
    io.deltaTime = static_cast<float>(dt > 0.1 ? 0.1 : dt);
    io.displayWidth = static_cast<float>(sw);
    io.displayHeight = static_cast<float>(sh);
    io.mouseX = static_cast<float>(mx);
    io.mouseY = static_cast<float>(my);
    io.mouseDown[0] = mouse_button_down(w, MOUSE_LEFT);
    io.mouseDown[1] = mouse_button_down(w, MOUSE_RIGHT);
    io.mouseDown[2] = mouse_button_down(w, MOUSE_MIDDLE);
    feedInput(w, d);

    rt::NewFrame();
    rt::WidgetApp::instance().update(io);
    rt::WidgetApp::instance().paint(*rt::GetDrawData(), d->font, nullptr);
    rt::Render();
    mouse_set_cursor(w, cursorShape(io.wantedCursor));

    Framebuffer fb;
    if (!window_lock_pixels(w, &fb))
        return;
    const double t0 = time_seconds();
    if (d->raster.render(*rt::GetDrawData(), fb.pixels, fb.width, fb.height, fb.stride, 0xff181a1eu))
    {
        d->rasterMs += (time_seconds() - t0) * 1000.0;
        ++d->rasterFrames;
    }
    window_present_pixels(w);

    ++d->statsFrames;
    const double elapsed = time_seconds() - d->statsTime;
    if (elapsed >= 1.0)
    {
        char title[160];
        snprintf(title, sizeof title, "iGUI on zen_plataform - %dx%d - AA %dx%d - %.0f fps - raster %.2f ms (%d/%d frames)",
                 fb.width, fb.height, d->raster.samples(), d->raster.samples(),
                 d->statsFrames / elapsed, d->rasterFrames ? d->rasterMs / d->rasterFrames : 0.0,
                 d->rasterFrames, d->statsFrames);
        window_set_title(w, title);
        d->statsTime = time_seconds();
        d->rasterMs = 0.0;
        d->statsFrames = 0;
        d->rasterFrames = 0;
    }
}

} // namespace

int main(int argc, char **argv)
{
    if (!platform_init())
        return 1;

    WindowConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.title = "iGUI on zen_plataform";
    cfg.width = 1280;
    cfg.height = 720;
    cfg.x = WINDOW_POS_CENTERED;
    cfg.y = WINDOW_POS_CENTERED;
    cfg.render = RENDER_PIXELS;
    cfg.resizable = true;
    PlatformWindow *w = window_create(&cfg);
    if (!w)
    {
        platform_shutdown();
        return 2;
    }

    Demo demo;
    if (argc > 2)
        demo.raster.setSamples(atoi(argv[2]));

    rt::SetCurrentContext(rt::CreateContext());
    rt::IO &io = rt::GetIO();
    io.setClipboardText = [](const char *text) { clipboard_set(text); };
    io.getClipboardText = [] { return rt::String(clipboard_get()); };

    rt::FontAtlas atlas;
    if (atlas.buildDefault())
    {
        atlas.setTexture(demo.raster.createTexture(atlas.width(), atlas.height(), atlas.pixels()));
        demo.font = &atlas.defaultFont();
        rt::SetWhitePixel(atlas.texture(), atlas.whitePixelUV());
    }

    rt::WidgetApp &app = rt::WidgetApp::instance();
    app.setTextureUpload([&demo](const unsigned char *p, int tw, int th) { return demo.raster.createTexture(tw, th, p); });
    app.setTextureUpdate([&demo](rt::TextureHandle t, const unsigned char *p, int tw, int th) { return demo.raster.updateTexture(t, p, tw, th); });
    app.setTextureDestroy([&demo](rt::TextureHandle t) { demo.raster.destroyTexture(t); });
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
    registerVideoStage(app);
    app.setStage(argc > 1 ? argv[1] : "menu");

    window_text_input_start(w);
    demo.statsTime = demo.lastFrame = time_seconds();
    app_run(w, frame, &demo);

    demo.raster.destroyTexture(atlas.texture());
    rt::DestroyContext(rt::GetCurrentContext());
    window_destroy(w);
    platform_shutdown();
    return 0;
}
