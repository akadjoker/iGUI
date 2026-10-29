#pragma once
#include <igui_zen/GlRaster.hpp>
#include <igui_zen/SoftRaster.hpp>
#include <WidgetApp.hpp>

#include <ct/function.hpp>
#include <ct/vector.hpp>

#include <stdint.h>

#include <platform.h>

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
    // Where and how a window appears; shared by the main window's Config and
    // the WindowDesc of the others.
    struct Placement
    {
        int x = -1;              // WINDOW_POS_CENTERED; -2 lets the system choose
        int y = -1;
        int monitor = -1;        // MONITOR_CURRENT; -2 the monitor under the mouse; else an index
        int mode = 0;            // 0 windowed, 1 fullscreen, 2 borderless fullscreen
        bool resizable = true;
        bool decorated = true;
        bool alwaysOnTop = false;
        int state = 0;           // 0 normal, 1 maximized, 2 minimized
        float opacity = 1.0f;
        int kind = 0;            // for a window with a parent: 0 normal, 1 dialog, 2 utility
        float scale = 1.0f;      // the UI drawn this many times bigger (2 = HiDPI look); 1 = the screen's own units
    };

    struct Config
    {
        const char* title = "iGUI";
        int width = 1280;
        int height = 720;
        Placement placement;
        // OpenGL when the machine has it, else the software rasteriser.
        bool preferOpenGL = true;
        int minGlMajor = 3; // the oldest OpenGL core the application accepts
        int minGlMinor = 3;
        // Ask for an OpenGL ES 3.0 context instead of a core one: for an engine whose shaders are
        // GLSL ES. Where the desktop driver has no ES profile the window falls back to the core
        // versions above.
        bool openGLES = false;
        int msaa = 4; // OpenGL multisampling, 0 to disable
        bool vsync = true;
        bool showStats = true; // fps and size in the window title, once a second
        int samples = 2;       // software only: anti-aliasing samples per axis, F2 cycles 1..4
        uint32_t background = 0xff181a1eu;
    };

    class Window;

    struct WindowDesc
    {
        const char *title = "iGUI";
        int width = 640;
        int height = 480;
        Placement placement;
        bool dialog = false;        // a child of `parent`: stays above it; placement.kind says dialog or utility
        bool modal = false;         // with dialog: the parent chain gets no input until this closes
        Window *parent = nullptr;   // nullptr: the main window
        uint32_t background = 0xff181a1eu;
    };

    App() = default;
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool create(const Config &config);
    // Returns when the main window closes.
    void run();
    // One frame of every window. False once the main window has closed.
    bool pump();
    // Calls fn once with the main window's next rendered frame (0xAARRGGBB
    // pixels, top row first), for screenshots and tests. OpenGL only.
    void requestCapture(ct::Function<void(const uint32_t *pixels, int width, int height)> fn);

    ig::retained::WidgetApp& widgets()
    {
        return ig::retained::WidgetApp::instance();
    }
    SoftRaster& raster()
    {
        return raster_;
    }
    // True when drawing with OpenGL. The GL context is current on the thread that runs the frame.
    bool usesOpenGL() const
    {
        return useGl_;
    }
    GlRaster& gl()
    {
        return gl_;
    }
    // Runs every frame after the widgets update and before they paint, with the delta time. With
    // OpenGL the context is current: the place to advance the application and draw into its own
    // framebuffers and textures, which the widgets then show. It must leave framebuffer 0 bound.
    void setFrameCallback(ct::Function<void(float)> callback)
    {
        frameCallback_ = std::move(callback);
    }
    // Runs every frame after the platform input is read into the IO and before the widgets update,
    // with the frame number: a test or a recorder can replace the mouse and add keys there.
    void setInputHook(ct::Function<void(int, ig::retained::IO&)> hook)
    {
        inputHook_ = std::move(hook);
    }
    // Sees every platform event as it is read, before the widgets get it: an application running a
    // game inside the interface feeds its own input from here.
    void setEventHook(ct::Function<void(const Event&)> hook)
    {
        eventHook_ = std::move(hook);
    }
    /// Called after the interface is drawn and before the swap (OpenGL path): the place to read the
    /// window.
    void setAfterRenderCallback(ct::Function<void()> callback)
    {
        afterRenderCallback_ = std::move(callback);
    }
    /// Reads the window as drawn this frame, top row first, 0xAARRGGBB. Valid inside the
    /// after-render callback.
    bool readWindowPixels(ct::Vector<uint32_t>& out, int& width, int& height);
    PlatformWindow* window()
    {
        return main_ ? main_->platform : nullptr;
    }

    // ── several windows ──────────────────────────────────────────────────
    bool supportsWindows() const;
    Window *mainWindow() { return main_; }
    // Opens a window and makes it current, so widgets() builds its tree.
    // nullptr where windows are not supported or the window could not open.
    Window *openWindow(const WindowDesc &desc);
    // Closes at the end of the frame; the main window ends run().
    void closeWindow(Window *w);
    // Applies the parts of a placement a live window can change: position,
    // mode, always-on-top, state, opacity, scale.
    void applyPlacement(Window *w, const Placement &p);
    // The UI zoom of a window: 2 draws everything twice as big. OpenGL only.
    void setScale(Window *w, float scale);
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
        float scale = 1.0f;   // UI zoom: widgets think the window is this many times smaller
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
    float scaleOf(PlatformWindow *pw) const;
    void updateStats();

    ct::Function<void(const uint32_t *, int, int)> capture_;
    ct::Vector<Window *> windows_;
    Window *main_ = nullptr;
    Window *current_ = nullptr;
    ig::retained::Context *context_ = nullptr;
    ig::retained::FontAtlas atlas_;
    const ig::retained::Font* font_ = nullptr;
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
    ct::Function<void(float)> frameCallback_;
    ct::Function<void()> afterRenderCallback_;
    ct::Function<void(int, ig::retained::IO&)> inputHook_;
    ct::Function<void(const Event&)> eventHook_;
    int frameIndex_ = 0;
};

} // namespace zen
} // namespace ig
