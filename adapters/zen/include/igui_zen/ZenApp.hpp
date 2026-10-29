#pragma once
#include <igui_zen/GlRaster.hpp>
#include <igui_zen/SoftRaster.hpp>
#include <WidgetApp.hpp>

#include <ct/vector.hpp>

#include <stdint.h>

struct PlatformWindow;

namespace ig
{
namespace zen
{

// zen_plataform windows running the retained widgets, drawn with OpenGL or
// the software rasteriser. Create the main window with create(), build its
// widget tree through widgets(), then run().
//
// On desktop with OpenGL an application can open more windows (openWindow):
// each is a top-level window with its own widget tree, focus, popups and
// stages, drawn with the one shared GL context so textures and fonts are
// loaded once. WidgetApp stays one: the state of the window being fed or
// painted is swapped in (WidgetApp::Surface), so widget code keeps calling
// WidgetApp::instance(). A dialog window opened as modal takes the input of
// its parent chain until it closes. Where there is one surface (web, Android,
// software rasteriser) openWindow returns nullptr and the application keeps
// its dialogs inside the main window.
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

    class Window;

    struct WindowDesc
    {
        const char *title = "iGUI";
        int width = 640;
        int height = 480;
        bool resizable = true;
        bool dialog = false;        // a dialog of `parent`: stays above it, no taskbar entry
        bool modal = false;         // with dialog: the parent chain gets no input until this closes
        Window *parent = nullptr;   // nullptr: the main window
        uint32_t background = 0xff181a1eu;
    };

    App() = default;
    ~App();
    App(const App &) = delete;
    App &operator=(const App &) = delete;

    bool create(const Config &config);
    // Returns when the main window closes.
    void run();
    // One frame of every window. False once the main window has closed.
    bool pump();

    ig::retained::WidgetApp &widgets() { return ig::retained::WidgetApp::instance(); }
    SoftRaster &raster() { return raster_; }
    // True when drawing with OpenGL. The GL context is current on the thread that runs the frame.
    bool usesOpenGL() const { return useGl_; }
    GlRaster &gl() { return gl_; }
    PlatformWindow *window() { return main_ ? main_->platform : nullptr; }

    // ── several windows ──────────────────────────────────────────────────
    bool supportsWindows() const;
    Window *mainWindow() { return main_; }
    // Opens a window and makes it current, so widgets() builds its tree.
    // nullptr where windows are not supported or the window could not open.
    Window *openWindow(const WindowDesc &desc);
    // Closes at the end of the frame; the main window ends run().
    void closeWindow(Window *w);
    // The window whose state widgets() holds. Switch to build or inspect
    // another window's tree; pump() switches as it goes through them.
    Window *current() { return current_; }
    void makeCurrent(Window *w);
    int windowCount() const { return static_cast<int>(windows_.size()); }
    Window *windowAt(int i) { return windows_[static_cast<size_t>(i)]; }
    bool isOpen(const Window *w) const;

    class Window
    {
    public:
        PlatformWindow *platform = nullptr;
        Window *parent = nullptr;
        bool dialog = false;
        bool modal = false;
        bool closing = false;
        uint32_t background = 0;
        void *user = nullptr; // for the application
        // The state while this window is not the current one.
        ig::retained::WidgetApp::Surface surface;
        // Set when the user asks the window to close (close button, Alt+F4).
        bool closeRequested() const;
    };

private:
    static void frameThunk(PlatformWindow *w, void *user);
    void frameWindow(Window *w, bool driven);
    void renderGl(Window *w);
    void framePixels();
    void destroyWindow(Window *w);
    bool blockedByModal(const Window *w) const;
    ig::retained::TextureHandle createTexture(int w, int h, const unsigned char *rgba);
    void destroyTexture(ig::retained::TextureHandle t);
    bool updateTexture(ig::retained::TextureHandle t, const unsigned char *rgba, int w, int h);
    void saveScreenshot(const uint32_t *pixels, int width, int height, int stride);
    void feedInput(PlatformWindow *pw);
    void updateStats();

    ct::Vector<Window *> windows_;
    Window *main_ = nullptr;
    Window *current_ = nullptr;
    ig::retained::Context *context_ = nullptr;
    ig::retained::FontAtlas atlas_;
    const ig::retained::Font *font_ = nullptr;
    SoftRaster raster_;
    GlRaster gl_;
    bool useGl_ = false;
    bool showStats_ = true;
    bool systemLoop_ = false; // app_run owns the loop (web, Android)
    double statsTime_ = 0.0;
    double lastFrame_ = 0.0;
    double rasterMs_ = 0.0;
    int statsFrames_ = 0;
    int rasterFrames_ = 0;
};

} // namespace zen
} // namespace ig
