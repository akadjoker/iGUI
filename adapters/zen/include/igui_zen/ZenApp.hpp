#pragma once

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
        int samples = 2; // anti-aliasing samples per axis, F2 cycles 1..4
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
    PlatformWindow *window() { return window_; }

private:
    static void frameThunk(PlatformWindow *w, void *user);
    void frame();
    void feedInput();

    PlatformWindow *window_ = nullptr;
    ig::retained::Context *context_ = nullptr;
    ig::retained::FontAtlas atlas_;
    const ig::retained::Font *font_ = nullptr;
    SoftRaster raster_;
    uint32_t background_ = 0;
    double statsTime_ = 0.0;
    double lastFrame_ = 0.0;
    double rasterMs_ = 0.0;
    int statsFrames_ = 0;
    int rasterFrames_ = 0;
};

} // namespace zen
} // namespace ig
