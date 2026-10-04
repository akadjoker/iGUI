#include <stdlib.h>
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
    destroyTexture(atlas_.texture());
    gl_.shutdown();
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
    cfg.resizable = true;
    cfg.vsync = config.vsync;
    cfg.gl.msaa = config.msaa;
    if (config.preferOpenGL)
    {
        // Core profile only: the newest version the driver gives, then 3.3 as the floor.
        static const int versions[][2] = {{4, 6}, {4, 5}, {4, 3}, {3, 3}};
        cfg.render = RENDER_GL;
        cfg.gl.profile = GL_PROFILE_CORE;
        for (const auto &version : versions)
        {
            if (version[0] < config.minGlMajor || (version[0] == config.minGlMajor && version[1] < config.minGlMinor))
                continue;
            cfg.gl.major = version[0];
            cfg.gl.minor = version[1];
            window_ = window_create(&cfg);
            if (!window_)
                continue;
            window_make_current(window_);
            useGl_ = gl_.init(gl_proc_address);
            if (useGl_)
                break;
            window_destroy(window_);
            window_ = nullptr;
        }
        if (useGl_)
            fprintf(stderr, "[ZenApp] OpenGL %d.%d core: %s\n", gl_.versionMajor(), gl_.versionMinor(), gl_.renderer());
        else
            fprintf(stderr, "[ZenApp] OpenGL core not available, using the software rasteriser\n");
    }
    if (!window_)
    {
        cfg.render = RENDER_PIXELS;
        window_ = window_create(&cfg);
    }
    if (!window_)
    {
        platform_shutdown();
        return false;
    }

    background_ = config.background;
    showStats_ = config.showStats;
    raster_.setSamples(config.samples);

    context_ = rt::CreateContext();
    rt::SetCurrentContext(context_);
    rt::IO &io = rt::GetIO();
    io.setClipboardText = [](const char *text) { clipboard_set(text); };
    io.getClipboardText = [] { return rt::String(clipboard_get()); };

    if (atlas_.buildDefault())
    {
        atlas_.setTexture(createTexture(atlas_.width(), atlas_.height(), atlas_.pixels()));
        font_ = &atlas_.defaultFont();
        rt::SetWhitePixel(atlas_.texture(), atlas_.whitePixelUV());
    }

    rt::WidgetApp &app = widgets();
    app.setTextureUpload([this](const unsigned char *p, int w, int h) { return createTexture(w, h, p); });
    app.setTextureUpdate([this](rt::TextureHandle t, const unsigned char *p, int w, int h) {
        return updateTexture(t, p, w, h);
    });
    app.setTextureDestroy([this](rt::TextureHandle t) { destroyTexture(t); });

    window_text_input_start(window_);
    return true;
}

rt::TextureHandle App::createTexture(int w, int h, const unsigned char *rgba)
{
    return useGl_ ? gl_.createTexture(w, h, rgba) : raster_.createTexture(w, h, rgba);
}

void App::destroyTexture(rt::TextureHandle t)
{
    if (useGl_)
        gl_.destroyTexture(t);
    else
        raster_.destroyTexture(t);
}

bool App::updateTexture(rt::TextureHandle t, const unsigned char *rgba, int w, int h)
{
    return useGl_ ? gl_.updateTexture(t, rgba, w, h) : raster_.updateTexture(t, rgba, w, h);
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
        else if (e.type == EVENT_WINDOW_DROP)
        {
            ct::Vector<rt::String> paths;
            for (int i = 0; i < e.data.drop.count; ++i)
                paths.push_back(rt::String(e.data.drop.paths[i]));
            int dx = 0, dy = 0;
            mouse_position(window_, &dx, &dy);
            io.addDropEvent(static_cast<float>(dx), static_cast<float>(dy), paths);
        }
        else if (e.type == EVENT_CHAR)
        {
            const uint32_t c = e.data.codepoint;
            if (c >= 32u && c != 127u)
                io.addInputCharacter(c);
        }
        else if (e.type == EVENT_KEY)
        {
            if (e.data.key.down && e.data.key.key == KEY_F2 && !useGl_)
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

    if (useGl_)
        frameGl();
    else
        framePixels();

    ++statsFrames_;
    const double elapsed = time_seconds() - statsTime_;
    if (elapsed >= 1.0 && !showStats_)
        statsTime_ = time_seconds();
    else if (elapsed >= 1.0)
    {
        int fbW = 0, fbH = 0;
        window_get_framebuffer_size(window_, &fbW, &fbH);
        char title[160];
        if (useGl_)
            snprintf(title, sizeof title, "%dx%d - OpenGL - %.0f fps", fbW, fbH, statsFrames_ / elapsed);
        else
            snprintf(title, sizeof title, "%dx%d - AA %dx%d - %.0f fps - raster %.2f ms (%d/%d frames)", fbW, fbH,
                     raster_.samples(), raster_.samples(), statsFrames_ / elapsed,
                     rasterFrames_ ? rasterMs_ / rasterFrames_ : 0.0, rasterFrames_, statsFrames_);
        window_set_title(window_, title);
        statsTime_ = time_seconds();
        rasterMs_ = 0.0;
        statsFrames_ = 0;
        rasterFrames_ = 0;
    }
}

void App::frameGl()
{
    int width = 0, height = 0;
    window_get_framebuffer_size(window_, &width, &height);
    if (width <= 0 || height <= 0)
        return;
    window_make_current(window_);
    gl_.render(*rt::GetDrawData(), width, height, background_);
    static const char *shotPath = getenv("ZEN_SCREENSHOT");
    if (shotPath)
    {
        ct::Vector<uint32_t> pixels;
        pixels.resize(static_cast<size_t>(width) * height);
        gl_.readPixels(pixels.data(), width, height);
        saveScreenshot(pixels.data(), width, height, width);
    }
    // No window_swap here: app_run swaps after every frame callback.
}

void App::framePixels()
{
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
    saveScreenshot(fb.pixels, fb.width, fb.height, fb.stride);
}

// ZEN_SCREENSHOT=file.ppm saves the 30th frame and closes the window: a way to look at the UI without a screen grab.
void App::saveScreenshot(const uint32_t *pixels, int width, int height, int stride)
{
    static const char *shotPath = getenv("ZEN_SCREENSHOT");
    static const int shotAt = getenv("ZEN_SCREENSHOT_FRAME") ? atoi(getenv("ZEN_SCREENSHOT_FRAME")) : 30;
    static int shotFrame = 0;
    if (!shotPath || ++shotFrame != shotAt)
        return;
    if (FILE *f = fopen(shotPath, "wb"))
    {
        fprintf(f, "P6\n%d %d\n255\n", width, height);
        for (int y = 0; y < height; ++y)
        {
            const uint32_t *row = pixels + static_cast<size_t>(y) * stride;
            for (int x = 0; x < width; ++x)
            {
                const unsigned char rgb[3] = {static_cast<unsigned char>(row[x] >> 16), static_cast<unsigned char>(row[x] >> 8),
                                              static_cast<unsigned char>(row[x])};
                fwrite(rgb, 1, 3, f);
            }
        }
        fclose(f);
    }
    window_set_should_close(window_, true);
}

} // namespace zen
} // namespace ig
