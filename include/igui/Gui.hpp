#pragma once

#include <stdint.h>

#include <ct/deque.hpp>
#include <ct/hashmap.hpp>
#include <ct/slotmap.hpp>
#include <ct/vector.hpp>

#include "Backend.hpp"
#include "Events.hpp"
#include "Math.hpp"
#include "Theme.hpp"

namespace ig
{

struct WindowState
{
    WidgetId id;
    String title;
    Rect bounds;
    bool open;
    bool focused;
    uint64_t zOrder;
    DrawList drawList;

    WindowState()
        : id(InvalidWidgetId), title(), bounds(), open(true), focused(false),
          zOrder(0), drawList()
    {
    }
};

using WindowHandle = ct::Handle<WindowState>;

class Context
{
public:
    explicit Context(Backend &backend, TextProvider *textProvider = nullptr);

    void pushEvent(const Event &event);
    void beginFrame(const FrameInfo &frame);
    const DrawData &endFrame();

    bool beginWindow(StringView title, const Rect &initialBounds, bool *open = nullptr);
    void endWindow();

    void pushId(uint64_t id);
    void pushId(StringView id);
    void popId();

    void setTheme(const Theme &theme);
    const Theme &theme() const;
    bool wantsPointer() const;
    bool wantsKeyboard() const;
    bool wantsTextInput() const;

    bool button(StringView label, const Rect &bounds);
    bool checkbox(StringView label, bool &value, const Rect &bounds);
    void label(StringView text, const Vec2 &position);

private:
    struct PointerState
    {
        Vec2 position;
        Vec2 pressedPosition[3];
        Vec2 releasedPosition[3];
        bool down[3];
        bool pressed[3];
        bool released[3];
        float wheelX;
        float wheelY;

        PointerState();
    };

    Backend &backend_;
    TextProvider *textProvider_;
    Theme theme_;
    FrameInfo frame_;
    PointerState pointer_;
    ct::Deque<Event> events_;
    ct::SlotMap<WindowState> windows_;
    ct::HashMap<WidgetId, WindowHandle> windowsById_;
    ct::Vector<WindowHandle> windowOrder_;
    ct::Vector<WidgetId> idStack_;
    DrawList frameDrawList_;
    DrawData drawData_;
    WindowHandle currentWindow_;
    WindowHandle focusedWindow_;
    WidgetId activeWidget_;
    WidgetId hotWidget_;
    WidgetId focusedWidget_;
    uint64_t frameNumber_;
    uint64_t nextZOrder_;
    bool wantsKeyboard_;
    bool wantsTextInput_;

    static WidgetId hashText(StringView text);
    static WidgetId combineIds(WidgetId a, WidgetId b);
    WidgetId makeWidgetId(StringView label) const;

    void consumeEvents();
    WindowState *currentWindow();
    const WindowState *currentWindow() const;
    DrawList *currentDrawList();
    TextMetrics measureText(FontId font, StringView text, float logicalSize) const;
    void drawText(DrawList &drawList, FontId font, StringView text, const Vec2 &position,
                  float logicalSize, const Color &color, const Rect &clip);
    WindowState *getOrCreateWindow(WidgetId id, StringView title, const Rect &bounds);
    Rect contentRect(const Rect &local) const;
    Rect contentClip() const;
    bool itemHovered(const Rect &rect, const Rect &clip, WidgetId id);
    bool itemClicked(const Rect &rect, const Rect &clip, WidgetId id);
    WindowHandle topWindowAt(const Vec2 &position) const;
    void focusWindow(WindowHandle handle);
    bool currentWindowReceivesPointer() const;
    void drawWindow(WindowState &window);
    static uint32_t buttonIndex(PointerButton button);
};

} // namespace ig
