// A headless window for the retained widgets: the real frame loop of the zen
// adapter (NewFrame, WidgetApp::update, WidgetApp::paint, Render) with a real
// font atlas (the dynamic page included) and fake texture handles, so layout,
// text measuring, input dispatch and painting all run as they do in an
// application, without a screen. No standard library: ct and C only.
#pragma once

#include <igui/widgets/Retained.hpp>
#include <igui/widgets/Theme.hpp>
#include <igui/widgets/WidgetApp.hpp>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace rh
{

using namespace ig::retained;

// One counter for the whole process: a failed check names itself and goes on.
inline int& failures()
{
    static int n = 0;
    return n;
}

inline void check(bool ok, const char* what)
{
    if (!ok)
    {
        ++failures();
        printf("  FAIL %s\n", what);
    }
}

inline TextureHandle handle(uintptr_t v)
{
    TextureHandle h;
    h.value = v;
    return h;
}

inline String& clipboardText()
{
    static String text;
    return text;
}

inline float minf(float a, float b)
{
    return a < b ? a : b;
}
inline float maxf(float a, float b)
{
    return a > b ? a : b;
}
inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// A small deterministic generator (xorshift32): the same seed replays the same run.
struct Rng
{
    uint32_t s;
    explicit Rng(uint32_t seed = 1u) : s(seed ? seed : 0x9e3779b9u) {}
    uint32_t next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    int below(int n)
    {
        return n <= 1 ? 0 : static_cast<int>(next() % static_cast<uint32_t>(n));
    }
    float unit()
    {
        return static_cast<float>(next() & 0xffffffu) / 16777216.0f;
    }
    float range(float lo, float hi)
    {
        return lo + (hi - lo) * unit();
    }
    bool chance(int percent)
    {
        return below(100) < percent;
    }
};

// The end of a test program: the window state and the context go, so a leak
// checker sees only what a test itself leaked.
inline void shutdown()
{
    WidgetApp::instance().releaseSurface();
    DestroyContext(GetCurrentContext());
}

class Harness
{
public:
    explicit Harness(float width = 1280.0f, float height = 720.0f, bool dynamicFonts = true)
            : app(WidgetApp::instance())
    {
        app.init();
        app.releaseSurface(); // a clean window, whatever an earlier test left
        atlasOk_ = atlas_.buildDefault();
        atlas_.setTexture(handle(1));
        if (dynamicFonts && atlas_.prepareDynamic()) atlas_.enableDynamic(handle(2));
        IO& io = GetIO();
        io = IO();
        io.displayWidth = width;
        io.displayHeight = height;
        io.setClipboardText = [](const char* text) { clipboardText() = String(text ? text : ""); };
        io.getClipboardText = []() { return clipboardText(); };
        stage_ = app.addStage("harness");
        app.setStage("harness", TransitionType::None);
        frame();
    }

    ~Harness()
    {
        app.releaseSurface();
        // The atlas goes with this object: the manager must not keep its font.
        IO& io = GetIO();
        io = IO();
    }

    Harness(const Harness&) = delete;
    Harness& operator=(const Harness&) = delete;

    bool ok() const
    {
        return atlasOk_;
    }
    Widget* root() const
    {
        return app.root();
    }
    IO& io()
    {
        return GetIO();
    }
    const Font* font() const
    {
        return &atlas_.defaultFont();
    }
    FontAtlas& atlas()
    {
        return atlas_;
    }
    int frames() const
    {
        return frames_;
    }

    // One frame, as ig::zen::App::frameWindow runs it.
    void frame()
    {
        NewFrame();
        app.update(GetIO());
        app.paint(*GetDrawData(), &atlas_.defaultFont(), nullptr);
        if (atlas_.dynamicDirty()) atlas_.clearDynamicDirty();
        Render();
        ++frames_;
    }

    // Tree changes do not ask for a layout by themselves (the application does): do it, then draw.
    void relayout()
    {
        app.requestLayout();
        frame();
    }

    void frames(int n)
    {
        for (int i = 0; i < n; ++i) frame();
    }

    void resize(float w, float h)
    {
        io().displayWidth = w;
        io().displayHeight = h;
        frame();
    }

    void moveTo(float x, float y)
    {
        io().mouseX = x;
        io().mouseY = y;
        frame();
    }
    void down(int button = 0)
    {
        io().mouseDown[button] = true;
        frame();
    }
    void up(int button = 0)
    {
        io().mouseDown[button] = false;
        frame();
    }
    void click(float x, float y, int button = 0)
    {
        moveTo(x, y);
        down(button);
        up(button);
    }
    void wheel(float dy, float dx = 0.0f)
    {
        io().mouseWheelX = dx;
        io().mouseWheelY = dy;
        frame();
    }
    void key(int code, bool shift = false, bool ctrl = false, bool alt = false)
    {
        io().keyShift = shift;
        io().keyCtrl = ctrl;
        io().keyAlt = alt;
        io().addKeyEvent(code, code, shift, ctrl, alt, true);
        io().addKeyEvent(code, code, shift, ctrl, alt, false);
        frame();
        io().keyShift = io().keyCtrl = io().keyAlt = false;
    }
    // Typed text, as codepoints (UTF-8 in, so a test can write "Música").
    void type(const char* utf8)
    {
        const unsigned char* s = reinterpret_cast<const unsigned char*>(utf8);
        while (*s)
        {
            uint32_t cp = *s;
            int n = 1;
            if (*s >= 0xF0) { cp = *s & 0x07u; n = 4; }
            else if (*s >= 0xE0) { cp = *s & 0x0Fu; n = 3; }
            else if (*s >= 0xC0) { cp = *s & 0x1Fu; n = 2; }
            int used = 1; // (a sequence cut short by the end of the text takes what is there, and the walk stops at the NUL)
            for (int i = 1; i < n && s[i]; ++i, ++used) cp = (cp << 6) | (s[i] & 0x3Fu);
            s += used;
            io().addInputCharacter(cp);
        }
        frame();
    }

    // The widget the pointer would hit at a point (the centre of a widget by default).
    static void centre(const Widget* w, float& x, float& y)
    {
        const Rect r = w->absoluteRect();
        x = r.x + r.w * 0.5f;
        y = r.y + r.h * 0.5f;
    }
    void clickWidget(const Widget* w, int button = 0)
    {
        float x = 0, y = 0;
        centre(w, x, y);
        click(x, y, button);
    }

    WidgetApp& app;

private:
    FontAtlas atlas_;
    bool atlasOk_ = false;
    Widget* stage_ = nullptr;
    int frames_ = 0;
};

} // namespace rh
