#include <igui_zen/ZenApp.hpp>

#include <platform.h>

#include <stdio.h>
#include <string.h>

namespace ig
{
namespace zen
{

namespace rt = ig::retained;

namespace
{

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

} // namespace

App::~App()
{
    if (!window_)
        return;
    raster_.destroyTexture(atlas_.texture());
    rt::DestroyContext(context_);
    window_destroy(window_);
    platform_shutdown();
}

bool App::create(const Config &config)
{
    if (!platform_init())
        return false;

    WindowConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.title = config.title;
    cfg.width = config.width;
    cfg.height = config.height;
    cfg.x = WINDOW_POS_CENTERED;
    cfg.y = WINDOW_POS_CENTERED;
    cfg.render = RENDER_PIXELS;
    cfg.resizable = true;
    window_ = window_create(&cfg);
    if (!window_)
    {
        platform_shutdown();
        return false;
    }

    background_ = config.background;
    raster_.setSamples(config.samples);

    context_ = rt::CreateContext();
    rt::SetCurrentContext(context_);
    rt::IO &io = rt::GetIO();
    io.setClipboardText = [](const char *text) { clipboard_set(text); };
    io.getClipboardText = [] { return rt::String(clipboard_get()); };

    if (atlas_.buildDefault())
    {
        atlas_.setTexture(raster_.createTexture(atlas_.width(), atlas_.height(), atlas_.pixels()));
        font_ = &atlas_.defaultFont();
        rt::SetWhitePixel(atlas_.texture(), atlas_.whitePixelUV());
    }

    rt::WidgetApp &app = widgets();
    app.setTextureUpload([this](const unsigned char *p, int w, int h) { return raster_.createTexture(w, h, p); });
    app.setTextureUpdate([this](rt::TextureHandle t, const unsigned char *p, int w, int h) {
        return raster_.updateTexture(t, p, w, h);
    });
    app.setTextureDestroy([this](rt::TextureHandle t) { raster_.destroyTexture(t); });

    window_text_input_start(window_);
    return true;
}

void App::run()
{
    statsTime_ = lastFrame_ = time_seconds();
    app_run(window_, &App::frameThunk, this);
}

void App::frameThunk(PlatformWindow *, void *user)
{
    static_cast<App *>(user)->frame();
}

void App::feedInput()
{
    rt::IO &io = rt::GetIO();
    const int mods = key_mods(window_);
    io.keyShift = (mods & KEYMOD_SHIFT) != 0;
    io.keyCtrl = (mods & KEYMOD_CTRL) != 0;
    io.keyAlt = (mods & KEYMOD_ALT) != 0;

    Event e;
    while (poll_event(window_, &e))
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
                raster_.setSamples(raster_.samples() % 4 + 1);
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

void App::frame()
{
    rt::IO &io = rt::GetIO();

    int sw = 0, sh = 0;
    window_get_size(window_, &sw, &sh);
    int mx = 0, my = 0;
    mouse_position(window_, &mx, &my);

    const double now = time_seconds();
    const double dt = now - lastFrame_;
    lastFrame_ = now;
    io.deltaTime = static_cast<float>(dt > 0.1 ? 0.1 : dt);
    io.displayWidth = static_cast<float>(sw);
    io.displayHeight = static_cast<float>(sh);
    io.mouseX = static_cast<float>(mx);
    io.mouseY = static_cast<float>(my);
    io.mouseDown[0] = mouse_button_down(window_, MOUSE_LEFT);
    io.mouseDown[1] = mouse_button_down(window_, MOUSE_RIGHT);
    io.mouseDown[2] = mouse_button_down(window_, MOUSE_MIDDLE);
    feedInput();

    rt::NewFrame();
    widgets().update(io);
    widgets().paint(*rt::GetDrawData(), font_, nullptr);
    rt::Render();
    mouse_set_cursor(window_, cursorShape(io.wantedCursor));

    Framebuffer fb;
    if (!window_lock_pixels(window_, &fb))
        return;
    const double t0 = time_seconds();
    if (raster_.render(*rt::GetDrawData(), fb.pixels, fb.width, fb.height, fb.stride, background_))
    {
        rasterMs_ += (time_seconds() - t0) * 1000.0;
        ++rasterFrames_;
    }
    window_present_pixels(window_);

    ++statsFrames_;
    const double elapsed = time_seconds() - statsTime_;
    if (elapsed >= 1.0)
    {
        char title[160];
        snprintf(title, sizeof title, "%dx%d - AA %dx%d - %.0f fps - raster %.2f ms (%d/%d frames)",
                 fb.width, fb.height, raster_.samples(), raster_.samples(), statsFrames_ / elapsed,
                 rasterFrames_ ? rasterMs_ / rasterFrames_ : 0.0, rasterFrames_, statsFrames_);
        window_set_title(window_, title);
        statsTime_ = time_seconds();
        rasterMs_ = 0.0;
        statsFrames_ = 0;
        rasterFrames_ = 0;
    }
}

} // namespace zen
} // namespace ig
