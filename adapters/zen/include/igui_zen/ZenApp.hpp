#pragma once

#include <igui_zen/GlRaster.hpp>
#include <igui_zen/SoftRaster.hpp>

#include <WidgetApp.hpp>

#include <stdint.h>

struct PlatformWindow;

namespace ig
{
namespace zen
{

// A zen_plataform window running the retained widgets, drawn by SoftRaster.
// Create it, build the widget tree through widgets(), then run().
class App
{
public:
    struct Config
    {
        const char *title = "iGUI";
        int width = 1280;
        int height = 720;
        // OpenGL when the machine has it, else the software rasteriser.
        bool preferOpenGL = true;
        int minGlMajor = 3; // the oldest OpenGL core the application accepts
        int minGlMinor = 3;
        int msaa = 4;    // OpenGL multisampling, 0 to disable
        bool vsync = true;
        bool showStats = true; // fps and size in the window title, once a second
        int samples = 2; // software only: anti-aliasing samples per axis, F2 cycles 1..4
        uint32_t background = 0xff181a1eu;
    };

    App() = default;
    ~App();
    App(const App &) = delete;
    App &operator=(const App &) = delete;

    bool create(const Config &config);
    // Returns when the window closes.
    void run();

    ig::retained::WidgetApp &widgets() { return ig::retained::WidgetApp::instance(); }
    SoftRaster &raster() { return raster_; }
    // True when drawing with OpenGL. The GL context is current on the thread that runs the frame.
    bool usesOpenGL() const { return useGl_; }
    GlRaster &gl() { return gl_; }
    PlatformWindow *window() { return window_; }

private:
    static void frameThunk(PlatformWindow *w, void *user);
    void frame();
    void frameGl();
    void framePixels();
    ig::retained::TextureHandle createTexture(int w, int h, const unsigned char *rgba);
    void destroyTexture(ig::retained::TextureHandle t);
    bool updateTexture(ig::retained::TextureHandle t, const unsigned char *rgba, int w, int h);
    void saveScreenshot(const uint32_t *pixels, int width, int height, int stride);
    void feedInput();

    PlatformWindow *window_ = nullptr;
    ig::retained::Context *context_ = nullptr;
    ig::retained::FontAtlas atlas_;
    const ig::retained::Font *font_ = nullptr;
    SoftRaster raster_;
    GlRaster gl_;
    bool useGl_ = false;
    bool showStats_ = true;
    uint32_t background_ = 0;
    double statsTime_ = 0.0;
    double lastFrame_ = 0.0;
    double rasterMs_ = 0.0;
    int statsFrames_ = 0;
    int rasterFrames_ = 0;
};

} // namespace zen
} // namespace ig
