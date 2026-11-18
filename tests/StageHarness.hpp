// A holder widget that fills a window and what is needed to point at widgets in it by feeding events of the app (mode 0, IN A FRAME:
// the real dispatcher, the deletions wait for the end of the frame phase) or by calling the widgets' own handlers (mode 1, OUTSIDE A
// FRAME: what a program does with setValue() or what a test does with onMouseRelease(); a widget removed from its parent is deleted
// at once, so one that touches itself after a signal whose handler removed it reads freed memory, which AddressSanitizer stops).
// Used by the tests of the handlers that remove their own widget (round 6 and 7). No standard library: ct and C only.
#pragma once

#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"

namespace rh
{

inline const char* modeName(int mode)
{
    return mode == 0 ? "in a frame" : "outside a frame";
}

inline String say(const char* what, int mode)
{
    return String(what) + ", " + modeName(mode);
}

struct Stage
{
    explicit Stage(int mode_) : screen(480.0f, 260.0f), h(screen.h), app(WidgetApp::instance()), mode(mode_)
    {
        holder = h.root()->createChild<Widget>();
        holder->setRect(Rect(0.0f, 0.0f, 480.0f, 260.0f));
    }
    Harness& harness()
    {
        return h;
    }
    // The widget the case is about, when it is not the only child of the holder: gone() then asks the app (WidgetApp::hasGone)
    void watch(Widget* w)
    {
        watched = w;
        watchedSeen = app.deletions();
    }
    bool gone() const
    {
        return watched ? app.hasGone(watched, watchedSeen) : holder->children().empty();
    }
    // the pointer: moved (a frame), or just remembered (the widget is called directly)
    void move(float x, float y)
    {
        px = x;
        py = y;
        if (mode == 0) app.feedMouseMove(x, y);
    }
    // the pointer at a place of a widget (its rectangle's corner plus the offset), by the widget's rectangle as it is now
    void moveTo(const Widget* target, float dx, float dy)
    {
        const Rect r = target->absoluteRect();
        move(r.x + dx, r.y + dy);
    }
    // a button down and up on a widget, as an event of the app (mode 0) or as a call of its handlers (mode 1)
    void down(Widget* target, int button = 0)
    {
        if (mode == 0)
        {
            app.feedMouseButton(button, true);
            return;
        }
        if (gone()) return;
        MouseEvent e = event(target, button);
        target->onMousePress(e);
    }
    void up(Widget* target, int button = 0)
    {
        if (mode == 0)
        {
            app.feedMouseButton(button, false);
            return;
        }
        if (gone()) return;
        MouseEvent e = event(target, button);
        target->onMouseRelease(e);
    }
    void drag(Widget* target, float x, float y)
    {
        move(x, y);
        if (mode == 1 && !gone())
        {
            MouseEvent e = event(target, 0);
            target->onMouseMove(e);
        }
    }
    // a click at a place of the widget
    void click(Widget* target, float dx, float dy, int button = 0)
    {
        moveTo(target, dx, dy);
        down(target, button);
        up(target, button);
    }
    // the wheel over the widget (the pointer is where it was last put)
    void wheel(Widget* target, float dy)
    {
        if (mode == 0)
        {
            app.feedMouseWheel(0.0f, dy);
            return;
        }
        if (gone()) return;
        MouseEvent e = event(target, 0);
        e.scrollY = dy;
        target->onMouseScroll(e);
    }
    // a key down and up: the widget takes the focus first (mode 0) or is called (mode 1)
    void key(Widget* target, int k, bool shift = false, bool ctrl = false)
    {
        if (mode == 0)
        {
            app.setFocused(target);
            app.feedKey(k, true, shift, ctrl);
            app.feedKey(k, false, shift, ctrl);
            return;
        }
        if (gone()) return;
        KeyEvent ke;
        ke.key = k;
        ke.shift = shift;
        ke.ctrl = ctrl;
        target->onKeyPress(ke);
        if (gone()) return;
        KeyEvent kr;
        kr.key = k;
        target->onKeyRelease(kr);
    }
    // typed text (UTF-8): fed to the focused widget (mode 0) or to onTextInput of the widget (mode 1)
    void type(Widget* target, const char* utf8)
    {
        if (mode == 0)
        {
            app.setFocused(target);
            app.feedText(utf8);
            return;
        }
        if (gone()) return;
        KeyEvent te;
        size_t i = 0;
        for (; utf8[i] && i + 1 < sizeof te.text; ++i) te.text[i] = utf8[i];
        te.text[i] = 0;
        target->onTextInput(te);
    }
    // something the program does through a widget's API (setValue, setSelectedIndex, removeItem): in mode 0 from inside a frame (a call
    // deferred to the events of the next update(), where the deletions wait for the end of the phase), in mode 1 right here
    template<class F>
    void act(F fn)
    {
        if (mode == 0)
        {
            app.callDeferred(holder, ct::Function<void()>(fn));
            h.frames(2);
            return;
        }
        fn();
    }
    MouseEvent event(Widget* target, int button) const
    {
        MouseEvent e;
        e.x = px;
        e.y = py;
        e.button = button;
        const Rect r = target->absoluteRect();
        e.localX = px - r.x;
        e.localY = py - r.y;
        return e;
    }
    Screen screen;
    Harness& h;
    WidgetApp& app;
    int mode;
    Widget* holder = nullptr;
    Widget* watched = nullptr;
    uint32_t watchedSeen = 0;
    float px = 0.0f, py = 0.0f;
};

} // namespace rh
